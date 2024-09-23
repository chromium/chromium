// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/touch_calibrator_view.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr char kWidgetName[] = "TouchCalibratorOverlay";

constexpr int kAnimationFrameRate = 100;
constexpr auto kFadeDuration = base::Milliseconds(150);
constexpr auto kPointMoveDuration = base::Milliseconds(400);
constexpr auto kPointMoveDurationLong = base::Milliseconds(500);

const SkColor kExitLabelColor = SkColorSetARGB(255, 138, 138, 138);
constexpr int kExitLabelWidth = 300;
constexpr int kExitLabelHeight = 20;

const SkColor kSkipLabelColor = SkColorSetARGB(255, 138, 138, 138);
constexpr int kSkipLabelWidth = 500;
constexpr int kSkipLabelHeight = 30;

const SkColor kTapHereLabelColor = SK_ColorWHITE;

constexpr int kHintBoxWidth = 298;
constexpr int kHintBoxHeight = 180;
constexpr int kHintBoxLabelTextSize = 5;
constexpr int kHintBoxSublabelTextSize = 3;

constexpr int kThrobberCircleViewWidth = 64;
constexpr float kThrobberCircleRadiusFactor = 3.f / 8.f;

constexpr auto kFinalMessageTransitionDuration = base::Milliseconds(200);
constexpr int kCompleteMessageViewWidth = 427;
constexpr int kCompleteMessageViewHeight = kThrobberCircleViewWidth;
constexpr int kCompleteMessageTextSize = 16;

constexpr int kTouchPointViewOffset = 100;

constexpr int kTapLabelHeight = 48;
constexpr int kTapLabelWidth = 80;

const SkColor kHintLabelTextColor = SK_ColorBLACK;
const SkColor kHintSublabelTextColor = SkColorSetARGB(255, 161, 161, 161);

const SkColor kInnerCircleColor = SK_ColorWHITE;
const SkColor kOuterCircleColor = SkColorSetA(kInnerCircleColor, 255 * 0.2);

constexpr auto kCircleAnimationDuration = base::Milliseconds(900);

constexpr int kHintRectBorderRadius = 4;

constexpr float kBackgroundFinalOpacity = 0.75f;

constexpr int kTouchTargetWidth = 64;
constexpr int kTouchTargetHeight = kTouchTargetWidth + kTouchTargetWidth / 2;

constexpr float kTouchTargetVerticalOffsetFactor = 11.f / 24.f;

const SkColor kTouchTargetInnerCircleColor = SkColorSetARGB(255, 66, 133, 244);
const SkColor kTouchTargetOuterCircleColor =
    SkColorSetA(kTouchTargetInnerCircleColor, 255 * 0.2);
const SkColor kHandIconColor = SkColorSetARGB(255, 201, 201, 201);
constexpr float kHandIconHorizontalOffsetFactor = 7.f / 32.f;

// Returns the initialization params for the widget that contains the touch
// calibrator view.
views::Widget::InitParams GetWidgetParams(aura::Window* root_window) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = kWidgetName;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.accept_events = true;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);
  return params;
}

// Returns the size of bounding box required for |text| of given |font_list|.
gfx::Size GetSizeForString(const std::u16string& text,
                           const gfx::FontList& font_list) {
  int height = 0, width = 0;
  gfx::Canvas::SizeStringInt(text, font_list, &width, &height, 0, 0);
  return gfx::Size(width, height);
}

void AnimateLayerToPosition(views::View* view,
                            base::TimeDelta duration,
                            gfx::Point end_position,
                            float opacity = 1.f) {
  ui::ScopedLayerAnimationSettings slide_settings(view->layer()->GetAnimator());
  slide_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  slide_settings.SetTransitionDuration(duration);
  view->SetBoundsRect(gfx::Rect(end_position, view->size()));
  view->layer()->SetOpacity(opacity);
}

}  // namespace

