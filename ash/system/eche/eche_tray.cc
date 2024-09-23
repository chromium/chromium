// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/eche/eche_tray.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/eche/eche_icon_loading_indicator_view.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom-shared.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "ash/wm/window_state.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/session_manager_types.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_target.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "url/gurl.h"

// Uncomment the following line to make a fake
// bubble for local testing only.
// #define FAKE_BUBBLE_FOR_DEBUG

namespace ash {

namespace {

const char kEchePrewarmConnectionUrl[] = "chrome://eche-app";

// The icon size should be smaller than the tray item size to avoid the icon
// padding becoming negative.
constexpr int kIconSize = 24;

// This is how much the icon shrinks to give space for the spinner to go
// around it.
constexpr int kIconShrinkSizeForSpinner = 4;

constexpr int kHeaderHeight = 40;
constexpr int kHeaderHorizontalInteriorMargins = 0;
constexpr auto kHeaderDefaultSpacing = gfx::Insets::VH(0, 6);

constexpr auto kBubblePadding = gfx::Insets::VH(8, 8);

constexpr float kDefaultAspectRatio = 16.0 / 9.0f;
constexpr gfx::Size kDefaultBubbleSize(360, 360 * kDefaultAspectRatio);

// Max percentage of the screen height that can be covered by the eche bubble.
constexpr float kMaxHeightPercentage = 0.85;

// Unload timeout to close Eche Bubble in case error from Ech web during closing
constexpr base::TimeDelta kUnloadTimeoutDuration = base::Milliseconds(500);

// Timeout for initializer connection attempts.
constexpr base::TimeDelta kInitializerTimeout = base::Seconds(6);

// The ID for the "Tablet mode not supported" toast.
constexpr char kEcheTrayTabletModeNotSupportedId[] =
    "eche_tray_toast_ids.tablet_mode_not_supported";

// AcceleratorsActions which should be handled by the AcceleratorController, not
// the eche tray.
constexpr AcceleratorAction kLocallyProcessedAcceleratorActions[] = {
    AcceleratorAction::kOpenFeedbackPage,           // Shift + Alt + I
    AcceleratorAction::kExit,                       // Shift + Ctrl + Q
    AcceleratorAction::kShowShortcutViewer,         // Ctrl + Alt + /
    AcceleratorAction::kToggleCapsLock,             // Alt + Search
    AcceleratorAction::kNewWindow,                  // Ctrl + N
    AcceleratorAction::kNewIncognitoWindow,         // Shift + Ctrl + N
    AcceleratorAction::kNewTab,                     // Ctrl + T
    AcceleratorAction::kOpenFileManager,            // Shift + Alt + M
    AcceleratorAction::kLaunchApp0,                 // Alt + 1
    AcceleratorAction::kLaunchApp1,                 // Alt + 2
    AcceleratorAction::kLaunchApp2,                 // Alt + 3
    AcceleratorAction::kLaunchApp3,                 // Alt + 4
    AcceleratorAction::kLaunchApp4,                 // Alt + 5
    AcceleratorAction::kLaunchApp5,                 // Alt + 6
    AcceleratorAction::kLaunchApp6,                 // Alt + 7
    AcceleratorAction::kLaunchApp7,                 // Alt + 8
    AcceleratorAction::kLaunchLastApp,              // Alt + 9
    AcceleratorAction::kToggleMessageCenterBubble,  // Shift + Alt + N
    AcceleratorAction::kScaleUiUp,                  // Shift + Ctrl + "+"
    AcceleratorAction::kScaleUiDown,                // Shift + Ctrl + "-"
    AcceleratorAction::kScaleUiReset,               // Shift + Ctrl + 0
    AcceleratorAction::kRotateScreen,               // Shift + Ctrl + Refresh
    AcceleratorAction::kToggleSpokenFeedback,       // Ctrl + Alt + Z
    AcceleratorAction::kFocusShelf,                 // Shift + Alt + L
    AcceleratorAction::kFocusNextPane,              // Ctrl + Back
    AcceleratorAction::kFocusPreviousPane,          // Ctrl + Forward
    AcceleratorAction::kToggleAppList               // Launcher(Search)
};

// Creates a button with the given callback, icon, and tooltip text.
// `message_id` is the resource id of the tooltip text of the icon.
std::unique_ptr<views::Button> CreateButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    int message_id) {
  auto button = views::CreateVectorImageButton(std::move(callback));

  views::SetImageFromVectorIconWithColorId(
      button.get(), icon,
      static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface),
      static_cast<ui::ColorId>(cros_tokens::kButtonIconColorPrimaryDisabled));
  button->SetTooltipText(l10n_util::GetStringUTF16(message_id));
  button->SizeToPreferredSize();

