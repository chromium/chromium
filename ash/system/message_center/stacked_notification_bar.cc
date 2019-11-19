// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/stacked_notification_bar.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/default_color_constants.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/rounded_label_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// The label button in the stacked notification bar, can be either a "Clear all"
// or "See all notifications" button.
class StackingBarLabelButton : public views::LabelButton {
 public:
  StackingBarLabelButton(views::ButtonListener* listener,
                         const base::string16& text,
                         UnifiedMessageCenterView* message_center_view)
      : views::LabelButton(listener, text),
        message_center_view_(message_center_view) {
    SetEnabledTextColors(kUnifiedMenuButtonColorActive);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetBorder(views::CreateEmptyBorder(gfx::Insets()));
    label()->SetSubpixelRenderingEnabled(false);
    label()->SetFontList(views::Label::GetDefaultFontList().Derive(
        1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
    TrayPopupUtils::ConfigureTrayPopupButton(this);

    background_color_ = AshColorProvider::Get()->DeprecatedGetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparentWithoutBlur,
        kNotificationBackgroundColor);
  }

  ~StackingBarLabelButton() override = default;

  // views::LabelButton:
  void AboutToRequestFocusFromTabTraversal(bool reverse) override {
    if (message_center_view_->collapsed() && HasFocus())
      message_center_view_->FocusOut(reverse);
  }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(label()->GetPreferredSize().width() +
                         kStackingNotificationClearAllButtonPadding.width(),
                     label()->GetPreferredSize().height() +
                         kStackingNotificationClearAllButtonPadding.height());
  }

  const char* GetClassName() const override { return "StackingBarLabelButton"; }

  int GetHeightForWidth(int width) const override {
    return label()->GetPreferredSize().height() +
           kStackingNotificationClearAllButtonPadding.height();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    views::LabelButton::PaintButtonContents(canvas);
  }

  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    auto ink_drop = TrayPopupUtils::CreateInkDrop(this);
    ink_drop->SetShowHighlightOnFocus(true);
    ink_drop->SetShowHighlightOnHover(true);
    return ink_drop;
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return TrayPopupUtils::CreateInkDropRipple(
        TrayPopupInkDropStyle::FILL_BOUNDS, this,
        GetInkDropCenterBasedOnLastEvent(), background_color_);
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return TrayPopupUtils::CreateInkDropHighlight(
        TrayPopupInkDropStyle::FILL_BOUNDS, this, background_color_);
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    SkScalar top_radius = SkIntToScalar(kUnifiedTrayCornerRadius);
    SkRect bounds = gfx::RectToSkRect(GetContentsBounds());
    SkPath path;

    if (base::i18n::IsRTL()) {
      SkScalar radii[8] = {top_radius, top_radius, 0, 0, 0, 0, 0, 0};
      path.addRoundRect(bounds, radii);
    } else {
      SkScalar radii[8] = {0, 0, top_radius, top_radius, 0, 0, 0, 0};
      path.addRoundRect(bounds, radii);
    }

    return std::make_unique<views::PathInkDropMask>(size(), path);
  }

 private:
  SkColor background_color_ = gfx::kPlaceholderColor;
  UnifiedMessageCenterView* message_center_view_;
  DISALLOW_COPY_AND_ASSIGN(StackingBarLabelButton);
};

}  // namespace

class StackedNotificationBar::StackedNotificationBarIcon
    : public views::ImageView {
 public:
  StackedNotificationBarIcon(const std::string& id)
      : views::ImageView(), id_(id) {}
  const std::string& id() const { return id_; }

 private:
  std::string id_;
};

StackedNotificationBar::StackedNotificationBar(
    UnifiedMessageCenterView* message_center_view)
    : message_center_view_(message_center_view),
      count_label_(new views::Label),
      clear_all_button_(new StackingBarLabelButton(
          this,
          l10n_util::GetStringUTF16(
              IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_LABEL),
          message_center_view)),
      expand_all_button_(new StackingBarLabelButton(
          this,
          l10n_util::GetStringUTF16(
              IDS_ASH_MESSAGE_CENTER_EXPAND_ALL_NOTIFICATIONS_BUTTON_LABEL),
          message_center_view)) {
  SetVisible(false);
  message_center::MessageCenter::Get()->AddObserver(this);
  int left_padding = features::IsUnifiedMessageCenterRefactorEnabled()
                         ? 0
                         : kStackingNotificationClearAllButtonPadding.left();
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, left_padding, 0, 0)));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  if (features::IsUnifiedMessageCenterRefactorEnabled()) {
    notification_icons_container_ = new views::View();
    notification_icons_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            kStackedNotificationIconsContainerPadding,
            kStackedNotificationBarIconSpacing));
    AddChildView(notification_icons_container_);
  }

  count_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextSecondary,
      AshColorProvider::AshColorMode::kLight));
  count_label_->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  AddChildView(count_label_);

  views::View* spacer = new views::View;
  AddChildView(spacer);
  layout->SetFlexForView(spacer, 1);

  clear_all_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_TOOLTIP));
  AddChildView(clear_all_button_);

  expand_all_button_->SetVisible(false);
  AddChildView(expand_all_button_);
}

