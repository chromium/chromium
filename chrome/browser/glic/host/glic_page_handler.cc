// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_page_handler.h"

#include <cmath>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/glic_web_client_handler.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#else
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"  // nogncheck
#endif

namespace glic {

GlicPageHandler::GlicPageHandler(
    content::WebContents* webui_contents,
    Host* host,
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::Page> page)
    : host_(host),
      webui_contents_(webui_contents),
      browser_context_(webui_contents->GetBrowserContext()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  VLOG(1) << "Glic [PageHandler] Constructor";
  CHECK(host_);
  MarkProcessAsGlic(webui_contents->GetPrimaryMainFrame()->GetProcess());
  host_observation_.Observe(host_);
  host_->WebUIPageHandlerAdded(this);
  host_->instance().AddStateObserver(this);

  UpdatePageState(host_->instance().GetPanelState().kind);
  subscriptions_.push_back(
      GetGlicService()->enabling().RegisterProfileReadyStateChanged(
          base::BindRepeating(&GlicPageHandler::UpdateProfileReadyState,
                              base::Unretained(this))));
  UpdateProfileReadyState();
}

GlicPageHandler::~GlicPageHandler() {
  VLOG(1) << "Glic [PageHandler] Destructor";
  host_->instance().RemoveStateObserver(this);
  host_->WebUIPageHandlerRemoved(this);
}

content::WebContents* GlicPageHandler::webui_contents() {
  return webui_contents_;
}

Host& GlicPageHandler::host() {
  return *host_;
}

GlicKeyedService* GlicPageHandler::GetGlicService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(browser_context_);
}


