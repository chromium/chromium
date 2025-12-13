// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_CONTEXT_SHARING_BORDER_VIEW_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_CONTEXT_SHARING_BORDER_VIEW_H_

#include "chrome/browser/glic/browser_ui/animated_effect_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/metadata/view_factory.h"

class Browser;
class ContentsWebView;

namespace gfx {
class Canvas;
}  // namespace gfx

namespace glic {

class ContextSharingBorderViewController;

class ContextSharingBorderView : public AnimatedEffectView {
  METADATA_HEADER(ContextSharingBorderView, views::View)

 public:
  // Allows the test to inject the tester at the border's creation.
  class Factory {
   public:
    static std::unique_ptr<ContextSharingBorderView> Create(
        std::unique_ptr<ContextSharingBorderViewController> controller,
        Browser*,
        ContentsWebView*);
    static void set_factory(Factory* factory) { factory_ = factory; }

   protected:
    Factory() = default;
    virtual ~Factory() = default;

    // For tests to override.
    virtual std::unique_ptr<ContextSharingBorderView> CreateBorderView(
        std::unique_ptr<ContextSharingBorderViewController> controller,
        Browser* browser,
        ContentsWebView* contents_web_view) = 0;

   private:
    static Factory* factory_;
  };

  ContextSharingBorderView(const ContextSharingBorderView&) = delete;
  ContextSharingBorderView& operator=(const ContextSharingBorderView&) = delete;
  ~ContextSharingBorderView() override;

  void SetRoundedCorners(const gfx::RoundedCornersF& radii);

  float emphasis_for_testing() const { return emphasis_; }

 protected:
  friend class Factory;
  explicit ContextSharingBorderView(
      std::unique_ptr<ContextSharingBorderViewController> controller,
      Browser* browser,
      ContentsWebView* contents_web_view,
      std::unique_ptr<Tester> tester);

 private:
  // `AnimatedEffectView`:
  bool IsCycleDone(base::TimeTicks timestamp) override;
  base::TimeDelta GetTotalDuration() const override;
  void PopulateShaderUniforms(
      std::vector<cc::PaintShader::FloatUniform>& float_uniforms,
      std::vector<cc::PaintShader::Float2Uniform>& float2_uniforms,
      std::vector<cc::PaintShader::Float4Uniform>& float4_uniforms,
      std::vector<cc::PaintShader::IntUniform>& int_uniforms) const override;
  void DrawEffect(gfx::Canvas* canvas, const cc::PaintFlags& flags) override;

  void DrawSimplifiedEffect(gfx::Canvas* canvas) const;

  // A value from 0 to 1 indicating how much the border is to be emphasized.
  float GetEmphasis(base::TimeDelta delta) const;

  // Returns the rounded corner radius to use for the border.
  gfx::RoundedCornersF GetContentBorderRadius() const;

  // The controller to notify the view about various browser UI status change.
  const std::unique_ptr<ContextSharingBorderViewController> controller_;
};

BEGIN_VIEW_BUILDER(, ContextSharingBorderView, AnimatedEffectView)
VIEW_BUILDER_PROPERTY(bool, Visible)
VIEW_BUILDER_PROPERTY(bool, CanProcessEventsWithinSubtree)
END_VIEW_BUILDER

}  // namespace glic

DEFINE_VIEW_BUILDER(, glic::ContextSharingBorderView)

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_CONTEXT_SHARING_BORDER_VIEW_H_
