// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"

#include <utility>

#include "base/check.h"
#include "base/json/string_escape.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic_overlay_ui.h"
#include "chrome/browser/glic/host/glic_theme_util.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/host/guest_source.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_perf_traits_tracker.h"
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
  ~GlicPwcPolicyDelegate() override = default;

  bool IsNavigationAllowed(const url::Origin& origin) const override {
    return IsGuestOriginAllowed(origin);
  }

  bool IsCapabilityOrigin(const url::Origin& origin) const override {
    return IsOriginAllowedGlicApi(origin);
  }
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
      if (window.__glic_bootstrap_timer) {
        clearTimeout(window.__glic_bootstrap_timer);
        window.__glic_bootstrap_timer = null;
      }
      const source = $1;
      const ping = () => {
        try {
          window.dispatchEvent(new MessageEvent('message', {
            data: { type: 'glic-bootstrap', glicApiSource: source },
            origin: 'chrome://glic',
            source: window
          }));
        } catch (e) {
          console.error('[GlicNoWebview Bootstrap Error]', e);
        }
        window.__glic_bootstrap_timer = setTimeout(ping, 50);
      };
      ping();
    })();
  )js";

  std::string guest_source = GetGuestAPISource();
  std::string escaped_source = base::GetQuotedJSONString(guest_source);
  std::string js = base::ReplaceStringPlaceholders(
      kBootstrapScriptTemplate, {std::move(escaped_source)}, nullptr);
  return base::UTF8ToUTF16(js);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// GlicNoWebviewContentsManager::OverlayContentsManager:

GlicNoWebviewContentsManager::OverlayContentsManager::OverlayContentsManager(
    Profile* profile,
    GlicNoWebviewContentsManager* owner)
    : profile_(profile), owner_(owner) {}

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
  if (auto* overlay_ui = GetOverlayUI()) {
    overlay_ui->SetOverlayState(mojom::OverlayState::NewError(error_type));
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
  if (!error_type_.has_value()) {
    return false;
  }
  switch (error_type_.value()) {
    case mojom::ErrorPanelType::kOffline:
    case mojom::ErrorPanelType::kError:
    case mojom::ErrorPanelType::kUnavailable:
      // Transient or recoverable failures: retrying with a fresh manager can
      // succeed.
      return true;
    case mojom::ErrorPanelType::kIneligibleAccount:
    case mojom::ErrorPanelType::kDisabledByAdmin:
    case mojom::ErrorPanelType::kDisabledByAdminWithLink:
    case mojom::ErrorPanelType::kSignIn:
    case mojom::ErrorPanelType::kLocationMismatch:
      // Deterministic / policy error states: retrying will produce the same
      // error. The container should be attached to show the error panel UI.
      return false;
  }
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
  if (error_type_) {
    overlay_ui->SetOverlayState(mojom::OverlayState::NewError(*error_type_));
  } else if (!owner_->is_guest_ready_) {
    overlay_ui->SetOverlayState(
        mojom::OverlayState::NewLoading(mojom::LoadingStyle::kSidePanel));
  }
}

