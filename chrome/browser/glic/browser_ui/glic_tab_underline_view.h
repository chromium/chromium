// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TAB_UNDERLINE_VIEW_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TAB_UNDERLINE_VIEW_H_

#include "base/scoped_observation.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/glic/browser_ui/glic_animated_effect_view.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

class Browser;
class Tab;

namespace gfx {
class Canvas;
}  // namespace gfx

namespace glic {

class GlicKeyedService;

class GlicTabUnderlineView : public GlicAnimatedEffectView {
  METADATA_HEADER(GlicTabUnderlineView, views::View)

 public:
  // Allows the test to inject the tester at the border's creation.
  class Factory {
   public:
    static std::unique_ptr<GlicTabUnderlineView> Create(Browser* browser,
                                                        Tab* tab);
    static void set_factory(Factory* factory) { factory_ = factory; }

   protected:
    Factory() = default;
    virtual ~Factory() = default;

    // For tests to override.
    virtual std::unique_ptr<GlicTabUnderlineView> CreateUnderlineView(
        Browser* browser,
        Tab* tab) = 0;

   private:
    static Factory* factory_;
  };

  GlicTabUnderlineView(const GlicTabUnderlineView&) = delete;
  GlicTabUnderlineView& operator=(const GlicTabUnderlineView&) = delete;
  ~GlicTabUnderlineView() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kGlicTabUnderlineElementId);

 protected:
  friend class Factory;
  explicit GlicTabUnderlineView(Browser* browser,
                                Tab* tab,
                                std::unique_ptr<Tester> tester);

 private:
  // `GlicAnimatedEffectView`:
  bool IsCycleDone(base::TimeTicks timestamp) override;
  base::TimeDelta GetTotalDuration() const override;
  void PopulateShaderUniforms(
      std::vector<cc::PaintShader::FloatUniform>& float_uniforms,
      std::vector<cc::PaintShader::Float2Uniform>& float2_uniforms,
      std::vector<cc::PaintShader::Float4Uniform>& float4_uniforms,
      std::vector<cc::PaintShader::IntUniform>& int_uniforms) const override;
  void DrawEffect(gfx::Canvas* canvas, const cc::PaintFlags& flags) override;

  int ComputeWidth();

  // A utility class that subscribes to `GlicKeyedService` for various browser
  // UI status changes that affect showing and animating of the tab underlines.
  class UnderlineViewUpdater;
  const std::unique_ptr<UnderlineViewUpdater> updater_;

  raw_ptr<Tab> tab_ = nullptr;
};

BEGIN_VIEW_BUILDER(, GlicTabUnderlineView, GlicAnimatedEffectView)
VIEW_BUILDER_PROPERTY(bool, Visible)
VIEW_BUILDER_PROPERTY(bool, CanProcessEventsWithinSubtree)
END_VIEW_BUILDER

}  // namespace glic

DEFINE_VIEW_BUILDER(, glic::GlicTabUnderlineView)

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TAB_UNDERLINE_VIEW_H_