// Creates a throbbing animated view with two concentric circles. The radius of
// the inner circle is fixed while that of the outer circle oscillates between a
// min and max radius. The animation takes |animation_duration| milliseconds
// to complete. The center of these circles are at the center of the view
// element.
class CircularThrobberView : public views::View,
                             public views::AnimationDelegateViews {
  METADATA_HEADER(CircularThrobberView, views::View)

 public:
  CircularThrobberView(int width,
                       const SkColor& inner_circle_color,
                       const SkColor& outer_circle_color,
                       base::TimeDelta animation_duration);
  CircularThrobberView(const CircularThrobberView&) = delete;
  CircularThrobberView& operator=(const CircularThrobberView&) = delete;
  ~CircularThrobberView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  // Radius of the inner circle.
  const int inner_radius_;

  // Current radius of the outer circle.
  int outer_radius_;

  // Minimum radius for outer animated circle.
  const int smallest_radius_animated_circle_;

  // Maximum radius for outer animated circle.
  const int largest_radius_animated_circle_;

  cc::PaintFlags inner_circle_flags_;
  cc::PaintFlags outer_circle_flags_;

  std::unique_ptr<gfx::ThrobAnimation> animation_;

  // Center of the concentric circles.
  const gfx::Point center_;
};

CircularThrobberView::CircularThrobberView(int width,
                                           const SkColor& inner_circle_color,
                                           const SkColor& outer_circle_color,
                                           base::TimeDelta animation_duration)
    : views::AnimationDelegateViews(this),
      inner_radius_(width / 4),
      outer_radius_(inner_radius_),
      smallest_radius_animated_circle_(width * kThrobberCircleRadiusFactor),
      largest_radius_animated_circle_(width / 2),
      center_(gfx::Point(width / 2, width / 2)) {
  SetSize(gfx::Size(width, width));

  inner_circle_flags_.setColor(inner_circle_color);
  inner_circle_flags_.setAntiAlias(true);
  inner_circle_flags_.setStyle(cc::PaintFlags::kFill_Style);

  outer_circle_flags_.setColor(outer_circle_color);
  outer_circle_flags_.setAntiAlias(true);
  outer_circle_flags_.setStyle(cc::PaintFlags::kFill_Style);

  animation_ = std::make_unique<gfx::ThrobAnimation>(this);
  animation_->SetThrobDuration(animation_duration);
  animation_->StartThrobbing(-1);

  SchedulePaint();
}

CircularThrobberView::~CircularThrobberView() = default;

void CircularThrobberView::OnPaint(gfx::Canvas* canvas) {
  canvas->DrawCircle(center_, outer_radius_, outer_circle_flags_);
  canvas->DrawCircle(center_, inner_radius_, inner_circle_flags_);
}

void CircularThrobberView::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animation != animation_.get())
    return;
  outer_radius_ = animation->CurrentValueBetween(
      smallest_radius_animated_circle_, largest_radius_animated_circle_);
  SchedulePaint();
}

BEGIN_METADATA(CircularThrobberView)
END_METADATA

class TouchTargetThrobberView : public CircularThrobberView {
  METADATA_HEADER(TouchTargetThrobberView, CircularThrobberView)

 public:
  TouchTargetThrobberView(const gfx::Rect& bounds,
                          const SkColor& inner_circle_color,
                          const SkColor& outer_circle_color,
                          const SkColor& hand_icon_color,
                          base::TimeDelta animation_duration);
  TouchTargetThrobberView(const TouchTargetThrobberView&) = delete;
  TouchTargetThrobberView& operator=(const TouchTargetThrobberView&) = delete;
  ~TouchTargetThrobberView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  const int horizontal_offset_;

  const int icon_width_;

  gfx::ImageSkia hand_icon_;
};