void GlicNoWebviewContentsManager::OverlayContentsManager::
    PrimaryMainFrameWasResized(bool width_changed) {
  if (owner_->is_guest_ready_ || !web_contents_ ||
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
    bool initially_hidden)
    : profile_(profile),
      overlay_manager_(profile, this),
      privileged_guest_contents_(pwc::PrivilegedWebContents::Create(
          pwc::PrivilegedComponent::kGlic,
          profile,
          std::make_unique<GlicPwcPolicyDelegate>())) {
  CHECK(privileged_guest_contents_);
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

  LoadGuest();
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

  web_client_manager_.AttachToHost(host);
  TransitionTo(DisplayState::kAttachedHidden);
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

void GlicNoWebviewContentsManager::NotifyWebContentsChanged() {
  web_contents_changed_callbacks_.Notify(active_web_contents());
}

void GlicNoWebviewContentsManager::ApplySizeToGuest() {
  if (cached_overlay_size_.IsEmpty() || !guest_contents() ||
      !guest_contents()->GetRenderWidgetHostView()) {
    return;
  }
  guest_contents()->GetRenderWidgetHostView()->SetSize(cached_overlay_size_);
  guest_contents()->UpdateWebContentsVisibility(content::Visibility::VISIBLE);
}

void GlicNoWebviewContentsManager::MaybeSwapToGuest() {
  if (!is_guest_ready_) {
    return;
  }
  if (is_visible_) {
    TransitionTo(DisplayState::kShowingGuest);
  } else {
    ScheduleOverlayDeletion(base::Milliseconds(0));
  }
}

void GlicNoWebviewContentsManager::OnGuestNavigationStarted() {
  StopGuestBootstrap();
  is_guest_ready_ = false;
  is_guest_error_ = false;
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
      SetErrorState(mojom::ErrorPanelType::kSignIn);
      break;
    case mojom::GuestPageType::kDisabledByAdmin:
      SetErrorState(mojom::ErrorPanelType::kDisabledByAdminWithLink);
      break;
    case mojom::GuestPageType::kLoadError:
      SetErrorState(mojom::ErrorPanelType::kError);
      break;
    case mojom::GuestPageType::kGuestError:
      // When the guest encounters an error page (/sorry/), present the guest
      // WebContents directly so the user can see the error or solve a CAPTCHA.
      is_guest_error_ = true;
      is_guest_ready_ = true;
      ApplySizeToGuest();
      MaybeSwapToGuest();
      break;
    case mojom::GuestPageType::kRegular:
      if (!is_api_allowed) {
        SetErrorState(mojom::ErrorPanelType::kError);
      } else {
        ApplySizeToGuest();
        StartGuestBootstrap();
      }
      break;
  }
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
      u"if (window.__glic_bootstrap_timer) { "
      u"clearTimeout(window.__glic_bootstrap_timer); "
      u"window.__glic_bootstrap_timer = null; }";
  guest_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptInIsolatedWorld(
      kStopScript, base::NullCallback(), ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

void GlicNoWebviewContentsManager::OnGuestProcessGone(
    base::TerminationStatus status) {
  StopGuestBootstrap();
  is_guest_ready_ = false;
  SetErrorState(mojom::ErrorPanelType::kError);
}

void GlicNoWebviewContentsManager::OnWebClientCreated() {
  StopGuestBootstrap();
  is_guest_ready_ = true;
  MaybeSwapToGuest();
}

void GlicNoWebviewContentsManager::OnWebClientStateChanged(
    mojom::WebClientState state) {
  switch (state) {
    case mojom::WebClientState::kResponsive:
      is_guest_ready_ = true;
      MaybeSwapToGuest();
      break;
    case mojom::WebClientState::kError:
      is_guest_ready_ = false;
      SetErrorState(mojom::ErrorPanelType::kError);
      break;
    case mojom::WebClientState::kUninitialized:
    case mojom::WebClientState::kWarmed:
    case mojom::WebClientState::kUnresponsive:
      break;
  }
}

void GlicNoWebviewContentsManager::LoadGuest() {
  is_guest_ready_ = false;
  is_guest_error_ = false;
  GURL guest_url = GetGuestURL();
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
    mojom::ErrorPanelType error_type) {
  StopGuestBootstrap();
  is_guest_ready_ = false;
  overlay_manager_.SetError(error_type);
  if (is_visible_) {
    TransitionTo(DisplayState::kShowingOverlay);
  }
}

void GlicNoWebviewContentsManager::SetVisibility(
    content::Visibility visibility) {
  is_visible_ = (visibility == content::Visibility::VISIBLE);
  if (is_visible_) {
    TransitionTo(is_guest_ready_ ? DisplayState::kShowingGuest
                                 : DisplayState::kShowingOverlay);
  } else {
    TransitionTo(host_ ? DisplayState::kAttachedHidden
                       : DisplayState::kWarming);
  }

  overlay_manager_.SetVisibility(visibility);
  if (!is_guest_ready_ && is_visible_ && overlay_contents() &&
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

// TODO(b/555365681): Remove once the legacy tab embedder is removed.
std::unique_ptr<content::WebContents>
GlicNoWebviewContentsManager::ReleaseWebContents() {
  NOTREACHED();
}

// TODO(b/555365681): Remove once the legacy tab embedder is removed.
void GlicNoWebviewContentsManager::ReclaimWebContents(
    std::unique_ptr<content::WebContents> web_contents) {
  NOTREACHED();
}

}  // namespace glic
