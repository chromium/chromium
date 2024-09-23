// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/style/system_shadow.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Different types of SystemShadow extensions.
enum class SystemShadowType {
  kShadowOnNinePatchLayer,        // Instance of `SystemShadowOnNinePatchLayer`.
  kViewShadowOnNinePatchLayer,    // Instance of
                                  // `SystemViewShadowOnNinePatchLayer`.
  kWindowShadowOnNinePatchLayer,  // Instance of
                                  // `SystemWindowShadowOnNinePatchLayer`.
  kShadowOnTextureLayer,          // Instance of `SystemShadowOnTextureLayer`.
};

// Gets the key and ambient shadow colors from a shadow.
std::pair<SkColor, SkColor> GetShadowColors(SystemShadow* shadow) {
  gfx::ShadowValues values = shadow->GetShadowValuesForTesting();
  return std::make_pair(values[0].color(), values[1].color());
}

// Add a shadow to the bottom of the native window of a widget and make the
// shadow observe the theme change of the widget.
void AddShadowToWidget(SystemShadow* shadow, views::Widget* widget) {
  auto* window_layer = widget->GetNativeWindow()->layer();
  auto* shadow_layer = shadow->GetLayer();
  window_layer->Add(shadow_layer);
  window_layer->StackAtBottom(shadow_layer);
  shadow->SetContentBounds(gfx::Rect(window_layer->bounds().size()));
  shadow->ObserveColorProviderSource(widget);
}

}  // namespace

// The parameterized test class for dynamic system shadow colors.
class SystemShadowColorTest
    : public AshTestBase,
      public testing::WithParamInterface<SystemShadowType> {
 public:
  SystemShadowColorTest() = default;
  SystemShadowColorTest(const SystemShadowColorTest&) = delete;
  SystemShadowColorTest& operator=(const SystemShadowColorTest&) = delete;
  ~SystemShadowColorTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    // Enable Jelly and Jellyroll features.
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kJelly, chromeos::features::kJellyroll}, {});
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

  // Creates a certain type of SystemShadow instance according to the test
  // parameter.
  std::unique_ptr<SystemShadow> MakeShadowAccordingToParam(
      SystemShadow::Type type) {
    switch (GetParam()) {
      case SystemShadowType::kShadowOnNinePatchLayer:
        return MakeShadowOnNinePatchLayer(type);
      case SystemShadowType::kViewShadowOnNinePatchLayer:
        return MakeViewShadowOnNinePatchLayer(type);
      case SystemShadowType::kWindowShadowOnNinePatchLayer:
        return MakeWindowShadowOnNinePatchLayer(type);
      case SystemShadowType::kShadowOnTextureLayer:
        return MakeShadowOnTextureLayer(type);
    }
  }

 private:
  // Creates an instance of `SystemShadowOnNinePatchLayer`.
  std::unique_ptr<SystemShadow> MakeShadowOnNinePatchLayer(
      SystemShadow::Type type) {
    auto shadow = SystemShadow::CreateShadowOnNinePatchLayer(
        type, SystemShadow::LayerRecreatedCallback());
    AddShadowToWidget(shadow.get(), widget_.get());
    return shadow;
  }

  // Creates an instance of `SystemViewShadowOnNinePatchLayer`.
  std::unique_ptr<SystemShadow> MakeViewShadowOnNinePatchLayer(
      SystemShadow::Type type) {
    auto shadow = SystemShadow::CreateShadowOnNinePatchLayerForView(
        widget_->GetContentsView(), type);
    return shadow;
  }

  // Creates an instance of `SystemWindowShadowOnNinePatchLayer`.
  std::unique_ptr<SystemShadow> MakeWindowShadowOnNinePatchLayer(
      SystemShadow::Type type) {
    auto shadow = SystemShadow::CreateShadowOnNinePatchLayerForWindow(
        widget_->GetNativeWindow(), type);
    return shadow;
  }

  // Creates an instance of `SystemShadowOnTextureLayer`.
  std::unique_ptr<SystemShadow> MakeShadowOnTextureLayer(
      SystemShadow::Type type) {
    auto shadow = SystemShadow::CreateShadowOnTextureLayer(type);
    AddShadowToWidget(shadow.get(), widget_.get());
    return shadow;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  // The test widget used as a shadow owner.
  std::unique_ptr<views::Widget> widget_;
};

INSTANTIATE_TEST_SUITE_P(
    MaterialNext,
    SystemShadowColorTest,
    testing::Values(SystemShadowType::kShadowOnNinePatchLayer,
                    SystemShadowType::kViewShadowOnNinePatchLayer,
                    SystemShadowType::kWindowShadowOnNinePatchLayer,
                    SystemShadowType::kShadowOnTextureLayer),
    [](const testing::TestParamInfo<SystemShadowColorTest::ParamType>& info) {
      switch (info.param) {
        case SystemShadowType::kShadowOnNinePatchLayer:
          return "ShadowOnNinePatchLayer";
        case SystemShadowType::kViewShadowOnNinePatchLayer:
          return "ViewShadowOnNinePatchLayer";
        case SystemShadowType::kWindowShadowOnNinePatchLayer:
          return "WindowShadowOnNinePatcherLayer";
        case SystemShadowType::kShadowOnTextureLayer:
          return "ShadowOnTextureLayer";
      }
    });

// Tests if the colors of system shadow change with shadow types.
TEST_P(SystemShadowColorTest, UpdateShadowColorsWithType) {
  auto shadow = MakeShadowAccordingToParam(SystemShadow::Type::kElevation4);
  EXPECT_EQ(GetShadowColors(shadow.get()),
            std::make_pair(
                GetColor(ui::kColorShadowValueKeyShadowElevationFour),
                GetColor(ui::kColorShadowValueAmbientShadowElevationFour)));

  // Change type to kElevation12.
  shadow->SetType(SystemShadow::Type::kElevation12);
  EXPECT_EQ(GetShadowColors(shadow.get()),
            std::make_pair(
                GetColor(ui::kColorShadowValueKeyShadowElevationTwelve),
                GetColor(ui::kColorShadowValueAmbientShadowElevationTwelve)));

  // Change type to kElevation24;
  shadow->SetType(SystemShadow::Type::kElevation24);
  EXPECT_EQ(
      GetShadowColors(shadow.get()),
      std::make_pair(
          GetColor(ui::kColorShadowValueKeyShadowElevationTwentyFour),
          GetColor(ui::kColorShadowValueAmbientShadowElevationTwentyFour)));
}

// Tests if the colors of system shadow change with the themes.
TEST_P(SystemShadowColorTest, UpdateShadowColorsWithTheme) {
  auto shadow = MakeShadowAccordingToParam(SystemShadow::Type::kElevation4);

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