TouchTargetThrobberView::TouchTargetThrobberView(
    const gfx::Rect& bounds,
    const SkColor& inner_circle_color,
    const SkColor& outer_circle_color,
    const SkColor& hand_icon_color,
    base::TimeDelta animation_duration)
    : CircularThrobberView(bounds.width(),
                           inner_circle_color,
                           outer_circle_color,
                           animation_duration),
      horizontal_offset_(bounds.width() * kHandIconHorizontalOffsetFactor),
      icon_width_(bounds.width() * 0.5f) {
  SetBoundsRect(bounds);

  hand_icon_ = gfx::CreateVectorIcon(kTouchCalibrationHandIcon, kHandIconColor);
}

TouchTargetThrobberView::~TouchTargetThrobberView() = default;

void TouchTargetThrobberView::OnPaint(gfx::Canvas* canvas) {
  CircularThrobberView::OnPaint(canvas);
  canvas->DrawImageInt(hand_icon_, horizontal_offset_, icon_width_);
}

BEGIN_METADATA(TouchTargetThrobberView)
END_METADATA

//   Circular      _________________________________
//   Throbber     |                                 |
//     View       |                                 |
//  ___________   |                                 |
// |           |  |                                 |
// |           |  |                                 |
// |     .     |  |            Hint Box             |
// |           |  |                                 |
// |___________|  |                                 |
//                |                                 |
//                |                                 |
//                |_________________________________|
//
// This view is set next to the throbber circle view such that their centers
// align. The hint box has a label text and a sublabel text to assist the
// user by informing them about the next step in the calibration process.
class HintBox : public views::View {
  METADATA_HEADER(HintBox, views::View)

 public:
  HintBox(const gfx::Rect& bounds, int border_radius);
  HintBox(const HintBox&) = delete;
  HintBox& operator=(const HintBox&) = delete;
  ~HintBox() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  void SetLabel(const std::u16string& text, const SkColor& color);
  void SetSubLabel(const std::u16string& text, const SkColor& color);

 private:
  void UpdateWidth(int updated_width);

  std::u16string label_text_;
  std::u16string sublabel_text_;

  SkColor label_color_;
  SkColor sublabel_color_;

  const int border_radius_;

  int base_border_;

  int arrow_width_;

  int horizontal_offset_;

  gfx::Rect rounded_rect_bounds_;

  gfx::FontList label_font_list_;
  gfx::FontList sublabel_font_list_;

  gfx::Rect label_text_bounds_;
  gfx::Rect sublabel_text_bounds_;

  cc::PaintFlags flags_;
};

HintBox::HintBox(const gfx::Rect& bounds, int border_radius)
    : border_radius_(border_radius) {
  auto border = std::make_unique<views::BubbleBorder>(
      base::i18n::IsRTL() ? views::BubbleBorder::RIGHT_CENTER
                          : views::BubbleBorder::LEFT_CENTER,
      views::BubbleBorder::NO_SHADOW);
  border->SetColor(SK_ColorWHITE);
  SetBorder(std::move(border));

  arrow_width_ = (GetInsets().right() - GetInsets().left()) *
                 (base::i18n::IsRTL() ? 1 : -1);

  // Border on all sides are the same except on the side of the arrow, in which
  // case the width of the arrow is additional.
  base_border_ = base::i18n::IsRTL() ? GetInsets().left() : GetInsets().right();

  SetBounds(bounds.x(), bounds.y() - base_border_,
            bounds.width() + 2 * base_border_ + arrow_width_,
            bounds.height() + 2 * base_border_);

  rounded_rect_bounds_ = GetContentsBounds();

  flags_.setColor(SK_ColorWHITE);
  flags_.setStyle(cc::PaintFlags::kFill_Style);
  flags_.setAntiAlias(true);

  horizontal_offset_ =
      arrow_width_ + base_border_ + rounded_rect_bounds_.width() * 0.08f;
  int top_offset = horizontal_offset_;
  int line_gap = rounded_rect_bounds_.height() * 0.018f;
  int label_height = rounded_rect_bounds_.height() * 0.11f;

  label_text_bounds_.SetRect(horizontal_offset_, top_offset, 0, label_height);

  top_offset += label_text_bounds_.height() + line_gap;

  sublabel_text_bounds_.SetRect(horizontal_offset_, top_offset, 0,
                                label_height);
}

