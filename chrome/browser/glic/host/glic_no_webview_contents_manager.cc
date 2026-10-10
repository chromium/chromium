// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"

#include <utility>

#include "base/check.h"
#include "base/json/string_escape.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_net_log.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic_overlay_ui.h"
#include "chrome/browser/glic/host/glic_pwc_permission_delegate.h"
#include "chrome/browser/glic/host/glic_pwc_policy_delegate.h"
#include "chrome/browser/glic/host/glic_theme_util.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/host/guest_source.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_perf_traits_tracker.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/widget/scoped_modal_dialog_manager_delegate.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/android/glic_navigation_utils_android.h"
#endif

namespace glic {

namespace {
content::WebContents::CreateParams MakeOverlayCreateParams(
    Profile* profile,
    bool initially_hidden) {
  auto params = content::WebContents::CreateParams(
      profile, content::SiteInstance::CreateForURL(
                   profile, GURL(chrome::kChromeUIGlicURL)));
  params.initially_hidden = initially_hidden;
  return params;
}

std::u16string GetBootstrapScript() {
  static constexpr char kBootstrapScriptTemplate[] = R"js(
(function() {
window.__glic_bootstrap_active = true;
const source = $1;
const ping = () => {
  if (!window.__glic_bootstrap_active) return;
  try {
    window.dispatchEvent(new MessageEvent('message', {
      data: { type: 'glic-bootstrap', glicApiSource: source },
      origin: 'chrome://glic',
      source: window
    }));
  } catch (e) {
    console.error('[GlicNoWebview Bootstrap Error]', e);
  }
  if (!window.__glic_bootstrap_active) return;
  setTimeout(ping, 50);
};

document.addEventListener('DOMContentLoaded', ping, { once: true });
document.addEventListener('readystatechange', ping);

ping();
})();
)js";

  std::string guest_source = GetGuestAPISource();
  std::string escaped_source = base::GetQuotedJSONString(guest_source);
  std::string js = base::ReplaceStringPlaceholders(
      kBootstrapScriptTemplate, {std::move(escaped_source)}, nullptr);
  return base::UTF8ToUTF16(js);
}

std::optional<mojom::ErrorPanelType> ErrorForProfileReadyState(
    mojom::ProfileReadyState ready_state) {
  switch (ready_state) {
    case mojom::ProfileReadyState::kReady:
      return std::nullopt;
    case mojom::ProfileReadyState::kSignInRequired:
      return mojom::ErrorPanelType::kSignIn;
    case mojom::ProfileReadyState::kIneligibleAccount:
      return mojom::ErrorPanelType::kIneligibleAccount;
    case mojom::ProfileReadyState::kLocationMismatch:
      return mojom::ErrorPanelType::kLocationMismatch;
    case mojom::ProfileReadyState::kDisabledByAdmin:
      return mojom::ErrorPanelType::kDisabledByAdmin;
    case mojom::ProfileReadyState::kIneligible:
    case mojom::ProfileReadyState::kUnknownError:
      return mojom::ErrorPanelType::kUnavailable;
  }
}

// Mirrors the webview client's mapping of ProfileReadyState to
// ClientLoadErrorReason. Only called when ErrorForProfileReadyState() returns
// an error.
ClientLoadErrorReason ReasonForProfileReadyState(
    mojom::ProfileReadyState ready_state) {
  switch (ready_state) {
    case mojom::ProfileReadyState::kReady:
      NOTREACHED();
    case mojom::ProfileReadyState::kSignInRequired:
      return ClientLoadErrorReason::kSignIn;
    case mojom::ProfileReadyState::kIneligibleAccount:
      return ClientLoadErrorReason::kIneligibleAccount;
    case mojom::ProfileReadyState::kLocationMismatch:
      return ClientLoadErrorReason::kLocationMismatch;
    case mojom::ProfileReadyState::kDisabledByAdmin:
      return ClientLoadErrorReason::kDisabledByAdmin;
    case mojom::ProfileReadyState::kIneligible:
    case mojom::ProfileReadyState::kUnknownError:
      return ClientLoadErrorReason::kUnavailable;
  }
}

// A transient error is only displayed if the guest is not ready. A
// non-transient error is displayed regardless.
bool IsTransientError(mojom::ErrorPanelType error_type) {
  switch (error_type) {
    case mojom::ErrorPanelType::kOffline:
    case mojom::ErrorPanelType::kError:
    case mojom::ErrorPanelType::kUnavailable:
      return true;
    case mojom::ErrorPanelType::kIneligibleAccount:
    case mojom::ErrorPanelType::kDisabledByAdmin:
    case mojom::ErrorPanelType::kDisabledByAdminWithLink:
    case mojom::ErrorPanelType::kSignIn:
    case mojom::ErrorPanelType::kLocationMismatch:
      return false;
  }
}

const char* ErrorPanelTypeToHistogramSuffix(mojom::ErrorPanelType error_type) {
  switch (error_type) {
    case mojom::ErrorPanelType::kOffline:
      return "Offline";
    case mojom::ErrorPanelType::kError:
      return "Error";
    case mojom::ErrorPanelType::kUnavailable:
      return "Unavailable";
    case mojom::ErrorPanelType::kIneligibleAccount:
      return "IneligibleAccount";
    case mojom::ErrorPanelType::kDisabledByAdmin:
    case mojom::ErrorPanelType::kDisabledByAdminWithLink:
      return "DisabledByAdmin";
    case mojom::ErrorPanelType::kSignIn:
      return "SignIn";
    case mojom::ErrorPanelType::kLocationMismatch:
      return "LocationMismatch";
  }
}

bool ShouldDisableLoadingOverlay() {
  return base::FeatureList::IsEnabled(features::kGlicSsr);
}

}  // namespace

