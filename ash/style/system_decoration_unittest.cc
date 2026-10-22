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
  kShadowOnNinePatchLayer,      // Instance of standard `SystemDecorationImpl`.
  kViewShadowOnNinePatchLayer,  // Instance of `SystemViewDecoration`.
  kWindowShadowOnNinePatchLayer,  // Instance of `SystemWindowDecoration`.
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

// The parameterized test class for dynamic system shadow colors.
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
  std::unique_ptr<SystemDecoration> MakeShadowAccordingToParam(
      SystemDecoration::ShadowType type) {
    switch (GetParam()) {
      case SystemDecorationType::kShadowOnNinePatchLayer:
        return MakeShadowOnNinePatchLayer(type);
      case SystemDecorationType::kViewShadowOnNinePatchLayer:
        return MakeViewShadowOnNinePatchLayer(type);
      case SystemDecorationType::kWindowShadowOnNinePatchLayer:
        return MakeWindowShadowOnNinePatchLayer(type);
    }
  }

 private:
  // Creates an instance of `SystemDecorationImpl`.
  std::unique_ptr<SystemDecoration> MakeShadowOnNinePatchLayer(
      SystemDecoration::ShadowType type) {
    auto shadow = SystemDecoration::CreateShadowOnNinePatchLayer(type);
    AddShadowToWidget(shadow.get(), widget_.get());
    return shadow;
  }

  // Creates an instance of `SystemViewDecoration`.
  std::unique_ptr<SystemDecoration> MakeViewShadowOnNinePatchLayer(
      SystemDecoration::ShadowType type) {
    auto shadow = SystemDecoration::CreateShadowOnNinePatchLayerForView(
        widget_->GetContentsView(), type);
    return shadow;
  }

  // Creates an instance of `SystemWindowDecoration`.
  std::unique_ptr<SystemDecoration> MakeWindowShadowOnNinePatchLayer(
      SystemDecoration::ShadowType type) {
    auto shadow = SystemDecoration::CreateShadowOnNinePatchLayerForWindow(
        widget_->GetNativeWindow(), type);
    return shadow;
  }

  // The test widget used as a shadow owner.
  std::unique_ptr<views::Widget> widget_;
};

INSTANTIATE_TEST_SUITE_P(
    MaterialNext,
    SystemDecorationColorTest,
    testing::Values(SystemDecorationType::kShadowOnNinePatchLayer,
                    SystemDecorationType::kViewShadowOnNinePatchLayer,
                    SystemDecorationType::kWindowShadowOnNinePatchLayer),
    [](const testing::TestParamInfo<SystemDecorationColorTest::ParamType>&
           info) {
      switch (info.param) {
        case SystemDecorationType::kShadowOnNinePatchLayer:
          return "ShadowOnNinePatchLayer";
        case SystemDecorationType::kViewShadowOnNinePatchLayer:
          return "ViewShadowOnNinePatchLayer";
        case SystemDecorationType::kWindowShadowOnNinePatchLayer:
          return "WindowShadowOnNinePatcherLayer";
      }
    });

// Tests if the colors of system shadow change with shadow types.
TEST_P(SystemDecorationColorTest, UpdateShadowColorsWithType) {
  auto shadow =
      MakeShadowAccordingToParam(SystemDecoration::ShadowType::kElevation4);
  EXPECT_EQ(GetShadowColors(shadow.get()),
            std::make_pair(
                GetColor(ui::kColorShadowValueKeyShadowElevationFour),
                GetColor(ui::kColorShadowValueAmbientShadowElevationFour)));

  // Change type to kElevation12.
  shadow->SetType(SystemDecoration::ShadowType::kElevation12);
  EXPECT_EQ(GetShadowColors(shadow.get()),
            std::make_pair(
                GetColor(ui::kColorShadowValueKeyShadowElevationTwelve),
                GetColor(ui::kColorShadowValueAmbientShadowElevationTwelve)));

  // Change type to kElevation24;
  shadow->SetType(SystemDecoration::ShadowType::kElevation24);
  EXPECT_EQ(
      GetShadowColors(shadow.get()),
      std::make_pair(
          GetColor(ui::kColorShadowValueKeyShadowElevationTwentyFour),
          GetColor(ui::kColorShadowValueAmbientShadowElevationTwentyFour)));
}

// Tests if the colors of system shadow change with the themes.
TEST_P(SystemDecorationColorTest, UpdateShadowColorsWithTheme) {
  auto shadow =
      MakeShadowAccordingToParam(SystemDecoration::ShadowType::kElevation4);

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

}  // namespace ash