StackedNotificationBar::~StackedNotificationBar() {
  // The MessageCenter may be destroyed already during shutdown. See
  // crbug.com/946153.
  if (message_center::MessageCenter::Get())
    message_center::MessageCenter::Get()->RemoveObserver(this);
}

bool StackedNotificationBar::Update(
    int total_notification_count,
    std::vector<message_center::Notification*> stacked_notifications) {
  int stacked_notification_count = stacked_notifications.size();

  if (total_notification_count == total_notification_count_ &&
      stacked_notification_count == stacked_notification_count_)
    return false;

  total_notification_count_ = total_notification_count;

  UpdateVisibility();

  auto tooltip = l10n_util::GetStringFUTF16Int(
      IDS_ASH_MESSAGE_CENTER_STACKING_BAR_CLEAR_ALL_BUTTON_TOOLTIP,
      total_notification_count_);
  clear_all_button_->SetTooltipText(tooltip);
  clear_all_button_->SetAccessibleName(tooltip);

  UpdateStackedNotifications(stacked_notifications);

  return true;
}

void StackedNotificationBar::SetAnimationState(
    UnifiedMessageCenterAnimationState animation_state) {
  animation_state_ = animation_state;
  UpdateVisibility();
}

void StackedNotificationBar::SetCollapsed() {
  clear_all_button_->SetVisible(false);
  expand_all_button_->SetVisible(true);
  UpdateVisibility();
  Layout();
}

void StackedNotificationBar::SetExpanded() {
  clear_all_button_->SetVisible(true);
  expand_all_button_->SetVisible(false);
  Layout();
}

void StackedNotificationBar::AddNotificationIcon(
    message_center::Notification* notification,
    bool at_front) {
  views::ImageView* icon_view_ =
      new StackedNotificationBarIcon(notification->id());
  if (at_front)
    notification_icons_container_->AddChildViewAt(icon_view_, 0);
  else
    notification_icons_container_->AddChildView(icon_view_);

  gfx::Image masked_small_icon = notification->GenerateMaskedSmallIcon(
      kStackedNotificationIconSize,
      message_center::kNotificationDefaultAccentColor);

  if (masked_small_icon.IsEmpty()) {
    icon_view_->SetImage(gfx::CreateVectorIcon(
        message_center::kProductIcon, kStackedNotificationIconSize,
        message_center::kNotificationDefaultAccentColor));
  } else {
    icon_view_->SetImage(masked_small_icon.AsImageSkia());
  }
}

const StackedNotificationBar::StackedNotificationBarIcon*
StackedNotificationBar::GetIconFromId(const std::string& id) {
  for (auto* v : notification_icons_container_->children()) {
    const StackedNotificationBarIcon* icon =
        static_cast<const StackedNotificationBarIcon*>(v);
    if (icon->id() == id)
      return icon;
  }
  return nullptr;
}

void StackedNotificationBar::ShiftIconsLeft(
    std::vector<message_center::Notification*> stacked_notifications) {
  int stacked_notification_count = stacked_notifications.size();
  int removed_icons_count =
      std::min(stacked_notification_count_ - stacked_notification_count,
               kStackedNotificationBarMaxIcons);

  // Remove required number of icons from the front.
  for (int i = 0; i < removed_icons_count; i++) {
    delete notification_icons_container_->children().front();
  }

  // Add icons to the back if there was a backfill.
  int backfill_start = kStackedNotificationBarMaxIcons - removed_icons_count;
  int backfill_end =
      std::min(kStackedNotificationBarMaxIcons, stacked_notification_count);
  for (int i = backfill_start; i < backfill_end; i++) {
    AddNotificationIcon(stacked_notifications[i], false /*at_front*/);
  }

  stacked_notification_count_ = stacked_notification_count;
}