class GlicNoWebviewContentsManager::Metrics {
 public:
  void SetDisplayState(DisplayState prev_state,
                       DisplayState next_state,
                       std::optional<mojom::ErrorPanelType> active_error) {
    base::TimeTicks now = base::TimeTicks::Now();

    if (prev_state == DisplayState::kShowingOverlay) {
      if (total_overlay_show_start_time_) {
        base::UmaHistogramCustomTimes("Glic.Overlay.DisplayDuration",
                                      now - *total_overlay_show_start_time_,
                                      base::Milliseconds(1), base::Seconds(60),
                                      50);
      }
      if (next_state == DisplayState::kShowingGuest) {
        if (loading_show_start_time_) {
          base::UmaHistogramCustomTimes(
              "Glic.Overlay.DisplayDuration.LoadingAndCompleted",
              now - *loading_show_start_time_, base::Milliseconds(1),
              base::Seconds(60), 50);
        }
      } else {
        if (loading_show_start_time_) {
          base::UmaHistogramCustomTimes("Glic.Overlay.DisplayDuration.Loading",
                                        now - *loading_show_start_time_,
                                        base::Milliseconds(1),
                                        base::Seconds(60), 50);
        }
        if (error_show_start_time_ && active_error_type_) {
          base::UmaHistogramLongTimes(
              base::StrCat(
                  {"Glic.Overlay.DisplayDuration.",
                   ErrorPanelTypeToHistogramSuffix(*active_error_type_)}),
              now - *error_show_start_time_);
        }
      }
    }

    switch (next_state) {
      case DisplayState::kWarming:
      case DisplayState::kAttachedHidden:
      case DisplayState::kShowingGuest:
        total_overlay_show_start_time_.reset();
        loading_show_start_time_.reset();
        error_show_start_time_.reset();
        active_error_type_.reset();
        break;

      case DisplayState::kShowingOverlay:
        if (!total_overlay_show_start_time_) {
          total_overlay_show_start_time_ = now;
        }
        if (active_error.has_value()) {
          active_error_type_ = active_error;
          error_show_start_time_ = now;
          loading_show_start_time_.reset();
        } else {
          active_error_type_.reset();
          loading_show_start_time_ = now;
          error_show_start_time_.reset();
        }
        break;
    }
  }

  void OnErrorChanged(std::optional<mojom::ErrorPanelType> new_error,
                      bool is_visible) {
    if (!is_visible) {
      active_error_type_ = new_error;
      error_show_start_time_.reset();
      return;
    }

    base::TimeTicks now = base::TimeTicks::Now();

    // If an error occurred while the loading overlay was displayed, the loading
    // attempt did not finish.
    if (new_error.has_value() && loading_show_start_time_) {
      base::UmaHistogramCustomTimes("Glic.Overlay.DisplayDuration.Loading",
                                    now - *loading_show_start_time_,
                                    base::Milliseconds(1), base::Seconds(60),
                                    50);
      loading_show_start_time_.reset();
    }

    // If an error panel was displayed and is now cleared or changed:
    if (active_error_type_.has_value() && error_show_start_time_) {
      base::UmaHistogramLongTimes(
          base::StrCat({"Glic.Overlay.DisplayDuration.",
                        ErrorPanelTypeToHistogramSuffix(*active_error_type_)}),
          now - *error_show_start_time_);
      error_show_start_time_.reset();
    }

    active_error_type_ = new_error;
    if (new_error.has_value()) {
      error_show_start_time_ = now;
      loading_show_start_time_.reset();
    } else {
      loading_show_start_time_ = now;
    }
  }

 private:
  std::optional<base::TimeTicks> total_overlay_show_start_time_;
  std::optional<base::TimeTicks> loading_show_start_time_;
  std::optional<base::TimeTicks> error_show_start_time_;
  std::optional<mojom::ErrorPanelType> active_error_type_;
};

///////////////////////////////////////////////////////////////////////////////
// GlicNoWebviewContentsManager::OverlayContentsManager:

GlicNoWebviewContentsManager::OverlayContentsManager::OverlayContentsManager(
    Profile* profile,
    GlicNoWebviewContentsManager* owner,
    ObservableValueView<GuestState>& guest_state)
    : profile_(profile),
      owner_(owner),
      guest_state_(guest_state),
      guest_state_subscription_(guest_state_->AddObserver(
          base::BindRepeating(&OverlayContentsManager::UpdateOverlayState,
                              base::Unretained(this)))) {}

