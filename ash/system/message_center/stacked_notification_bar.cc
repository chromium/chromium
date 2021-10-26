// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/stacked_notification_bar.h"

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/element_style.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/rounded_label_button.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// The label button in the stacked notification bar, can be either a "Clear all"
// or "See all notifications" button.
class StackingBarLabelButton : public views::LabelButton {
 public:
  StackingBarLabelButton(PressedCallback callback,
                         const std::u16string& text,
                         UnifiedMessageCenterView* message_center_view)
      : views::LabelButton(std::move(callback), text),
        message_center_view_(message_center_view) {
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetBorder(views::CreateEmptyBorder(gfx::Insets()));
    label()->SetSubpixelRenderingEnabled(false);
    label()->SetFontList(views::Label::GetDefaultFontList().Derive(
        1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
    TrayPopupUtils::ConfigureTrayPopupButton(
        this, TrayPopupInkDropStyle::FILL_BOUNDS, /*highlight_on_hover=*/true,
        /*highlight_on_focus=*/true);

    if (features::IsNotificationsRefreshEnabled()) {
      // Need a textured layer here since the parent uses a solid color layer.
      SetPaintToLayer();
      layer()->SetFillsBoundsOpaquely(false);

      views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                    kTrayItemSize / 2.f);
    } else {
      SetEnabledTextColors(message_center_style::kUnifiedMenuButtonColorActive);
      // SetCreateHighlightCallback and SetCreateRippleCallback are
      // explicitly called after ConfigureTrayPopupButton as
      // ConfigureTrayPopupButton configures the InkDrop and these callbacks
      // override that behavior.
      views::InkDrop::Get(this)->SetCreateHighlightCallback(base::BindRepeating(
          [](Button* host) {
            auto highlight = std::make_unique<views::InkDropHighlight>(
                gfx::SizeF(host->size()),
                message_center_style::kInkRippleColor);
            highlight->set_visible_opacity(
                message_center_style::kInkRippleOpacity);
            return highlight;
          },
          this));
      views::InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
          [](Button* host) -> std::unique_ptr<views::InkDropRipple> {
            return std::make_unique<views::FloodFillInkDropRipple>(
                host->size(),
                views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
                message_center_style::kInkRippleColor,
                message_center_style::kInkRippleOpacity);
          },
          this));
    }
  }

  StackingBarLabelButton(const StackingBarLabelButton&) = delete;
  StackingBarLabelButton& operator=(const StackingBarLabelButton&) = delete;

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

  void OnThemeChanged() override {
    views::LabelButton::OnThemeChanged();
    if (features::IsNotificationsRefreshEnabled()) {
      views::FocusRing::Get(this)->SetColor(
          AshColorProvider::Get()->GetControlsLayerColor(
              AshColorProvider::ControlsLayerType::kFocusRingColor));
      element_style::DecorateIconlessFloatingPillButton(this);
    }
  }

 private:
  UnifiedMessageCenterView* message_center_view_;
};

}  // namespace