  views::InstallCircleHighlightPathGenerator(button.get());

  return button;
}

std::unique_ptr<AshWebView> CreateWebview() {
  AshWebView::InitParams params;
  params.can_record_media = true;
  return AshWebViewFactory::Get()->Create(params);
}

void ConfigureLabelText(views::Label* title) {
  title->SetMultiLine(false);
  title->SetAllowCharacterBreak(true);
  title->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));
  title->SetProperty(views::kCrossAxisAlignmentKey,
                     views::LayoutAlignment::kStretch);
  title->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosHeadline1,
                                        *title);
}

}  // namespace

EcheTray::EventInterceptor::EventInterceptor(EcheTray* eche_tray)
    : eche_tray_(eche_tray) {}
EcheTray::EventInterceptor::~EventInterceptor() = default;

void EcheTray::EventInterceptor::OnKeyEvent(ui::KeyEvent* event) {
  if (eche_tray_->ProcessAcceleratorKeys(event)) {
    event->StopPropagation();
    return;
  }
}

EcheTray::EcheTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kEche),
      icon_(
          tray_container()->AddChildView(std::make_unique<views::ImageView>())),
      event_interceptor_(std::make_unique<EventInterceptor>(this)) {
  SetCallback(
      base::BindRepeating(&EcheTray::OnButtonPressed, base::Unretained(this)));

  const int icon_padding = (kTrayItemSize - kIconSize) / 2;

  icon_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(icon_padding, icon_padding)));

  // Observers setup
  // Note: `ScreenLayoutObserver` starts observing at its constructor.
  observed_session_.Observe(Shell::Get()->session_controller());
  icon_->SetTooltipText(GetAccessibleNameForTray());
  UpdateTrayItemColor(is_active());

  shelf_observation_.Observe(shelf);
  shell_observer_.Observe(Shell::Get());
  keyboard_observation_.Observe(keyboard::KeyboardUIController::Get());
}

EcheTray::~EcheTray() {
  if (bubble_) {
    bubble_->bubble_view()->ResetDelegate();
  }
  if (features::IsEcheNetworkConnectionStateEnabled() &&
      eche_connection_status_handler_) {
    eche_connection_status_handler_->RemoveObserver(this);
  }
}

bool EcheTray::IsInitialized() const {
  return GetBubbleWidget() != nullptr;
}

void EcheTray::ClickedOutsideBubble(const ui::LocatedEvent& event) {
  //  Do nothing
}

void EcheTray::UpdateTrayItemColor(bool is_active) {
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kPhoneHubPhoneIcon, is_active
                              ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                              : cros_tokens::kCrosSysOnSurface));
}

std::u16string EcheTray::GetAccessibleNameForTray() {
  // TODO(nayebi): Change this based on the final model of interaction
  // between phone hub and Eche.
  return l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME);
}

void EcheTray::HandleLocaleChange() {
  icon_->SetTooltipText(GetAccessibleNameForTray());
}

void EcheTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    HideBubble();
}

void EcheTray::AnchorUpdated() {
  if (bubble_)
    bubble_->bubble_view()->UpdateBubble();
}

void EcheTray::Initialize() {
  TrayBackgroundView::Initialize();

  // By default the icon is not visible until Eche notification is clicked on.
  bool visibility = false;
#ifdef FAKE_BUBBLE_FOR_DEBUG
  visibility = true;
#endif
  SetVisiblePreferred(visibility);
}

void EcheTray::CloseBubbleInternal() {
  if (bubble_)
    HideBubble();
}

void EcheTray::ShowBubble() {
#ifdef FAKE_BUBBLE_FOR_DEBUG
  LoadBubble(GURL("http://google.com"), std::move(gfx::Image()),
             u"visible_name");
  return;
#endif

  if (!bubble_)
    return;
  SetIconVisibility(true);
  StopLoadingAnimation();

  bubble_->GetBubbleWidget()->Show();
  bubble_->GetBubbleWidget()->Activate();

  bubble_->bubble_view()->SetVisible(true);
  // Since this tray already initialize the bubble before showing it in
  // `LoadBubble()`, we need to call `NotifyTrayBubbleOpen()` here.
  bubble_->bubble_view()->NotifyTrayBubbleOpen();

  SetIsActive(true);
  web_view_->GetInitiallyFocusedView()->RequestFocus();

  aura::Window* window = bubble_->GetBubbleWidget()->GetNativeWindow();
  if (!window)
    return;
  window = window->GetToplevelWindow();
  WindowState* window_state = WindowState::Get(window);
  // We need this as `WorkspaceLayoutManager` conflicts with our resizing.
  // See b/229111865#comment5
  window_state->set_ignore_keyboard_bounds_change(true);
  bubble_->GetBubbleWidget()->GetNativeWindow()->AddPreTargetHandler(
      event_interceptor_.get());
  shelf()->UpdateAutoHideState();
  if (bubble_shown_callback_) {
    bubble_shown_callback_.Run(web_view_);
  }
}

