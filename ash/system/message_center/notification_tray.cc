// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/notification_tray.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/ash_popup_alignment_delegate.h"
#include "ash/system/message_center/message_center_bubble.h"
#include "ash/system/message_center/message_center_ui_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_utils.h"
#include "base/auto_reset.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

constexpr int kMaximumSmallIconCount = 3;

constexpr int kTrayItemInnerIconSize = 16;

constexpr gfx::Size kTrayItemOuterSize(26, 26);
constexpr int kTrayMainAxisInset = 3;
constexpr int kTrayCrossAxisInset = 0;

constexpr int kTrayItemAnimationDurationMS = 200;

constexpr size_t kMaximumNotificationNumber = 99;

// in px. See https://crbug.com/754307.
constexpr size_t kPaddingFromScreenTop = 8;

constexpr float kBackgroundBlurRadius = 30.f;

// Flag to disable animation. Only for testing.
bool disable_animations_for_test = false;

}  // namespace

// Class to initialize and manage the NotificationBubble and TrayBubbleWrapper
// instances for a bubble.
class NotificationBubbleWrapper {
 public:
  // Takes ownership of |bubble| and creates |bubble_wrapper_|.
  NotificationBubbleWrapper(NotificationTray* tray,
                            TrayBackgroundView* anchor_tray,
                            MessageCenterBubble* bubble,
                            bool show_by_click) {
    bubble_.reset(bubble);
    TrayBubbleView::InitParams init_params;
    init_params.delegate = tray;
    init_params.parent_window = anchor_tray->GetBubbleWindowContainer();
    init_params.anchor_view = anchor_tray->GetBubbleAnchor();
    init_params.anchor_alignment = tray->GetAnchorAlignment();
    const int width = message_center::kNotificationWidth +
                      message_center::kNotificationBorderThickness * 2 +
                      message_center::kMarginBetweenItemsInList * 2;
    init_params.min_width = width;
    init_params.max_width = width;
    init_params.max_height = bubble->max_height();
    init_params.show_by_click = show_by_click;

    TrayBubbleView* bubble_view = new TrayBubbleView(init_params);
    bubble_view->set_color(SK_ColorTRANSPARENT);
    bubble_view->layer()->SetFillsBoundsOpaquely(false);
    bubble_view->set_anchor_view_insets(anchor_tray->GetBubbleAnchorInsets());
    bubble_wrapper_ = std::make_unique<TrayBubbleWrapper>(
        tray, bubble_view, false /* is_persistent */);
    bubble->InitializeContents(bubble_view);

    if (app_list_features::IsBackgroundBlurEnabled()) {
      // ClientView's layer (See TrayBubbleView::InitializeAndShowBubble())
      bubble_view->layer()->parent()->SetBackgroundBlur(kBackgroundBlurRadius);
    }
  }

  MessageCenterBubble* bubble() const { return bubble_.get(); }

  // Convenience accessors.
  TrayBubbleView* bubble_view() const { return bubble_->bubble_view(); }

 private:
  std::unique_ptr<MessageCenterBubble> bubble_;
  std::unique_ptr<TrayBubbleWrapper> bubble_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(NotificationBubbleWrapper);
};