class StackedNotificationBar::StackedNotificationBarIcon
    : public views::ImageView,
      public ui::LayerAnimationObserver {
 public:
  StackedNotificationBarIcon(StackedNotificationBar* notification_bar,
                             const std::string& id)
      : views::ImageView(), notification_bar_(notification_bar), id_(id) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }

  ~StackedNotificationBarIcon() override {
    StopObserving();
    if (is_animating_out())
      layer()->GetAnimator()->StopAnimating();
  }

  void OnThemeChanged() override {
    views::ImageView::OnThemeChanged();

    const auto* color_provider = GetColorProvider();

    auto* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(id_);
    // The notification icon could be waiting to be cleaned up after the
    // notification removal animation completes.
    if (!notification)
      return;

    SkColor accent_color =
        color_provider->GetColor(ui::kColorNotificationHeaderForeground);
    gfx::Image masked_small_icon = notification->GenerateMaskedSmallIcon(
        kStackedNotificationIconSize, accent_color,
        color_provider->GetColor(ui::kColorNotificationIconBackground),
        color_provider->GetColor(ui::kColorNotificationIconForeground));

    if (masked_small_icon.IsEmpty()) {
      SetImage(gfx::CreateVectorIcon(message_center::kProductIcon,
                                     kStackedNotificationIconSize,
                                     accent_color));
    } else {
      SetImage(masked_small_icon.AsImageSkia());
    }
  }

  void AnimateIn() {
    DCHECK(!is_animating_out());

    std::unique_ptr<ui::InterpolatedTransform> scale =
        std::make_unique<ui::InterpolatedScale>(
            gfx::Point3F(kNotificationIconAnimationScaleFactor,
                         kNotificationIconAnimationScaleFactor, 1),
            gfx::Point3F(1, 1, 1));

    std::unique_ptr<ui::InterpolatedTransform> scale_about_pivot =
        std::make_unique<ui::InterpolatedTransformAboutPivot>(
            GetLocalBounds().CenterPoint(), std::move(scale));

    scale_about_pivot->SetChild(std::make_unique<ui::InterpolatedTranslation>(
        gfx::PointF(0, kNotificationIconAnimationLowPosition),
        gfx::PointF(0, kNotificationIconAnimationHighPosition)));

    std::unique_ptr<ui::LayerAnimationElement> scale_and_move_up =
        ui::LayerAnimationElement::CreateInterpolatedTransformElement(
            std::move(scale_about_pivot),
            base::Milliseconds(kNotificationIconAnimationUpDurationMs));
    scale_and_move_up->set_tween_type(gfx::Tween::EASE_IN);

    std::unique_ptr<ui::LayerAnimationElement> move_down =
        ui::LayerAnimationElement::CreateInterpolatedTransformElement(
            std::make_unique<ui::InterpolatedTranslation>(
                gfx::PointF(0, kNotificationIconAnimationHighPosition),
                gfx::PointF(0, 0)),
            base::Milliseconds(kNotificationIconAnimationDownDurationMs));

    std::unique_ptr<ui::LayerAnimationSequence> sequence =
        std::make_unique<ui::LayerAnimationSequence>();

    sequence->AddElement(std::move(scale_and_move_up));
    sequence->AddElement(std::move(move_down));
    layer()->GetAnimator()->StartAnimation(sequence.release());
  }

  void AnimateOut() {
    layer()->GetAnimator()->StopAnimating();

    std::unique_ptr<ui::InterpolatedTransform> scale =
        std::make_unique<ui::InterpolatedScale>(
            gfx::Point3F(1, 1, 1),
            gfx::Point3F(kNotificationIconAnimationScaleFactor,
                         kNotificationIconAnimationScaleFactor, 1));
    std::unique_ptr<ui::InterpolatedTransform> scale_about_pivot =
        std::make_unique<ui::InterpolatedTransformAboutPivot>(
            gfx::Point(bounds().width() * 0.5, bounds().height() * 0.5),
            std::move(scale));

    scale_about_pivot->SetChild(std::make_unique<ui::InterpolatedTranslation>(
        gfx::PointF(0, 0),
        gfx::PointF(0, kNotificationIconAnimationLowPosition)));

    std::unique_ptr<ui::LayerAnimationElement> scale_and_move_down =
        ui::LayerAnimationElement::CreateInterpolatedTransformElement(
            std::move(scale_about_pivot),
            base::Milliseconds(kNotificationIconAnimationOutDurationMs));
    scale_and_move_down->set_tween_type(gfx::Tween::EASE_IN);

    std::unique_ptr<ui::LayerAnimationSequence> sequence =
        std::make_unique<ui::LayerAnimationSequence>();

    sequence->AddElement(std::move(scale_and_move_down));
    sequence->AddObserver(this);
    set_animating_out(true);
    layer()->GetAnimator()->StartAnimation(sequence.release());
    // Note |this| may be deleted after this point.
  }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    set_animating_out(false);
    notification_bar_->OnIconAnimatedOut(this);
    // Note |this| is deleted after this point.
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}

  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  const std::string& id() const { return id_; }
  bool is_animating_out() const { return animating_out_; }
  void set_animating_out(bool animating_out) { animating_out_ = animating_out; }

 private:
  StackedNotificationBar* notification_bar_;
  std::string id_;
  bool animating_out_ = false;
};