TrayBubbleView* EcheTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

views::Widget* EcheTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

void EcheTray::OnVirtualKeyboardVisibilityChanged() {
  OnKeyboardVisibilityChanged(KeyboardController::Get()->IsKeyboardVisible());
  TrayBackgroundView::OnVirtualKeyboardVisibilityChanged();
}

bool EcheTray::CacheBubbleViewForHide() const {
  return true;
}

std::u16string EcheTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool EcheTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void EcheTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void EcheTray::OnStreamStatusChanged(eche_app::mojom::StreamStatus status) {
  switch (status) {
    case eche_app::mojom::StreamStatus::kStreamStatusStarted:
      // Reset the timestamp when the streaming is started.
      init_stream_timestamp_.reset();
      is_stream_started_ = true;
      ShowBubble();
      break;
    case eche_app::mojom::StreamStatus::kStreamStatusStopped:
      is_stream_started_ = false;
      PurgeAndClose();
      break;
    case eche_app::mojom::StreamStatus::kStreamStatusInitializing:
      is_stream_started_ = false;
      break;
    case eche_app::mojom::StreamStatus::kStreamStatusUnknown:
      PA_LOG(WARNING) << "Unexpected stream status";
      is_stream_started_ = false;
      break;
  }
}

void EcheTray::OnLockStateChanged(bool locked) {
  if (bubble_ && locked)
    PurgeAndClose();
}

void EcheTray::OnKeyboardUIDestroyed() {
  if (!IsBubbleVisible())
    return;
  UpdateEcheSizeAndBubbleBounds();
}

void EcheTray::OnKeyboardHidden(bool is_temporary_hide) {
  if (!IsBubbleVisible())
    return;
  UpdateEcheSizeAndBubbleBounds();
}

void EcheTray::OnConnectionStatusChanged(
    eche_app::mojom::ConnectionStatus connection_status) {
  if (!features::IsEcheNetworkConnectionStateEnabled() ||
      !initializer_webview_) {
    return;
  }

  switch (connection_status) {
    case eche_app::mojom::ConnectionStatus::kConnectionStatusConnecting:
      break;

    case eche_app::mojom::ConnectionStatus::kConnectionStatusConnected:
      PA_LOG(INFO) << "Connection successful, updating UI to connected.";
      eche_connection_status_handler_->SetConnectionStatusForUi(
          connection_status);
      has_reported_initializer_result_ = true;
      base::UmaHistogramBoolean("Eche.NetworkCheck.Result", true);
      StartGracefulCloseInitializer();
      break;
    case eche_app::mojom::ConnectionStatus::kConnectionStatusFailed:
      PA_LOG(WARNING) << "Connection failed, updating UI to error state.";
      eche_connection_status_handler_->SetConnectionStatusForUi(
          connection_status);
      base::UmaHistogramBoolean("Eche.NetworkCheck.Result", false);
      has_reported_initializer_result_ = true;
      StartGracefulCloseInitializer();
      break;
    case eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected:
      // If we've timed out or been disconnected before a success/failure has
      // come in, report failure, unless we intentionally disconnected in
      // preparation for an app stream launch.
      if (!has_reported_initializer_result_ && !on_initializer_closed_) {
        PA_LOG(WARNING)
            << "Disconnected without result, updating UI to error state.";
        base::UmaHistogramBoolean("Eche.NetworkCheck.Result", false);
        eche_connection_status_handler_->SetConnectionStatusForUi(
            eche_app::mojom::ConnectionStatus::kConnectionStatusFailed);
      }
      // If the status is changed kConnectionStatusDisconnected before the
      // timeout, manually cancel the timeout task. Also notify that the
      // connection has been closed so that each component can clean up.
      if (initializer_timeout_) {
        initializer_timeout_.reset();
        eche_connection_status_handler_->NotifyConnectionClosed();
      }
      initializer_webview_.reset();
      break;
  }
}