class NotificationTraySubview : public views::View,
                                public gfx::AnimationDelegate {
 public:
  NotificationTraySubview(gfx::AnimationContainer* container,
                          NotificationTray* tray)
      : tray_(tray) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    views::View::SetVisible(false);
    set_owned_by_client();

    SetLayoutManager(std::make_unique<views::FillLayout>());

    animation_ = std::make_unique<gfx::SlideAnimation>(this);
    animation_->SetContainer(container);
    animation_->SetSlideDuration(kTrayItemAnimationDurationMS);
    animation_->SetTweenType(gfx::Tween::LINEAR);
  }

  void SetVisible(bool set_visible) override {
    if (!GetWidget() || disable_animations_for_test) {
      views::View::SetVisible(set_visible);
      return;
    }

    if (!set_visible) {
      animation_->Hide();
      AnimationProgressed(animation_.get());
    } else {
      animation_->Show();
      AnimationProgressed(animation_.get());
      views::View::SetVisible(true);
    }
  }

  void HideAndDelete() {
    SetVisible(false);

    if (!visible() && !animation_->is_animating()) {
      if (parent())
        parent()->RemoveChildView(this);
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    } else {
      delete_after_animation_ = true;
    }
  }

 protected:
  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override {
    if (!animation_.get() || !animation_->is_animating())
      return kTrayItemOuterSize;

    // Animate the width (or height) when this item shows (or hides) so that
    // the icons on the left are shifted with the animation.
    // Note that TrayItemView does the same thing.
    gfx::Size size = kTrayItemOuterSize;
    if (tray_->shelf()->IsHorizontalAlignment()) {
      size.set_width(std::max(
          1, gfx::ToRoundedInt(size.width() * animation_->GetCurrentValue())));
    } else {
      size.set_height(std::max(
          1, gfx::ToRoundedInt(size.height() * animation_->GetCurrentValue())));
    }
    return size;
  }

  int GetHeightForWidth(int width) const override {
    return GetPreferredSize().height();
  }

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    // After HideAndDelete() has been called and |this| is responsible for
    // deleting itself, but |tray_| has been deleted before the animation
    // reached its end, |this| will be parentless. It's ok to stop the animation
    // (deleting |this|). See https://crbug.com/841768
    if (!parent()) {
      DCHECK(delete_after_animation_);
      animation_->Stop();
      return;
    }

    gfx::Transform transform;
    if (tray_->shelf()->IsHorizontalAlignment()) {
      transform.Translate(0, animation->CurrentValueBetween(
                                 static_cast<double>(height()) / 2., 0.));
    } else {
      transform.Translate(
          animation->CurrentValueBetween(static_cast<double>(width() / 2.), 0.),
          0);
    }
    transform.Scale(animation->GetCurrentValue(), animation->GetCurrentValue());
    layer()->SetTransform(transform);
    PreferredSizeChanged();
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    if (animation->GetCurrentValue() < 0.1)
      views::View::SetVisible(false);

    if (delete_after_animation_) {
      if (parent())
        parent()->RemoveChildView(this);
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    }
  }
  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  std::unique_ptr<gfx::SlideAnimation> animation_;
  bool delete_after_animation_ = false;
  NotificationTray* tray_;

  DISALLOW_COPY_AND_ASSIGN(NotificationTraySubview);
};

class NotificationTrayImageSubview : public NotificationTraySubview {
 public:
  NotificationTrayImageSubview(const gfx::ImageSkia& image,
                               gfx::AnimationContainer* container,
                               NotificationTray* tray)
      : NotificationTraySubview(container, tray) {
    DCHECK(image.size() ==
           gfx::Size(kTrayItemInnerIconSize, kTrayItemInnerIconSize));
    view_ = new views::ImageView();
    view_->SetImage(image);
    view_->set_tooltip_text(
        l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_FOOTER_TITLE));
    AddChildView(view_);
  }

 private:
  views::ImageView* view_;

  DISALLOW_COPY_AND_ASSIGN(NotificationTrayImageSubview);
};

class NotificationTrayLabelSubview : public NotificationTraySubview {
 public:
  NotificationTrayLabelSubview(gfx::AnimationContainer* container,
                               NotificationTray* tray)
      : NotificationTraySubview(container, tray) {
    view_ = new views::Label();
    SetupLabelForTray(view_);
    AddChildView(view_);
  }

  void SetNotificationCount(bool small_icons_exist, size_t notification_count) {
    notification_count = std::min(notification_count,
                                  kMaximumNotificationNumber);  // cap with 99

    // TODO(yoshiki): Use a string for "99" and "+99".

    base::string16 str = base::FormatNumber(notification_count);
    if (small_icons_exist) {
      str = base::ASCIIToUTF16("+") + str;
      if (base::i18n::IsRTL())
        base::i18n::WrapStringWithRTLFormatting(&str);
    }

    view_->SetText(str);
    SchedulePaint();
  }

