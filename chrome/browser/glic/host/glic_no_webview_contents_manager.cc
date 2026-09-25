// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"

#include <utility>

#include "base/check.h"
#include "base/json/string_escape.h"
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
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#else
#include "chrome/browser/glic/android/glic_navigation_utils_android.h"
#endif

namespace glic {

namespace {
class GlicPwcPolicyDelegate : public pwc::PwcPolicyDelegate {
 public:
  explicit GlicPwcPolicyDelegate(Profile* profile) : profile_(profile) {}
  ~GlicPwcPolicyDelegate() override = default;

  bool IsNavigationAllowed(const url::Origin& origin) const override {
    return IsGuestOriginAllowed(origin, profile_);
  }

  bool IsCapabilityOrigin(const url::Origin& origin) const override {
    return IsOriginAllowedGlicApi(origin, profile_);
  }

 private:
  const raw_ptr<Profile> profile_;
};

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

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// GlicNoWebviewContentsManager::OverlayContentsManager:

GlicNoWebviewContentsManager::OverlayContentsManager::OverlayContentsManager(
    Profile* profile,
    GlicNoWebviewContentsManager* owner,
    ObservableValueView<bool>& guest_ready)
    : profile_(profile),
      owner_(owner),
      guest_ready_(guest_ready),
      guest_ready_subscription_(guest_ready_->AddObserver(
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
#if !BUILDFLAG(IS_ANDROID)
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_contents_.get());
#endif
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

void GlicNoWebviewContentsManager::OverlayContentsManager::AttachToHost(
    Host* host) {
  panel_state_observation_.Reset();
  panel_state_observation_.Observe(&host->instance());
  UpdateOverlayState();
}

void GlicNoWebviewContentsManager::OverlayContentsManager::PanelStateChanged(
    const mojom::PanelState& panel_state) {
  UpdateOverlayState();
}

mojom::OverlayStatePtr
GlicNoWebviewContentsManager::OverlayContentsManager::DetermineOverlayState(
    std::optional<mojom::ErrorPanelType> error_type,
    bool is_guest_ready,
    std::optional<mojom::PanelStateKind> panel_state_kind) {
  // Input 1: Active error state. An error panel always takes precedence.
  if (error_type.has_value()) {
    return mojom::OverlayState::NewError(*error_type);
  }

  // Input 2: Guest readiness. When the guest is ready and no error is active,
  // the overlay is not needed.
  if (is_guest_ready) {
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
  if (panel_state_observation_.IsObserving()) {
    panel_state_kind =
        panel_state_observation_.GetSource()->GetPanelState().kind;
  }
  return DetermineOverlayState(error_type_, guest_ready_->get(),
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
  if (guest_ready_->get() || !web_contents_ ||
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
  if (owner_->host_) {
    // Asynchronously request reload on the host so that Mojo message dispatch
    // and caller promises complete before this manager is destroyed.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&Host::Reload, owner_->host_->GetWeakPtr()));
  }
}

void GlicNoWebviewContentsManager::OverlayContentsManager::OnSignInClicked() {
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
  OpenUrlAndClosePanel(GURL(features::kGlicIneligibleAccountHelpUrl.Get()));
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    OnLocationMismatchHelpClicked() {
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

GlicNoWebviewContentsManager::GlicNoWebviewContentsManager(
    Profile* profile,
    GlicEnabling* enabling,
    bool initially_hidden)
    : profile_(profile),
      enabling_(enabling),
      guest_ready_{false},
      overlay_manager_(profile, this, guest_ready_),
      privileged_guest_contents_(pwc::PrivilegedWebContents::Create(
          pwc::PrivilegedComponent::kGlic,
          profile,
          std::make_unique<GlicPwcPolicyDelegate>(profile))),
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
#if !BUILDFLAG(IS_ANDROID)
  web_modal::WebContentsModalDialogManager::CreateForWebContents(guest);
#endif

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
  overlay_manager_.AttachToHost(host);
  // Move from warming pool state to attached-hidden state.
  UpdateDisplayState();
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
  if ((guest_contents() && guest_contents()->IsCrashed()) || is_guest_error_) {
    return true;
  }
  return overlay_manager_.ShouldReloadOnShow();
}

void GlicNoWebviewContentsManager::Zoom(mojom::ZoomAction zoom_action,
                                        ZoomSource source) {
  zoom_controller_.Zoom(zoom_action, source);
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
  guest_contents()->UpdateWebContentsVisibility(content::Visibility::VISIBLE);
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
  if (guest_ready_.get()) {
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
  return overlay_manager_.error_type().has_value() || is_guest_error_ ||
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
  if (state_ == desired) {
    // If hidden/warming and the guest becomes ready, immediately reclaim any
    // overlay WebContents that was previously allocated.
    if (!is_visible_ && guest_ready_.get() &&
        !overlay_manager_.error_type().has_value()) {
      ScheduleOverlayDeletion(base::Milliseconds(0));
    }
  } else {
    TransitionTo(desired);
  }
  UpdateLoadingTimer();
}

void GlicNoWebviewContentsManager::OnGuestNavigationStarted() {
  StopGuestBootstrap();
  ClearTransientErrorState();
  guest_ready_.Set(false);
  is_guest_error_ = false;
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
  is_guest_error_ = false;

  switch (page_type) {
    case mojom::GuestPageType::kLogin:
      SetErrorState(mojom::ErrorPanelType::kSignIn,
                    ClientLoadErrorReason::kSignIn);
      break;
    case mojom::GuestPageType::kDisabledByAdmin:
      SetErrorState(mojom::ErrorPanelType::kDisabledByAdminWithLink,
                    ClientLoadErrorReason::kDisabledByAdmin);
      break;
    case mojom::GuestPageType::kLoadError:
      SetErrorState(mojom::ErrorPanelType::kError,
                    ClientLoadErrorReason::kGuestLoadFailed);
      break;
    case mojom::GuestPageType::kGuestError:
      // When the guest encounters an error page (/sorry/), present the guest
      // WebContents directly so the user can see the error or solve a CAPTCHA.
      ClearTransientErrorState();
      is_guest_error_ = true;
      guest_ready_.Set(true);
      ApplySizeToGuest();
      // Guest navigated to /sorry/ CAPTCHA; swap to guest directly so user can
      // solve it.
      UpdateDisplayState();
      break;
    case mojom::GuestPageType::kRegular:
      if (!is_api_allowed) {
        // TODO(markeh): report kGuestApiNotAllowed once it exists. The
        // webview host currently rewrites this case into a plain load error
        // too, so reporting kGuestLoadFailed keeps the two worlds comparable
        // until both are split at once.
        SetErrorState(mojom::ErrorPanelType::kError,
                      ClientLoadErrorReason::kGuestLoadFailed);
      } else {
        ClearTransientErrorState();
        if (!overlay_manager_.error_type().has_value()) {
          ApplySizeToGuest();
          StartGuestBootstrap();
        }
      }
      break;
  }

  // Every branch above either clears `is_guest_error_` or changes the overlay
  // error, so the Host's failure bit has to follow in all of them. The ones
  // that went through `UpdateDisplayState()` have already reported it and this
  // is a no-op for them.
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
  guest_ready_.Set(false);
  SetErrorState(mojom::ErrorPanelType::kError,
                ClientLoadErrorReason::kGuestProcessGone);
}

void GlicNoWebviewContentsManager::OnWebClientCreated() {
  StopGuestBootstrap();
  ClearTransientErrorState();
  guest_ready_.Set(true);
  // Client script connected; swap to guest if visible and error-free.
  UpdateDisplayState();
}

void GlicNoWebviewContentsManager::OnWebClientStateChanged(
    mojom::WebClientState state) {
  switch (state) {
    case mojom::WebClientState::kResponsive:
      ClearTransientErrorState();
      guest_ready_.Set(true);
      // Client state became responsive; swap to guest if visible and
      // error-free.
      UpdateDisplayState();
      break;
    case mojom::WebClientState::kError:
      guest_ready_.Set(false);
      SetErrorState(mojom::ErrorPanelType::kError,
                    ClientLoadErrorReason::kClientError);
      break;
    case mojom::WebClientState::kUninitialized:
    case mojom::WebClientState::kWarmed:
    case mojom::WebClientState::kUnresponsive:
      break;
  }
}

void GlicNoWebviewContentsManager::LoadGuest() {
  guest_ready_.Set(false);
  is_guest_error_ = false;
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
  state_ = next_state;

  switch (state_) {
    case DisplayState::kWarming:
    case DisplayState::kAttachedHidden:
      // Debounce overlay deletion during transient hides (e.g. tab switches).
      ScheduleOverlayDeletion(base::Milliseconds(100));
      break;

    case DisplayState::kShowingOverlay:
      CancelOverlayDeletion();
      EnsureOverlayContents();
      NotifyWebContentsChanged();
      break;

    case DisplayState::kShowingGuest:
      CancelOverlayDeletion();
      NotifyWebContentsChanged();
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
  guest_ready_.Set(false);
  if (host_) {
    host_->ClientLoadErrorOccurred(reason);
  } else {
    // Still warming; no Host to report to yet. AttachToHost() flushes this.
    pending_client_load_error_ = reason;
  }
  overlay_manager_.SetError(error_type);
  // An error occurred; transition to overlay if visible, or record for when
  // shown.
  UpdateDisplayState();
}

void GlicNoWebviewContentsManager::ClearErrorState() {
  overlay_manager_.ClearError();
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
  if (!guest_ready_.get() && is_visible_ && overlay_contents() &&
      overlay_contents()->GetRenderWidgetHostView()) {
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
  bool should_run_timer = is_visible_ && !guest_ready_.get() &&
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
  SetErrorState(mojom::ErrorPanelType::kError);
  if (host_) {
    host_->ClientLoadErrorOccurred(ClientLoadErrorReason::kClientLoadTimeout);
  }
}

}  // namespace glic
