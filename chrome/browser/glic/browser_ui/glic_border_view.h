// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BORDER_VIEW_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BORDER_VIEW_H_

#include "base/scoped_observation.h"
#include "cc/paint/paint_shader.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

class Browser;
class ThemeService;

namespace gfx {
class Canvas;
}  // namespace gfx

namespace content {
class GpuDataManager;
}  // namespace content

namespace glic {

class GlicKeyedService;

class GlicBorderView : public views::View,
                       public ui::CompositorAnimationObserver,
                       public ui::CompositorObserver,
                       public content::GpuDataManagerObserver {
  METADATA_HEADER(GlicBorderView, views::View)

 public:
  // Note: We should avoid add test-only code in production as it is an
  // anti-pattern. There is a planned effort to remove these codes and migrate
  // to unittests + pixel tests. See https://crbug.com/412335211
  class Tester;
  // Allows the test to inject the tester at the border's creation.
  class Factory {
   public:
    static std::unique_ptr<GlicBorderView> Create(Browser* browser);
    static void set_factory(Factory* factory) { factory_ = factory; }

   protected:
    Factory() = default;
    virtual ~Factory() = default;

    // For tests to override.
    virtual std::unique_ptr<GlicBorderView> CreateBorderView(
        Browser* browser) = 0;

   private:
    static Factory* factory_;
  };

  GlicBorderView(const GlicBorderView&) = delete;
  GlicBorderView& operator=(const GlicBorderView&) = delete;
  ~GlicBorderView() override;

  // `views::View`:
  void OnPaint(gfx::Canvas* canvas) override;

  // `ui::CompositorAnimationObserver`:
  void OnAnimationStep(base::TimeTicks timestamp) override;

  // `ui::CompositorObserver`:
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // `content::GpuDataManagerObserver`:
  void OnGpuInfoUpdate() override;

  bool IsShowing() const;

  // TODO(crbug.com/384712084): Ideally we shouldn't expose these internals for
  // testing. The pixel comparison tests were flaky thus reverted. Remove these
  // once we set up the Skia Gold tests.
  float opacity_for_testing() const { return opacity_; }
  float emphasis_for_testing() const { return emphasis_; }
  float progress_for_testing() const { return progress_; }
  float GetEffectTimeForTesting() const;

  // Allows tests to alternate some animation APIs, for the deterministic
  // testing.
  class Tester {
   public:
    virtual ~Tester() = default;
    virtual base::TimeTicks GetTestTimestamp() = 0;
    virtual base::TimeTicks GetTestCreationTime() = 0;
    virtual void AnimationStarted() = 0;
    virtual void EmphasisRestarted() = 0;
    virtual void RampDownStarted() = 0;
  };
  Tester* tester() const { return tester_.get(); }

 protected:
  friend class Factory;
  explicit GlicBorderView(Browser* browser, std::unique_ptr<Tester> tester);

 private:
  void Show();
  void StopShowing();

  // A value from 0 to 1 indicating how much the border is to be emphasized.
  float GetEmphasis(base::TimeDelta delta) const;

  // Only valid to call after the animation has started.
  void ResetEmphasisAndReplay();

  // A value from 0 to 1 indicating the opacity of the border.
  float GetOpacity(base::TimeTicks timestamp);

  // Sets the necessary bits to start ramping down the opacity once it's called.
  void StartRampingDown();

  // Returns the effect evolution time; wraps after an hour.
  float GetEffectTime() const;

  // Returns a value from 0 to 1 indicating progress through the effect.
  float GetEffectProgress(base::TimeTicks timestamp) const;

  // Returns the timestamp when the instance was created (but permits being
  // adjusted by the Tester).
  base::TimeTicks GetCreationTime() const;

  bool ForceSimplifiedShader() const;

  GlicKeyedService* GetGlicService() const;

  // A utility class that subscribe to `GlicKeyedService` for various browser UI
  // status change.
  class BorderViewUpdater;
  const std::unique_ptr<BorderViewUpdater> updater_;

  std::string shader_;

  // When it is true, the class directly presents a static border and when it is
  // false, it animates the border first.
  bool skip_emphasis_animation_ = false;

  float opacity_ = 0.f;
  float emphasis_ = 0.f;
  float progress_ = 0.f;

  const base::TimeTicks creation_time_;
  base::TimeTicks first_frame_time_;
  base::TimeTicks first_emphasis_frame_;
  base::TimeTicks last_emphasis_frame_;
  base::TimeTicks last_animation_step_time_;
  base::TimeDelta total_steady_time_;

  bool record_first_ramp_down_frame_ = false;
  base::TimeTicks first_ramp_down_frame_;
  // See crbug.com/407106595: Allows the border animation to play seamlessly
  // when the browser UI has lost focus temporarily.
  // TODO(crbug.com/408210785): Add a test for this case.
  float ramp_down_opacity_ = 0.f;

  bool has_hardware_acceleration_ = false;
  base::ScopedObservation<content::GpuDataManager,
                          content::GpuDataManagerObserver>
      gpu_data_manager_observer_{this};

  // Empty in production environment.
  const std::unique_ptr<Tester> tester_;

  sk_sp<cc::PaintShader> cached_paint_shader_;

  raw_ptr<ui::Compositor> compositor_ = nullptr;
  raw_ptr<ThemeService> theme_service_ = nullptr;
  raw_ptr<Browser> browser_ = nullptr;
};

BEGIN_VIEW_BUILDER(, GlicBorderView, views::View)
VIEW_BUILDER_PROPERTY(bool, Visible)
VIEW_BUILDER_PROPERTY(bool, CanProcessEventsWithinSubtree)
END_VIEW_BUILDER

}  // namespace glic

DEFINE_VIEW_BUILDER(, glic::GlicBorderView)

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BORDER_VIEW_H_
