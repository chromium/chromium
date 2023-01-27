// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_tray.h"
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/eche/eche_icon_loading_indicator_view.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/phonehub/phone_hub_content_view.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/quick_actions_view.h"
#include "ash/system/phonehub/task_continuation_view.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/phonehub/icon_decoder.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/phone_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

// Padding for tray icons (dp; the button that shows the phone_hub menu).
constexpr int kTrayIconMainAxisInset = 6;
constexpr int kTrayIconCrossAxisInset = 0;
constexpr int kEcheIconMinSize = 24;
constexpr int kIconSpacing = 12;

constexpr auto kBubblePadding =
    gfx::Insets::TLBR(0, 0, kBubbleBottomPaddingDip, 0);

bool IsInUserSession() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         !session_controller->IsRunningInAppMode();
}

}  // namespace

PhoneHubTray::PhoneHubTray(Shelf* shelf)
    : TrayBackgroundView(shelf, TrayBackgroundViewCatalogName::kPhoneHub),
      ui_controller_(new PhoneHubUiController()),
      phone_hub_nudge_controller_(
          features::IsPhoneHubNudgeEnabled()
              ? std::make_unique<PhoneHubNudgeController>()
              : nullptr) {
  // By default, if the individual buttons did not handle the event consider it
  // as a phone hub icon event.
  SetPressedCallback(base::BindRepeating(&PhoneHubTray::PhoneHubIconActivated,
                                         base::Unretained(this)));
  observed_phone_hub_ui_controller_.Observe(ui_controller_.get());
  observed_session_.Observe(Shell::Get()->session_controller());

  tray_container()->SetMargin(kTrayIconMainAxisInset, kTrayIconCrossAxisInset);
  // TODO(nayebi): Think about constructing the eche_icon outside of this class,
  // either as an input argument or being set through a setter.
  if (features::IsEcheSWAEnabled()) {
    auto eche_icon = std::make_unique<views::ImageButton>(base::BindRepeating(
        &PhoneHubTray::EcheIconActivated, weak_factory_.GetWeakPtr()));
    eche_icon->SetImageVerticalAlignment(
        views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
    eche_icon->SetImageHorizontalAlignment(
        views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
    eche_icon->SetMinimumImageSize(
        gfx::Size(kEcheIconMinSize, kEcheIconMinSize));
    eche_icon->SetVisible(false);
    eche_loading_indicator_ = eche_icon->AddChildView(
        std::make_unique<EcheIconLoadingIndicatorView>(eche_icon.get()));
    eche_loading_indicator_->SetVisible(false);
    eche_icon_ = tray_container()->AddChildView(std::move(eche_icon));
    tray_container()->SetSpacingBetweenChildren(kIconSpacing);
  }
  auto icon = std::make_unique<views::ImageButton>(base::BindRepeating(
      &PhoneHubTray::PhoneHubIconActivated, weak_factory_.GetWeakPtr()));
  icon->SetFocusBehavior(FocusBehavior::NEVER);
  icon->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME));
  icon->SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  icon->SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  icon->SetImageModel(views::ImageButton::STATE_NORMAL,
                      ui::ImageModel::FromVectorIcon(
                          kPhoneHubPhoneIcon, kColorAshIconColorPrimary));
  icon_ = tray_container()->AddChildView(std::move(icon));
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
}

PhoneHubTray::~PhoneHubTray() {
  if (bubble_)
    bubble_->bubble_view()->ResetDelegate();
  if (phone_hub_manager_) {
    phone_hub_manager_->GetAppStreamManager()->RemoveObserver(this);
  }
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
}

void PhoneHubTray::SetPhoneHubManager(
    phonehub::PhoneHubManager* phone_hub_manager) {
  ui_controller_->SetPhoneHubManager(phone_hub_manager);
  if (phone_hub_manager_) {
    phone_hub_manager_->GetAppStreamManager()->RemoveObserver(this);
  }
  if (phone_hub_manager) {
    phone_hub_manager->GetAppStreamManager()->AddObserver(this);
  }
  phone_hub_manager_ = phone_hub_manager;
}

void PhoneHubTray::ClickedOutsideBubble() {
  CloseBubble();
}

std::u16string PhoneHubTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME);
}

void PhoneHubTray::HandleLocaleChange() {
  icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME));
}

void PhoneHubTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    CloseBubble();
}

