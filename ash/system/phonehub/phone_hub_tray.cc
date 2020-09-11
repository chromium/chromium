// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_tray.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_status_view.h"
#include "ash/system/phonehub/quick_actions_view.h"
#include "ash/system/phonehub/task_continuation_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"

namespace ash {

namespace {

// Padding for tray icon (dp; the button that shows the phone_hub menu).
constexpr int kTrayIconMainAxisInset = 8;
constexpr int kTrayIconCrossAxisInset = 0;

constexpr gfx::Insets kBubblePadding(4, 16);
constexpr int kBubbleWidth = 400;
constexpr int kPaddingBetweenTitleAndSeparator = 3;

// A view of the Phone Hub panel, displaying phone status and utility actions
// such as phone status, task continuation, etc.
class PhoneHubView : public views ::View {
 public:
  explicit PhoneHubView(TrayBubbleView* bubble_view)
      : bubble_view_(bubble_view) {
    auto setup_layered_view = [](views::View* view) {
      view->SetPaintToLayer();
      view->layer()->SetFillsBoundsOpaquely(false);
    };

    setup_layered_view(
        bubble_view_->AddChildView(std::make_unique<PhoneStatusView>()));

    AddSeparator();

    setup_layered_view(
        bubble_view_->AddChildView(std::make_unique<QuickActionsView>()));

    AddSeparator();

    setup_layered_view(
        bubble_view_->AddChildView(std::make_unique<TaskContinuationView>()));
  }
  ~PhoneHubView() override = default;

  // views::View:
  const char* GetClassName() const override { return "PhoneHubView"; }

 private:
  void AddSeparator() {
    auto* separator =
        bubble_view_->AddChildView(std::make_unique<views::Separator>());
    separator->SetPaintToLayer();
    separator->layer()->SetFillsBoundsOpaquely(false);
    separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kSeparatorColor));
    separator->SetBorder(views::CreateEmptyBorder(
        gfx::Insets(kPaddingBetweenTitleAndSeparator, 0,
                    kMenuSeparatorVerticalPadding, 0)));
  }

  TrayBubbleView* bubble_view_ = nullptr;
};

}  // namespace

PhoneHubTray::PhoneHubTray(Shelf* shelf) : TrayBackgroundView(shelf) {
  // TODO(tengs): Update icon to spec.
  auto icon = std::make_unique<views::ImageView>();
  icon->set_tooltip_text(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME));
  icon->SetImage(CreateVectorIcon(
      kSystemMenuPhoneIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));

  tray_container()->SetMargin(kTrayIconMainAxisInset, kTrayIconCrossAxisInset);
  icon_ = tray_container()->AddChildView(std::move(icon));
}

PhoneHubTray::~PhoneHubTray() {
  if (bubble_)
    bubble_->bubble_view()->ResetDelegate();
  CleanUpPhoneHubManager();
}

void PhoneHubTray::SetPhoneHubManager(
    chromeos::phonehub::PhoneHubManager* phone_hub_manager) {
  if (phone_hub_manager == phone_hub_manager_)
    return;

  CleanUpPhoneHubManager();

  phone_hub_manager_ = phone_hub_manager;
  if (phone_hub_manager_)
    phone_hub_manager_->GetFeatureStatusProvider()->AddObserver(this);

  OnFeatureStatusChanged();
}

void PhoneHubTray::ClickedOutsideBubble() {
  CloseBubble();
}

base::string16 PhoneHubTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME);
}

void PhoneHubTray::HandleLocaleChange() {
  icon_->set_tooltip_text(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME));
}

void PhoneHubTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (bubble_->bubble_view() == bubble_view)
    CloseBubble();
}

base::string16 PhoneHubTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool PhoneHubTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

void PhoneHubTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void PhoneHubTray::AnchorUpdated() {
  if (bubble_)
    bubble_->bubble_view()->UpdateBubble();
}

void PhoneHubTray::Initialize() {
  TrayBackgroundView::Initialize();
  UpdateVisibility();
}

bool PhoneHubTray::PerformAction(const ui::Event& event) {
  // TODO(tengs): Log usage metrics.

  if (bubble_)
    CloseBubble();
  else
    ShowBubble(event.IsMouseEvent() || event.IsGestureEvent());
  return true;
}

void PhoneHubTray::ShowBubble(bool show_by_click) {
  if (bubble_)
    return;

  TrayBubbleView::InitParams init_params;
  init_params.delegate = this;
  init_params.parent_window = GetBubbleWindowContainer();
  init_params.anchor_view = GetBubbleAnchor();
  init_params.shelf_alignment = shelf()->alignment();
  init_params.preferred_width = kBubbleWidth;
  init_params.close_on_deactivate = true;
  init_params.has_shadow = false;
  init_params.translucent = true;
  init_params.corner_radius = kTrayItemCornerRadius;
  init_params.show_by_click = show_by_click;

  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
  bubble_view->set_anchor_view_insets(GetBubbleAnchorInsets());
  bubble_view->set_margins(GetSecondaryBubbleInsets());
  bubble_view->SetBorder(views::CreateEmptyBorder(kBubblePadding));

  bubble_view->AddChildView(std::make_unique<PhoneHubView>(bubble_view));

  bubble_ = std::make_unique<TrayBubbleWrapper>(this, bubble_view,
                                                false /* is_persistent */);
  SetIsActive(true);
}

TrayBubbleView* PhoneHubTray::GetBubbleView() {
  return bubble_ ? bubble_->bubble_view() : nullptr;
}

const char* PhoneHubTray::GetClassName() const {
  return "PhoneHubTray";
}

void PhoneHubTray::CloseBubble() {
  bubble_.reset();
  SetIsActive(false);
  shelf()->UpdateAutoHideState();
}

void PhoneHubTray::OnFeatureStatusChanged() {
  UpdateVisibility();
}

void PhoneHubTray::UpdateVisibility() {
  if (!phone_hub_manager_) {
    SetVisiblePreferred(false);
    return;
  }

  auto feature_status =
      phone_hub_manager_->GetFeatureStatusProvider()->GetStatus();
  bool is_visible;
  switch (feature_status) {
    case chromeos::phonehub::FeatureStatus::kNotEligibleForFeature:
      FALLTHROUGH;
    case chromeos::phonehub::FeatureStatus::kDisabled:
      is_visible = false;
      break;
    default:
      is_visible = true;
      break;
  }

  SetVisiblePreferred(is_visible);
}

void PhoneHubTray::CleanUpPhoneHubManager() {
  if (!phone_hub_manager_)
    return;

  phone_hub_manager_->GetFeatureStatusProvider()->RemoveObserver(this);
}

}  // namespace ash