GlicNoWebviewContentsManager::OverlayContentsManager::
    ~OverlayContentsManager() {
  Observe(nullptr);
}

content::WebContents*
GlicNoWebviewContentsManager::OverlayContentsManager::EnsureWebContents() {
  if (web_contents_) {
    return web_contents_.get();
  }
  // The overlay is only created when Glic is visible (kShowingOverlay), so it
  // is never created hidden regardless of `initially_hidden`.
  web_contents_ = content::WebContents::Create(
      MakeOverlayCreateParams(profile_, /*initially_hidden=*/false));
  CHECK(web_contents_);

  SkColor glic_bg_color =
      GetGlicBackgroundColor(profile_, web_contents_->GetColorProvider());
  web_contents_->SetPageBaseBackgroundColor(glic_bg_color);
#if !BUILDFLAG(IS_ANDROID)
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_contents_.get(), glic_bg_color);
#endif
  if (web_contents_->GetRenderWidgetHostView()) {
    web_contents_->GetRenderWidgetHostView()->SetBackgroundColor(glic_bg_color);
  }

  Observe(web_contents_.get());
  PrefsTabHelper::CreateForWebContents(web_contents_.get());
  CreateGlicOverlayData(web_contents_.get());
  web_contents_->SetSupportsDraggableRegions(true);

  // Load the overlay WebUI.
  web_contents_->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(
          GURL{chrome::kChromeUIGlicOverlayURL}));
  return web_contents_.get();
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    DestroyWebContents() {
  if (web_contents_) {
    Observe(nullptr);
    web_contents_.reset();
  }
}

content::WebContents*
GlicNoWebviewContentsManager::OverlayContentsManager::web_contents() const {
  return web_contents_.get();
}

bool GlicNoWebviewContentsManager::OverlayContentsManager::IsCrashed() const {
  return web_contents_ && web_contents_->IsCrashed();
}

GlicOverlayUI*
GlicNoWebviewContentsManager::OverlayContentsManager::GetOverlayUI() const {
  if (!web_contents_) {
    return nullptr;
  }
  content::WebUI* web_ui = web_contents_->GetWebUI();
  return web_ui && web_ui->GetController()
             ? web_ui->GetController()->GetAs<GlicOverlayUI>()
             : nullptr;
}

std::optional<mojom::ErrorPanelType>
GlicNoWebviewContentsManager::OverlayContentsManager::error_type() const {
  return error_type_;
}

void GlicNoWebviewContentsManager::OverlayContentsManager::SetError(
    mojom::ErrorPanelType error_type) {
  error_type_ = error_type;
  UpdateOverlayState();
}

void GlicNoWebviewContentsManager::OverlayContentsManager::ClearError() {
  error_type_.reset();
  UpdateOverlayState();
}

void GlicNoWebviewContentsManager::OverlayContentsManager::ObservePanelState(
    ObservableValueView<mojom::PanelState>& panel_state) {
  panel_state_ = &panel_state;
  panel_state_subscription_ = panel_state_->AddObserver(base::BindRepeating(
      &OverlayContentsManager::UpdateOverlayState, base::Unretained(this)));
  UpdateOverlayState();
}

mojom::OverlayStatePtr
GlicNoWebviewContentsManager::OverlayContentsManager::DetermineOverlayState(
    std::optional<mojom::ErrorPanelType> error_type,
    GuestState guest_state,
    std::optional<mojom::PanelStateKind> panel_state_kind) {
  // Input 1: Active error state. An error panel always takes precedence.
  if (error_type.has_value()) {
    return mojom::OverlayState::NewError(*error_type);
  }

  // Input 2: Guest presentation. When the guest is ready, displaying a login
  // or in-page error page, or when the loading overlay is disabled, the guest
  // WebContents is shown and no overlay is needed.
  if (guest_state != GuestState::kLoading || ShouldDisableLoadingOverlay()) {
    return nullptr;
  }

  // Input 3: Panel state. When loading, detached panels show a floating
  // skeleton; otherwise (attached to side panel or warming in background),
  // show side panel.
  mojom::LoadingStyle loading_style =
      (panel_state_kind == mojom::PanelStateKind::kDetached)
          ? mojom::LoadingStyle::kFloating
          : mojom::LoadingStyle::kSidePanel;
  return mojom::OverlayState::NewLoading(loading_style);
}