HintBox::~HintBox() = default;

void HintBox::UpdateWidth(int updated_width) {
  SetSize(gfx::Size(updated_width + 2 * base_border_ + arrow_width_, height()));
  rounded_rect_bounds_ = GetContentsBounds();
}

void HintBox::SetLabel(const std::u16string& text, const SkColor& color) {
  label_text_ = text;
  label_color_ = color;

  label_font_list_ =
      ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
          kHintBoxLabelTextSize);

  // Adjust size of label bounds based on text and font.
  gfx::Size size = GetSizeForString(label_text_, label_font_list_);
  label_text_bounds_.set_size(
      gfx::Size(size.width(), label_text_bounds_.height()));

  // Check if the width of hint box needs to be updated.
  int minimum_expected_width =
      size.width() + 2 * horizontal_offset_ - arrow_width_;
  if (minimum_expected_width > rounded_rect_bounds_.width())
    UpdateWidth(minimum_expected_width);
}

void HintBox::SetSubLabel(const std::u16string& text, const SkColor& color) {
  sublabel_text_ = text;
  sublabel_color_ = color;

  sublabel_font_list_ =
      ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
          kHintBoxSublabelTextSize);

  // Adjust size of sublabel label bounds based on text and font.
  gfx::Size size = GetSizeForString(sublabel_text_, sublabel_font_list_);
  sublabel_text_bounds_.set_size(
      gfx::Size(size.width(), sublabel_text_bounds_.height()));

  // Check if the width of hint box needs to be updated.
  int minimum_expected_width =
      size.width() + 2 * horizontal_offset_ - arrow_width_;
  if (minimum_expected_width > rounded_rect_bounds_.width())
    UpdateWidth(minimum_expected_width);
}

void HintBox::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  canvas->DrawRoundRect(rounded_rect_bounds_, border_radius_, flags_);
  canvas->DrawStringRectWithFlags(label_text_, label_font_list_, label_color_,
                                  label_text_bounds_, gfx::Canvas::NO_ELLIPSIS);
  canvas->DrawStringRectWithFlags(sublabel_text_, sublabel_font_list_,
                                  sublabel_color_, sublabel_text_bounds_,
                                  gfx::Canvas::NO_ELLIPSIS);
}

BEGIN_METADATA(HintBox)
END_METADATA

class CompletionMessageView : public views::View {
  METADATA_HEADER(CompletionMessageView, views::View)

 public:
  CompletionMessageView(const gfx::Rect& bounds, const std::u16string& message);
  CompletionMessageView(const CompletionMessageView&) = delete;
  CompletionMessageView& operator=(const CompletionMessageView&) = delete;
  ~CompletionMessageView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  const std::u16string message_;
  gfx::FontList font_list_;

  gfx::Rect text_bounds_;

  gfx::ImageSkia check_icon_;

  cc::PaintFlags flags_;
};

CompletionMessageView::CompletionMessageView(const gfx::Rect& bounds,
                                             const std::u16string& message)
    : message_(message) {
  SetBoundsRect(bounds);

  int x_offset = height() * 5.f / 4.f;
  text_bounds_.SetRect(x_offset, 0, width() - x_offset, height());

  font_list_ = ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
      kCompleteMessageTextSize);

  // crbug/676513 moves this file to src/ash which will require an ash icon
  // file.
  check_icon_ =
      gfx::CreateVectorIcon(kTouchCalibrationCompleteCheckIcon, SK_ColorWHITE);

  flags_.setColor(SK_ColorWHITE);
  flags_.setStyle(cc::PaintFlags::kFill_Style);
  flags_.setAntiAlias(true);
}

