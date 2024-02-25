// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"

#include <tuple>
#include <utility>

#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/ukm/app_source_url_recorder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/badging/badge_manager_delegate_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/browser/badging/badge_manager_delegate_win.h"
#endif

using web_app::WebAppProvider;

namespace {

bool IsLastBadgingTimeWithin(base::TimeDelta time_frame,
                             const webapps::AppId& app_id,
                             const base::Clock* clock,
                             Profile* profile) {
  const base::Time last_badging_time =
      WebAppProvider::GetForLocalAppsUnchecked(profile)
          ->registrar_unsafe()
          .GetAppLastBadgingTime(app_id);
  return clock->Now() < last_badging_time + time_frame;
}

// When web apps are disabled, there is no WebAppProvider.
web_app::WebAppSyncBridge* GetWebAppSyncBridgeForProfile(Profile* profile) {
  auto* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  return provider ? &provider->sync_bridge_unsafe() : nullptr;
}

}  // namespace

namespace badging {

BadgeManager::BadgeManager(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {
  // The delegate is also set for Chrome OS but is set from the constructor of
  // web_apps_chromeos.cc.
#if BUILDFLAG(IS_MAC)
  SetDelegate(std::make_unique<BadgeManagerDelegateMac>(profile, this));
#elif BUILDFLAG(IS_WIN)
  SetDelegate(std::make_unique<BadgeManagerDelegateWin>(profile, this));
#endif
}

BadgeManager::~BadgeManager() = default;

void BadgeManager::SetDelegate(std::unique_ptr<BadgeManagerDelegate> delegate) {
  delegate_ = std::move(delegate);
}

void BadgeManager::BindFrameReceiverIfAllowed(
    content::RenderFrameHost* frame,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The Badging API is not allowed for the fenced frames.
  if (frame->IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage("The Badging API is not allowed in a fenced frame");
    return;
  }

  auto* profile = Profile::FromBrowserContext(frame->GetBrowserContext());

  auto* badge_manager =
      badging::BadgeManagerFactory::GetInstance()->GetForProfile(profile);
  if (!badge_manager)
    return;

  auto context = std::make_unique<FrameBindingContext>(
      frame->GetProcess()->GetID(), frame->GetRoutingID());
  badge_manager->receivers_.Add(badge_manager, std::move(receiver),
                                std::move(context));
}

void BadgeManager::BindServiceWorkerReceiverIfAllowed(
    content::RenderProcessHost* service_worker_process_host,
    const content::ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The Badging API is not allowed for the fenced frames.
  if (info.ancestor_frame_type ==
      blink::mojom::AncestorFrameType::kFencedFrame) {
    mojo::ReportBadMessage("The Badging API is not allowed in a fenced frame");
    return;
  }

  auto* profile = Profile::FromBrowserContext(
      service_worker_process_host->GetBrowserContext());

  auto* badge_manager =
      badging::BadgeManagerFactory::GetInstance()->GetForProfile(profile);
  if (!badge_manager)
    return;

  auto context = std::make_unique<BadgeManager::ServiceWorkerBindingContext>(
      service_worker_process_host->GetID(), info.scope);

  badge_manager->receivers_.Add(badge_manager, std::move(receiver),
                                std::move(context));
}

std::optional<BadgeManager::BadgeValue> BadgeManager::GetBadgeValue(
    const webapps::AppId& app_id) {
  const auto& it = badged_apps_.find(app_id);
  if (it == badged_apps_.end())
    return std::nullopt;

  return it->second;
}

bool BadgeManager::HasRecentApiUsage(const webapps::AppId& app_id) const {
  return IsLastBadgingTimeWithin(kBadgingOverrideLifetime, app_id, clock_,
                                 profile_);
}

void BadgeManager::SetBadgeForTesting(const webapps::AppId& app_id,
                                      BadgeValue value,
                                      ukm::UkmRecorder* test_recorder) {
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  if (value == std::nullopt) {
    ukm::builders::Badging(source_id)
        .SetUpdateAppBadge(kSetFlagBadge)
        .Record(test_recorder);
  } else {
    ukm::builders::Badging(source_id)
        .SetUpdateAppBadge(kSetNumericBadge)
        .Record(test_recorder);
  }
  UpdateBadge(app_id, value);
}

void BadgeManager::ClearBadgeForTesting(const webapps::AppId& app_id,
                                        ukm::UkmRecorder* test_recorder) {
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::builders::Badging(source_id)
      .SetUpdateAppBadge(kClearBadge)
      .Record(test_recorder);
  UpdateBadge(app_id, std::nullopt);
}

const base::Clock* BadgeManager::SetClockForTesting(const base::Clock* clock) {
  const base::Clock* previous = clock_;
  clock_ = clock;
  return previous;
}

void BadgeManager::UpdateBadge(const webapps::AppId& app_id,
                               std::optional<BadgeValue> value) {
  web_app::WebAppSyncBridge* sync_bridge =
      GetWebAppSyncBridgeForProfile(profile_.get());
  if (sync_bridge &&
      !IsLastBadgingTimeWithin(badging::kBadgingMinimumUpdateInterval, app_id,
                               clock_, profile_)) {
    sync_bridge->SetAppLastBadgingTime(app_id, clock_->Now());
  }

  if (!value)
    badged_apps_.erase(app_id);
  else
    badged_apps_[app_id] = value.value();

  if (!delegate_)
    return;

  delegate_->OnAppBadgeUpdated(app_id);
}

void BadgeManager::SetBadge(blink::mojom::BadgeValuePtr mojo_value) {
  if (mojo_value->is_number() && mojo_value->get_number() == 0) {
    mojo::ReportBadMessage(
        "|value| should not be zero when it is |number| (ClearBadge should be "
        "called instead)!");
    return;
  }

  const std::vector<std::tuple<webapps::AppId, GURL>> app_ids_and_urls =
      receivers_.current_context()->GetAppIdsAndUrlsForBadging();

  // Convert the mojo badge representation into a BadgeManager::BadgeValue.
  BadgeValue value = mojo_value->is_flag()
                         ? std::nullopt
                         : std::make_optional(mojo_value->get_number());

  // ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  for (const auto& app : app_ids_and_urls) {
    GURL url = std::get<1>(app);
    // The app's start_url is used to identify the app
    // for recording badging usage per app.
    ukm::SourceId source_id = ukm::AppSourceUrlRecorder::GetSourceIdForPWA(url);
    if (value == std::nullopt) {
      ukm::builders::Badging(source_id)
          .SetUpdateAppBadge(kSetFlagBadge)
          .Record(recorder);
    } else {
      ukm::builders::Badging(source_id)
          .SetUpdateAppBadge(kSetNumericBadge)
          .Record(recorder);
    }
    ukm::AppSourceUrlRecorder::MarkSourceForDeletion(source_id);

    UpdateBadge(/*app_id=*/std::get<0>(app), std::make_optional(value));
  }
}

void BadgeManager::ClearBadge() {
  const std::vector<std::tuple<webapps::AppId, GURL>> app_ids_and_urls =
      receivers_.current_context()->GetAppIdsAndUrlsForBadging();

  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  for (const auto& app : app_ids_and_urls) {
    // The app's start_url is used to identify the app
    // for recording badging usage per app.
    GURL url = std::get<1>(app);
    ukm::SourceId source_id = ukm::AppSourceUrlRecorder::GetSourceIdForPWA(url);
    ukm::builders::Badging(source_id)
        .SetUpdateAppBadge(kClearBadge)
        .Record(recorder);
    ukm::AppSourceUrlRecorder::MarkSourceForDeletion(source_id);
    UpdateBadge(/*app_id=*/std::get<0>(app), std::nullopt);
  }
}

std::vector<std::tuple<webapps::AppId, GURL>>
BadgeManager::FrameBindingContext::GetAppIdsAndUrlsForBadging() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(process_id_, frame_id_);
  if (!frame)
    return std::vector<std::tuple<webapps::AppId, GURL>>{};

