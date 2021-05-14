// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_util.h"

#include "ash/style/ash_color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/painter.h"

namespace ash {
namespace holding_space_util {

namespace {

// CirclePainter ---------------------------------------------------------------

class CirclePainter : public views::Painter {
 public:
  CirclePainter(SkColor color, size_t fixed_size)
      : color_(color), fixed_size_(fixed_size) {}

  CirclePainter(SkColor color, const gfx::InsetsF& insets)
      : color_(color), insets_(insets) {}

  CirclePainter(const CirclePainter&) = delete;
  CirclePainter& operator=(const CirclePainter&) = delete;
  ~CirclePainter() override = default;

 private:
  // views::Painter:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    gfx::RectF bounds{gfx::SizeF(size)};

    if (insets_.has_value())
      bounds.Inset(insets_.value());

    const float radius =
        fixed_size_.has_value()
            ? fixed_size_.value() / 2.f
            : std::min(bounds.size().width(), bounds.size().height()) / 2.f;

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);

    canvas->DrawCircle(bounds.CenterPoint(), radius, flags);
  }

  const SkColor color_;
  const absl::optional<size_t> fixed_size_;
  const absl::optional<gfx::InsetsF> insets_;
};

// LabelWithThemeChangedCallback -----------------------------------------------

// A label which invokes a constructor-specified callback in `OnThemeChanged()`.
class LabelWithThemeChangedCallback : public views::Label {
 public:
  using ThemeChangedCallback = base::RepeatingCallback<void(views::Label*)>;

  LabelWithThemeChangedCallback(const std::u16string& text,
                                ThemeChangedCallback theme_changed_callback)
      : views::Label(text),
        theme_changed_callback_(std::move(theme_changed_callback)) {}

  LabelWithThemeChangedCallback(const LabelWithThemeChangedCallback&) = delete;
  LabelWithThemeChangedCallback& operator=(
      const LabelWithThemeChangedCallback&) = delete;
  ~LabelWithThemeChangedCallback() override = default;

 private:
  // views::Label:
  void OnThemeChanged() override {
    views::Label::OnThemeChanged();
    theme_changed_callback_.Run(this);
  }

  ThemeChangedCallback theme_changed_callback_;
};

// Helpers ---------------------------------------------------------------------

// Creates a `ui::LayerAnimationSequence` for the specified `element` with
// optional `delay`, observed by the specified `observer`.
std::unique_ptr<ui::LayerAnimationSequence> CreateObservedSequence(
    std::unique_ptr<ui::LayerAnimationElement> element,
    base::TimeDelta delay,
    ui::LayerAnimationObserver* observer) {
  auto sequence = std::make_unique<ui::LayerAnimationSequence>();
  if (!delay.is_zero()) {
    sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
        element->properties(), delay));
  }
  sequence->AddElement(std::move(element));
  sequence->AddObserver(observer);
  return sequence;
}

// Animates the specified `view` to a target `opacity` with the specified
// `duration` and optional `delay`, associating `observer` with the created
// animation sequences.
void AnimateTo(views::View* view,
               float opacity,
               base::TimeDelta duration,
               base::TimeDelta delay,
               ui::LayerAnimationObserver* observer) {
  // Opacity animation.
  auto opacity_element =
      ui::LayerAnimationElement::CreateOpacityElement(opacity, duration);
  opacity_element->set_tween_type(gfx::Tween::Type::LINEAR);

  // Note that the `ui::LayerAnimator` takes ownership of any animation
  // sequences so they need to be released.
  view->layer()->GetAnimator()->StartAnimation(
      CreateObservedSequence(std::move(opacity_element), delay, observer)
          .release());
}

}  // namespace

// Animates in the specified `view` with the specified `duration` and optional
// `delay`, associating `observer` with the created animation sequences.
void AnimateIn(views::View* view,
               base::TimeDelta duration,
               base::TimeDelta delay,
               ui::LayerAnimationObserver* observer) {
  view->layer()->SetOpacity(0.f);
  AnimateTo(view, /*opacity=*/1.f, duration, delay, observer);
}

// Animates out the specified `view` with the specified `duration, associating
// `observer` with the created animation sequences.
void AnimateOut(views::View* view,
                base::TimeDelta duration,
                ui::LayerAnimationObserver* observer) {
  AnimateTo(view, /*opacity=*/0.f, duration, /*delay=*/base::TimeDelta(),
            observer);
}

void ApplyStyle(views::Label* label, LabelStyle style) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));

  switch (style) {
    case LabelStyle::kBadge:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::MEDIUM));
      break;
    case LabelStyle::kBody:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 14,
                                       gfx::Font::Weight::NORMAL));
      break;
    case LabelStyle::kChip:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 13,
                                       gfx::Font::Weight::NORMAL));
      break;
    case LabelStyle::kHeader:
      label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 16,
                                       gfx::Font::Weight::MEDIUM));
      break;
  }
}

std::unique_ptr<views::Label> CreateLabel(LabelStyle style,
                                          const std::u16string& text) {
  auto label = std::make_unique<LabelWithThemeChangedCallback>(
      text,
      /*theme_changed_callback=*/base::BindRepeating(
          [](LabelStyle style, views::Label* label) {
            ApplyStyle(label, style);
          },
          style));
  // Apply `style` to `label` manually in case the view is painted without ever
  // having being added to the view hierarchy. In such cases, the `label` will
  // not receive an `OnThemeChanged()` event. This occurs, for example, with
  // holding space drag images.
  ApplyStyle(label.get(), style);
  return label;
}

std::unique_ptr<views::Background> CreateCircleBackground(SkColor color,
                                                          size_t fixed_size) {
  return views::CreateBackgroundFromPainter(
      std::make_unique<CirclePainter>(color, fixed_size));
}

std::unique_ptr<views::Background> CreateCircleBackground(
    SkColor color,
    const gfx::InsetsF& insets) {
  return views::CreateBackgroundFromPainter(
      std::make_unique<CirclePainter>(color, insets));
}

}  // namespace holding_space_util
}  // namespace ash