 private:
  views::Label* view_;

  DISALLOW_COPY_AND_ASSIGN(NotificationTrayLabelSubview);
};

NotificationTray::NotificationTray(Shelf* shelf,
                                   aura::Window* status_area_window)
    : TrayBackgroundView(shelf),
      status_area_window_(status_area_window),
      show_message_center_on_unlock_(false),
      should_update_tray_content_(false) {
  DCHECK(shelf);
  DCHECK(status_area_window_);

  SetInkDropMode(InkDropMode::ON);
  gfx::ImageSkia bell_image =
      CreateVectorIcon(kShelfNotificationsIcon, kShelfIconColor);
  bell_icon_ = std::make_unique<NotificationTrayImageSubview>(
      bell_image, animation_container_.get(), this);
  tray_container()->AddChildView(bell_icon_.get());

  gfx::ImageSkia quiet_mode_image =
      CreateVectorIcon(kNotificationCenterDoNotDisturbOnIcon,
                       kTrayItemInnerIconSize, kShelfIconColor);
  quiet_mode_icon_ = std::make_unique<NotificationTrayImageSubview>(
      quiet_mode_image, animation_container_.get(), this);
  tray_container()->AddChildView(quiet_mode_icon_.get());

  counter_ = std::make_unique<NotificationTrayLabelSubview>(
      animation_container_.get(), this);
  tray_container()->AddChildView(counter_.get());

  message_center_ui_controller_ =
      std::make_unique<MessageCenterUiController>(this);
  popup_alignment_delegate_ =
      std::make_unique<AshPopupAlignmentDelegate>(shelf);
  popup_collection_ = std::make_unique<message_center::MessagePopupCollection>(
      popup_alignment_delegate_.get());
  display::Screen* screen = display::Screen::GetScreen();
  popup_alignment_delegate_->StartObserving(
      screen, screen->GetDisplayNearestWindow(status_area_window_));
  OnMessageCenterContentsChanged();

  tray_container()->SetMargin(kTrayMainAxisInset, kTrayCrossAxisInset);
}

NotificationTray::~NotificationTray() {
  // Release any child views that might have back pointers before ~View().
  message_center_bubble_.reset();
  popup_alignment_delegate_.reset();
  popup_collection_.reset();
}

// static
void NotificationTray::DisableAnimationsForTest(bool disable) {
  disable_animations_for_test = disable;
}

// Public methods.

bool NotificationTray::ShowMessageCenter(bool show_by_click) {
  if (!ShouldShowMessageCenter())
    return false;

  if (IsMessageCenterVisible())
    return true;

  MessageCenterBubble* message_center_bubble =
      new MessageCenterBubble(message_center());

  // In the horizontal case, message center starts from the top of the shelf.
  // In the vertical case, it starts from the bottom of NotificationTray.
  const int max_height = (shelf()->IsHorizontalAlignment()
                              ? shelf()->GetUserWorkAreaBounds().height()
                              : GetBoundsInScreen().bottom() -
                                    shelf()->GetUserWorkAreaBounds().y());
  // Sets the maximum height, considering the padding from the top edge of
  // screen. This padding should be applied in all types of shelf alignment.
  message_center_bubble->SetMaxHeight(max_height - kPaddingFromScreenTop);

  // For vertical shelf alignments, anchor to the NotificationTray, but for
  // horizontal (i.e. bottom) shelves, anchor to the system tray.
  TrayBackgroundView* anchor_tray = this;
  if (shelf()->IsHorizontalAlignment())
    anchor_tray = shelf()->GetSystemTrayAnchorView();

  message_center_bubble_ = std::make_unique<NotificationBubbleWrapper>(
      this, anchor_tray, message_center_bubble, show_by_click);

  shelf()->UpdateAutoHideState();
  SetIsActive(true);
  return true;
}

void NotificationTray::HideMessageCenter() {
  if (!message_center_bubble())
    return;

  SetIsActive(false);
  message_center_bubble_.reset();
  show_message_center_on_unlock_ = false;
  shelf()->UpdateAutoHideState();
}