void EcheTray::OnRequestBackgroundConnectionAttempt() {
  if (!features::IsEcheNetworkConnectionStateEnabled() || web_view_) {
    return;
  }
  has_reported_initializer_result_ = false;
  initializer_webview_ = CreateWebview();
  initializer_webview_->Navigate(GURL(kEchePrewarmConnectionUrl));
  initializer_timeout_ = std::make_unique<base::DelayTimer>(
      FROM_HERE, kInitializerTimeout, this,
      &EcheTray::OnBackgroundConnectionTimeout);
  initializer_timeout_->Reset();  // Starts the timer.
  SetIconVisibility(false);
}

void EcheTray::OnStatusAreaAnchoredBubbleVisibilityChanged(
    TrayBubbleView* tray_bubble,
    bool visible) {
  // We only care about "other" bubbles being shown.
  if (!bubble_ || tray_bubble == GetBubbleView()) {
    return;
  }

  // Another bubble has become visible, so minimize this one.
  if (visible && IsBubbleVisible()) {
    HideBubble();
  }
}

void EcheTray::CloseInitializer() {
  initializer_webview_.reset();
  if (on_initializer_closed_) {
    std::move(on_initializer_closed_).Run();
  }
}

void EcheTray::OnBackgroundConnectionTimeout() {
  if (!initializer_webview_ || web_view_) {
    return;
  }

  // Notify that the connection attempt failed reset the connection status for
  // timeouts, this happens automatically for other failures.
  eche_connection_status_handler_->SetConnectionStatusForUi(
      eche_app::mojom::ConnectionStatus::kConnectionStatusFailed);
  StartGracefulCloseInitializer();
}

void EcheTray::StartGracefulCloseInitializer() {
  if (!initializer_webview_) {
    return;
  }

  initializer_timeout_.reset();
  eche_connection_status_handler_->NotifyRequestCloseConnection();
  unload_timer_ = std::make_unique<base::DelayTimer>(
      FROM_HERE, kUnloadTimeoutDuration, this, &EcheTray::CloseInitializer);
  unload_timer_->Reset();  // Starts the timer.
}

void EcheTray::OnButtonPressed() {
  // The `bubble_` is cached, so don't check for existence (which is the base
  // TrayBackgroundView implementation), check for visibility to decide on
  // whether to show or hide.
  if (IsBubbleVisible()) {
    HideBubble();
    return;
  }
  ShowBubble();
}

void EcheTray::SetUrl(const GURL& url) {
  if (web_view_ && url_ != url)
    web_view_->Navigate(url);
  url_ = url;
}

void EcheTray::SetIcon(const gfx::Image& icon,
                       const std::u16string& tooltip_text) {
  views::ImageButton* icon_view = GetIcon();
  if (icon_view) {
    icon_view->SetImageModel(
        views::ImageButton::STATE_NORMAL,
        ui::ImageModel::FromImageSkia(
            gfx::ImageSkiaOperations::CreateResizedImage(
                icon.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
                gfx::Size(kIconSize, kIconSize))));
    icon_view->SetTooltipText(tooltip_text);
    SetIconVisibility(true);
  }
}

bool EcheTray::LoadBubble(
    const GURL& url,
    const gfx::Image& icon,
    const std::u16string& visible_name,
    const std::u16string& phone_name,
    eche_app::mojom::ConnectionStatus last_connection_status,
    eche_app::mojom::AppStreamLaunchEntryPoint entry_point) {
  if (Shell::Get()->IsInTabletMode()) {
    ash::ToastManager::Get()->Show(ash::ToastData(
        kEcheTrayTabletModeNotSupportedId,
        ash::ToastCatalogName::kEcheTrayTabletModeNotSupported,
        l10n_util::GetStringUTF16(IDS_ASH_ECHE_TOAST_TABLET_MODE_NOT_SUPPORTED),
        ash::ToastData::kDefaultToastDuration,
        /*visible_on_lock_screen=*/false));
    PA_LOG(WARNING) << "Eche load failed due to tablet mode.";
    base::UmaHistogramEnumeration(
        "Eche.StreamEvent.ConnectionFail",
        EcheTray::ConnectionFailReason::kConnectionFailInTabletMode);
    return false;
  }
  SetUrl(url);
  SetIcon(icon, /*tooltip_text=*/visible_name);
  // If the bubble is already initialized, setting the icon and url was enough
  // to navigate the bubble to the new address.
  if (IsInitialized()) {
    ShowBubble();
    return true;
  }
  InitBubble(phone_name, last_connection_status, entry_point);
  StartLoadingAnimation();
  auto* phone_hub_tray = GetPhoneHubTray();
  if (phone_hub_tray) {
    phone_hub_tray->SetEcheIconActivationCallback(base::BindRepeating(
        &EcheTray::OnButtonPressed, base::Unretained(this)));
  }
  // Hide bubble first until the streaming is ready.
  HideBubble();
  return true;
}

