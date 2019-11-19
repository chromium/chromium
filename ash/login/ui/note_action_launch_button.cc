// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/note_action_launch_button.h"

#include <memory>

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/rtl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/view_targeter.h"

namespace ash {

namespace {

// The default note action bubble size.
constexpr int kSmallBubbleRadiusDp = 48;

// The default note action bubble opacity.
constexpr float kSmallBubbleOpacity = 0.34;

// The note action bubble size when it's hovered/focused.
constexpr int kLargeBubbleRadiusDp = 56;

// The note action bubble opacity when it's hovered/focused.
constexpr float kLargeBubbleOpacity = 0.46;

// The note action background color.
constexpr int kBubbleColor = SkColorSetRGB(0x9E, 0x9E, 0x9E);

// The note action icon size.
constexpr int kIconSizeDp = 16;

// The note action icon padding from top and right bubble edges (assuming LTR
// layout).
constexpr int kIconPaddingDp = 12;

// Layer delegate for painting a bubble of the specified radius and color to a
// layer. The bubble is painted as a quarter of a circle with the center in top
// right corner of the painted bounds.
// This is used to paint the bubble on the background view's layer.
class BubbleLayerDelegate : public views::BasePaintedLayerDelegate {
 public:
  BubbleLayerDelegate(SkColor color, int radius)
      : views::BasePaintedLayerDelegate(color), radius_(radius) {}

  ~BubbleLayerDelegate() override = default;

  // views::BasePaintedLayerDelegate:
  gfx::RectF GetPaintedBounds() const override {
    return gfx::RectF(0, 0, radius_, radius_);
  }

  void OnPaintLayer(const ui::PaintContext& context) override {
    cc::PaintFlags flags;
    flags.setColor(color());
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);

    ui::PaintRecorder recorder(context, gfx::Size(radius_, radius_));
    gfx::Canvas* canvas = recorder.canvas();
    canvas->Save();
    canvas->ClipRect(GetPaintedBounds());
    canvas->DrawCircle(base::i18n::IsRTL() ? GetPaintedBounds().origin()
                                           : GetPaintedBounds().top_right(),
                       radius_, flags);
    canvas->Restore();
  }

 private:
  // The radius of the circle.
  int radius_;

  DISALLOW_COPY_AND_ASSIGN(BubbleLayerDelegate);
};

}  // namespace

// The (background) view that paints and animates the note action bubble.
class NoteActionLaunchButton::BackgroundView : public NonAccessibleView {
 public:
  // NOTE: the background layer is set to the large bubble bounds and scaled
  // down when needed.
  BackgroundView()
      : background_layer_delegate_(kBubbleColor, kLargeBubbleRadiusDp) {
    // Painted to layer to provide background animations when the background
    // bubble is resized due to the action button changing its state (for
    // example when the button is hovered, the background should be expanded to
    // kLargeBubbleRadiusDp).
    SetPaintToLayer();

    layer()->set_delegate(&background_layer_delegate_);
    layer()->SetMasksToBounds(true);
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetVisible(true);
    layer()->SetOpacity(opacity_);
  }

  ~BackgroundView() override = default;

  // Updates the bubble's opacity and radius. The bubble radius is updated
  // applying scale transform to the view's layout. Transformations are
  // animated.
  void SetBubbleRadiusAndOpacity(int target_radius, float opacity) {
    if (target_radius == bubble_radius_ && opacity_ == opacity)
      return;

    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTweenType(gfx::Tween::EASE_IN);

    gfx::Transform transform;
    if (target_radius != kLargeBubbleRadiusDp) {
      // Move the buble to it's new origin before scaling the image - note that
      // in RTL layout, the origin remains the same - (0, 0) in local bounds.
      if (!base::i18n::IsRTL())
        transform.Translate(kLargeBubbleRadiusDp - target_radius, 0);
      float scale = target_radius / static_cast<float>(kLargeBubbleRadiusDp);
      transform.Scale(scale, scale);
    }

    layer()->SetTransform(transform);
    layer()->SetOpacity(opacity);

    bubble_radius_ = target_radius;
  }

