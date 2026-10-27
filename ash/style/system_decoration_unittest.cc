// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_decoration.h"

#include <memory>

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/window.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Different types of SystemDecoration implementations.
enum class SystemDecorationType {
  kDecoration,        // Instance of standard `SystemDecoration`.
  kViewDecoration,    // Instance of `SystemViewDecoration`.
  kWindowDecoration,  // Instance of `SystemWindowDecoration`.
};

// Gets the key and ambient shadow colors from a shadow.
std::pair<SkColor, SkColor> GetShadowColors(SystemDecoration* shadow) {
  gfx::ShadowValues values = shadow->GetShadowValuesForTesting();
  return std::make_pair(values[0].color(), values[1].color());
}

// Add a shadow to the bottom of the native window of a widget and make the
// shadow observe the theme change of the widget.
void AddShadowToWidget(SystemDecoration* shadow, views::Widget* widget) {
  auto* window_layer = widget->GetNativeWindow()->layer();
  auto* shadow_layer = shadow->GetLayer();
  window_layer->Add(shadow_layer);
  window_layer->StackAtBottom(shadow_layer);
  shadow->SetContentBounds(gfx::Rect(window_layer->bounds().size()));
  shadow->ObserveColorProviderSource(widget);
}

}  // namespace

// The parameterized test class for dynamic system shadow and highlight border
// colors.
class SystemDecorationColorTest
    : public AshTestBase,
      public testing::WithParamInterface<SystemDecorationType> {
 public:
  SystemDecorationColorTest() = default;
  SystemDecorationColorTest(const SystemDecorationColorTest&) = delete;
  SystemDecorationColorTest& operator=(const SystemDecorationColorTest&) =
      delete;
  ~SystemDecorationColorTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    // Create a test widget as the owner of the shadow instances.
    widget_ = CreateTestWidget(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        /*delegate=*/nullptr,
        /*container_id=*/desks_util::GetActiveDeskContainerId(),
        /*bounds=*/gfx::Rect(100, 100, 320, 200));
  }

 protected:
  // Gets the dynamic color resolved by the color provider according to the
  // theme of the `widget_`.
  SkColor GetColor(ui::ColorId color_id) {
    return widget_->GetColorProvider()->GetColor(color_id);
  }

  // Creates a certain type of SystemDecoration instance according to the test
  // parameter.
  std::unique_ptr<SystemDecoration> MakeDecorationAccordingToParam(
      SystemDecoration::ShadowType shadow_type,
      SystemDecoration::HighlightBorderMode border_mode =
          SystemDecoration::HighlightBorderMode::kNone) {
    switch (GetParam()) {
      case SystemDecorationType::kDecoration: {
        auto decoration = SystemDecoration::Create(shadow_type, border_mode);
        AddShadowToWidget(decoration.get(), widget_.get());
        return decoration;
      }
      case SystemDecorationType::kViewDecoration:
        return SystemDecoration::CreateForView(widget_->GetContentsView(),
                                               shadow_type, border_mode);
      case SystemDecorationType::kWindowDecoration:
        return SystemDecoration::CreateForWindow(widget_->GetNativeWindow(),
                                                 shadow_type, border_mode);
    }
  }

 private:
  // The test widget used as a shadow owner.
  std::unique_ptr<views::Widget> widget_;
};

INSTANTIATE_TEST_SUITE_P(
    MaterialNext,
    SystemDecorationColorTest,
    testing::Values(SystemDecorationType::kDecoration,
                    SystemDecorationType::kViewDecoration,
                    SystemDecorationType::kWindowDecoration),
    [](const testing::TestParamInfo<SystemDecorationColorTest::ParamType>&
           info) {
      switch (info.param) {
        case SystemDecorationType::kDecoration:
          return "Decoration";
        case SystemDecorationType::kViewDecoration:
          return "ViewDecoration";
        case SystemDecorationType::kWindowDecoration:
          return "WindowDecoration";
      }
    });

// Tests if the colors of system shadow change with shadow types.
TEST_P(SystemDecorationColorTest, UpdateShadowColorsWithType) {
  auto shadow =
      MakeDecorationAccordingToParam(SystemDecoration::ShadowType::kElevation4);
  EXPECT_EQ(GetShadowColors(shadow.get()),
            std::make_pair(
                GetColor(ui::kColorShadowValueKeyShadowElevationFour),
                GetColor(ui::kColorShadowValueAmbientShadowElevationFour)));

  // Change type to kElevation12.
  shadow->SetShadow(SystemDecoration::ShadowType::kElevation12);
  EXPECT_EQ(GetShadowColors(shadow.get()),
            std::make_pair(
                GetColor(ui::kColorShadowValueKeyShadowElevationTwelve),
                GetColor(ui::kColorShadowValueAmbientShadowElevationTwelve)));

  // Change type to kElevation24;
  shadow->SetShadow(SystemDecoration::ShadowType::kElevation24);
  EXPECT_EQ(
      GetShadowColors(shadow.get()),
      std::make_pair(
          GetColor(ui::kColorShadowValueKeyShadowElevationTwentyFour),
          GetColor(ui::kColorShadowValueAmbientShadowElevationTwentyFour)));
}