void StackedNotificationBar::ShiftIconsRight(
    std::vector<message_center::Notification*> stacked_notifications) {
  int new_stacked_notification_count = stacked_notifications.size();

  while (stacked_notification_count_ < new_stacked_notification_count) {
    // Remove icon from the back in case there is an overflow.
    if (stacked_notification_count_ >= kStackedNotificationBarMaxIcons) {
      delete notification_icons_container_->children().back();
    }
    // Add icon to the front.
    AddNotificationIcon(stacked_notifications[new_stacked_notification_count -
                                              stacked_notification_count_ - 1],
                        true /*at_front*/);
    ++stacked_notification_count_;
  }
}

void StackedNotificationBar::UpdateStackedNotifications(
    std::vector<message_center::Notification*> stacked_notifications) {
  int stacked_notification_count = stacked_notifications.size();
  int notification_overflow_count = 0;

  if (features::IsUnifiedMessageCenterRefactorEnabled()) {
    if (stacked_notification_count_ > stacked_notification_count)
      ShiftIconsLeft(stacked_notifications);
    else if (stacked_notification_count_ < stacked_notification_count)
      ShiftIconsRight(stacked_notifications);

    notification_overflow_count = std::max(
        stacked_notification_count_ - kStackedNotificationBarMaxIcons, 0);
  } else {
    stacked_notification_count_ = stacked_notification_count;
    notification_overflow_count = stacked_notification_count;
  }

  // Update overflow count label
  if (notification_overflow_count > 0) {
    count_label_->SetText(l10n_util::GetStringFUTF16Int(
        IDS_ASH_MESSAGE_CENTER_HIDDEN_NOTIFICATION_COUNT_LABEL,
        notification_overflow_count));
    count_label_->SetVisible(true);
  } else {
    count_label_->SetVisible(false);
  }

  Layout();
}

void StackedNotificationBar::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setColor(AshColorProvider::Get()->DeprecatedGetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparentWithoutBlur,
      kNotificationBackgroundColor));
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  SkPath background_path;
  SkScalar top_radius = SkIntToScalar(kUnifiedTrayCornerRadius);
  SkScalar radii[8] = {top_radius, top_radius, top_radius, top_radius,
                       0,          0,          0,          0};

  gfx::Rect bounds = GetLocalBounds();
  background_path.addRoundRect(gfx::RectToSkRect(bounds), radii);
  canvas->DrawPath(background_path, flags);

  // We draw a border here than use a views::Border so the ink drop highlight
  // of the clear all button overlays the border.
  if (clear_all_button_->GetVisible()) {
    canvas->DrawSharpLine(
        gfx::PointF(bounds.bottom_left() - gfx::Vector2d(0, 1)),
        gfx::PointF(bounds.bottom_right() - gfx::Vector2d(0, 1)),
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kSeparator,
            AshColorProvider::AshColorMode::kLight));
  }
}

const char* StackedNotificationBar::GetClassName() const {
  return "StackedNotificationBar";
}

void StackedNotificationBar::UpdateVisibility() {
  switch (animation_state_) {
    case UnifiedMessageCenterAnimationState::IDLE:
      SetVisible(total_notification_count_ > 1 ||
                 expand_all_button_->GetVisible());
      break;
    case UnifiedMessageCenterAnimationState::HIDE_STACKING_BAR:
      SetVisible(true);
      break;
    case UnifiedMessageCenterAnimationState::COLLAPSE:
      SetVisible(total_notification_count_ > 1 ||
                 expand_all_button_->GetVisible());
      break;
  }
}

void StackedNotificationBar::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (sender == clear_all_button_) {
    message_center_view_->ClearAllNotifications();
  } else if (sender == expand_all_button_) {
    message_center_view_->ExpandMessageCenter();
  }
}

void StackedNotificationBar::OnNotificationAdded(const std::string& id) {
  // Reset the stacked icons bar if a notification is added since we don't
  // know the position where it may have been added.
  notification_icons_container_->RemoveAllChildViews(true);
  stacked_notification_count_ = 0;
  UpdateStackedNotifications(message_center_view_->GetStackedNotifications());
}

void StackedNotificationBar::OnNotificationRemoved(const std::string& id,
                                                   bool by_user) {
  const StackedNotificationBarIcon* icon = GetIconFromId(id);
  if (icon) {
    delete icon;
    stacked_notification_count_--;
  }
}

void StackedNotificationBar::OnNotificationUpdated(const std::string& id) {}

}  // namespace ash