std::u16string PhoneHubTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool PhoneHubTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void PhoneHubTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void PhoneHubTray::OnPhoneHubUiStateChanged() {
  UpdateVisibility();
  UpdateHeaderVisibility();

  if (!bubble_)
    return;
  TrayBubbleView* bubble_view = bubble_->bubble_view();

  DCHECK(ui_controller_.get());
  std::unique_ptr<PhoneHubContentView> content_view =
      ui_controller_->CreateContentView(this);
  if (!content_view.get()) {
    CloseBubble();
    return;
  }

  if (content_view_) {
    // If we are already showing the same content_view, no need to remove and
    // update the tray.
    // TODO(crbug.com/1185316) : Find way to update views without work around
    // when same view is removed and added.
    if (content_view->GetID() == content_view_->GetID())
      return;

    bubble_view->RemoveChildView(content_view_);
    delete content_view_;
  }
  content_view_ = bubble_view->AddChildView(std::move(content_view));

  // Updates bubble to handle possible size change with a different child view.
  bubble_view->UpdateBubble();
}

void PhoneHubTray::OnSessionStateChanged(session_manager::SessionState state) {
  TemporarilyDisableAnimation();
}

void PhoneHubTray::OnActiveUserSessionChanged(const AccountId& account_id) {
  TemporarilyDisableAnimation();
}

void PhoneHubTray::AnchorUpdated() {
  if (bubble_)
    bubble_->bubble_view()->UpdateBubble();
}

void PhoneHubTray::OnDisplayConfigurationChanged() {
  if (!bubble_ || !bubble_->GetBubbleView())
    return;
  bubble_->GetBubbleView()->ChangeAnchorRect(
      shelf()->GetSystemTrayAnchorRect());
}

void PhoneHubTray::Initialize() {
  TrayBackgroundView::Initialize();
  // For secondary displays to have Phone Hub visible, manager must
  // be set.
  phonehub::PhoneHubManager* phone_hub_tray_manager =
      Shell::Get()->system_tray_model()->phone_hub_manager();
  if (phone_hub_tray_manager) {
    SetPhoneHubManager(phone_hub_tray_manager);
  }
  UpdateVisibility();
}

void PhoneHubTray::ShowBubble() {
  if (bubble_)
    return;

  ui_controller_->HandleBubbleOpened();

  TrayBubbleView::InitParams init_params;
  init_params.delegate = GetWeakPtr();
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = shelf()->GetSystemTrayAnchorRect();
  init_params.insets = GetTrayBubbleInsets();
  init_params.shelf_alignment = shelf()->alignment();
  init_params.preferred_width = kTrayMenuWidth;
  init_params.close_on_deactivate = true;
  init_params.translucent = true;
  init_params.reroute_event_handler = true;
  init_params.corner_radius = kTrayItemCornerRadius;

  auto bubble_view = std::make_unique<TrayBubbleView>(init_params);
  bubble_view->SetBorder(views::CreateEmptyBorder(kBubblePadding));

  // Creates header view on top for displaying phone status and settings icon.
  auto phone_status = ui_controller_->CreateStatusHeaderView(this);
  phone_status_view_dont_use_ = phone_status.get();
  DCHECK(phone_status_view_dont_use_);
  bubble_view->AddChildView(std::move(phone_status));

  // Other contents, i.e. the connected view and the interstitial views,
  // will be positioned underneath the phone status view and updated based
  // on the current mode.
  auto content_view = ui_controller_->CreateContentView(this);
  content_view_ = content_view.get();

  if (!content_view_) {
    CloseBubble();
    return;
  }

  bubble_view->AddChildView(std::move(content_view));

  bubble_ = std::make_unique<TrayBubbleWrapper>(this);
  bubble_->ShowBubble(std::move(bubble_view));
  UpdateHeaderVisibility();

  SetIsActive(true);

  phone_hub_metrics::LogScreenOnBubbleOpen(
      content_view_->GetScreenForMetrics());
}

TrayBubbleView* PhoneHubTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

views::Widget* PhoneHubTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

const char* PhoneHubTray::GetClassName() const {
  return "PhoneHubTray";
}

bool PhoneHubTray::CanOpenConnectedDeviceSettings() {
  return TrayPopupUtils::CanOpenWebUISettings();
}

void PhoneHubTray::OpenConnectedDevicesSettings() {
  DCHECK(content_view_);
  phone_hub_metrics::LogScreenOnSettingsButtonClicked(
      content_view_->GetScreenForMetrics());

  DCHECK(CanOpenConnectedDeviceSettings());
  Shell::Get()->system_tray_model()->client()->ShowConnectedDevicesSettings();
}

void PhoneHubTray::HideStatusHeaderView() {
  if (!GetPhoneStatusView())
    return;

  GetPhoneStatusView()->SetVisible(false);
  bubble_->bubble_view()->UpdateBubble();
}