void EcheTray::PurgeAndClose() {
  StopLoadingAnimation();
  SetIconVisibility(false);
  is_landscape_ = false;

  if (!bubble_)
    return;

  auto* bubble_view = bubble_->GetBubbleView();
  if (bubble_view)
    bubble_view->ResetDelegate();

  SetIsActive(false);
  SetVisiblePreferred(false);
  web_view_ = nullptr;
  close_button_ = nullptr;
  minimize_button_ = nullptr;
  arrow_back_button_ = nullptr;
  unload_timer_.reset();
  bubble_.reset();
  init_stream_timestamp_.reset();
}

void EcheTray::SetGracefulCloseCallback(
    GracefulCloseCallback graceful_close_callback) {
  if (!graceful_close_callback)
    return;
  graceful_close_callback_ = std::move(graceful_close_callback);
}

void EcheTray::SetGracefulGoBackCallback(
    GracefulGoBackCallback graceful_go_back_callback) {
  if (!graceful_go_back_callback)
    return;
  graceful_go_back_callback_ = std::move(graceful_go_back_callback);
}

void EcheTray::SetBubbleShownCallback(
    BubbleShownCallback bubble_shown_callback) {
  if (!bubble_shown_callback) {
    return;
  }
  bubble_shown_callback_ = std::move(bubble_shown_callback);
}

void EcheTray::HideBubble() {
  if (!bubble_)
    return;
  bubble_->GetBubbleWidget()->GetNativeWindow()->RemovePreTargetHandler(
      event_interceptor_.get());
  SetIsActive(false);
  bubble_->bubble_view()->SetVisible(false);

  // Since this tray just hide and do not destroy the bubble when closing, we
  // need to call `NotifyTrayBubbleClosed()`.
  bubble_->bubble_view()->NotifyTrayBubbleClosed();

  bubble_->GetBubbleWidget()->Deactivate();
  bubble_->GetBubbleWidget()->Hide();
  shelf()->UpdateAutoHideState();
}

void EcheTray::InitBubble(
    const std::u16string& phone_name,
    eche_app::mojom::ConnectionStatus last_connection_status,
    eche_app::mojom::AppStreamLaunchEntryPoint entry_point) {
  // We only support a single connection between the phone and chromebook, if
  // there's an existing background connection it must first be disconnected
  // before we can continue with the app stream initialization.
  // TODO(b/283880725) re-use the existing connection instead of terminating it
  // and starting a new one.
  if (initializer_webview_) {
    PA_LOG(INFO)
        << "Active background connection must be terminated prior to launching "
           "app.  Saving launch details and will retry once ready.";
    on_initializer_closed_ =
        base::BindOnce(&EcheTray::InitBubble, base::Unretained(this),
                       phone_name, last_connection_status, entry_point);
    StartGracefulCloseInitializer();
    return;
  }

  if (features::IsEcheNetworkConnectionStateEnabled() &&
      last_connection_status !=
          eche_app::mojom::ConnectionStatus::kConnectionStatusConnected &&
      entry_point == eche_app::mojom::AppStreamLaunchEntryPoint::NOTIFICATION) {
    base::UmaHistogramEnumeration(
        "Eche.StreamEvent.FromNotification.PreviousNetworkCheckFailed.Result",
        eche_app::mojom::StreamStatus::kStreamStatusInitializing);
  } else {
    base::UmaHistogramEnumeration(
        "Eche.StreamEvent",
        eche_app::mojom::StreamStatus::kStreamStatusInitializing);
    switch (entry_point) {
      case eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST:
        base::UmaHistogramEnumeration(
            "Eche.StreamEvent.FromLauncher",
            eche_app::mojom::StreamStatus::kStreamStatusInitializing);
        break;
      case eche_app::mojom::AppStreamLaunchEntryPoint::NOTIFICATION:
        base::UmaHistogramEnumeration(
            "Eche.StreamEvent.FromNotification",
            eche_app::mojom::StreamStatus::kStreamStatusInitializing);
        break;
      case eche_app::mojom::AppStreamLaunchEntryPoint::RECENT_APPS:
        base::UmaHistogramEnumeration(
            "Eche.StreamEvent.FromRecentApps",
            eche_app::mojom::StreamStatus::kStreamStatusInitializing);
        break;
      case eche_app::mojom::AppStreamLaunchEntryPoint::UNKNOWN:
        NOTREACHED();
    }
  }
  init_stream_timestamp_ = base::TimeTicks::Now();
  TrayBubbleView::InitParams init_params = CreateInitParamsForTrayBubble(
      /*tray=*/this, /*anchor_to_shelf_corner=*/true);

  // Note: The container id must be smaller than `kShellWindowId_ShelfContainer`
  // in order to let the notifications be shown on top of the eche window.
  init_params.parent_window = Shell::GetContainer(
      tray_container()->GetWidget()->GetNativeWindow()->GetRootWindow(),
      kShellWindowId_AlwaysOnTopContainer);
  const gfx::Size eche_size = CalculateSizeForEche();
  init_params.preferred_width = eche_size.width();
  init_params.close_on_deactivate = false;
  init_params.reroute_event_handler = false;

  phone_name_ = phone_name;

  auto bubble_view = std::make_unique<TrayBubbleView>(init_params);
  bubble_view->SetCanActivate(true);
  bubble_view->SetBorder(views::CreateEmptyBorder(kBubblePadding));

  header_view_ = bubble_view->AddChildView(CreateBubbleHeaderView(phone_name));

  // We need the header be always visible with the same size.
  bubble_view->box_layout()->SetFlexForView(header_view_, 0, true);
  bubble_view->box_layout()->set_inside_border_insets(kBubblePadding);

  // TODO(b/271478560): Re-use initializer_webview_ when available, once support
  // launching apps on prewarmed connection is available.
  auto web_view = CreateWebview();
  web_view->SetPreferredSize(eche_size);
  if (!url_.is_empty())
    web_view->Navigate(url_);
  web_view_ = bubble_view->AddChildView(std::move(web_view));

  bubble_ = std::make_unique<TrayBubbleWrapper>(this,
                                                /*event_handling=*/false);
  bubble_->ShowBubble(std::move(bubble_view));
  SetIsActive(true);
  bubble_->GetBubbleView()->UpdateBubble();
}