  const WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(
      Profile::FromBrowserContext(frame->GetBrowserContext()));
  if (!provider)
    return std::vector<std::tuple<webapps::AppId, GURL>>{};

  const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
  const std::optional<webapps::AppId> app_id =
      registrar.FindAppWithUrlInScope(frame->GetLastCommittedURL());
  if (!app_id)
    return std::vector<std::tuple<webapps::AppId, GURL>>{};
  return std::vector<std::tuple<webapps::AppId, GURL>>{std::make_tuple(
      app_id.value(), registrar.GetAppStartUrl(app_id.value()))};
}

std::vector<std::tuple<webapps::AppId, GURL>>
BadgeManager::ServiceWorkerBindingContext::GetAppIdsAndUrlsForBadging() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(process_id_);
  if (!render_process_host)
    return std::vector<std::tuple<webapps::AppId, GURL>>{};

  const WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(
      Profile::FromBrowserContext(render_process_host->GetBrowserContext()));
  if (!provider)
    return std::vector<std::tuple<webapps::AppId, GURL>>{};

  const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
  std::vector<std::tuple<webapps::AppId, GURL>> app_ids_urls{};
  for (const auto& app_id : registrar.FindAppsInScope(scope_)) {
    app_ids_urls.push_back(
        std::make_tuple(app_id, registrar.GetAppStartUrl(app_id)));
  }
  return app_ids_urls;
}

std::string GetBadgeString(std::optional<uint64_t> badge_content) {
  if (!badge_content)
    return "•";

  if (badge_content > kMaxBadgeContent) {
    return base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
        IDS_SATURATED_BADGE_CONTENT, base::FormatNumber(kMaxBadgeContent)));
  }

  return base::UTF16ToUTF8(base::FormatNumber(badge_content.value()));
}

}  // namespace badging