void GlicPageHandler::PrepareForClient(
    base::OnceCallback<void(mojom::PrepareForClientResult)> callback) {
  TRACE_EVENT_INSTANT("glic", "GlicPageHandler::PrepareForClient - Request",
                      perfetto::Flow::FromPointer(this));

  auto wrapped_callback = base::BindOnce(
      [](base::WeakPtr<GlicPageHandler> origin_this,
         base::OnceCallback<void(mojom::PrepareForClientResult)> callback,
         mojom::PrepareForClientResult result) {
        if (origin_this) {
          TRACE_EVENT_INSTANT(
              "glic", "GlicPageHandler::PrepareForClient - Response",
              perfetto::TerminatingFlow::FromPointer(origin_this.get()));
        }
        std::move(callback).Run(std::move(result));
      },
      this->weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  if (auto* auth_controller = GetGlicService()->GetAuthController()) {
    auth_controller->CheckAuthBeforeLoad(std::move(wrapped_callback));
  } else {
    std::move(wrapped_callback).Run(mojom::PrepareForClientResult::kSuccess);
  }
}

void GlicPageHandler::WebviewCommitted(const GURL& url) {
  VLOG(1) << "Glic [PageHandler] WebviewCommitted, url=" << url.spec();
  // TODO(crbug.com/388328847): Remove this code once launch issues are ironed
  // out.
  if (url.DomainIs("login.corp.google.com") ||
      url.DomainIs("accounts.google.com")) {
    host().LoginPageCommitted(this);
  }
}

void GlicPageHandler::OnZoomLevelChange(double zoom_factor) {
  // LINT.IfChange(GlicZoomFactors)
  // Ignore values outside of the supported range (defined in glic/webview.ts).
  if (zoom_factor < 1.0 || zoom_factor > 2.0) {
    LOG(ERROR) << "Glic [PageHandler] Invalid zoom level: " << zoom_factor;
    return;
  }
  int zoom_percent = std::round(zoom_factor * 100);
  auto* pref_service =
      Profile::FromBrowserContext(browser_context_)->GetPrefs();
  // The webui sends a zoom level change on initialization. Skip these.
  if (pref_service->GetInteger(prefs::kGlicZoomLevel) != zoom_percent) {
    // Note that zoom level is already persisted in the glic webview partition -
    // this pref is only used for metrics.
    pref_service->SetInteger(prefs::kGlicZoomLevel, zoom_percent);
    host().instance_metrics().OnZoomLevelChange();
  }
  // LINT.ThenChange(//chrome/browser/resources/glic/webview.ts:GlicZoomFactors,//chrome/browser/glic/host/guest_util.cc:GlicZoomFactors)
}

void GlicPageHandler::NotifyWindowIntentToShow() {
  page_->IntentToShow();
}

void GlicPageHandler::Zoom(mojom::ZoomAction zoom_action, ZoomSource source) {
  auto* pref_service =
      Profile::FromBrowserContext(browser_context_)->GetPrefs();
  int current_zoom = pref_service->GetInteger(prefs::kGlicZoomLevel);

  GlicZoomAction action_metric;
  switch (zoom_action) {
    case mojom::ZoomAction::kZoomIn:
      if (current_zoom >= 200) {
        action_metric = GlicZoomAction::kZoomInAtMax;
        base::RecordAction(base::UserMetricsAction("Glic.ZoomInAtMax"));
      } else {
        action_metric = GlicZoomAction::kZoomIn;
        base::RecordAction(base::UserMetricsAction("Glic.ZoomIn"));
      }
      break;
    case mojom::ZoomAction::kZoomOut:
      if (current_zoom <= 100) {
        action_metric = GlicZoomAction::kZoomOutAtMin;
        base::RecordAction(base::UserMetricsAction("Glic.ZoomOutAtMin"));
      } else {
        action_metric = GlicZoomAction::kZoomOut;
        base::RecordAction(base::UserMetricsAction("Glic.ZoomOut"));
      }
      break;
    case mojom::ZoomAction::kReset:
      action_metric = GlicZoomAction::kReset;
      base::RecordAction(base::UserMetricsAction("Glic.ZoomReset"));
      break;
  }

  // Log the aggregate base metric for reporting continuity.
  base::UmaHistogramEnumeration("Glic.ZoomAction", action_metric);

  // Log the sliced metric.
  const char* zoom_action_by_source_metric_name;
  switch (source) {
    case ZoomSource::kHotkey:
      zoom_action_by_source_metric_name = "Glic.ZoomAction.Hotkey";
      break;
    case ZoomSource::kHotkeyWithShift:
      zoom_action_by_source_metric_name = "Glic.ZoomAction.HotkeyWithShift";
      break;
    case ZoomSource::kScroll:
      zoom_action_by_source_metric_name = "Glic.ZoomAction.Scroll";
      break;
  }

  base::UmaHistogramEnumeration(zoom_action_by_source_metric_name,
                                action_metric);

  page_->Zoom(zoom_action);
}

void GlicPageHandler::SetProfileReadyState(
    glic::mojom::ProfileReadyState ready_state) {
  page_->SetProfileReadyState(ready_state);
}

void GlicPageHandler::ClosePanel(ClosePanelCallback callback) {
  host().ClosePanel();
  std::move(callback).Run();
}

void GlicPageHandler::OpenProfilePickerAndClosePanel() {
  glic::GlicProfileManager::GetInstance()->ShowProfilePicker();
  host().ClosePanel();
}

void GlicPageHandler::OpenDisabledByAdminLinkAndClosePanel() {
  GURL disabled_by_admin_link_url = GURL(features::kGlicCaaLinkUrl.Get());
  std::unique_ptr<NavigateParams> params = std::make_unique<NavigateParams>(
      Profile::FromBrowserContext(browser_context_), disabled_by_admin_link_url,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  glic::NavigateAsync(std::move(params), base::DoNothing());
  host().ClosePanel();
  base::RecordAction(
      base::UserMetricsAction("Glic.DisabledByAdminPanelLinkClicked"));
}

void GlicPageHandler::OpenLinkInPopup(const GURL& url,
                                      int32_t popup_width,
                                      int32_t popup_height) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  gfx::NativeView native_view = webui_contents_->GetContentNativeView();
  const display::Display& display =
      display::Screen::Get()->GetDisplayNearestView(native_view);
  const gfx::Rect work_area = display.work_area();

  const int x = work_area.x() + (work_area.width() - popup_width) / 2;
  const int y = work_area.y() + (work_area.height() - popup_height) / 2;

  std::unique_ptr<NavigateParams> params = std::make_unique<NavigateParams>(
      Profile::FromBrowserContext(browser_context_), url,
      ui::PAGE_TRANSITION_LINK);
  params->disposition = WindowOpenDisposition::NEW_POPUP;
  params->opened_by_another_window = true;
  params->window_features.bounds = gfx::Rect(x, y, popup_width, popup_height);
  glic::NavigateAsync(std::move(params), base::DoNothing());
}

void GlicPageHandler::OpenLinkInNewTab(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  std::unique_ptr<NavigateParams> params = std::make_unique<NavigateParams>(
      Profile::FromBrowserContext(browser_context_), url,
      ui::PAGE_TRANSITION_LINK);
  params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  glic::NavigateAsync(std::move(params), base::DoNothing());
}

void GlicPageHandler::ShouldAllowGeolocationPermissionRequest(
    ShouldAllowGeolocationPermissionRequestCallback callback) {
  std::move(callback).Run(Profile::FromBrowserContext(browser_context_)
                              ->GetPrefs()
                              ->GetBoolean(prefs::kGlicGeolocationEnabled) &&
                          host_->IsWidgetShowing(nullptr));
}

void GlicPageHandler::OpenHelpCenterTopicAndClosePanel(
    glic::mojom::HelpCenterTopic topic) {
  // Safe fallback URL in case a newer web client passes a topic not yet
  // supported by this version of Chrome.
  GURL help_url = GURL(features::kGlicIneligibleAccountHelpUrl.Get());
  switch (topic) {
    case mojom::HelpCenterTopic::kLocationMismatch:
      help_url = GURL(features::kGlicLocationMismatchHelpUrl.Get());
      break;
    case mojom::HelpCenterTopic::kIneligibleAccount:
      help_url = GURL(features::kGlicIneligibleAccountHelpUrl.Get());
      break;
  }
  auto params = std::make_unique<NavigateParams>(
      Profile::FromBrowserContext(browser_context_), help_url,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  glic::Navigate(std::move(params));
  host().ClosePanel();
}

void GlicPageHandler::SignInAndClosePanel() {
  if (auto* auth_controller = GetGlicService()->GetAuthController()) {
    auth_controller->ShowReauthForAccount(webui_contents_);
  }
}

void GlicPageHandler::ResizeWidget(const gfx::Size& size,
                                   base::TimeDelta duration,
                                   ResizeWidgetCallback callback) {
  host().ResizePanel(size, duration, std::move(callback));
}

void GlicPageHandler::EnableDragResize(bool enabled) {
  // features::kGlicUserResize is not checked here because the WebUI page
  // invokes this method when it is disabled, too (when its state changes).
  host().EnableDragResize(enabled);
}

void GlicPageHandler::OnWebUiStateChanged(glic::mojom::WebUiState new_state) {
  host().WebUiStateChanged(this, new_state);
}

void GlicPageHandler::ClientReadyToShow(const mojom::OpenPanelInfo& open_info) {
  page_->ClientReadyStateChanged(true);
}

void GlicPageHandler::PanelStateChanged(
    const glic::mojom::PanelState& panel_state) {
  UpdatePageState(panel_state.kind);
}

void GlicPageHandler::UpdatePageState(mojom::PanelStateKind panelStateKind) {
  page_->UpdatePageState(panelStateKind);
}

void GlicPageHandler::UpdateProfileReadyState() {
  page_->SetProfileReadyState(GlicEnabling::GetProfileReadyState(
      Profile::FromBrowserContext(browser_context_)));
}

}  // namespace glic
