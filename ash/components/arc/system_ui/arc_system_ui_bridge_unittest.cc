// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/system_ui/arc_system_ui_bridge.h"

#include <memory>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_system_ui_instance.h"
#include "ash/style/mojom/color_scheme.mojom-shared.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_log.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"

using ::testing::_;
namespace arc {

#define EXPECT_ERROR_LOG(matcher)                                    \
  if (DLOG_IS_ON(ERROR)) {                                           \
    EXPECT_CALL(log_, Log(logging::LOGGING_ERROR, _, _, _, matcher)) \
        .WillOnce(testing::Return(true)); /* suppress logging */     \
  }

class TestColorPaletteController : public ash::ColorPaletteController {
 public:
  TestColorPaletteController() = default;
  ~TestColorPaletteController() override = default;

  void SetSeed(ash::ColorPaletteSeed seed) { seed_ = seed; }

  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  void SetColorScheme(ash::style::mojom::ColorScheme scheme,
                      const AccountId& account_id,
                      base::OnceClosure on_complete) override {}
  void SetStaticColor(SkColor seed_color,
                      const AccountId& account_id,
                      base::OnceClosure on_complete) override {}
  void SelectLocalAccount(const AccountId& account_id) override {}
  SkColor GetUserWallpaperColorOrDefault(SkColor default_color) const override {
    return SK_ColorGREEN;
  }
  std::optional<ash::ColorPaletteSeed> GetColorPaletteSeed(
      const AccountId& account_id) const override {
    return seed_;
  }
  std::optional<ash::ColorPaletteSeed> GetCurrentSeed() const override {
    return seed_;
  }
  bool UsesWallpaperSeedColor(const AccountId& account_id) const override {
    return true;
  }
  ash::style::mojom::ColorScheme GetColorScheme(
      const AccountId& account_id) const override {
    return seed_.scheme;
  }
  std::optional<SkColor> GetStaticColor(
      const AccountId& account_id) const override {
    return seed_.seed_color;
  }
  bool GetUseKMeansPref(const AccountId& account_id) const override {
    return false;
  }
  void GenerateSampleColorSchemes(
      base::span<const ash::style::mojom::ColorScheme> color_scheme_buttons,
      ash::ColorPaletteController::SampleColorSchemeCallback callback)
      const override {}

 private:
  ash::ColorPaletteSeed seed_;
};

class ArcSystemUIBridgeTest : public testing::Test {
 protected:
  ArcSystemUIBridgeTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        bridge_(ArcSystemUIBridge::GetForBrowserContextForTesting(&context_)) {
    // ARC has VLOG(1) enabled. Ignore and suppress these logs if the test
    // will verify log output. Note the "if" must match the "if" in
    // `EXPECT_ERROR_LOG`.
    if (DLOG_IS_ON(ERROR)) {
      EXPECT_CALL(log_, Log(-1, _, _, _, _))
          .WillRepeatedly(testing::Return(true));
    }
  }
  ~ArcSystemUIBridgeTest() override = default;

  void SetUp() override {
    test_palette_ = std::make_unique<TestColorPaletteController>();
    bridge_->SetColorPaletteControllerForTesting(test_palette_.get());
    ArcServiceManager::Get()->arc_bridge_service()->system_ui()->SetInstance(
        &system_ui_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->system_ui());
  }

  void TearDown() override {
    ArcServiceManager::Get()->arc_bridge_service()->system_ui()->CloseInstance(
        &system_ui_instance_);
    bridge_->Shutdown();
    if (test_palette_) {
      // Only run cleanup if simulating that Shell still exists.
      bridge_->SetColorPaletteControllerForTesting(nullptr);
      test_palette_.reset();
    }
  }