mojom::OverlayStatePtr
GlicNoWebviewContentsManager::OverlayContentsManager::DetermineOverlayState() {
  std::optional<mojom::PanelStateKind> panel_state_kind;
  if (panel_state_) {
    panel_state_kind = panel_state_->get().kind;
  }
  return DetermineOverlayState(error_type_, guest_state_->get(),
                               panel_state_kind);
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    UpdateOverlayState() {
  auto* overlay_ui = GetOverlayUI();
  if (!overlay_ui) {
    return;
  }
  mojom::OverlayStatePtr state = DetermineOverlayState();
  if (state) {
    overlay_ui->SetOverlayState(std::move(state));
  }
}

void GlicNoWebviewContentsManager::OverlayContentsManager::SetVisibility(
    content::Visibility visibility) {
  // The overlay WebContents is created on demand when needed and destroyed when
  // not in use. Visibility is applied once created.
  if (web_contents_) {
    web_contents_->UpdateWebContentsVisibility(visibility);
  }
}

const gfx::Size&
GlicNoWebviewContentsManager::OverlayContentsManager::cached_size() const {
  return cached_size_;
}

bool GlicNoWebviewContentsManager::OverlayContentsManager::ShouldReloadOnShow()
    const {
  if (IsCrashed()) {
    return true;
  }
  return error_type_.has_value() && IsTransientError(*error_type_);
}

void GlicNoWebviewContentsManager::OverlayContentsManager::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->GetParentOrOuterDocument() ||
      !render_frame_host->GetView() || !web_contents_) {
    return;
  }
  render_frame_host->GetView()->SetBackgroundColor(
      GetGlicBackgroundColor(profile_, web_contents_->GetColorProvider()));
}

void GlicNoWebviewContentsManager::OverlayContentsManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  auto* overlay_ui = GetOverlayUI();
  if (!overlay_ui) {
    return;
  }
  overlay_ui->SetPageHandler(this);
  UpdateOverlayState();
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    PrimaryMainFrameWasResized(bool width_changed) {
  if (guest_state_->get() != GuestState::kLoading || !web_contents_ ||
      !web_contents_->GetRenderWidgetHostView()) {
    return;
  }
  gfx::Size size =
      web_contents_->GetRenderWidgetHostView()->GetVisibleViewportSize();
  if (size.IsEmpty()) {
    return;
  }
  cached_size_ = size;
  owner_->cached_overlay_size_ = size;
  owner_->ApplySizeToGuest();
}

void GlicNoWebviewContentsManager::OverlayContentsManager::OnRetryClicked() {
  base::RecordAction(base::UserMetricsAction("Glic.Overlay.RetryClicked"));
  if (owner_->host_) {
    // Asynchronously request reload on the host so that Mojo message dispatch
    // and caller promises complete before this manager is destroyed.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&Host::Reload, owner_->host_->GetWeakPtr()));
  }
}

void GlicNoWebviewContentsManager::OverlayContentsManager::OnSignInClicked() {
  base::RecordAction(base::UserMetricsAction("Glic.Overlay.SignInClicked"));
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  std::string email;
  if (identity_manager) {
    email =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .email;
  }
#if !BUILDFLAG(IS_ANDROID)
  signin_ui_util::ShowReauthForAccount(
      profile_, email, signin_metrics::AccessPoint::kGlicLaunchButton);
#else
  glic::ShowSignIn(
      profile_, web_contents_ ? web_contents_.get() : owner_->guest_contents());
#endif
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    OnProfilePickerClicked() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Overlay.ProfilePickerClicked"));
  GlicProfileManager::GetInstance()->ShowProfilePicker();
}