CompletionMessageView::~CompletionMessageView() = default;

void CompletionMessageView::OnPaint(gfx::Canvas* canvas) {
  canvas->DrawImageInt(check_icon_, 0, 0);

  // TODO(malaykeshav): Work with elizabethchiu@ to get better UX for RTL.
  canvas->DrawStringRectWithFlags(
      message_, font_list_, flags_.getColor(), text_bounds_,
      gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::NO_SUBPIXEL_RENDERING);
}

BEGIN_METADATA(CompletionMessageView)
END_METADATA

// static
views::UniqueWidgetPtr TouchCalibratorView::Create(
    const display::Display& target_display,
    bool is_primary_view,
    bool is_for_touchscreen_mapping) {
  aura::Window* root = Shell::GetRootWindowForDisplayId(target_display.id());
  views::UniqueWidgetPtr widget(
      std::make_unique<views::Widget>(GetWidgetParams(root)));
  widget->SetContentsView(base::WrapUnique(new TouchCalibratorView(
      target_display, is_primary_view, is_for_touchscreen_mapping)));
  widget->SetBounds(target_display.bounds());
  widget->Show();
  return widget;
}

TouchCalibratorView::TouchCalibratorView(const display::Display& target_display,
                                         bool is_primary_view,
                                         bool is_for_touchscreen_mapping)
    : views::AnimationDelegateViews(this),
      display_(target_display),
      is_primary_view_(is_primary_view),
      animator_(std::make_unique<gfx::LinearAnimation>(kFadeDuration,
                                                       kAnimationFrameRate,
                                                       this)) {
  InitViewContents(is_for_touchscreen_mapping);
  AdvanceToNextState();
}

TouchCalibratorView::~TouchCalibratorView() {
  state_ = UNKNOWN;
  animator_->End();
}

