// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_ANIMATED_EFFECT_VIEW_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_ANIMATED_EFFECT_VIEW_H_

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "cc/paint/paint_shader.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

class Browser;
class ThemeService;

namespace content {
class GpuDataManager;
}  // namespace content

namespace glic {

class AnimatedEffectView : public views::View,
                           public ui::CompositorAnimationObserver,
                           public ui::CompositorObserver,
                           public content::GpuDataManagerObserver {
  METADATA_HEADER(AnimatedEffectView, views::View)

 public:
  AnimatedEffectView(const AnimatedEffectView&) = delete;
  AnimatedEffectView& operator=(const AnimatedEffectView&) = delete;
  ~AnimatedEffectView() override;

  // `views::View`:
  void OnPaint(gfx::Canvas* canvas) override;

  // `ui::CompositorAnimationObserver`:
  void OnAnimationStep(base::TimeTicks timestamp) override;

  // `ui::CompositorObserver`:
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // `content::GpuDataManagerObserver`:
  void OnGpuInfoUpdate() override;

  bool IsShowing() const;

  // Called to start showing the view.
  void Show();

  // Called to stop showing the view.
  void StopShowing();

  // Only valid to call after the animation has started.
  void ResetAnimationCycle();

  // Sets the necessary bits to start ramping down the opacity once it's called.
  void StartRampingDown();

  // Note: We should avoid adding test-only code in production as it is an
  // anti-pattern. There is a planned effort to remove this code and migrate
  // to unittests + pixel tests. See https://crbug.com/412335211
  float opacity_for_testing() const { return opacity_; }
  float progress_for_testing() const { return progress_; }
  float GetEffectTimeForTesting() const;

  class Tester {
   public:
    virtual ~Tester() = default;
    virtual base::TimeTicks GetTestTimestamp() = 0;
    virtual base::TimeTicks GetTestCreationTime() = 0;
    virtual void AnimationStarted() = 0;
    virtual void AnimationReset() = 0;
    virtual void RampDownStarted() = 0;
  };
  Tester* tester() const { return tester_.get(); }

 protected:
  AnimatedEffectView(Browser* browser, std::unique_ptr<Tester> tester);

  // Returns whether the current effect's animation has completed.
  virtual bool IsCycleDone(base::TimeTicks timestamp) = 0;

  // Sets the shader uniforms for the given effect.
  virtual void PopulateShaderUniforms(
      std::vector<cc::PaintShader::FloatUniform>& float_uniforms,
      std::vector<cc::PaintShader::Float2Uniform>& float2_uniforms,
      std::vector<cc::PaintShader::Float4Uniform>& float4_uniforms,
      std::vector<cc::PaintShader::IntUniform>& int_uniforms) const = 0;

  // Computes the bounds for the effect and draws to `canvas`.
  virtual void DrawEffect(gfx::Canvas* canvas, const cc::PaintFlags& flags) = 0;

  // A value from 0 to 1 indicating the opacity of the effect.
  float GetOpacity(base::TimeTicks timestamp);

  // Returns the effect evolution time; wraps after an hour.
  float GetEffectTime() const;

  // Returns a value from 0 to 1 indicating progress through the effect.
  float GetEffectProgress(base::TimeTicks timestamp) const;
  // new helper
  virtual base::TimeDelta GetTotalDuration() const = 0;

  // Returns the timestamp when the instance was created (but permits being
  // adjusted by the Tester).
  base::TimeTicks GetCreationTime() const;

  bool ForceSimplifiedShader() const;

  void UpdateShader();

  void SetDefaultColors(cc::PaintFlags& paint_flags,
                        const gfx::RectF& bounds) const;

  raw_ptr<Browser> browser_ = nullptr;

  std::string shader_;

  // When it is true, the class directly presents a static visual component and
  // when it is false, it animates the effect first.
  // TODO(crbug.com/433136761): Implement a simplified underline with clearer
  // difference in motion.
  bool skip_animation_cycle_ = false;

  float opacity_ = 0.f;
  float progress_ = 0.f;
  // Note: not relevant to calculations for the tab underline implementation of
  // the effect.
  float emphasis_ = 0.f;

  gfx::RoundedCornersF corner_radius_;

  const base::TimeTicks creation_time_;
  base::TimeTicks first_frame_time_;
  base::TimeTicks first_cycle_frame_;
  base::TimeTicks last_cycle_frame_;
  base::TimeTicks last_animation_step_time_;
  base::TimeDelta total_steady_time_;

  bool record_first_ramp_down_frame_ = false;
  base::TimeTicks first_ramp_down_frame_;
  // See crbug.com/407106595: Allows the effect animation to play seamlessly
  // when the browser UI has lost focus temporarily.
  // TODO(crbug.com/408210785): Add a test for this case.
  float ramp_down_opacity_ = 0.f;

  bool has_hardware_acceleration_ = false;
  base::ScopedObservation<content::GpuDataManager,
                          content::GpuDataManagerObserver>
      gpu_data_manager_observer_{this};

  base::ScopedObservation<ui::Compositor, ui::CompositorObserver>
      compositor_observation_{this};
  base::ScopedObservation<ui::Compositor, ui::CompositorAnimationObserver>
      compositor_animation_observation_{this};

  // Empty in production environment.
  const std::unique_ptr<Tester> tester_;

  sk_sp<cc::PaintShader> cached_paint_shader_;

  const std::vector<SkColor> colors_;
  const std::vector<float> floats_;

  raw_ptr<ui::Compositor> compositor_ = nullptr;
  raw_ptr<ThemeService> theme_service_ = nullptr;
};

BEGIN_VIEW_BUILDER(, AnimatedEffectView, views::View)
END_VIEW_BUILDER

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_ANIMATED_EFFECT_VIEW_H_
