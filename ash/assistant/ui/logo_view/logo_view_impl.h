// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_LOGO_VIEW_LOGO_VIEW_IMPL_H_
#define ASH_ASSISTANT_UI_LOGO_VIEW_LOGO_VIEW_IMPL_H_

#include <cstdint>

#include "ash/assistant/ui/logo_view/logo_view.h"
#include "ash/assistant/ui/logo_view/shape/mic_part_shape.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/assistant/internal/logo_view/input_value_provider/sound_level_input_value_provider.h"
#include "chromeos/assistant/internal/logo_view/logo_model/logo.h"
#include "chromeos/assistant/internal/logo_view/state_animator.h"
#include "chromeos/assistant/internal/logo_view/state_animator_timer_delegate.h"
#include "chromeos/assistant/internal/logo_view/state_model.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/compositor_animation_observer.h"

namespace chromeos {
namespace assistant {
class Dot;
}  // namespace assistant
}  // namespace chromeos

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ui {
class Compositor;
}

namespace views {
class View;
}

namespace ash {

class Shape;

// Displays the GLIF (Google Logo and Identity Family). It displays the dots and
// the google logo. It does not display the Super G.
class LogoViewImpl : public LogoView,
                     public chromeos::assistant::StateAnimatorTimerDelegate,
                     public ui::CompositorAnimationObserver {
  METADATA_HEADER(LogoViewImpl, LogoView)

 public:
  using Dot = chromeos::assistant::Dot;
  using Logo = chromeos::assistant::Logo;
  using StateAnimator = chromeos::assistant::StateAnimator;
  using StateModel = chromeos::assistant::StateModel;

  LogoViewImpl();
  LogoViewImpl(const LogoViewImpl&) = delete;
  LogoViewImpl& operator=(const LogoViewImpl&) = delete;
  ~LogoViewImpl() override;

  void SetState(State state, bool animate) override;
  void SetSpeechLevel(float speech_level) override;

  // chromeos::assistant::StateAnimatorTimerDelegate:
  int64_t StartTimer() override;
  void StopTimer() override;

 private:
  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  void DrawDots(gfx::Canvas* canvas);
  void DrawDot(gfx::Canvas* canvas, Dot* dot);
  void DrawMicPart(gfx::Canvas* canvas, Dot* dot, float x, float y);
  void DrawShape(gfx::Canvas* canvas, Shape* shape, SkColor color);
  void DrawLine(gfx::Canvas* canvas, Dot* dot, float x, float y);
  void DrawCircle(gfx::Canvas* canvas, Dot* dot, float x, float y);

  // TODO(b/79579731): Implement the letter animation.
  void DrawLetter(gfx::Canvas* canvas, Dot* dot, float x, float y) {}

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  Logo logo_;

  MicPartShape mic_part_shape_;

  StateModel state_model_;

  StateAnimator state_animator_;

  raw_ptr<ui::Compositor> animating_compositor_ = nullptr;

  float dots_scale_ = 1.0f;

  chromeos::assistant::SoundLevelInputValueProvider
      sound_level_input_value_provider_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_LOGO_VIEW_LOGO_VIEW_IMPL_H_