  explicit ArcSystemUIBridgeTest(const ArcSystemUIBridge&) = delete;
  ArcSystemUIBridgeTest& operator=(const ArcSystemUIBridge&) = delete;

  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  user_prefs::TestBrowserContextWithPrefs context_;
  FakeSystemUiInstance system_ui_instance_;
  std::unique_ptr<TestColorPaletteController> test_palette_;
  const raw_ptr<ArcSystemUIBridge> bridge_;
  base::test::MockLog log_;
};

TEST_F(ArcSystemUIBridgeTest, ConstructDestruct) {}

TEST_F(ArcSystemUIBridgeTest, DestroyColorPaletteControllerFirst) {
  // Simulate Shell destruction.
  bridge_->OnShellDestroying();
  // Delete the ColorPaletteController like Shell would.
  test_palette_.reset();
  // This would crash in `TearDown()` before https://crbug.com/1431544 was
  // fixed.
}

TEST_F(ArcSystemUIBridgeTest, OnColorModeChanged) {
  EXPECT_FALSE(system_ui_instance_.dark_theme_status());
  ash::ColorPaletteSeed seed;
  seed.color_mode = ui::ColorProviderKey::ColorMode::kDark;
  bridge_->OnColorPaletteChanging(seed);
  EXPECT_TRUE(system_ui_instance_.dark_theme_status());
  ArcServiceManager::Get()->arc_bridge_service()->system_ui()->CloseInstance(
      &system_ui_instance_);
  EXPECT_ERROR_LOG(testing::HasSubstr("Failed to send theme status"));
  log_.StartCapturingLogs();
  seed.color_mode = ui::ColorProviderKey::ColorMode::kLight;
  bridge_->OnColorPaletteChanging(seed);
}

TEST_F(ArcSystemUIBridgeTest, OnConnectionReady) {
  EXPECT_FALSE(system_ui_instance_.dark_theme_status());
  ash::ColorPaletteSeed seed;
  seed.color_mode = ui::ColorProviderKey::ColorMode::kDark;
  seed.scheme = ash::style::mojom::ColorScheme::kVibrant;
  seed.seed_color = SK_ColorMAGENTA;
  test_palette_->SetSeed(seed);

  // When the connection is ready, bridge will read the current seed from the
  // ColorPaletteController.
  bridge_->OnConnectionReady();
  EXPECT_TRUE(system_ui_instance_.dark_theme_status());
  EXPECT_EQ(static_cast<uint32_t>(SK_ColorMAGENTA),
            system_ui_instance_.source_color());
  EXPECT_EQ(mojom::ThemeStyleType::VIBRANT, system_ui_instance_.theme_style());
}

TEST_F(ArcSystemUIBridgeTest, SendOverlayColor) {
  // Verify that the test data is not the default
  ASSERT_NE((uint32_t)50, system_ui_instance_.source_color());
  ASSERT_NE(mojom::ThemeStyleType::EXPRESSIVE,
            system_ui_instance_.theme_style());

  bridge_->SendOverlayColor(50, mojom::ThemeStyleType::EXPRESSIVE);
  EXPECT_EQ((uint32_t)50, system_ui_instance_.source_color());
  EXPECT_EQ(mojom::ThemeStyleType::EXPRESSIVE,
            system_ui_instance_.theme_style());
}

TEST_F(ArcSystemUIBridgeTest, OnConnectionReady_NeutralToSpritzConversion) {
  EXPECT_FALSE(system_ui_instance_.dark_theme_status());
  ash::ColorPaletteSeed seed;
  seed.color_mode = ui::ColorProviderKey::ColorMode::kLight;
  seed.scheme = ash::style::mojom::ColorScheme::kNeutral;
  seed.seed_color = SK_ColorCYAN;
  test_palette_->SetSeed(seed);

  // When the connection is ready, bridge will read the current seed from the
  // ColorPaletteController.
  bridge_->OnConnectionReady();
  EXPECT_FALSE(system_ui_instance_.dark_theme_status());
  EXPECT_EQ(static_cast<uint32_t>(SK_ColorCYAN),
            system_ui_instance_.source_color());
  EXPECT_EQ(mojom::ThemeStyleType::SPRITZ, system_ui_instance_.theme_style());
}

}  // namespace arc