void EcheTray::StartGracefulClose() {
  if (init_stream_timestamp_.has_value()) {
    base::UmaHistogramLongTimes100(
        "Eche.StreamEvent.Duration.FromInitializeToClose",
        base::TimeTicks::Now() - *init_stream_timestamp_);
    init_stream_timestamp_.reset();
  }

  // If there's an initializer session running it should also be shutdown.
  StartGracefulCloseInitializer();

  if (!graceful_close_callback_) {
    PurgeAndClose();
    return;
  }
  HideBubble();
  std::move(graceful_close_callback_).Run();
  // Graceful close will let Eche Web to close connection release then notify
  // back to native code to close window. In case there is any exception happens
  // in js layer, start a timer to force close widget in case unload can't be
  // finished.
  if (!unload_timer_) {
    unload_timer_ = std::make_unique<base::DelayTimer>(
        FROM_HERE, kUnloadTimeoutDuration, this, &EcheTray::PurgeAndClose);
    unload_timer_->Reset();
  }
}

gfx::Size EcheTray::CalculateSizeForEche() const {
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(
              tray_container()->GetWidget()->GetNativeWindow())
          .work_area();
  float height_scale =
      (static_cast<float>(work_area_bounds.height()) * kMaxHeightPercentage) /
      kDefaultBubbleSize.height();
  height_scale = std::min(height_scale, 1.0f);
  gfx::Size size = gfx::ScaleToFlooredSize(kDefaultBubbleSize, height_scale);

  // TODO(b/258306301): Verify the correct sizing for Landscape
  if (is_landscape_) {
    size = gfx::Size(size.height(), size.width());
  }

  return size;
}

void EcheTray::OnArrowBackActivated() {
  if (web_view_) {
    // TODO(b/228909439): Call `web_view_` GoBack with
    // `graceful_go_back_callback_` together to avoid the back button not
    // working when the stream action GoBack isn’t ready in web content yet.
    // Remove this when the stream action GoBack is ready in web content.
    web_view_->GoBack();

    if (graceful_go_back_callback_)
      graceful_go_back_callback_.Run();
  }
}