StackedNotificationBar::StackedNotificationBar(
    UnifiedMessageCenterView* message_center_view)
    : message_center_view_(message_center_view),
      notification_icons_container_(
          AddChildView(std::make_unique<views::View>())),
      count_label_(AddChildView(std::make_unique<views::Label>())),
      spacer_(AddChildView(std::make_unique<views::View>())),
      clear_all_button_(AddChildView(std::make_unique<StackingBarLabelButton>(
          base::BindRepeating(&UnifiedMessageCenterView::ClearAllNotifications,
                              base::Unretained(message_center_view_)),
          l10n_util::GetStringUTF16(
              IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_LABEL),
          message_center_view))),
      expand_all_button_(AddChildView(std::make_unique<StackingBarLabelButton>(
          base::BindRepeating(&UnifiedMessageCenterView::ExpandMessageCenter,
                              base::Unretained(message_center_view_)),
          l10n_util::GetStringUTF16(
              IDS_ASH_MESSAGE_CENTER_EXPAND_ALL_NOTIFICATIONS_BUTTON_LABEL),
          message_center_view))) {
  SetVisible(false);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      features::IsNotificationsRefreshEnabled()
          ? gfx::Insets(kNotificationBarVerticalPadding,
                        kNotificationBarHorizontalPadding)
          : gfx::Insets()));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  notification_icons_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kStackedNotificationIconsContainerPadding,
          kStackedNotificationBarIconSpacing));
  message_center::MessageCenter::Get()->AddObserver(this);

  count_label_->SetEnabledColor(message_center_style::kCountLabelColor);
  count_label_->SetFontList(views::Label::GetDefaultFontList().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));

  layout->SetFlexForView(spacer_, 1);

  clear_all_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_TOOLTIP));

  expand_all_button_->SetVisible(false);

  if (!features::IsNotificationsRefreshEnabled())
    SetPaintToLayer();
}

StackedNotificationBar::~StackedNotificationBar() {
  // The MessageCenter may be destroyed already during shutdown. See
  // crbug.com/946153.
  if (message_center::MessageCenter::Get())
    message_center::MessageCenter::Get()->RemoveObserver(this);
}

bool StackedNotificationBar::Update(
    int total_notification_count,
    int pinned_notification_count,
    std::vector<message_center::Notification*> stacked_notifications) {
  int stacked_notification_count = stacked_notifications.size();

  if (total_notification_count == total_notification_count_ &&
      pinned_notification_count == pinned_notification_count_ &&
      stacked_notification_count == stacked_notification_count_)
    return false;

  total_notification_count_ = total_notification_count;
  pinned_notification_count_ = pinned_notification_count;

  UpdateStackedNotifications(stacked_notifications);
  UpdateVisibility();

  int unpinned_count = total_notification_count_ - pinned_notification_count_;

  auto tooltip = l10n_util::GetStringFUTF16Int(
      IDS_ASH_MESSAGE_CENTER_STACKING_BAR_CLEAR_ALL_BUTTON_TOOLTIP,
      unpinned_count);
  clear_all_button_->SetTooltipText(tooltip);
  clear_all_button_->SetAccessibleName(tooltip);

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
}

void StackedNotificationBar::SetExpanded() {
  clear_all_button_->SetVisible(true);
  expand_all_button_->SetVisible(false);
}

void StackedNotificationBar::AddNotificationIcon(
    message_center::Notification* notification,
    bool at_front) {
  views::ImageView* icon_view =
      new StackedNotificationBarIcon(this, notification->id());
  if (at_front)
    notification_icons_container_->AddChildViewAt(icon_view, 0);
  else
    notification_icons_container_->AddChildView(icon_view);
}

void StackedNotificationBar::OnIconAnimatedOut(views::View* icon) {
  delete icon;
  Layout();
}

StackedNotificationBar::StackedNotificationBarIcon*
StackedNotificationBar::GetFrontIcon() {
  const auto i = std::find_if(
      notification_icons_container_->children().cbegin(),
      notification_icons_container_->children().cend(), [](const auto* v) {
        return !static_cast<const StackedNotificationBarIcon*>(v)
                    ->is_animating_out();
      });

  return (i == notification_icons_container_->children().cend()
              ? nullptr
              : static_cast<StackedNotificationBarIcon*>(*i));
}