void GlicNoWebviewContentsManager::OverlayContentsManager::OpenUrlAndClosePanel(
    const GURL& url) {
  auto params = std::make_unique<NavigateParams>(
      profile_, url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  glic::NavigateAsync(std::move(params), base::DoNothing());
  OnClosePanelClicked();
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    OnIneligibleAccountHelpClicked() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Overlay.IneligibleAccountHelpClicked"));
  OpenUrlAndClosePanel(GURL(features::kGlicIneligibleAccountHelpUrl.Get()));
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    OnLocationMismatchHelpClicked() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Overlay.LocationMismatchHelpClicked"));
  OpenUrlAndClosePanel(GURL(features::kGlicLocationMismatchHelpUrl.Get()));
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    OnDisabledByAdminCloseClicked() {
  OnClosePanelClicked();
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    OnDisabledByAdminLinkClicked() {
  OpenUrlAndClosePanel(GURL(features::kGlicCaaLinkUrl.Get()));
  base::RecordAction(
      base::UserMetricsAction("Glic.DisabledByAdminPanelLinkClicked"));
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    OnClosePanelClicked() {
  if (owner_->host_) {
    owner_->host_->ClosePanel();
  }
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    OnShowErrorClicked() {
  if (error_type_ != mojom::ErrorPanelType::kError ||
      !GlicOverlayUI::IsShowErrorAllowed(profile_) ||
      !owner_->guest_contents() || owner_->guest_contents()->IsCrashed()) {
    return;
  }
  owner_->StopGuestBootstrap();
  owner_->ShowGuestDirectly(GuestState::kGuestError);
}

GlicNoWebviewContentsManager::GlicNoWebviewContentsManager(
    Profile* profile,
    GlicEnabling* enabling,
    bool initially_hidden)
    : profile_(profile),
      enabling_(enabling),
      metrics_(std::make_unique<Metrics>()),
      guest_state_{GuestState::kLoading},
      overlay_manager_(profile, this, guest_state_),
      privileged_guest_contents_(pwc::PrivilegedWebContents::Create(
          pwc::PrivilegedComponent::kGlic,
          profile,
          std::make_unique<GlicPwcPolicyDelegate>(profile),
          {.initially_hidden = initially_hidden})),
      zoom_controller_(
          privileged_guest_contents_->web_contents(),
          profile ? profile->GetPrefs() : nullptr,
          base::BindRepeating(&GlicNoWebviewContentsManager::OnZoomLevelChange,
                              base::Unretained(this))) {
  CHECK(enabling_);
  CHECK(privileged_guest_contents_);
  privileged_guest_contents_->SetPermissionDelegate(
      std::make_unique<GlicPwcPermissionDelegate>(profile));
  content::WebContents* guest = guest_contents();
  CHECK(guest);

  SkColor glic_bg_color =
      GetGlicBackgroundColor(profile, guest->GetColorProvider());
  guest->SetPageBaseBackgroundColor(glic_bg_color);
#if !BUILDFLAG(IS_ANDROID)
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      guest, glic_bg_color);
#endif
  if (guest->GetRenderWidgetHostView()) {
    guest->GetRenderWidgetHostView()->SetBackgroundColor(glic_bg_color);
  }

  PrepareGlicGuestWebContents(*guest, *this);
  web_client_manager_.AttachGuestContents(guest);
  web_client_manager_.SetDelegate(this);

  profile_ready_subscription_ =
      enabling_->RegisterProfileReadyStateChanged(base::BindRepeating(
          &GlicNoWebviewContentsManager::OnProfileReadyStateChanged,
          base::Unretained(this)));

  UpdateForProfileReadyState(/*is_initial=*/true);
}

GlicNoWebviewContentsManager::~GlicNoWebviewContentsManager() = default;

content::WebContents* GlicNoWebviewContentsManager::guest_contents() const {
  return privileged_guest_contents_ ? privileged_guest_contents_->web_contents()
                                    : nullptr;
}

content::WebContents* GlicNoWebviewContentsManager::overlay_contents() const {
  return overlay_manager_.web_contents();
}

mojom::GlicOverlayPageHandler*
GlicNoWebviewContentsManager::GetOverlayPageHandlerForTesting() const {
  return const_cast<OverlayContentsManager&>(overlay_manager_)
      .GetPageHandlerForTesting();
}

content::WebContents* GlicNoWebviewContentsManager::EnsureOverlayContents() {
  return overlay_manager_.EnsureWebContents();
}

void GlicNoWebviewContentsManager::DestroyOverlayContents() {
  CancelOverlayDeletion();
  overlay_manager_.DestroyWebContents();
}

void GlicNoWebviewContentsManager::ScheduleOverlayDeletion(
    base::TimeDelta delay) {
  if (!overlay_contents()) {
    return;
  }
  overlay_deletion_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&GlicNoWebviewContentsManager::DestroyOverlayContents,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlicNoWebviewContentsManager::CancelOverlayDeletion() {
  overlay_deletion_timer_.Stop();
}

void GlicNoWebviewContentsManager::AttachToHost(Host* host) {
  CHECK(!host_);
  host_ = host;

  if (guest_contents()) {
    SetHostForGuest(*guest_contents(), host);
  }

  if (auto error = std::exchange(pending_client_load_error_, std::nullopt)) {
    host_->ClientLoadErrorOccurred(*error);
  }

  web_client_manager_.AttachToHost(host);
  overlay_manager_.ObservePanelState(host->instance().GetPanelState());
  // Move from warming pool state to attached-hidden state.
  UpdateDisplayState();
}

void GlicNoWebviewContentsManager::AttachModalDialogManagerDelegate(
    ScopedModalDialogManagerDelegate& delegate) {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* overlay = overlay_manager_.web_contents()) {
    delegate.AddWebContents(overlay);
  }
  if (auto* guest = guest_contents()) {
    delegate.AddWebContents(guest);
  }
#endif
}

base::CallbackListSubscription
GlicNoWebviewContentsManager::RegisterWebContentsChangedCallback(
    WebContentsChangedCallback callback) {
  return web_contents_changed_callbacks_.Add(std::move(callback));
}

GlicWebClientManager& GlicNoWebviewContentsManager::web_client_manager() {
  return web_client_manager_;
}

bool GlicNoWebviewContentsManager::ShouldReloadOnShow() const {
  if ((guest_contents() && guest_contents()->IsCrashed()) ||
      guest_state_.get() == GuestState::kGuestError) {
    return true;
  }
  return overlay_manager_.ShouldReloadOnShow();
}

bool GlicNoWebviewContentsManager::IsCrashed() const {
  return (guest_contents() && guest_contents()->IsCrashed()) ||
         overlay_manager_.IsCrashed();
}

void GlicNoWebviewContentsManager::Zoom(mojom::ZoomAction zoom_action,
                                        ZoomSource source) {
  zoom_controller_.Zoom(zoom_action, source);
}

void GlicNoWebviewContentsManager::SetErrorCallback(
    base::RepeatingClosure callback) {
  error_callback_ = std::move(callback);
  if (ShouldReloadOnShow()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                             error_callback_);
  }
}

void GlicNoWebviewContentsManager::OnZoomLevelChange() {
  if (host_) {
    host_->instance_metrics().OnZoomLevelChange();
  }
}

void GlicNoWebviewContentsManager::NotifyWebContentsChanged() {
  web_contents_changed_callbacks_.Notify(active_web_contents());
}

void GlicNoWebviewContentsManager::ApplySizeToGuest() {
  gfx::Size target_size;
  if (host_) {
    target_size = host_->instance().GetPanelSize();
    if (!target_size.IsEmpty()) {
      cached_overlay_size_ = target_size;
    }
  }
  if (target_size.IsEmpty()) {
    target_size = cached_overlay_size_;
  }
  if (target_size.IsEmpty() || !guest_contents() ||
      !guest_contents()->GetRenderWidgetHostView()) {
    return;
  }
  guest_contents()->GetRenderWidgetHostView()->SetSize(target_size);
  // While the overlay is shown, the guest is not attached to a view, so mark it
  // visible to let it load and render at full priority.
  if (is_visible_) {
    guest_contents()->UpdateWebContentsVisibility(content::Visibility::VISIBLE);
  }
}

void GlicNoWebviewContentsManager::ShowGuestDirectly(GuestState state) {
  CHECK_NE(state, GuestState::kLoading);
  ClearTransientErrorState();
  if (!overlay_manager_.error_type().has_value()) {
    guest_state_.Set(state);
    ApplySizeToGuest();
  }
  UpdateDisplayState();
  if (error_callback_ && ShouldReloadOnShow()) {
    error_callback_.Run();
  }
}

GlicNoWebviewContentsManager::DisplayState
GlicNoWebviewContentsManager::CalculateDesiredState() const {
  if (!is_visible_) {
    return host_ ? DisplayState::kAttachedHidden : DisplayState::kWarming;
  }
  // When an overlay error is active, keep displaying the overlay UI regardless
  // of whether the guest client connects in the background.
  if (overlay_manager_.error_type().has_value()) {
    return DisplayState::kShowingOverlay;
  }
  if (guest_state_.get() != GuestState::kLoading ||
      ShouldDisableLoadingOverlay()) {
    return DisplayState::kShowingGuest;
  }
  return DisplayState::kShowingOverlay;
}

mojom::WebClientState GlicNoWebviewContentsManager::web_client_state() const {
  GlicWebClientAccess* access = web_client_manager_.web_client_access();
  return access ? access->web_client_state()
                : mojom::WebClientState::kUninitialized;
}

bool GlicNoWebviewContentsManager::HasClientLoadFailed() const {
  // Unresponsiveness is deliberately not included: the client is still
  // connected and can become responsive again without being reloaded.
  return overlay_manager_.error_type().has_value() ||
         guest_state_.get() == GuestState::kGuestError ||
         web_client_state() == mojom::WebClientState::kError;
}

void GlicNoWebviewContentsManager::UpdateClientLoadFailed() {
  if (host_) {
    host_->SetClientLoadFailed(HasClientLoadFailed());
  }
}

void GlicNoWebviewContentsManager::UpdateDisplayState() {
  UpdateClientLoadFailed();
  DisplayState desired = CalculateDesiredState();
  if (state_ != desired) {
    TransitionTo(desired);
  }
  UpdateLoadingTimer();
}

void GlicNoWebviewContentsManager::OnGuestNavigationStarted() {
  StopGuestBootstrap();
  ClearTransientErrorState();
  guest_state_.Set(GuestState::kLoading);
  UpdateClientLoadFailed();
  // Allow the full loading time after navigation. UpdateDisplayState() will
  // start the timer.
  loading_timer_.Stop();
  UpdateDisplayState();
}

void GlicNoWebviewContentsManager::OnGuestNavigated(
    const GURL& url,
    bool is_api_allowed,
    mojom::GuestPageType page_type,
    bool is_initial_commit) {
  StopGuestBootstrap();

  switch (page_type) {
    case mojom::GuestPageType::kDisabledByAdmin:
      guest_state_.Set(GuestState::kLoading);
      SetErrorState(mojom::ErrorPanelType::kDisabledByAdminWithLink,
                    ClientLoadErrorReason::kDisabledByAdmin);
      break;
    case mojom::GuestPageType::kLoadError:
      guest_state_.Set(GuestState::kLoading);
      SetErrorState(mojom::ErrorPanelType::kError,
                    ClientLoadErrorReason::kGuestLoadFailed);
      break;
    case mojom::GuestPageType::kLogin:
      // When the guest encounters a web login or proxy authentication page,
      // present the guest WebContents directly so the user can enter
      // credentials.
      ShowGuestDirectly(GuestState::kLogin);
      break;
    case mojom::GuestPageType::kGuestError:
      // When the guest encounters an in-page error (e.g. /sorry/ or CAPTCHA),
      // present the guest WebContents directly so the user can see the error
      // or solve the challenge.
      ShowGuestDirectly(GuestState::kGuestError);
      break;
    case mojom::GuestPageType::kRegular:
      if (!is_api_allowed) {
        guest_state_.Set(GuestState::kLoading);
        // TODO(markeh): report kGuestApiNotAllowed once it exists. The
        // webview host currently rewrites this case into a plain load error
        // too, so reporting kGuestLoadFailed keeps the two worlds comparable
        // until both are split at once.
        SetErrorState(mojom::ErrorPanelType::kError,
                      ClientLoadErrorReason::kGuestLoadFailed);
      } else {
        ClearTransientErrorState();
        guest_state_.Set(GuestState::kLoading);
        if (!overlay_manager_.error_type().has_value()) {
          ApplySizeToGuest();
          StartGuestBootstrap();
        }
        UpdateDisplayState();
      }
      break;
  }

  UpdateClientLoadFailed();
}

void GlicNoWebviewContentsManager::StartGuestBootstrap() {
  if (!guest_contents() || !guest_contents()->GetPrimaryMainFrame()) {
    return;
  }
  guest_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      GetBootstrapScript(), base::NullCallback(),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

void GlicNoWebviewContentsManager::StopGuestBootstrap() {
  if (!guest_contents() || !guest_contents()->GetPrimaryMainFrame()) {
    return;
  }
  static constexpr char16_t kStopScript[] =
      u"window.__glic_bootstrap_active = false;";
  guest_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      kStopScript, base::NullCallback(), ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

void GlicNoWebviewContentsManager::OnGuestProcessGone(
    base::TerminationStatus status) {
  StopGuestBootstrap();
  guest_state_.Set(GuestState::kLoading);
  SetErrorState(mojom::ErrorPanelType::kError,
                ClientLoadErrorReason::kGuestProcessGone);
}

void GlicNoWebviewContentsManager::OnWebClientCreated() {
  StopGuestBootstrap();
  ClearTransientErrorState();
  guest_state_.Set(GuestState::kReady);
  // Client script connected; swap to guest if visible and error-free.
  UpdateDisplayState();
}

void GlicNoWebviewContentsManager::OnWebClientStateChanged(
    mojom::WebClientState state) {
  switch (state) {
    case mojom::WebClientState::kWarmed:
      StopGuestBootstrap();
      [[fallthrough]];
    case mojom::WebClientState::kResponsive:
      ClearTransientErrorState();
      guest_state_.Set(GuestState::kReady);
      // Client state became responsive; swap to guest if visible and
      // error-free.
      UpdateDisplayState();
      break;
    case mojom::WebClientState::kError:
      guest_state_.Set(GuestState::kLoading);
      SetErrorState(mojom::ErrorPanelType::kError,
                    ClientLoadErrorReason::kClientError);
      break;
    case mojom::WebClientState::kUninitialized:
    case mojom::WebClientState::kUnresponsive:
      break;
  }
}

void GlicNoWebviewContentsManager::LoadGuest() {
  guest_state_.Set(GuestState::kLoading);
  UpdateClientLoadFailed();
  GURL guest_url = GetGuestURL(profile_);
  net_log::LogDummyNetworkRequestForTrafficAnnotation(guest_url);
  guest_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(guest_url));
}

void GlicNoWebviewContentsManager::TransitionTo(DisplayState next_state) {
  if (state_ == next_state) {
    return;
  }
  metrics_->SetDisplayState(state_, next_state, overlay_manager_.error_type());
  state_ = next_state;

  switch (state_) {
    case DisplayState::kWarming:
    case DisplayState::kAttachedHidden:
      // If the guest is already ready to display, immediately reclaim the
      // overlay. Otherwise debounce overlay deletion during transient hides
      // (e.g. tab switches).
      if (guest_state_.get() != GuestState::kLoading &&
          !overlay_manager_.error_type().has_value()) {
        ScheduleOverlayDeletion(base::Milliseconds(0));
      } else {
        ScheduleOverlayDeletion(base::Milliseconds(100));
      }
      break;

    case DisplayState::kShowingOverlay:
      CancelOverlayDeletion();
      EnsureOverlayContents();
      NotifyWebContentsChanged();
      // Restart timer when showing overlay to grant full visible loading
      // duration.
      if (!overlay_manager_.error_type().has_value()) {
        loading_timer_.Stop();
      }
      break;

    case DisplayState::kShowingGuest:
      CancelOverlayDeletion();
      NotifyWebContentsChanged();
      // Stop the timer so UpdateLoadingTimer() restarts it from zero if the
      // guest is still loading (e.g. under kGlicSsr), granting the full visible
      // loading duration.
      loading_timer_.Stop();
      // Destroy the loading overlay once the guest is ready and swapped.
      ScheduleOverlayDeletion(base::Milliseconds(0));
      break;
  }
}

void GlicNoWebviewContentsManager::SetErrorState(
    mojom::ErrorPanelType error_type,
    ClientLoadErrorReason reason) {
  // If we already have a deterministic / policy error state (like sign-in
  // required, ineligible account, disabled by admin, location mismatch), do not
  // overwrite it with a generic transient error (e.g. kError or kOffline).
  if (overlay_manager_.error_type().has_value() &&
      !IsTransientError(*overlay_manager_.error_type()) &&
      IsTransientError(error_type)) {
    return;
  }
  StopGuestBootstrap();
  guest_state_.Set(GuestState::kLoading);
  if (host_) {
    host_->ClientLoadErrorOccurred(reason);
  } else {
    // Still warming; no Host to report to yet. AttachToHost() flushes this.
    pending_client_load_error_ = reason;
  }
  overlay_manager_.SetError(error_type);
  metrics_->OnErrorChanged(error_type, is_visible_);

  // An error occurred; transition to overlay if visible, or record for when
  // shown.
  UpdateDisplayState();
  if (error_callback_ && ShouldReloadOnShow()) {
    error_callback_.Run();
  }
}

void GlicNoWebviewContentsManager::ClearErrorState() {
  pending_client_load_error_.reset();
  overlay_manager_.ClearError();
  metrics_->OnErrorChanged(std::nullopt, is_visible_);
  UpdateDisplayState();
}

void GlicNoWebviewContentsManager::ClearTransientErrorState() {
  if (overlay_manager_.error_type().has_value() &&
      IsTransientError(*overlay_manager_.error_type())) {
    ClearErrorState();
  }
}

void GlicNoWebviewContentsManager::UpdateForProfileReadyState(bool is_initial) {
  mojom::ProfileReadyState ready_state =
      GlicEnabling::GetProfileReadyState(profile_);
  std::optional<mojom::ErrorPanelType> error =
      ErrorForProfileReadyState(ready_state);
  if (error) {
    SetErrorState(*error, ReasonForProfileReadyState(ready_state));
  } else if (is_initial || overlay_manager_.error_type().has_value()) {
    // Transitioned back to ready while showing an error, or initially ready;
    // ensure error state is cleared and load the guest.
    ClearErrorState();
    LoadGuest();
  }
}

void GlicNoWebviewContentsManager::OnProfileReadyStateChanged() {
  UpdateForProfileReadyState(/*is_initial=*/false);
}

void GlicNoWebviewContentsManager::SetVisibility(
    content::Visibility visibility) {
  is_visible_ = (visibility == content::Visibility::VISIBLE);
  // Re-evaluate display state on visibility change (swaps to guest/overlay
  // when visible, or tears down/debounces overlay when hidden).
  UpdateDisplayState();

  overlay_manager_.SetVisibility(visibility);
  if (guest_state_.get() == GuestState::kLoading && is_visible_ &&
      overlay_contents() && overlay_contents()->GetRenderWidgetHostView()) {
    gfx::Size size =
        overlay_contents()->GetRenderWidgetHostView()->GetVisibleViewportSize();
    if (!size.IsEmpty()) {
      cached_overlay_size_ = size;
      ApplySizeToGuest();
    }
  }
  if (guest_contents()) {
    guest_contents()->UpdateWebContentsVisibility(visibility);
  }
}

content::WebContents* GlicNoWebviewContentsManager::active_web_contents()
    const {
  switch (state_) {
    case DisplayState::kShowingGuest:
      return guest_contents();
    case DisplayState::kShowingOverlay:
      return overlay_contents();
    case DisplayState::kAttachedHidden:
    case DisplayState::kWarming:
      return nullptr;
  }
}

void GlicNoWebviewContentsManager::OnActuatingChanged(bool actuating) {
  if (!actuating) {
    is_actuating_ = false;
    guest_capture_runner_.RunAndReset();
  }
  if (!guest_contents()) {
    return;
  }
  is_actuating_ = actuating;
  if (actuating && !guest_capture_runner_) {
    guest_capture_runner_ = guest_contents()->IncrementCapturerCount(
        gfx::Size(), /*stay_hidden=*/true, /*stay_awake=*/true,
        /*is_activity=*/true);
  }

  UpdateActuationTracker();
}

void GlicNoWebviewContentsManager::OnTaskTabsVisibilityChanged(
    bool has_visible_tab) {
  is_actuating_on_visible_tab_ = has_visible_tab;
  UpdateActuationTracker();
}

void GlicNoWebviewContentsManager::UpdateActuationTracker() {
  if (!guest_contents()) {
    return;
  }
  GlicActuationState state = GlicActuationState::kNone;
  if (is_actuating_) {
    state = is_actuating_on_visible_tab_
                ? GlicActuationState::kActuatingOnVisibleTab
                : GlicActuationState::kActuatingOnBackgroundTab;
  }
  glic::GlicPerfTraitsTracker::GetInstance()->NotifyActuationStateChanged(
      guest_contents(), state);
}

void GlicNoWebviewContentsManager::UpdateLoadingTimer() {
  bool should_run_timer = guest_state_.get() == GuestState::kLoading &&
                          !overlay_manager_.error_type().has_value();
  if (should_run_timer) {
    if (!loading_timer_.IsRunning()) {
      loading_timer_.Start(
          FROM_HERE, GetMaxLoadingTime(),
          base::BindOnce(&GlicNoWebviewContentsManager::OnLoadingTimeout,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    loading_timer_.Stop();
  }
}

void GlicNoWebviewContentsManager::OnLoadingTimeout() {
  if (!is_visible_ && guest_contents()) {
    guest_contents()->Stop();
  }
  SetErrorState(mojom::ErrorPanelType::kError,
                ClientLoadErrorReason::kClientLoadTimeout);
}

}  // namespace glic