std::unique_ptr<views::View> EcheTray::CreateBubbleHeaderView(
    const std::u16string& phone_name) {
  auto header = std::make_unique<views::View>();
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(gfx::Insets::VH(0, kHeaderHorizontalInteriorMargins))
      .SetCollapseMargins(false)
      .SetMinimumCrossAxisSize(kHeaderHeight)
      .SetDefault(views::kMarginsKey, kHeaderDefaultSpacing)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Add arrowback button
  arrow_back_button_ = header->AddChildView(
      CreateButton(base::BindRepeating(&EcheTray::OnArrowBackActivated,
                                       weak_factory_.GetWeakPtr()),
                   kEcheArrowBackIcon, IDS_APP_ACCNAME_BACK));

  views::Label* title = header->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringFUTF16(ID_ASH_ECHE_APP_STREAMING_BUBBLE_TITLE,
                                 phone_name),
      views::style::CONTEXT_DIALOG_TITLE, views::style::STYLE_PRIMARY,
      gfx::DirectionalityMode::DIRECTIONALITY_FROM_TEXT));
  ConfigureLabelText(title);

  // Add minimize button
  minimize_button_ = header->AddChildView(CreateButton(
      base::BindRepeating(&EcheTray::CloseBubble, weak_factory_.GetWeakPtr()),
      kEcheMinimizeIcon, IDS_APP_ACCNAME_MINIMIZE));

  // Add close button
  close_button_ = header->AddChildView(
      CreateButton(base::BindRepeating(&EcheTray::StartGracefulClose,
                                       weak_factory_.GetWeakPtr()),
                   kEcheCloseIcon, IDS_APP_ACCNAME_CLOSE));

  return header;
}

views::Button* EcheTray::GetMinimizeButtonForTesting() const {
  return minimize_button_;
}

views::Button* EcheTray::GetCloseButtonForTesting() const {
  return close_button_;
}

views::Button* EcheTray::GetArrowBackButtonForTesting() const {
  return arrow_back_button_;
}

views::ImageButton* EcheTray::GetIcon() {
  PhoneHubTray* phone_hub_tray = GetPhoneHubTray();
  if (!phone_hub_tray)
    return nullptr;
  return phone_hub_tray->eche_icon_view();
}

void EcheTray::ResizeIcon(int offset_dip) {
  views::ImageButton* icon_view = GetIcon();
  if (icon_view) {
    auto icon = icon_view->GetImage(views::ImageButton::STATE_NORMAL);
    icon_view->SetImageModel(
        views::ImageButton::STATE_NORMAL,
        ui::ImageModel::FromImageSkia(
            gfx::ImageSkiaOperations::CreateResizedImage(
                icon, skia::ImageOperations::RESIZE_BEST,
                gfx::Size(kIconSize - offset_dip, kIconSize - offset_dip))));
    GetPhoneHubTray()->tray_container()->UpdateLayout();
  }
}

void EcheTray::StopLoadingAnimation() {
  ResizeIcon(0);
  auto* loading_indicator = GetLoadingIndicator();
  if (loading_indicator && loading_indicator->GetAnimating()) {
    loading_indicator->SetAnimating(false);
  }
}

void EcheTray::StartLoadingAnimation() {
  ResizeIcon(kIconShrinkSizeForSpinner);
  auto* loading_indicator = GetLoadingIndicator();
  if (loading_indicator) {
    loading_indicator->SetAnimating(true);
  }
}

void EcheTray::SetIconVisibility(bool visibility) {
  auto* icon = GetIcon();
  if (!icon)
    return;
  icon->SetVisible(visibility);
  GetPhoneHubTray()->tray_container()->UpdateLayout();
}

PhoneHubTray* EcheTray::GetPhoneHubTray() {
  return shelf()->GetStatusAreaWidget()->phone_hub_tray();
}

EcheIconLoadingIndicatorView* EcheTray::GetLoadingIndicator() {
  PhoneHubTray* phone_hub_tray = GetPhoneHubTray();
  if (!phone_hub_tray)
    return nullptr;
  return phone_hub_tray->eche_loading_indicator();
}

void EcheTray::UpdateEcheSizeAndBubbleBounds() {
  if (!bubble_ || !bubble_->GetBubbleView())
    return;
  gfx::Size eche_size = CalculateSizeForEche();
  bubble_->GetBubbleView()->SetPreferredWidth(eche_size.width());
  web_view_->SetPreferredSize(eche_size);
  bubble_->GetBubbleView()->ChangeAnchorRect(
      shelf()->GetSystemTrayAnchorRect());
}

void EcheTray::OnDidApplyDisplayChanges() {
  UpdateEcheSizeAndBubbleBounds();
}

void EcheTray::OnAutoHideStateChanged(ShelfAutoHideState state) {
  UpdateEcheSizeAndBubbleBounds();
}