const StackedNotificationBar::StackedNotificationBarIcon*
StackedNotificationBar::GetIconFromId(const std::string& id) const {
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
  // Only animate if we're removing one icon.
  if (removed_icons_count == 1) {
    StackedNotificationBarIcon* icon = GetFrontIcon();
    if (icon) {
      icon->AnimateOut();
    }
  } else {
    for (int i = 0; i < removed_icons_count; i++) {
      StackedNotificationBarIcon* icon = GetFrontIcon();
      if (icon) {
        delete icon;
      }
    }
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
  // Animate in the first stacked notification icon.
  StackedNotificationBarIcon* icon = GetFrontIcon();
  if (icon)
    GetFrontIcon()->AnimateIn();
}

void StackedNotificationBar::UpdateStackedNotifications(
    std::vector<message_center::Notification*> stacked_notifications) {
  int stacked_notification_count = stacked_notifications.size();
  int notification_overflow_count = 0;

  if (stacked_notification_count_ > stacked_notification_count)
    ShiftIconsLeft(stacked_notifications);
  else if (stacked_notification_count_ < stacked_notification_count)
    ShiftIconsRight(stacked_notifications);

  notification_overflow_count = std::max(
      stacked_notification_count_ - kStackedNotificationBarMaxIcons, 0);

  // Update overflow count label
  if (notification_overflow_count > 0) {
    count_label_->SetText(l10n_util::GetStringFUTF16Int(
        IDS_ASH_MESSAGE_CENTER_HIDDEN_NOTIFICATION_COUNT_LABEL,
        notification_overflow_count));
    count_label_->SetVisible(true);
  } else {
    count_label_->SetVisible(false);
  }
}

void StackedNotificationBar::OnPaint(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  if (!features::IsNotificationsRefreshEnabled())
    flags.setColor(message_center_style::kNotificationBackgroundColor);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);

  gfx::Rect bounds = GetLocalBounds();
  canvas->DrawRect(bounds, flags);

  // We draw a border here than use a views::Border so the ink drop highlight
  // of the clear all button overlays the border.
  if (clear_all_button_->GetVisible()) {
    canvas->DrawSharpLine(
        gfx::PointF(bounds.bottom_left() - gfx::Vector2d(0, 1)),
        gfx::PointF(bounds.bottom_right() - gfx::Vector2d(0, 1)),
        message_center_style::kSeperatorColor);
  }
}

const char* StackedNotificationBar::GetClassName() const {
  return "StackedNotificationBar";
}

void StackedNotificationBar::UpdateVisibility() {
  // In the refreshed message center view the notification bar is always
  // visible.
  if (features::IsNotificationsRefreshEnabled()) {
    if (!GetVisible())
      SetVisible(true);
    return;
  }

  int unpinned_count = total_notification_count_ - pinned_notification_count_;

  // In expanded state, clear all button should be visible when (rule is subject
  // to change):
  //     1. There are more than one notification.
  //     2. There is at least one unpinned notification
  bool show_clear_all = total_notification_count_ > 1 && unpinned_count >= 1;
  if (!expand_all_button_->GetVisible())
    clear_all_button_->SetVisible(show_clear_all);

  switch (animation_state_) {
    case UnifiedMessageCenterAnimationState::IDLE:
      SetVisible(stacked_notification_count_ || show_clear_all ||
                 expand_all_button_->GetVisible());
      break;
    case UnifiedMessageCenterAnimationState::HIDE_STACKING_BAR:
      SetVisible(true);
      break;
    case UnifiedMessageCenterAnimationState::COLLAPSE:
      SetVisible(stacked_notification_count_ || show_clear_all ||
                 expand_all_button_->GetVisible());
      break;
  }
}

void StackedNotificationBar::OnNotificationAdded(const std::string& id) {
  // Reset the stacked icons bar if a notification is added since we don't
  // know the position where it may have been added.
  notification_icons_container_->RemoveAllChildViews();
  stacked_notification_count_ = 0;
  UpdateStackedNotifications(message_center_view_->GetStackedNotifications());
}

void StackedNotificationBar::OnNotificationRemoved(const std::string& id,
                                                   bool by_user) {
  const StackedNotificationBarIcon* icon = GetIconFromId(id);
  if (icon && !icon->is_animating_out()) {
    delete icon;
    stacked_notification_count_--;
  }
}

void StackedNotificationBar::OnNotificationUpdated(const std::string& id) {}

}  // namespace ash
