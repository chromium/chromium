// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/stacked_notification_bar.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/vector_icons.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// The label button in the stacked notification bar, used for the "Clear All"
// button.
class StackingBarLabelButton : public PillButton {
  METADATA_HEADER(StackingBarLabelButton, PillButton)

 public:
  StackingBarLabelButton(PressedCallback callback,
                         const std::u16string& text,
                         NotificationCenterView* notification_center_view)
      : PillButton(std::move(callback),
                   text,
                   PillButton::Type::kFloatingWithoutIcon,
                   /*icon=*/nullptr,
                   kNotificationPillButtonHorizontalSpacing) {
    SetEnabled(false);
    StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                     /*highlight_on_hover=*/true,
                                     /*highlight_on_focus=*/true);
  }

  StackingBarLabelButton(const StackingBarLabelButton&) = delete;
  StackingBarLabelButton& operator=(const StackingBarLabelButton&) = delete;

  ~StackingBarLabelButton() override = default;
};

BEGIN_METADATA(StackingBarLabelButton)
END_METADATA

}  // namespace

class StackedNotificationBar::StackedNotificationBarIcon
    : public views::ImageView,
      public ui::LayerAnimationObserver {
  METADATA_HEADER(StackedNotificationBarIcon, views::ImageView)

 public:
  explicit StackedNotificationBarIcon(const std::string& id) : id_(id) {
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

    auto* notification =
        message_center::MessageCenter::Get()->FindVisibleNotificationById(id_);
    // The notification icon could be waiting to be cleaned up after the
    // notification removal animation completes.
    if (!notification)
      return;

    SkColor accent_color =
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface);
    gfx::Image masked_small_icon = notification->GenerateMaskedSmallIcon(
        kStackedNotificationIconSize, accent_color, SK_ColorTRANSPARENT,
        accent_color);

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

  using AnimationCompleteCallback = base::OnceCallback<void(views::View*)>;

  void AnimateOut(AnimationCompleteCallback animation_complete_callback) {
    DCHECK(animation_complete_callback_.is_null());

    animation_complete_callback_ = std::move(animation_complete_callback);

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
    std::move(animation_complete_callback_).Run(this);
    // Note |this| is deleted after this point.
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}

  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  const std::string& id() const { return id_; }
  bool is_animating_out() const { return animating_out_; }
  void set_animating_out(bool animating_out) { animating_out_ = animating_out; }

 private:
  std::string id_;
  bool animating_out_ = false;

  // Used to notify the parent of animation completion. This is deleted after
  // the callback is run.
  // Registered in `AnimateOut()`.
  AnimationCompleteCallback animation_complete_callback_;
};

BEGIN_METADATA(StackedNotificationBar, StackedNotificationBarIcon)
END_METADATA

StackedNotificationBar::StackedNotificationBar(
    NotificationCenterView* notification_center_view)
    : notification_center_view_(notification_center_view),
      notification_icons_container_(
          AddChildView(std::make_unique<views::View>())),
      count_label_(AddChildView(std::make_unique<views::Label>())),
      spacer_(AddChildView(std::make_unique<views::View>())),
      clear_all_button_(AddChildView(std::make_unique<StackingBarLabelButton>(
          base::BindRepeating(&NotificationCenterView::ClearAllNotifications,
                              base::Unretained(notification_center_view_)),
          l10n_util::GetStringUTF16(
              IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_LABEL),
          notification_center_view))),
      layout_manager_(SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kNotificationBarPadding))) {
  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  notification_icons_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          kStackedNotificationIconsContainerPadding,
          kStackedNotificationBarIconSpacing));

  message_center::MessageCenter::Get()->AddObserver(this);

  count_label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                        *count_label_);

  layout_manager_->SetFlexForView(spacer_, 1);

  clear_all_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_TOOLTIP));
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
    std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
        stacked_notifications) {
  int stacked_notification_count = stacked_notifications.size();
  if (total_notification_count == total_notification_count_ &&
      pinned_notification_count == pinned_notification_count_ &&
      stacked_notification_count == stacked_notification_count_) {
    return false;
  }

  total_notification_count_ = total_notification_count;
  pinned_notification_count_ = pinned_notification_count;

  UpdateStackedNotifications(stacked_notifications);

  const int unpinned_count =
      total_notification_count_ - pinned_notification_count_;

  auto tooltip = l10n_util::GetStringFUTF16Int(
      IDS_ASH_MESSAGE_CENTER_STACKING_BAR_CLEAR_ALL_BUTTON_TOOLTIP,
      unpinned_count);
  clear_all_button_->SetTooltipText(tooltip);
  clear_all_button_->GetViewAccessibility().SetName(tooltip);
  clear_all_button_->SetEnabled(unpinned_count > 0);

  return true;
}

