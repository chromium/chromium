// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/webui_contents_container.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_perf_traits_tracker.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "printing/buildflags/buildflags.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/webview/webview.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_init.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#endif

namespace glic {

namespace {
content::WebContents::CreateParams MakeCreateParams(Profile* profile,
                                                    bool initially_hidden) {
  auto params = content::WebContents::CreateParams(profile);
  params.initially_hidden = initially_hidden;
  return params;
}

SkColor GetGlicBackgroundColor(Profile* profile,
                               const ui::ColorProvider& color_provider) {
#if !BUILDFLAG(IS_ANDROID)
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile);
  bool use_dark = theme_service->BrowserUsesDarkColors();

  ui::ColorProviderKey key;
  key.color_mode = use_dark ? ui::ColorProviderKey::ColorMode::kDark
                            : ui::ColorProviderKey::ColorMode::kLight;

  const ui::ColorProvider* explicit_color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(key);

  return explicit_color_provider->GetColor(kColorGlicBackground);
#else
  return color_provider.GetColor(kColorGlicBackground);
#endif
}

}  // namespace

WebUIContentsContainer::WebUIContentsContainer()
    : creation_time_(base::TimeTicks::Now()) {}
WebUIContentsContainer::~WebUIContentsContainer() = default;

WebUIContentsContainerImpl::WebUIContentsContainerImpl(Profile* profile,
                                                       bool initially_hidden)
    : profile_keep_alive_(profile, ProfileKeepAliveOrigin::kGlicView),
      web_contents_(content::WebContents::Create(
          MakeCreateParams(profile, initially_hidden))),
      web_contents_ptr_(web_contents_.get()),
      profile_(profile) {
  TRACE_EVENT_INSTANT("glic",
                      "WebUIContentsContainerImpl::WebUIContentsContainerImpl",
                      perfetto::Flow::FromPointer(this));
  CHECK(web_contents_);
  CreateGlicWebUiData(web_contents_.get());
  Observe(web_contents_.get());
  PrefsTabHelper::CreateForWebContents(web_contents_.get());
  web_contents_->SetPageBaseBackgroundColor(
      GetGlicBackgroundColor(profile, web_contents_->GetColorProvider()));

  web_contents_->SetSupportsDraggableRegions(true);

#if !BUILDFLAG(IS_ANDROID)
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_contents_.get());
#endif

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrintingForWebContents(web_contents_.get());
#endif

  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL{chrome::kChromeUIGlicURL}));
}

WebUIContentsContainerImpl::~WebUIContentsContainerImpl() {
  Observe(nullptr);
  if (web_contents_) {
    web_contents_->ClosePage();
  }
}

void WebUIContentsContainerImpl::AttachToHost(Host* host) {
  // This is only allowed to be called once.
  CHECK(!host_);
  host_ = host;
  if (auto* glic_ui = GlicUI::From(web_contents())) {
    glic_ui->AttachToHost(host);
  }
}

void WebUIContentsContainerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    TRACE_EVENT_INSTANT(
        "glic",
        "WebUIContentsContainerImpl::DidFinishNavigation - PrimaryMainFrame",
        perfetto::Flow::FromPointer(this));
    navigation_commit_time_ = base::TimeTicks::Now();
    base::UmaHistogramTimes("Glic.Contents.NavigationCommitTime",
                            navigation_commit_time_ - creation_time_);
  }
  if (!host_ || !navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

#if BUILDFLAG(ENABLE_PRINTING)
  printing::InitializePrintingForWebContents(web_contents());
#endif

  host_->OnWebContentsNavigated();

  // Re-attach to the (possibly new) GlicUI.
  if (auto* glic_ui = GlicUI::From(web_contents())) {
    glic_ui->AttachToHost(host_);
  }
}

void WebUIContentsContainerImpl::PrimaryMainDocumentElementAvailable() {
  TRACE_EVENT_INSTANT(
      "glic", "WebUIContentsContainerImpl::PrimaryMainDocumentElementAvailable",
      perfetto::Flow::FromPointer(this));
}