// Tests if the colors of system shadow change with the themes.
TEST_P(SystemDecorationColorTest, UpdateShadowColorsWithTheme) {
  auto shadow =
      MakeDecorationAccordingToParam(SystemDecoration::ShadowType::kElevation4);

  // Set light mode.
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
  EXPECT_EQ(GetShadowColors(shadow.get()),
            std::make_pair(
                GetColor(ui::kColorShadowValueKeyShadowElevationFour),
                GetColor(ui::kColorShadowValueAmbientShadowElevationFour)));

  // Set dark mode.
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);
  EXPECT_EQ(GetShadowColors(shadow.get()),
            std::make_pair(
                GetColor(ui::kColorShadowValueKeyShadowElevationFour),
                GetColor(ui::kColorShadowValueAmbientShadowElevationFour)));
}

// Tests highlight border configuration and dynamic color updates across types
// and themes.
TEST_P(SystemDecorationColorTest, HighlightBorderAndShadow) {
  auto decoration =
      MakeDecorationAccordingToParam(SystemDecoration::ShadowType::kElevation4);
  EXPECT_TRUE(decoration->HasShadow());
  EXPECT_FALSE(decoration->HasHighlightBorder());
  EXPECT_FALSE(decoration->GetHighlightBorderValueForTesting().has_value());

  decoration->SetHighlightBorder(
      SystemDecoration::HighlightBorderMode::kHighlightBorder);
  EXPECT_TRUE(decoration->HasHighlightBorder());

  DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
  auto border_value = decoration->GetHighlightBorderValueForTesting();
  ASSERT_TRUE(border_value.has_value());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlight),
            border_value->highlight_color());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlightBorder1),
            border_value->border_color());

  // Switch theme to dark mode and verify highlight border colors update.
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);
  border_value = decoration->GetHighlightBorderValueForTesting();
  ASSERT_TRUE(border_value.has_value());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlight),
            border_value->highlight_color());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlightBorder1),
            border_value->border_color());

  // Clear shadow: the highlight border automatically switches to
  // kHighlightBorderNoShadow.
  decoration->SetShadow(SystemDecoration::ShadowType::kNone);
  EXPECT_FALSE(decoration->HasShadow());
  border_value = decoration->GetHighlightBorderValueForTesting();
  ASSERT_TRUE(border_value.has_value());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlight),
            border_value->highlight_color());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlightBorder),
            border_value->border_color());

  // Restore shadow: the highlight border automatically switches back to
  // kHighlightBorderOnShadow.
  decoration->SetShadow(SystemDecoration::ShadowType::kElevation4);
  EXPECT_TRUE(decoration->HasShadow());
  border_value = decoration->GetHighlightBorderValueForTesting();
  ASSERT_TRUE(border_value.has_value());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlight),
            border_value->highlight_color());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlightBorder1),
            border_value->border_color());

  decoration->SetHighlightBorder(SystemDecoration::HighlightBorderMode::kNone);
  EXPECT_FALSE(decoration->HasHighlightBorder());
  EXPECT_FALSE(decoration->GetHighlightBorderValueForTesting().has_value());
}

// Tests creating a highlight-border-only decoration via the factory methods.
TEST_P(SystemDecorationColorTest, CreateHighlightBorder) {
  auto decoration = MakeDecorationAccordingToParam(
      SystemDecoration::ShadowType::kNone,
      SystemDecoration::HighlightBorderMode::kHighlightBorder);
  EXPECT_FALSE(decoration->HasShadow());
  EXPECT_TRUE(decoration->HasHighlightBorder());

  DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
  auto border_value = decoration->GetHighlightBorderValueForTesting();
  ASSERT_TRUE(border_value.has_value());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlight),
            border_value->highlight_color());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlightBorder),
            border_value->border_color());

  DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);
  border_value = decoration->GetHighlightBorderValueForTesting();
  ASSERT_TRUE(border_value.has_value());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlight),
            border_value->highlight_color());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlightBorder),
            border_value->border_color());
}

