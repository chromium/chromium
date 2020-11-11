// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_ui_tab_helper.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_content_util.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/network_time/network_time_tracker.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

const void* const kOptOutEventKey = 0;

// Adds the preview navigation to the black list.
void AddPreviewNavigationCallback(content::BrowserContext* browser_context,
                                  const GURL& url,
                                  previews::PreviewsType type,
                                  uint64_t page_id,
                                  bool opt_out) {
  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (previews_service && previews_service->previews_ui_service()) {
    previews_service->previews_ui_service()->AddPreviewNavigation(
        url, type, opt_out, page_id);
  }
}

void InformPLMOfOptOut(content::WebContents* web_contents) {
  page_load_metrics::MetricsWebContentsObserver* metrics_web_contents_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents);
  if (!metrics_web_contents_observer)
    return;

  metrics_web_contents_observer->BroadcastEventToObservers(
      PreviewsUITabHelper::OptOutEventKey());
}

}  // namespace

PreviewsUITabHelper::~PreviewsUITabHelper() {
  // Report a non-opt out for the previous page if it was a preview and was not
  // reloaded without previews.
  if (!on_dismiss_callback_.is_null()) {
    std::move(on_dismiss_callback_).Run(false);
  }
}

PreviewsUITabHelper::PreviewsUITabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void PreviewsUITabHelper::ShowUIElement(
    previews::PreviewsType previews_type,
    OnDismissPreviewsUICallback on_dismiss_callback) {
  on_dismiss_callback_ = std::move(on_dismiss_callback);
  displayed_preview_ui_ = true;
#if defined(OS_ANDROID)
  should_display_android_omnibox_badge_ = true;
#endif

  // Record a local histogram for testing.
  base::BooleanHistogram::FactoryGet(
      base::StringPrintf("Previews.PreviewShown.%s",
                         GetStringNameForType(previews_type).c_str()),
      base::HistogramBase::kNoFlags)
      ->Add(true);
}

void PreviewsUITabHelper::ReloadWithoutPreviews() {
  DCHECK(GetPreviewsUserData());
  ReloadWithoutPreviews(GetPreviewsUserData()->CommittedPreviewsType());
}

void PreviewsUITabHelper::ReloadWithoutPreviews(
    previews::PreviewsType previews_type) {
  InformPLMOfOptOut(web_contents());
#if defined(OS_ANDROID)
  should_display_android_omnibox_badge_ = false;
#endif
  if (on_dismiss_callback_)
    std::move(on_dismiss_callback_).Run(true);
  switch (previews_type) {
    case previews::PreviewsType::NOSCRIPT:
    case previews::PreviewsType::RESOURCE_LOADING_HINTS:
    case previews::PreviewsType::DEFER_ALL_SCRIPT:
      // Previews may cause a redirect, so we should use the original URL. The
      // black list prevents showing the preview again.
      web_contents()->GetController().Reload(
          content::ReloadType::ORIGINAL_REQUEST_URL, true);
      break;
    case previews::PreviewsType::NONE:
    case previews::PreviewsType::UNSPECIFIED:
    case previews::PreviewsType::LAST:
    case previews::PreviewsType::DEPRECATED_AMP_REDIRECTION:
    case previews::PreviewsType::DEPRECATED_LITE_PAGE:
    case previews::PreviewsType::DEPRECATED_LITE_PAGE_REDIRECT:
    case previews::PreviewsType::DEPRECATED_LOFI:
    case previews::PreviewsType::DEPRECATED_OFFLINE:
      NOTREACHED();
      break;
  }
}

void PreviewsUITabHelper::MaybeRecordPreviewReload(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetReloadType() == content::ReloadType::NONE)
    return;
  previews::PreviewsUserData* previews_user_data = GetPreviewsUserData();
  if (!previews_user_data)
    return;
  if (!previews_user_data->HasCommittedPreviewsType())
    return;
  if (previews_user_data->coin_flip_holdback_result() ==
      previews::CoinFlipHoldbackResult::kHoldback) {
    return;
  }

  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (previews_service && previews_service->previews_ui_service()) {
    previews_service->previews_ui_service()
        ->previews_decider_impl()
        ->AddPreviewReload();
  }
}

void PreviewsUITabHelper::MaybeShowInfoBar(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));

  if (!previews_service)
    return;

  PreviewsHTTPSNotificationInfoBarDecider* decider =
      previews_service->previews_https_notification_infobar_decider();

  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());

  if (!drp_settings)
    return;

  if (drp_settings->IsDataReductionProxyEnabled() &&
      decider->NeedsToNotifyUser()) {
    decider->NotifyUser(web_contents());
  }
}

void PreviewsUITabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  MaybeRecordPreviewReload(navigation_handle);

  MaybeShowInfoBar(navigation_handle);
}

void PreviewsUITabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Delete Previews information later, so that other DidFinishNavigation
  // methods can reliably use GetPreviewsUserData regardless of order of
  // WebContentsObservers.
  // Note that a lot of Navigations (sub-frames, same document, non-committed,
  // etc.) might not have PreviewsUserData associated with them, but we reduce
  // likelihood of future leaks by always trying to remove the data.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PreviewsUITabHelper::RemovePreviewsUserData,
                                weak_factory_.GetWeakPtr(),
                                navigation_handle->GetNavigationId()));

  // Only show the ui if this is a full main frame navigation.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() || navigation_handle->IsSameDocument())
    return;

  // Report a non-opt out for the previous page if it was a preview and was not
  // reloaded without previews.
  if (!on_dismiss_callback_.is_null()) {
    std::move(on_dismiss_callback_).Run(false);
  }

#if defined(OS_ANDROID)
  should_display_android_omnibox_badge_ = false;
#endif
  previews::PreviewsUserData* navigation_data =
      GetPreviewsUserData(navigation_handle);

  previews::PreviewsUserData* previews_user_data = nullptr;
  // If the navigation is served from the back-forward cache, we should use the
  // previews data from the first time we navigated to the page.
  if (navigation_handle->IsServedFromBackForwardCache()) {
    auto* holder =
        previews::PreviewsUserData::DocumentDataHolder::GetForCurrentDocument(
            navigation_handle->GetRenderFrameHost());
    if (holder)
      previews_user_data = holder->GetPreviewsUserData();
  } else if (navigation_data) {
    // Otherwise, we should store Previews information for this navigation.
    auto* holder = previews::PreviewsUserData::DocumentDataHolder::
        GetOrCreateForCurrentDocument(navigation_handle->GetRenderFrameHost());
    holder->SetPreviewsUserData(
        std::make_unique<previews::PreviewsUserData>(*navigation_data));
    previews_user_data = holder->GetPreviewsUserData();
  }

  uint64_t page_id = (previews_user_data) ? previews_user_data->page_id() : 0;

  displayed_preview_ui_ = false;

  // Check for committed main frame preview.
  if (previews_user_data && previews_user_data->HasCommittedPreviewsType()) {
    previews::PreviewsType main_frame_preview =
        previews_user_data->CommittedPreviewsType();
    if (main_frame_preview != previews::PreviewsType::NONE) {
      ShowUIElement(main_frame_preview,
                    base::BindOnce(&AddPreviewNavigationCallback,
                                   web_contents()->GetBrowserContext(),
                                   navigation_handle->GetRedirectChain()[0],
                                   main_frame_preview, page_id));
    }
  }
}

previews::PreviewsUserData*
PreviewsUITabHelper::CreatePreviewsUserDataForNavigationHandle(
    content::NavigationHandle* navigation_handle,
    int64_t page_id) {
  inflight_previews_user_datas_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(navigation_handle->GetNavigationId()),
      std::forward_as_tuple(page_id));

  auto data =
      inflight_previews_user_datas_.find(navigation_handle->GetNavigationId());

  return data == inflight_previews_user_datas_.end() ? nullptr : &data->second;
}

previews::PreviewsUserData* PreviewsUITabHelper::GetPreviewsUserData(
    content::NavigationHandle* navigation_handle) {
  auto data =
      inflight_previews_user_datas_.find(navigation_handle->GetNavigationId());
  return data == inflight_previews_user_datas_.end() ? nullptr
                                                     : &(data->second);
}

void PreviewsUITabHelper::RemovePreviewsUserData(int64_t navigation_id) {
  inflight_previews_user_datas_.erase(navigation_id);
}

previews::PreviewsUserData* PreviewsUITabHelper::GetPreviewsUserData() const {
  auto* holder =
      previews::PreviewsUserData::DocumentDataHolder::GetForCurrentDocument(
          web_contents()->GetMainFrame());
  return holder ? holder->GetPreviewsUserData() : nullptr;
}

// static
const void* PreviewsUITabHelper::OptOutEventKey() {
  return &kOptOutEventKey;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PreviewsUITabHelper)