void PhoneHubTray::OnAppStreamUpdate(
    const phonehub::proto::AppStreamUpdate app_stream_update) {
  auto* app = &app_stream_update.foreground_app();
  // TODO(nayebi): Try to extract this decoding process into a shared code
  // inside the icon decoder.
  // Decode the icon
  std::unique_ptr<std::vector<phonehub::IconDecoder::DecodingData>>
      decoding_data_list =
          std::make_unique<std::vector<phonehub::IconDecoder::DecodingData>>();
  std::hash<std::string> str_hash;
  std::string key = app->package_name() + base::NumberToString(app->user_id());
  phonehub::IconDecoder::DecodingData decoding_data =
      phonehub::IconDecoder::DecodingData(str_hash(key), app->icon());
  // load with default image
  decoding_data.result =
      gfx::Image(CreateVectorIcon(kPhoneHubPhoneIcon, gfx::kGoogleGrey700));
  decoding_data_list->push_back(decoding_data);
  phone_hub_manager_->GetIconDecoder()->BatchDecode(
      std::move(decoding_data_list),
      base::BindOnce(&PhoneHubTray::OnIconsDecoded, weak_factory_.GetWeakPtr(),
                     app->visible_name()));
}

void PhoneHubTray::OnIconsDecoded(
    std::string visible_name,
    std::unique_ptr<std::vector<phonehub::IconDecoder::DecodingData>>
        decoding_data_list) {
  if (decoding_data_list->empty())
    return;
  EcheTray* eche_tray = Shell::GetPrimaryRootWindowController()
                            ->GetStatusAreaWidget()
                            ->eche_tray();
  if (!eche_tray)
    return;

  eche_tray->SetIcon(decoding_data_list->front().result,
                     base::UTF8ToUTF16(visible_name));
}

void PhoneHubTray::SetEcheIconActivationCallback(
    base::RepeatingCallback<bool(const ui::Event&)> callback) {
  eche_icon_callback_ = std::move(callback);
}

void PhoneHubTray::CloseBubble() {
  if (!bubble_)
    return;

  auto* bubble_view = bubble_->GetBubbleView();
  if (bubble_view)
    bubble_view->ResetDelegate();

  if (content_view_) {
    phone_hub_metrics::LogScreenOnBubbleClose(
        content_view_->GetScreenForMetrics());

    content_view_->OnBubbleClose();
    content_view_ = nullptr;
  }

  if (phone_status_view_dont_use_) {
    phone_status_view_dont_use_ = nullptr;
  }

  if (features::IsEcheSWAEnabled() && features::IsEcheLauncherEnabled() &&
      phone_hub_manager_->GetAppStreamLauncherDataModel()) {
    phone_hub_manager_->GetAppStreamLauncherDataModel()
        ->SetShouldShowMiniLauncher(false);
  }

  bubble_.reset();
  SetIsActive(false);
  shelf()->UpdateAutoHideState();
}

void PhoneHubTray::UpdateVisibility() {
  DCHECK(ui_controller_.get());
  auto ui_state = ui_controller_->ui_state();
  SetVisiblePreferred(ui_state != PhoneHubUiController::UiState::kHidden &&
                      IsInUserSession());
  if (features::IsPhoneHubNudgeEnabled() && IsInUserSession()) {
    if (ui_state == PhoneHubUiController::UiState::kOnboardingWithoutPhone) {
      phone_hub_nudge_controller_->ShowNudge();
      // TODO (b/266853434): Animation of icon.
    }
  }
}

void PhoneHubTray::UpdateHeaderVisibility() {
  if (!features::IsEcheLauncherEnabled())
    return;
  if (!GetPhoneStatusView())
    return;

  DCHECK(ui_controller_.get());
  auto ui_state = ui_controller_->ui_state();
  GetPhoneStatusView()->SetVisible(
      ui_state != PhoneHubUiController::UiState::kMiniLauncher);
}

void PhoneHubTray::TemporarilyDisableAnimation() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, DisableShowAnimation().Release(), base::Seconds(5));
}

void PhoneHubTray::EcheIconActivated(const ui::Event& event) {
  eche_icon_callback_.Run(event);
}

void PhoneHubTray::PhoneHubIconActivated(const ui::Event& event) {
  // Simply toggle between visible/invisibvle
  if (bubble_ && bubble_->bubble_view()->GetVisible()) {
    CloseBubble();
    return;
  }
  ShowBubble();
  if (features::IsPhoneHubNudgeEnabled()) {
    phone_hub_nudge_controller_->HideNudge();
  }
}

views::View* PhoneHubTray::GetPhoneStatusView() {
  if (!bubble_ || !bubble_->GetBubbleView()) {
    phone_status_view_dont_use_ = nullptr;
  }
  return phone_status_view_dont_use_;
}

}  // namespace ash