 private:
  float opacity_ = 0;
  int bubble_radius_ = 0;

  BubbleLayerDelegate background_layer_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundView);
};

// The event target delegate used for the note action view. It matches the
// shape of the bubble with the provided radius.
class BubbleTargeterDelegate : public views::MaskedTargeterDelegate {
 public:
  explicit BubbleTargeterDelegate(int view_width, int circle_radius)
      : view_width_(view_width), circle_radius_(circle_radius) {}
  ~BubbleTargeterDelegate() override = default;

  bool GetHitTestMask(SkPath* mask) const override {
    int center_x = base::i18n::IsRTL() ? 0 : view_width_;
    mask->addCircle(SkIntToScalar(center_x), SkIntToScalar(0),
                    SkIntToScalar(circle_radius_));
    return true;
  }

 private:
  int view_width_;
  int circle_radius_;

  DISALLOW_COPY_AND_ASSIGN(BubbleTargeterDelegate);
};

// The action button foreground - an image button with actionable area matching
// the (small) bubble shape centered in the top right corner of the action
// button bounds.
class NoteActionLaunchButton::ActionButton : public views::ImageButton,
                                             public views::ButtonListener {
 public:
  explicit ActionButton(NoteActionLaunchButton::BackgroundView* background)
      : views::ImageButton(this),
        background_(background),
        event_targeter_delegate_(kLargeBubbleRadiusDp, kSmallBubbleRadiusDp) {
    SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_CREATE_NOTE_ACTION));
    SetImage(views::Button::STATE_NORMAL,
             CreateVectorIcon(kTrayActionNewLockScreenNoteIcon,
                              ShelfConfig::Get()->shelf_icon_color()));
    SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
    SetFocusPainter(nullptr);
    EnableCanvasFlippingForRTLUI(true);
    SetPreferredSize(gfx::Size(kLargeBubbleRadiusDp, kLargeBubbleRadiusDp));
    SetEventTargeter(
        std::make_unique<views::ViewTargeter>(&event_targeter_delegate_));

    // Paint to layer because the background is painted to layer - if the button
    // was not painted to layer as well, the background would be painted over
    // it.
    SetPaintToLayer();
    layer()->SetMasksToBounds(true);
    layer()->SetFillsBoundsOpaquely(false);

    UpdateBubbleRadiusAndOpacity();
  }

  ~ActionButton() override = default;

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    canvas->Save();
    // Correctly position the icon image on the button's canvas.
    canvas->Translate(gfx::Vector2d(
        kLargeBubbleRadiusDp - kIconSizeDp - kIconPaddingDp, kIconPaddingDp));
    canvas->ClipRect(gfx::Rect(0, 0, kIconSizeDp, kIconSizeDp));

    views::ImageButton::PaintButtonContents(canvas);
    canvas->Restore();
  }
  void StateChanged(ButtonState old_state) override {
    UpdateBubbleRadiusAndOpacity();
  }
  void OnFocus() override {
    ImageButton::OnFocus();
    UpdateBubbleRadiusAndOpacity();
  }
  void OnBlur() override {
    ImageButton::OnBlur();
    UpdateBubbleRadiusAndOpacity();
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN:
        // Mark scroll begin handled, so the view starts receiving scroll
        // events (in particular) fling/swipe.
        // Ignore multi-finger gestures - the note action requests are
        // restricted to single finger gestures.
        if (event->details().touch_points() == 1) {
          SetTrackingPotentialActivationGesture(true);
          event->SetHandled();
        }
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        // If the user has added fingers to the gesture, cancel the fling
        // detection - the note action requests are restricted to single finger
        // gestures.
        if (event->details().touch_points() != 1)
          SetTrackingPotentialActivationGesture(false);
        break;
      case ui::ET_GESTURE_END:
      case ui::ET_GESTURE_SCROLL_END:
      case ui::ET_SCROLL_FLING_CANCEL:
        SetTrackingPotentialActivationGesture(false);
        break;
      case ui::ET_SCROLL_FLING_START:
        MaybeActivateActionOnFling(event);
        SetTrackingPotentialActivationGesture(false);
        event->StopPropagation();
        break;
      default:
        break;
    }

    if (!event->handled())
      views::ImageButton::OnGestureEvent(event);
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    UserMetricsRecorder::RecordUserClickOnTray(
        LoginMetricsRecorder::TrayClickTarget::kTrayActionNoteButton);
    if (event.IsKeyEvent()) {
      Shell::Get()->tray_action()->RequestNewLockScreenNote(
          mojom::LockScreenNoteOrigin::kLockScreenButtonKeyboard);
    } else {
      Shell::Get()->tray_action()->RequestNewLockScreenNote(
          mojom::LockScreenNoteOrigin::kLockScreenButtonTap);
    }
  }

 private:
  // Updates the background view size and opacity depending on the current note
  // action button state.
  void UpdateBubbleRadiusAndOpacity() {
    bool show_large_bubble = HasFocus() || state() == STATE_HOVERED ||
                             state() == STATE_PRESSED ||
                             tracking_activation_gesture_;
    background_->SetBubbleRadiusAndOpacity(
        show_large_bubble ? kLargeBubbleRadiusDp : kSmallBubbleRadiusDp,
        show_large_bubble ? kLargeBubbleOpacity : kSmallBubbleOpacity);
  }

  // Called when a fling is detected - if the gesture direction was bottom-left,
  // it requests a new lock screen note.
  void MaybeActivateActionOnFling(ui::GestureEvent* event) {
    int adjust_x_for_rtl = base::i18n::IsRTL() ? -1 : 1;
    if (!tracking_activation_gesture_ || event->details().touch_points() != 1 ||
        adjust_x_for_rtl * event->details().velocity_x() > 0 ||
        event->details().velocity_y() < 0) {
      return;
    }

    Shell::Get()->tray_action()->RequestNewLockScreenNote(
        mojom::LockScreenNoteOrigin::kLockScreenButtonSwipe);
  }

  // Sets a flag indicating that the button is tracking a potential note
  // activation gesture, updating the background view appearance (as the
  // background bubble should be expanded if a gesture is tracked.
  void SetTrackingPotentialActivationGesture(bool tracking_activation_gesture) {
    tracking_activation_gesture_ = tracking_activation_gesture;
    UpdateBubbleRadiusAndOpacity();
  }

  // The background view, which paints the note action bubble.
  NoteActionLaunchButton::BackgroundView* background_;

  BubbleTargeterDelegate event_targeter_delegate_;

  // Set when a potention note activation gesture is tracked - i.e. while a
  // scroll gesture (which could lead to a fling) is in progress.
  bool tracking_activation_gesture_ = false;

  DISALLOW_COPY_AND_ASSIGN(ActionButton);
};

NoteActionLaunchButton::TestApi::TestApi(NoteActionLaunchButton* launch_button)
    : launch_button_(launch_button) {}

NoteActionLaunchButton::TestApi::~TestApi() = default;

const views::View* NoteActionLaunchButton::TestApi::ActionButtonView() const {
  return launch_button_->action_button_;
}

const views::View* NoteActionLaunchButton::TestApi::BackgroundView() const {
  return launch_button_->background_;
}

NoteActionLaunchButton::NoteActionLaunchButton(
    mojom::TrayActionState initial_note_action_state) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  background_ = new BackgroundView();
  AddChildView(background_);

  action_button_ = new ActionButton(background_);
  AddChildView(action_button_);

  UpdateVisibility(initial_note_action_state);
}

NoteActionLaunchButton::~NoteActionLaunchButton() = default;

void NoteActionLaunchButton::UpdateVisibility(
    mojom::TrayActionState action_state) {
  SetVisible(action_state == mojom::TrayActionState::kAvailable);
}

}  // namespace ash