void NotificationTray::SetTrayBubbleHeight(int height) {
  popup_alignment_delegate_->SetTrayBubbleHeight(height);
}

int NotificationTray::tray_bubble_height_for_test() const {
  return popup_alignment_delegate_->tray_bubble_height_for_test();
}

bool NotificationTray::ShowPopups() {
  if (IsMessageCenterVisible())
    return false;
  return true;
}

void NotificationTray::HidePopups() {
  DCHECK(popup_collection_.get());
}

// Private methods.

bool NotificationTray::ShouldShowMessageCenter() const {
  // Hidden at login screen, during supervised user creation, etc.
  return Shell::Get()->session_controller()->ShouldShowNotificationTray();
}

bool NotificationTray::IsMessageCenterVisible() const {
  return message_center_bubble() &&
         message_center_bubble()->bubble()->IsVisible();
}

void NotificationTray::UpdateAfterShelfAlignmentChange() {
  TrayBackgroundView::UpdateAfterShelfAlignmentChange();
  // Destroy existing message center bubble so that it won't be reused.
  message_center_ui_controller_->HideMessageCenterBubble();

  // Destroy any existing popup bubbles and rebuilt if necessary.
  message_center_ui_controller_->HidePopupBubble();
  message_center_ui_controller_->ShowPopupBubble();
}

void NotificationTray::UpdateAfterRootWindowBoundsChange(
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  TrayBackgroundView::UpdateAfterRootWindowBoundsChange(old_bounds, new_bounds);
  // Hide the message center bubble, since the bounds may not have enough to
  // show the current size of the message center. This  handler is invoked when
  // the screen is rotated or the screen size is changed.
  message_center_ui_controller_->HideMessageCenterBubble();
}

void NotificationTray::AnchorUpdated() {
  if (message_center_bubble()) {
    UpdateClippingWindowBounds();
    shelf()->GetSystemTrayAnchorView()->UpdateClippingWindowBounds();
    message_center_bubble()->bubble_view()->UpdateBubble();
    // Should check |message_center_bubble_| again here. Since UpdateBubble
    // above set the bounds of the bubble which will stop the current
    // animation. If notification bubble is during animation to close,
    // CloseBubbleObserver in TrayBackgroundView will close the bubble if
    // animation finished.
    if (message_center_bubble())
      UpdateBubbleViewArrow(message_center_bubble()->bubble_view());
  }
}

base::string16 NotificationTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringFUTF16Int(
      IDS_MESSAGE_CENTER_ACCESSIBLE_NAME,
      static_cast<int>(message_center_ui_controller_->message_center()
                           ->NotificationCount()));
}

void NotificationTray::HideBubbleWithView(const TrayBubbleView* bubble_view) {
  if (message_center_bubble() &&
      bubble_view == message_center_bubble()->bubble_view()) {
    message_center_ui_controller_->HideMessageCenterBubble();
  } else if (popup_collection_.get()) {
    message_center_ui_controller_->HidePopupBubble();
  }
}

void NotificationTray::BubbleViewDestroyed() {
  if (message_center_bubble())
    message_center_bubble()->bubble()->BubbleViewDestroyed();
}

base::string16 NotificationTray::GetAccessibleNameForBubble() {
  return GetAccessibleNameForTray();
}

bool NotificationTray::ShouldEnableExtraKeyboardAccessibility() {
  return Shell::Get()->accessibility_controller()->IsSpokenFeedbackEnabled();
}

void NotificationTray::HideBubble(const TrayBubbleView* bubble_view) {
  HideBubbleWithView(bubble_view);
}

void NotificationTray::OnMessageCenterContentsChanged() {
  // Do not update the tray contents directly. Multiple change events can happen
  // consecutively, and calling Update in the middle of those events will show
  // intermediate unread counts for a moment.
  should_update_tray_content_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&NotificationTray::UpdateTrayContent, AsWeakPtr()));
}