void StackedNotificationBar::AddNotificationIcon(
    message_center::Notification* notification,
    bool at_front) {
  if (at_front)
    notification_icons_container_->AddChildViewAt(
        std::make_unique<StackedNotificationBarIcon>(notification->id()), 0);
  else
    notification_icons_container_->AddChildView(
        std::make_unique<StackedNotificationBarIcon>(notification->id()));
}

void StackedNotificationBar::OnIconAnimatedOut(std::string notification_id,
                                               views::View* icon) {
  delete icon;

  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);
  // This is only called when icons animate out, so never add icons to the
  // front.
  if (notification)
    AddNotificationIcon(notification, /*at_front=*/false);

  DeprecatedLayoutImmediately();
}

StackedNotificationBar::StackedNotificationBarIcon*
StackedNotificationBar::GetFrontIcon(bool animating_out) {
  const auto i = base::ranges::find(
      notification_icons_container_->children(), animating_out,
      [](const views::View* v) {
        return static_cast<const StackedNotificationBarIcon*>(v)
            ->is_animating_out();
      });

  return (i == notification_icons_container_->children().cend()
              ? nullptr
              : static_cast<StackedNotificationBarIcon*>(*i));
}

const StackedNotificationBar::StackedNotificationBarIcon*
StackedNotificationBar::GetIconFromId(const std::string& id) const {
  for (views::View* v : notification_icons_container_->children()) {
    const StackedNotificationBarIcon* icon =
        static_cast<const StackedNotificationBarIcon*>(v);
    if (icon->id() == id)
      return icon;
  }
  return nullptr;
}

void StackedNotificationBar::ShiftIconsLeft(
    std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
        stacked_notifications) {
  auto* front_animating_out_icon = GetFrontIcon(/*animating_out=*/true);
  bool is_already_animating_a_left_shift = front_animating_out_icon != nullptr;
  // If we need to animate a second icon, the scroll is faster than the icon can
  // animate out (this is possible with a very fast scroll), so immediately
  // finish that animation before starting a new one.
  if (is_already_animating_a_left_shift) {
    front_animating_out_icon->layer()->GetAnimator()->StopAnimating();
    // `front_animating_out_icon` is now deleted, and StackedNotificationBar has
    // been reloaded with another icon in the back.
  }

  int stacked_notification_count = stacked_notifications.size();
  int removed_icons_count =
      std::min(stacked_notification_count_ - stacked_notification_count,
               kStackedNotificationBarMaxIcons);

  stacked_notification_count_ = stacked_notification_count;

  // Remove required number of icons from the front.
  // Only animate if we're removing one icon.
  int backfill_start = kStackedNotificationBarMaxIcons - removed_icons_count;
  int backfill_end =
      std::min(kStackedNotificationBarMaxIcons, stacked_notification_count);
  const bool will_animate = removed_icons_count == 1;
  if (will_animate) {
    auto* icon = GetFrontIcon(/*animating_out=*/false);
    if (icon) {
      // If there are notifications to backfill, do not add the
      // icon until the animation completes, this avoids a jumping overflow
      // label/icons and having more than 3 icons in the stack.
      message_center::Notification* next_notification =
          backfill_start < backfill_end ? stacked_notifications[backfill_start]
                                        : nullptr;
      icon->AnimateOut(base::BindOnce(
          &StackedNotificationBar::OnIconAnimatedOut,
          weak_ptr_factory_.GetWeakPtr(),
          next_notification ? next_notification->id() : std::string()));
    }
    return;
  }

  // No animation.
  for (int i = 0; i < removed_icons_count; i++) {
    auto* icon = GetFrontIcon(/*animating_out=*/false);
    if (icon) {
      delete icon;
    }
  }

  for (int i = backfill_start; i < backfill_end; i++)
    AddNotificationIcon(stacked_notifications[i], false /*at_front*/);
}

void StackedNotificationBar::ShiftIconsRight(
    std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
        stacked_notifications) {
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
  auto* icon = GetFrontIcon(/*animating_out=*/false);
  if (icon)
    icon->AnimateIn();
}

void StackedNotificationBar::UpdateStackedNotifications(
    std::vector<raw_ptr<message_center::Notification, VectorExperimental>>
        stacked_notifications) {
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

void StackedNotificationBar::OnNotificationAdded(const std::string& id) {
  // Reset the stacked icons bar if a notification is added since we don't
  // know the position where it may have been added.
  notification_icons_container_->RemoveAllChildViews();
  stacked_notification_count_ = 0;
  UpdateStackedNotifications(
      notification_center_view_->GetStackedNotifications());
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

BEGIN_METADATA(StackedNotificationBar)
END_METADATA

}  // namespace ash