void TouchCalibratorView::InitViewContents(bool is_for_touchscreen_mapping) {
  // Initialize the background rect.
  background_rect_ =
      gfx::RectF(0, 0, display_.bounds().width(), display_.bounds().height());

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // Initialize exit label that informs the user how to exit the touch
  // calibration setup.
  if (!is_for_touchscreen_mapping) {
    exit_label_ = AddChildView(std::make_unique<views::Label>(
        rb.GetLocalizedString(IDS_DISPLAY_TOUCH_CALIBRATION_EXIT_LABEL),
        views::Label::CustomFont{rb.GetFontListWithDelta(8)}));
    exit_label_->SetBounds((display_.bounds().width() - kExitLabelWidth) / 2,
                           display_.bounds().height() * 3.f / 4,
                           kExitLabelWidth, kExitLabelHeight);
    exit_label_->SetEnabledColor(kExitLabelColor);
  } else {
    exit_label_ = AddChildView(std::make_unique<views::Label>(
        rb.GetLocalizedString(
            is_primary_view_
                ? IDS_DISPLAY_TOUCH_CALIBRATION_PRIMARY_SKIP_LABEL
                : IDS_DISPLAY_TOUCH_CALIBRATION_SECONDARY_SKIP_LABEL),
        views::Label::CustomFont{rb.GetFontListWithDelta(8)}));
    exit_label_->SetBounds((display_.bounds().width() - kSkipLabelWidth) / 2,
                           display_.bounds().height() * 3.f / 4,
                           kSkipLabelWidth, kSkipLabelHeight);
    exit_label_->SetEnabledColor(kSkipLabelColor);
  }
  exit_label_->SetAutoColorReadabilityEnabled(false);
  exit_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  exit_label_->SetSubpixelRenderingEnabled(false);
  exit_label_->SetVisible(false);

  // If this is not the screen that is being calibrated, then this is all we
  // need to display.
  if (!is_primary_view_)
    return;

  // Initialize the touch point view that contains the animated circle that the
  // user needs to tap.
  const int kTouchPointViewHeight = kThrobberCircleViewWidth + kTapLabelHeight;
  const int kThrobberCircleViewHorizontalOffset =
      (kTapLabelWidth - kThrobberCircleViewWidth) / 2;

  touch_point_view_ = AddChildView(std::make_unique<views::View>());
  touch_point_view_->SetBounds(kTouchPointViewOffset, kTouchPointViewOffset,
                               kTapLabelWidth, kTouchPointViewHeight);
  touch_point_view_->SetVisible(false);
  touch_point_view_->SetPaintToLayer();
  touch_point_view_->layer()->SetFillsBoundsOpaquely(false);
  touch_point_view_->layer()->GetAnimator()->AddObserver(this);
  touch_point_view_->SetBackground(
      views::CreateSolidBackground(SK_ColorTRANSPARENT));

  throbber_circle_ =
      touch_point_view_->AddChildView(std::make_unique<CircularThrobberView>(
          kThrobberCircleViewWidth, kInnerCircleColor, kOuterCircleColor,
          kCircleAnimationDuration));
  throbber_circle_->SetPosition(
      gfx::Point(kThrobberCircleViewHorizontalOffset, 0));

  // Initialize the tap label.
  tap_label_ = touch_point_view_->AddChildView(std::make_unique<views::Label>(
      rb.GetLocalizedString(IDS_DISPLAY_TOUCH_CALIBRATION_TAP_HERE_LABEL),
      views::Label::CustomFont{rb.GetFontListWithDelta(6)}));
  tap_label_->SetBounds(0, kThrobberCircleViewWidth, kTapLabelWidth,
                        kTapLabelHeight);
  tap_label_->SetEnabledColor(kTapHereLabelColor);
  tap_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  tap_label_->SetAutoColorReadabilityEnabled(false);
  tap_label_->SetSubpixelRenderingEnabled(false);
  tap_label_->SetVisible(false);

  // Initialize the Hint Box view.
  std::u16string hint_label_text =
      rb.GetLocalizedString(IDS_DISPLAY_TOUCH_CALIBRATION_HINT_LABEL_TEXT);
  std::u16string hint_sublabel_text =
      rb.GetLocalizedString(IDS_DISPLAY_TOUCH_CALIBRATION_HINT_SUBLABEL_TEXT);

  int tpv_width = touch_point_view_->width();

  gfx::Size size(kHintBoxWidth, kHintBoxHeight);

  gfx::Point position(touch_point_view_->x() + tpv_width * 1.2f,
                      touch_point_view_->y() +
                          (kThrobberCircleViewWidth / 2.f) -
                          (size.height() / 2.f));

  hint_box_view_ = AddChildView(std::make_unique<HintBox>(
      gfx::Rect(position, size), kHintRectBorderRadius));
  hint_box_view_->SetVisible(false);
  hint_box_view_->SetLabel(hint_label_text, kHintLabelTextColor);
  hint_box_view_->SetSubLabel(hint_sublabel_text, kHintSublabelTextColor);

  // Initialize the animated hint box throbber view.
  auto* target_view =
      hint_box_view_->AddChildView(std::make_unique<TouchTargetThrobberView>(
          gfx::Rect((hint_box_view_->width() - kTouchTargetWidth) / 2,
                    hint_box_view_->height() * kTouchTargetVerticalOffsetFactor,
                    kTouchTargetWidth, kTouchTargetHeight),
          kTouchTargetInnerCircleColor, kTouchTargetOuterCircleColor,
          kHandIconColor, kCircleAnimationDuration));
  target_view->SetVisible(true);

  // Initialize the view that contains the calibration complete message which
  // will be displayed at the end.
  std::u16string finish_msg_text =
      rb.GetLocalizedString(IDS_DISPLAY_TOUCH_CALIBRATION_FINISH_LABEL);

  gfx::Rect msg_view_bounds(
      (display_.bounds().width() - kCompleteMessageViewWidth) / 2,
      display_.bounds().height() / 3, kCompleteMessageViewWidth,
      kCompleteMessageViewHeight);
  completion_message_view_ =
      AddChildView(std::make_unique<CompletionMessageView>(msg_view_bounds,
                                                           finish_msg_text));
  completion_message_view_->SetVisible(false);
  completion_message_view_->SetPaintToLayer();
  completion_message_view_->layer()->SetFillsBoundsOpaquely(false);
  completion_message_view_->layer()->GetAnimator()->AddObserver(this);
  completion_message_view_->SetBackground(
      views::CreateSolidBackground(SK_ColorTRANSPARENT));
}