void WebUIContentsContainerImpl::DocumentOnLoadCompletedInPrimaryMainFrame() {
  TRACE_EVENT_INSTANT(
      "glic",
      "WebUIContentsContainerImpl::DocumentOnLoadCompletedInPrimaryMainFrame",
      perfetto::Flow::FromPointer(this));
  base::UmaHistogramTimes("Glic.Contents.LoadCompleteTime",
                          base::TimeTicks::Now() - navigation_commit_time_);
}

void WebUIContentsContainerImpl::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  TRACE_EVENT_INSTANT(
      "glic", "WebUIContentsContainerImpl::PrimaryMainFrameRenderProcessGone",
      perfetto::TerminatingFlow::FromPointer(this), "status", status);
  base::UmaHistogramEnumeration("Glic.Session.WebUiCrash.TerminationStatus",
                                status, base::TERMINATION_STATUS_MAX_ENUM);
  if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    base::RecordAction(base::UserMetricsAction("GlicSessionWebUiCrash"));
  }
  // During browser shutdown, skip cleaning up keyed services as they may
  // already be partially destroyed.
  if (browser_shutdown::HasShutdownStarted()) {
    return;
  }
  auto* keyed_service = GlicKeyedServiceFactory::GetGlicKeyedService(profile_);
  // TODO(crbug.com/454120908): swap for a reloaded host in case of a crash.
  keyed_service->CloseAndShutdown(web_contents()->GetPrimaryMainFrame());
  // WARNING: Do not do any more work, as `this` may have been destroyed.
}

void WebUIContentsContainerImpl::WebContentsDestroyed() {
  web_contents_ptr_ = nullptr;
}

void WebUIContentsContainerImpl::SetVisibility(content::Visibility visibility) {
  web_contents()->UpdateWebContentsVisibility(visibility);
}

content::WebContents* WebUIContentsContainerImpl::web_contents() const {
  return web_contents_ptr_;
}

void WebUIContentsContainerImpl::OnActuatingChanged(bool actuating) {
  if (!actuating) {
    // Cleanup the capturers even if the webcontents are gone.
    webui_capture_runner_.RunAndReset();
    guest_capture_runner_.RunAndReset();
  }
  if (!web_contents()) {
    return;
  }
  auto* guest = GetGlicGuestWebContents(web_contents());
  if (!guest) {
    return;
  }
  is_actuating_ = actuating;
  if (actuating && !webui_capture_runner_) {
    webui_capture_runner_ = web_contents()->IncrementCapturerCount(
        gfx::Size(), /*stay_hidden=*/true, /*stay_awake=*/true,
        /*is_activity=*/true);
    guest_capture_runner_ = guest->IncrementCapturerCount(
        gfx::Size(), /*stay_hidden=*/true, /*stay_awake=*/true,
        /*is_activity=*/true);
  }

  UpdateActuationTracker();
}

void WebUIContentsContainerImpl::OnTaskTabsVisibilityChanged(
    bool has_visible_tab) {
  is_actuating_on_visible_tab_ = has_visible_tab;
  UpdateActuationTracker();
}

void WebUIContentsContainerImpl::UpdateActuationTracker() {
  auto* guest = GetGlicGuestWebContents(web_contents());
  if (!guest) {
    // Visibility might change before the guest is created or after it is
    // teared down. In both cases, there is no point in tracking the actuation
    // state.
    return;
  }
  GlicActuationState state = GlicActuationState::kNone;
  if (is_actuating_) {
    state = is_actuating_on_visible_tab_
                ? GlicActuationState::kActuatingOnVisibleTab
                : GlicActuationState::kActuatingOnBackgroundTab;
  }
  glic::GlicPerfTraitsTracker::GetInstance()->NotifyActuationStateChanged(
      web_contents(), state);
  glic::GlicPerfTraitsTracker::GetInstance()->NotifyActuationStateChanged(
      guest, state);
}

std::unique_ptr<content::WebContents>
WebUIContentsContainerImpl::ReleaseWebContents() {
  CHECK(web_contents_);
  return std::move(web_contents_);
}

void WebUIContentsContainerImpl::ReclaimWebContents(
    std::unique_ptr<content::WebContents> web_contents) {
  CHECK(!web_contents_);
  CHECK(web_contents);
  web_contents_ = std::move(web_contents);
}

}  // namespace glic
