// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BORDER_VIEW_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BORDER_VIEW_H_

#include "chrome/browser/glic/browser_ui/glic_animated_effect_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/metadata/view_factory.h"

class Browser;
class ContentsWebView;

namespace gfx {
class Canvas;
}  // namespace gfx

namespace glic {

class GlicKeyedService;

class GlicBorderView : public GlicAnimatedEffectView {
  METADATA_HEADER(GlicBorderView, views::View)

 public:
  // Allows the test to inject the tester at the border's creation.
  class Factory {
   public:
    static std::unique_ptr<GlicBorderView> Create(Browser*, ContentsWebView*);
    static void set_factory(Factory* factory) { factory_ = factory; }

   protected:
    Factory() = default;
    virtual ~Factory() = default;

    // For tests to override.
    virtual std::unique_ptr<GlicBorderView> CreateBorderView(
        Browser* browser,
        ContentsWebView* contents_web_view) = 0;

   private:
    static Factory* factory_;
  };

  GlicBorderView(const GlicBorderView&) = delete;
  GlicBorderView& operator=(const GlicBorderView&) = delete;
  ~GlicBorderView() override;

  void SetRoundedCorners(const gfx::RoundedCornersF& radii);

  float emphasis_for_testing() const { return emphasis_; }

 protected:
  friend class Factory;
  explicit GlicBorderView(Browser* browser,
                          ContentsWebView* contents_web_view,
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

  // A value from 0 to 1 indicating how much the border is to be emphasized.
  float GetEmphasis(base::TimeDelta delta) const;

  // Returns the rounded corner radius to use for the border.
  gfx::RoundedCornersF GetContentBorderRadius() const;

  // A utility class that subscribe to `GlicKeyedService` for various browser UI
  // status change.
  class BorderViewUpdater;
  const std::unique_ptr<BorderViewUpdater> updater_;
};

BEGIN_VIEW_BUILDER(, GlicBorderView, GlicAnimatedEffectView)
VIEW_BUILDER_PROPERTY(bool, Visible)
VIEW_BUILDER_PROPERTY(bool, CanProcessEventsWithinSubtree)
END_VIEW_BUILDER

}  // namespace glic

DEFINE_VIEW_BUILDER(, glic::GlicBorderView)

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_BORDER_VIEW_H_