void TouchCalibratorView::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
}

void TouchCalibratorView::OnPaintBackground(gfx::Canvas* canvas) {
  float opacity;

  // If current state is a fade in or fade out state then update opacity
  // based on how far the animation has progressed.
  if (animator_ && (state_ == TouchCalibratorView::BACKGROUND_FADING_OUT ||
                    state_ == TouchCalibratorView::BACKGROUND_FADING_IN)) {
    opacity = static_cast<float>(animator_->CurrentValueBetween(
        start_opacity_value_, end_opacity_value_));
  } else {
    opacity = state_ == BACKGROUND_FADING_OUT ? 0.0f : kBackgroundFinalOpacity;
  }

  flags_.setColor(SkColorSetA(SK_ColorBLACK,
                              std::numeric_limits<uint8_t>::max() * opacity));
  canvas->DrawRect(background_rect_, flags_);
}

void TouchCalibratorView::AnimationProgressed(const gfx::Animation* animation) {
  if (!is_primary_view_) {
    SchedulePaint();
    return;
  }
}

void TouchCalibratorView::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void TouchCalibratorView::AnimationEnded(const gfx::Animation* animation) {
  switch (state_) {
    case BACKGROUND_FADING_IN:
      exit_label_->SetVisible(true);
      state_ = is_primary_view_ ? DISPLAY_POINT_1 : CALIBRATION_COMPLETE;
      if (is_primary_view_) {
        touch_point_view_->SetVisible(true);
        hint_box_view_->SetVisible(true);
      }
      break;
    case BACKGROUND_FADING_OUT:
      exit_label_->SetVisible(false);
      if (is_primary_view_)
        completion_message_view_->SetVisible(false);
      GetWidget()->Hide();
      break;
    default:
      break;
  }
}

void TouchCalibratorView::OnLayerAnimationStarted(
    ui::LayerAnimationSequence* sequence) {}

void TouchCalibratorView::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  switch (state_) {
    case ANIMATING_1_TO_2:
      state_ = DISPLAY_POINT_2;
      tap_label_->SetVisible(true);
      break;
    case ANIMATING_2_TO_3:
      state_ = DISPLAY_POINT_3;
      break;
    case ANIMATING_3_TO_4:
      state_ = DISPLAY_POINT_4;
      break;
    case ANIMATING_FINAL_MESSAGE:
      state_ = CALIBRATION_COMPLETE;
      break;
    default:
      break;
  }
}

void TouchCalibratorView::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  OnLayerAnimationEnded(sequence);
}

void TouchCalibratorView::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {}

