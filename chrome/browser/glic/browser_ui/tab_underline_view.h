// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_TAB_UNDERLINE_VIEW_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_TAB_UNDERLINE_VIEW_H_

#include "base/scoped_observation.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/glic/browser_ui/animated_effect_view.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

namespace gfx {
class Canvas;
}  // namespace gfx

namespace glic {

class TabUnderlineViewController;

class TabUnderlineView : public AnimatedEffectView {
  METADATA_HEADER(TabUnderlineView, views::View)

 public:
  // The height of the underline effect. Also used for the padding outside the
  // underline.
  static constexpr int kEffectThickness = 2;

  // The radius to use for rounded corners of the underline effect.
  static constexpr float kCornerRadius = kEffectThickness / 2.0f;

  // Allows the test to inject the tester at the border's creation.
  class Factory {
   public:
    static std::unique_ptr<TabUnderlineView> Create(
        std::unique_ptr<TabUnderlineViewController> controller,
        BrowserWindowInterface* browser_window_interface,
        tabs::TabHandle tab_handle);
    static void set_factory(Factory* factory) { factory_ = factory; }

   protected:
    Factory() = default;
    virtual ~Factory() = default;

    // For tests to override.
    virtual std::unique_ptr<TabUnderlineView> CreateUnderlineView(
        std::unique_ptr<TabUnderlineViewController> controller,
        BrowserWindowInterface* browser_window_interface,
        tabs::TabHandle tab) = 0;

   private:
    static Factory* factory_;
  };

  TabUnderlineView(const TabUnderlineView&) = delete;
  TabUnderlineView& operator=(const TabUnderlineView&) = delete;
  ~TabUnderlineView() override;

  // Returns the TabInterface corresponding to `underline_view_`, if it is
  // valid.
  tabs::TabInterface* GetTabInterface();

  enum class Orientation {
    kHorizontal,
    kVertical,
  };

  void SetOrientation(Orientation orientation);

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kGlicTabUnderlineElementId);

 protected:
  friend class Factory;
  explicit TabUnderlineView(
      std::unique_ptr<TabUnderlineViewController> controller,
      BrowserWindowInterface* browser_window_interface,
      tabs::TabHandle tab_handle,
      std::unique_ptr<Tester> tester);

 private:
  // `AnimatedEffectView`:
  bool IsCycleDone(base::TimeTicks timestamp) override;
  base::TimeDelta GetTotalDuration() const override;
  std::vector<SkColor> GetEffectColors() override;
  void PopulateShaderUniforms(
      std::vector<cc::PaintShader::FloatUniform>& float_uniforms,
      std::vector<cc::PaintShader::Float2Uniform>& float2_uniforms,
      std::vector<cc::PaintShader::Float4Uniform>& float4_uniforms,
      std::vector<cc::PaintShader::IntUniform>& int_uniforms) const override;
  void DrawEffect(gfx::Canvas* canvas, const cc::PaintFlags& flags) override;

  // `views::View`:
  void OnThemeChanged() override;
  void AddedToWidget() override;

  int ComputeDimension();

  void OnActiveTabChanged(BrowserWindowInterface* browser_window_interface);

  // The controller responsible for notifying the view about various browser
  // UI status changes that affect showing and animating of the tab underlines.
  const std::unique_ptr<TabUnderlineViewController> controller_;

  tabs::TabHandle tab_handle_;

  Orientation orientation_ = Orientation::kHorizontal;

  base::CallbackListSubscription active_tab_subscription_;
};

BEGIN_VIEW_BUILDER(, TabUnderlineView, AnimatedEffectView)
VIEW_BUILDER_PROPERTY(bool, Visible)
VIEW_BUILDER_PROPERTY(bool, CanProcessEventsWithinSubtree)
END_VIEW_BUILDER

}  // namespace glic

DEFINE_VIEW_BUILDER(, glic::TabUnderlineView)

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_TAB_UNDERLINE_VIEW_H_