void NotificationTray::UpdateTrayContent() {
  if (!should_update_tray_content_)
    return;
  should_update_tray_content_ = false;

  std::unordered_set<std::string> notification_ids;
  for (auto& pair : visible_small_icons_)
    notification_ids.insert(pair.first);

  // Add small icons (up to kMaximumSmallIconCount = 3).
  message_center::MessageCenter* message_center =
      message_center_ui_controller_->message_center();
  size_t visible_small_icon_count = 0;
  for (const auto* notification : message_center->GetVisibleNotifications()) {
    gfx::Image image = notification->GenerateMaskedSmallIcon(
        kTrayItemInnerIconSize, kTrayIconColor);
    if (image.IsEmpty())
      continue;

    if (visible_small_icon_count >= kMaximumSmallIconCount)
      break;
    visible_small_icon_count++;

    notification_ids.erase(notification->id());
    if (visible_small_icons_.count(notification->id()) != 0)
      continue;

    auto item = std::make_unique<NotificationTrayImageSubview>(
        image.AsImageSkia(), animation_container_.get(), this);
    tray_container()->AddChildViewAt(item.get(), 0);
    item->SetVisible(true);
    visible_small_icons_.insert(
        std::make_pair(notification->id(), std::move(item)));
  }

  // Remove unnecessary icons.
  for (const std::string& id : notification_ids) {
    NotificationTrayImageSubview* item = visible_small_icons_[id].release();
    visible_small_icons_.erase(id);
    item->HideAndDelete();
  }

  // Show or hide the bell icon.
  size_t visible_notification_count = message_center->NotificationCount();
  bell_icon_->SetVisible(visible_notification_count == 0 &&
                         !message_center->IsQuietMode());
  quiet_mode_icon_->SetVisible(visible_notification_count == 0 &&
                               message_center->IsQuietMode());

  // Show or hide the counter.
  size_t hidden_icon_count =
      visible_notification_count - visible_small_icon_count;
  if (hidden_icon_count != 0) {
    counter_->SetVisible(true);
    counter_->SetNotificationCount(
        (visible_small_icon_count != 0),  // small_icons_exist
        hidden_icon_count);
  } else {
    counter_->SetVisible(false);
  }

  SetVisible(ShouldShowMessageCenter());
  PreferredSizeChanged();
  Layout();
  SchedulePaint();
}

void NotificationTray::ClickedOutsideBubble() {
  // Only hide the message center
  if (!IsMessageCenterVisible())
    return;

  message_center_ui_controller_->HideMessageCenterBubble();
}

bool NotificationTray::PerformAction(const ui::Event& event) {
  UserMetricsRecorder::RecordUserClickOnTray(
      LoginMetricsRecorder::TrayClickTarget::kNotificationTray);
  if (IsMessageCenterVisible())
    CloseBubble();
  else
    ShowBubble(event.IsMouseEvent() || event.IsGestureEvent());
  return true;
}

void NotificationTray::CloseBubble() {
  message_center_ui_controller_->HideMessageCenterBubble();
}

void NotificationTray::ShowBubble(bool show_by_click) {
  if (!IsMessageCenterVisible())
    message_center_ui_controller_->ShowMessageCenterBubble(show_by_click);
}

void NotificationTray::ActivateBubble() {
  TrayBubbleView* bubble_view = GetBubbleView();
  // If the bubble is in the process of closing, do not try to activate it.
  if (bubble_view->GetWidget()->IsClosed())
    return;
  bubble_view->set_can_activate(true);
  bubble_view->GetWidget()->Activate();
}

TrayBubbleView* NotificationTray::GetBubbleView() {
  return message_center_bubble_ ? message_center_bubble_->bubble_view()
                                : nullptr;
}

message_center::MessageCenter* NotificationTray::message_center() const {
  return message_center_ui_controller_->message_center();
}

// Methods for testing

bool NotificationTray::IsPopupVisible() const {
  return message_center_ui_controller_->popups_visible();
}

MessageCenterBubble* NotificationTray::GetMessageCenterBubbleForTest() {
  if (!message_center_bubble())
    return nullptr;
  return static_cast<MessageCenterBubble*>(message_center_bubble()->bubble());
}

}  // namespace ash