void TouchCalibratorView::AdvanceToNextState() {
  // Stop any previous animations and skip them to the end.
  SkipCurrentAnimation();

  switch (state_) {
    case UNKNOWN:
    case BACKGROUND_FADING_IN:
      state_ = BACKGROUND_FADING_IN;
      start_opacity_value_ = 0.0f;
      end_opacity_value_ = kBackgroundFinalOpacity;

      flags_.setStyle(cc::PaintFlags::kFill_Style);
      animator_->SetDuration(kFadeDuration);
      animator_->Start();
      return;
    case DISPLAY_POINT_1:
      state_ = ANIMATING_1_TO_2;

      // The touch point has to be animated from the top left corner of the
      // screen to the top right corner.
      AnimateLayerToPosition(
          touch_point_view_, kPointMoveDuration,
          gfx::Point(display_.bounds().width() - kTouchPointViewOffset -
                         touch_point_view_->width(),
                     touch_point_view_->y()));
      hint_box_view_->SetVisible(false);
      return;
    case DISPLAY_POINT_2:
      state_ = ANIMATING_2_TO_3;

      // The touch point has to be animated from the top right corner of the
      // screen to the bottom left corner.
      AnimateLayerToPosition(
          touch_point_view_, kPointMoveDurationLong,
          gfx::Point(kTouchPointViewOffset, display_.bounds().height() -
                                                kTouchPointViewOffset -
                                                touch_point_view_->height()));
      return;
    case DISPLAY_POINT_3:
      state_ = ANIMATING_3_TO_4;

      // The touch point has to be animated from the bottom left corner of the
      // screen to the bottom right corner.
      AnimateLayerToPosition(
          touch_point_view_, kPointMoveDuration,
          gfx::Point(display_.bounds().width() - kTouchPointViewOffset -
                         touch_point_view_->width(),
                     touch_point_view_->y()));
      return;
    case DISPLAY_POINT_4:
      state_ = ANIMATING_FINAL_MESSAGE;
      completion_message_view_->layer()->SetOpacity(0.0f);
      completion_message_view_->SetVisible(true);

      touch_point_view_->SetVisible(false);

      AnimateLayerToPosition(completion_message_view_,
                             kFinalMessageTransitionDuration,
                             gfx::Point(completion_message_view_->x(),
                                        display_.bounds().height() / 2));
      return;
    case CALIBRATION_COMPLETE:
      state_ = BACKGROUND_FADING_OUT;
      if (is_primary_view_) {
        // In case of primary view, we also need to fade out the calibration
        // complete message view.
        AnimateLayerToPosition(
            completion_message_view_, kFadeDuration,
            gfx::Point(completion_message_view_->x(),
                       completion_message_view_->y() +
                           2 * completion_message_view_->height()),
            0.0f);
      }

      start_opacity_value_ = kBackgroundFinalOpacity;
      end_opacity_value_ = 0.0f;

      flags_.setStyle(cc::PaintFlags::kFill_Style);
      animator_->SetDuration(kFadeDuration);
      animator_->Start();
      return;
    default:
      return;
  }
}

bool TouchCalibratorView::GetDisplayPointLocation(gfx::Point* location) {
  DCHECK(location);
  if (!is_primary_view_)
    return false;

  if (state_ != DISPLAY_POINT_1 && state_ != DISPLAY_POINT_2 &&
      state_ != DISPLAY_POINT_3 && state_ != DISPLAY_POINT_4) {
    return false;
  }

  if (!touch_point_view_ || !throbber_circle_)
    return false;
  // TODO(malaykeshav): Can use views::ConvertPointToScreen()
  location->SetPoint(touch_point_view_->x() + touch_point_view_->width() / 2.f,
                     touch_point_view_->y() + touch_point_view_->width() / 2.f);
  return true;
}

void TouchCalibratorView::SkipToFinalState() {
  state_ = CALIBRATION_COMPLETE;

  exit_label_->SetVisible(false);

  if (is_primary_view_) {
    touch_point_view_->SetVisible(false);
    hint_box_view_->SetVisible(false);
  }

  AdvanceToNextState();
}

void TouchCalibratorView::SkipCurrentAnimation() {
  if (animator_->is_animating())
    animator_->End();
  if (touch_point_view_ &&
      touch_point_view_->layer()->GetAnimator()->is_animating()) {
    touch_point_view_->layer()->GetAnimator()->StopAnimating();
  }
}

BEGIN_METADATA(TouchCalibratorView)
END_METADATA

}  // namespace ash