// Tests creating a combined shadow and highlight border decoration via the
// factory methods.
TEST_P(SystemDecorationColorTest, CreateShadowWithHighlightBorder) {
  auto decoration = MakeDecorationAccordingToParam(
      SystemDecoration::ShadowType::kElevation12,
      SystemDecoration::HighlightBorderMode::kHighlightBorder);
  EXPECT_TRUE(decoration->HasShadow());
  EXPECT_TRUE(decoration->HasHighlightBorder());

  DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
  EXPECT_EQ(GetShadowColors(decoration.get()),
            std::make_pair(
                GetColor(ui::kColorShadowValueKeyShadowElevationTwelve),
                GetColor(ui::kColorShadowValueAmbientShadowElevationTwelve)));
  auto border_value = decoration->GetHighlightBorderValueForTesting();
  ASSERT_TRUE(border_value.has_value());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlight),
            border_value->highlight_color());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlightBorder1),
            border_value->border_color());

  DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);
  EXPECT_EQ(GetShadowColors(decoration.get()),
            std::make_pair(
                GetColor(ui::kColorShadowValueKeyShadowElevationTwelve),
                GetColor(ui::kColorShadowValueAmbientShadowElevationTwelve)));
  border_value = decoration->GetHighlightBorderValueForTesting();
  ASSERT_TRUE(border_value.has_value());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlight),
            border_value->highlight_color());
  EXPECT_EQ(GetColor(ui::kColorCrosSystemHighlightBorder1),
            border_value->border_color());
}

using SystemDecorationTest = AshTestBase;

// Tests that SystemWindowDecoration layers are stacked at the top when a
// highlight border is present and at the bottom when borderless.
TEST_F(SystemDecorationTest, WindowDecorationLayerStacking) {
  auto widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       /*delegate=*/nullptr,
                       /*container_id=*/desks_util::GetActiveDeskContainerId(),
                       /*bounds=*/gfx::Rect(100, 100, 320, 200));
  aura::Window* window = widget->GetNativeWindow();

  auto decoration = SystemDecoration::CreateForWindow(
      window, SystemDecoration::ShadowType::kElevation4);
  ui::Layer* decoration_layer = decoration->GetLayer();

  // Initially, shadow-only decoration layer is stacked at the bottom.
  EXPECT_EQ(window->layer()->children().front(), decoration_layer);
  EXPECT_NE(window->layer()->children().back(), decoration_layer);

  // After setting highlight border, decoration layer is stacked at the top.
  decoration->SetHighlightBorder(
      SystemDecoration::HighlightBorderMode::kHighlightBorder);
  EXPECT_EQ(window->layer()->children().back(), decoration_layer);
  EXPECT_NE(window->layer()->children().front(), decoration_layer);

  // After clearing highlight border, decoration layer returns to the bottom.
  decoration->SetHighlightBorder(SystemDecoration::HighlightBorderMode::kNone);
  EXPECT_EQ(window->layer()->children().front(), decoration_layer);
  EXPECT_NE(window->layer()->children().back(), decoration_layer);

  // Creating directly with a highlight border stacks the layer at the top.
  decoration = SystemDecoration::CreateForWindow(
      window, SystemDecoration::ShadowType::kElevation4,
      SystemDecoration::HighlightBorderMode::kHighlightBorder);
  EXPECT_EQ(window->layer()->children().back(), decoration->GetLayer());
  EXPECT_NE(window->layer()->children().front(), decoration->GetLayer());
}

// Tests that SystemViewDecoration layers are placed in LayerRegion::kAbove when
// a highlight border is present and LayerRegion::kBelow when borderless.
TEST_F(SystemDecorationTest, ViewDecorationLayerStacking) {
  auto widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       /*delegate=*/nullptr,
                       /*container_id=*/desks_util::GetActiveDeskContainerId(),
                       /*bounds=*/gfx::Rect(100, 100, 320, 200));
  views::View* contents_view = widget->GetContentsView();
  contents_view->SetPaintToLayer();

  auto decoration = SystemDecoration::CreateForView(
      contents_view, SystemDecoration::ShadowType::kElevation4);
  ui::Layer* decoration_layer = decoration->GetLayer();
  ui::Layer* view_layer = contents_view->layer();

  // Initially, shadow-only decoration layer is in LayerRegion::kBelow.
  std::vector<ui::Layer*> layers = contents_view->GetLayersInOrder();
  ASSERT_EQ(layers.size(), 2u);
  EXPECT_EQ(layers[0], decoration_layer);
  EXPECT_EQ(layers[1], view_layer);

  // After setting highlight border, decoration layer moves to
  // LayerRegion::kAbove.
  decoration->SetHighlightBorder(
      SystemDecoration::HighlightBorderMode::kHighlightBorder);
  layers = contents_view->GetLayersInOrder();
  ASSERT_EQ(layers.size(), 2u);
  EXPECT_EQ(layers[0], view_layer);
  EXPECT_EQ(layers[1], decoration_layer);

  // After clearing highlight border, decoration layer returns to
  // LayerRegion::kBelow.
  decoration->SetHighlightBorder(SystemDecoration::HighlightBorderMode::kNone);
  layers = contents_view->GetLayersInOrder();
  ASSERT_EQ(layers.size(), 2u);
  EXPECT_EQ(layers[0], decoration_layer);
  EXPECT_EQ(layers[1], view_layer);

  // Creating directly with a highlight border places the layer in
  // LayerRegion::kAbove.
  decoration = SystemDecoration::CreateForView(
      contents_view, SystemDecoration::ShadowType::kElevation4,
      SystemDecoration::HighlightBorderMode::kHighlightBorder);
  layers = contents_view->GetLayersInOrder();
  ASSERT_EQ(layers.size(), 2u);
  EXPECT_EQ(layers[0], view_layer);
  EXPECT_EQ(layers[1], decoration->GetLayer());
}