void EcheTray::OnDisplayTabletStateChanged(display::TabletState state) {
  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
    case display::TabletState::kInTabletMode:
      OnTabletModeStarted();
      break;
    case display::TabletState::kInClamshellMode:
      UpdateEcheSizeAndBubbleBounds();
      break;
  }
}

void EcheTray::OnTabletModeStarted() {
  if (!IsBubbleVisible())
    return;

  // Device changes to tablet mode but the streaming has not started yet, we
  // should log as connection failure.
  if (!is_stream_started_) {
    base::UmaHistogramEnumeration(
        "Eche.StreamEvent.ConnectionFail",
        EcheTray::ConnectionFailReason::kConnectionFailInTabletMode);
  }
  ash::ToastManager::Get()->Show(ash::ToastData(
      kEcheTrayTabletModeNotSupportedId,
      ash::ToastCatalogName::kEcheTrayTabletModeNotSupported,
      l10n_util::GetStringUTF16(IDS_ASH_ECHE_TOAST_TABLET_MODE_NOT_SUPPORTED),
      ash::ToastData::kDefaultToastDuration,
      /*visible_on_lock_screen=*/false));
  PurgeAndClose();
}

void EcheTray::OnShelfAlignmentChanged(aura::Window* root_window,
                                       ShelfAlignment old_alignment) {
  UpdateEcheSizeAndBubbleBounds();
}

void EcheTray::OnStreamOrientationChanged(bool is_landscape) {
  if (is_landscape_ == is_landscape) {
    return;
  }

  is_landscape_ = is_landscape;
  UpdateEcheSizeAndBubbleBounds();
}

// TODO(b/234848974): Try to use View::AddAccelerator for the bubble view
// and then add the handler in View::AcceleratorPressed.
bool EcheTray::ProcessAcceleratorKeys(ui::KeyEvent* event) {
  ui::Accelerator accelerator(*event);

  auto* accelerator_controller = AcceleratorController::Get();

  // Process minimize action
  // Please note that the bubble is not a normal window and it has a special
  // minimize behavior that is closer to hide than real minimize.
  //
  // TODO(https://crbug/1338650): See if we can just leave this to be handled
  // upper in the chain and perform the minimize by reacting to
  // ToggleMinimized().
  if (accelerator_controller->DoesAcceleratorMatchAction(
          accelerator, AcceleratorAction::kWindowMinimize)) {
    CloseBubble();
    return true;
  }

  for (AcceleratorAction accelerator_action :
       kLocallyProcessedAcceleratorActions) {
    if (accelerator_controller->DoesAcceleratorMatchAction(
            accelerator, accelerator_action)) {
      views::ViewsDelegate::GetInstance()->ProcessAcceleratorWhileMenuShowing(
          accelerator);
      event->StopPropagation();
      return true;
    }
  }

  const ui::KeyboardCode key_code = event->key_code();
  const bool is_only_control_down = ui::Accelerator::MaskOutKeyEventFlags(
                                        event->flags()) == ui::EF_CONTROL_DOWN;
  const bool any_modifier_pressed =
      ui::Accelerator::MaskOutKeyEventFlags(event->flags());

  if (event->type() != ui::EventType::kKeyPressed) {
    return false;
  }

  switch (key_code) {
    case ui::VKEY_W:
      if (!is_only_control_down)
        return false;
      // Please note that ctrl+w does not have a global accelerator action
      // similar to AcceleratorAction::kWindowMinimize that was used above.
      //
      // TODO(https://crbug/1338650): See if we can just leave this to be
      // handled upper in the chain.
      StartGracefulClose();
      return true;
    case ui::VKEY_ESCAPE:
      StartGracefulClose();
      return true;
    case ui::VKEY_BROWSER_BACK:
      if (any_modifier_pressed)
        return false;
      OnArrowBackActivated();
      return true;

    default:
      return false;
  }
}

bool EcheTray::IsBubbleVisible() {
  return bubble_ && bubble_->GetBubbleView() &&
         bubble_->GetBubbleView()->GetVisible();
}

void EcheTray::SetEcheConnectionStatusHandler(
    eche_app::EcheConnectionStatusHandler* eche_connection_status_handler) {
  if (features::IsEcheNetworkConnectionStateEnabled()) {
    eche_connection_status_handler_ = eche_connection_status_handler;
    eche_connection_status_handler_->AddObserver(this);
  }
}

bool EcheTray::IsBackgroundConnectionAttemptInProgress() {
  return initializer_webview_ ? true : false;
}

BEGIN_METADATA(EcheTray)
END_METADATA

}  // namespace ash
