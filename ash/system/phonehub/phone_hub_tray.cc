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
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
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
  return Shell::Get()->accessibility_controller()->spoken_feedback_enabled();
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
  init_params.preferred_width = kTrayMenuWidth;
  init_params.close_on_deactivate = true;
  init_params.has_shadow = false;
  init_params.translucent = true;
  init_params.corner_radius = kTrayItemCornerRadius;
  init_params.show_by_click = show_by_click;

  TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
  bubble_view->set_anchor_view_insets(GetBubbleAnchorInsets());
  bubble_view->set_margins(GetSecondaryBubbleInsets());

  // TODO(tengs): Implement the PhoneHub UI view.
  bubble_view->AddChildView(std::make_unique<views::Label>());

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

void PhoneHubTray::UpdateVisibility() {
  // TODO(tengs): Hook up visibility with phonehub::FeatureStatusProvider.
  SetVisiblePreferred(chromeos::features::IsPhoneHubEnabled());
}

}  // namespace ash