// Tests that creating a decoration without a color provider source defers
// highlight border creation until a color provider source is observed.
TEST_F(SystemDecorationTest, DeferHighlightBorderUntilColorProviderObserved) {
  auto widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       /*delegate=*/nullptr,
                       /*container_id=*/desks_util::GetActiveDeskContainerId(),
                       /*bounds=*/gfx::Rect(100, 100, 320, 200));

  auto decoration = SystemDecoration::Create(
      SystemDecoration::ShadowType::kElevation4,
      SystemDecoration::HighlightBorderMode::kHighlightBorder);
  decoration->SetContentBounds(gfx::Rect(100, 100));
  EXPECT_TRUE(decoration->HasShadow());
  EXPECT_TRUE(decoration->HasHighlightBorder());
  // The border is not created yet because there is no color provider source.
  EXPECT_FALSE(decoration->GetHighlightBorderValueForTesting().has_value());

  // Observe the widget's color provider source.
  decoration->ObserveColorProviderSource(widget.get());
  auto border_value = decoration->GetHighlightBorderValueForTesting();
  ASSERT_TRUE(border_value.has_value());
  EXPECT_EQ(widget->GetColorProvider()->GetColor(ui::kColorCrosSystemHighlight),
            border_value->highlight_color());
  EXPECT_EQ(widget->GetColorProvider()->GetColor(
                ui::kColorCrosSystemHighlightBorder1),
            border_value->border_color());
}

// Tests that destroying a SystemViewDecoration removes its layer from the
// view's layer regions.
TEST_F(SystemDecorationTest, ViewDecorationDestruction) {
  auto widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                       /*delegate=*/nullptr,
                       /*container_id=*/desks_util::GetActiveDeskContainerId(),
                       /*bounds=*/gfx::Rect(100, 100, 320, 200));
  views::View* contents_view = widget->GetContentsView();
  contents_view->SetPaintToLayer();

  auto decoration = SystemDecoration::CreateForView(
      contents_view, SystemDecoration::ShadowType::kElevation4);
  EXPECT_EQ(contents_view->GetLayersInOrder().size(), 2u);

  decoration.reset();
  EXPECT_EQ(contents_view->GetLayersInOrder().size(), 1u);
  EXPECT_EQ(contents_view->GetLayersInOrder()[0], contents_view->layer());
}

// Tests that destroying the view before the decoration detaches the
// decoration's layer and does not crash on subsequent decoration destruction.
TEST_F(SystemDecorationTest, ViewDecorationViewDeletion) {
  auto widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                       /*delegate=*/nullptr,
                       /*container_id=*/desks_util::GetActiveDeskContainerId(),
                       /*bounds=*/gfx::Rect(100, 100, 320, 200));
  views::View* contents_view = widget->GetContentsView();
  auto* child_view =
      contents_view->AddChildView(std::make_unique<views::View>());
  child_view->SetPaintToLayer();

  auto decoration = SystemDecoration::CreateForView(
      child_view, SystemDecoration::ShadowType::kElevation4);
  EXPECT_EQ(child_view->GetLayersInOrder().size(), 2u);

  // Deleting child_view invokes OnViewIsDeleting() on the decoration.
  contents_view->RemoveChildViewT(child_view);
  EXPECT_EQ(decoration->GetLayer()->parent(), nullptr);

  // Resetting the decoration afterwards should be safe and clean.
  decoration.reset();
}

}  // namespace ash
