// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_theme_provider_impl.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_metrics.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash::personalization_app {

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@personalization";

class TestThemeObserver
    : public ash::personalization_app::mojom::ThemeObserver {
 public:
  void OnColorModeChanged(bool dark_mode_enabled) override {
    dark_mode_enabled_ = dark_mode_enabled;
  }

  void OnColorModeAutoScheduleChanged(bool enabled) override {
    color_mode_auto_schedule_enabled_ = enabled;
  }

  void OnColorSchemeChanged(ash::ColorScheme color_scheme) override {
    color_scheme_ = color_scheme;
  }

  void OnStaticColorChanged(absl::optional<::SkColor> static_color) override {
    static_color_ = static_color;
  }

  mojo::PendingRemote<ash::personalization_app::mojom::ThemeObserver>
  pending_remote() {
    if (theme_observer_receiver_.is_bound()) {
      theme_observer_receiver_.reset();
    }
    return theme_observer_receiver_.BindNewPipeAndPassRemote();
  }

  absl::optional<bool> is_dark_mode_enabled() {
    if (!theme_observer_receiver_.is_bound())
      return absl::nullopt;

    theme_observer_receiver_.FlushForTesting();
    return dark_mode_enabled_;
  }

  bool is_color_mode_auto_schedule_enabled() {
    if (theme_observer_receiver_.is_bound())
      theme_observer_receiver_.FlushForTesting();
    return color_mode_auto_schedule_enabled_;
  }

  ash::ColorScheme GetColorScheme() {
    if (theme_observer_receiver_.is_bound()) {
      theme_observer_receiver_.FlushForTesting();
    }
    return color_scheme_;
  }

  absl::optional<SkColor> GetStaticColor() {
    if (!theme_observer_receiver_.is_bound())
      return absl::nullopt;
    theme_observer_receiver_.FlushForTesting();
    return static_color_;
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::ThemeObserver>
      theme_observer_receiver_{this};

  bool dark_mode_enabled_ = false;
  bool color_mode_auto_schedule_enabled_ = false;
  ash::ColorScheme color_scheme_ = ash::ColorScheme::kTonalSpot;
  absl::optional<::SkColor> static_color_ = absl::nullopt;
};

}  // namespace

class PersonalizationAppThemeProviderImplTest : public ChromeAshTestBase {
 public:
  PersonalizationAppThemeProviderImplTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures({chromeos::features::kDarkLightMode},
                                          {});
  }
  PersonalizationAppThemeProviderImplTest(
      const PersonalizationAppThemeProviderImplTest&) = delete;
  PersonalizationAppThemeProviderImplTest& operator=(
      const PersonalizationAppThemeProviderImplTest&) = delete;
  ~PersonalizationAppThemeProviderImplTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    ChromeAshTestBase::SetUp();
    ash::DarkLightModeControllerImpl::Get()->OnActiveUserPrefServiceChanged(
        ash::Shell::Get()->session_controller()->GetActivePrefService());

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeTestEmail);

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_.set_web_contents(web_contents_.get());

    theme_provider_ =
        std::make_unique<PersonalizationAppThemeProviderImpl>(&web_ui_);
    theme_provider_->BindInterface(
        theme_provider_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    theme_provider_.reset();
    ChromeAshTestBase::TearDown();
  }

  TestingProfile* profile() { return profile_; }

  mojo::Remote<ash::personalization_app::mojom::ThemeProvider>*
  theme_provider_remote() {
    return &theme_provider_remote_;
  }

  PersonalizationAppThemeProviderImpl* theme_provider() {
    return theme_provider_.get();
  }

  void SetThemeObserver() {
    theme_provider_remote_->SetThemeObserver(
        test_theme_observer_.pending_remote());
  }

  absl::optional<bool> is_dark_mode_enabled() {
    if (theme_provider_remote_.is_bound())
      theme_provider_remote_.FlushForTesting();
    return test_theme_observer_.is_dark_mode_enabled();
  }

  bool is_color_mode_auto_schedule_enabled() {
    if (theme_provider_remote_.is_bound())
      theme_provider_remote_.FlushForTesting();
    return test_theme_observer_.is_color_mode_auto_schedule_enabled();
  }

  ash::ColorScheme GetColorScheme() {
    if (theme_provider_remote_.is_bound()) {
      theme_provider_remote_.FlushForTesting();
    }
    return test_theme_observer_.GetColorScheme();
  }

  absl::optional<SkColor> GetStaticColor() {
    if (theme_provider_remote_.is_bound())
      theme_provider_remote_.FlushForTesting();
    return test_theme_observer_.GetStaticColor();
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  user_manager::ScopedUserManager scoped_user_manager_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  TestingProfile* profile_;
  mojo::Remote<ash::personalization_app::mojom::ThemeProvider>
      theme_provider_remote_;
  TestThemeObserver test_theme_observer_;
  std::unique_ptr<PersonalizationAppThemeProviderImpl> theme_provider_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PersonalizationAppThemeProviderImplTest, SetColorModePref) {
  SetThemeObserver();
  theme_provider()->SetColorModePref(/*dark_mode_enabled=*/false);
  EXPECT_FALSE(is_dark_mode_enabled().value());

  theme_provider()->SetColorModePref(/*dark_mode_enabled=*/true);
  EXPECT_TRUE(is_dark_mode_enabled().value());
  histogram_tester().ExpectBucketCount(
      kPersonalizationThemeColorModeHistogramName, ColorMode::kDark, 1);
}

TEST_F(PersonalizationAppThemeProviderImplTest, OnColorModeChanged) {
  SetThemeObserver();

  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  bool dark_mode_enabled = dark_light_mode_controller->IsDarkModeEnabled();
  dark_light_mode_controller->ToggleColorMode();
  EXPECT_NE(is_dark_mode_enabled().value(), dark_mode_enabled);

  dark_light_mode_controller->ToggleColorMode();
  EXPECT_EQ(is_dark_mode_enabled().value(), dark_mode_enabled);
}

TEST_F(PersonalizationAppThemeProviderImplTest,
       SetColorModeAutoScheduleEnabled) {
  SetThemeObserver();
  theme_provider_remote()->FlushForTesting();
  theme_provider()->SetColorModeAutoScheduleEnabled(/*enabled=*/false);
  EXPECT_FALSE(is_color_mode_auto_schedule_enabled());
  histogram_tester().ExpectBucketCount(
      kPersonalizationThemeColorModeHistogramName, ColorMode::kAuto, 0);

  theme_provider()->SetColorModeAutoScheduleEnabled(/*enabled=*/true);
  EXPECT_TRUE(is_color_mode_auto_schedule_enabled());
  histogram_tester().ExpectBucketCount(
      kPersonalizationThemeColorModeHistogramName, ColorMode::kAuto, 1);
}

class PersonalizationAppThemeProviderImplJellyTest
    : public PersonalizationAppThemeProviderImplTest {
 public:
  PersonalizationAppThemeProviderImplJellyTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kJelly);
  }

  PersonalizationAppThemeProviderImplJellyTest(
      const PersonalizationAppThemeProviderImplJellyTest&) = delete;
  PersonalizationAppThemeProviderImplJellyTest& operator=(
      const PersonalizationAppThemeProviderImplJellyTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PersonalizationAppThemeProviderImplJellyTest, SetStaticColor) {
  SetThemeObserver();
  theme_provider_remote()->FlushForTesting();
  SkColor color = SK_ColorMAGENTA;
  EXPECT_NE(color, GetStaticColor());
  EXPECT_NE(ash::ColorScheme::kStatic, GetColorScheme());

  theme_provider()->SetStaticColor(color);

  EXPECT_EQ(color, GetStaticColor());
  EXPECT_EQ(ash::ColorScheme::kStatic, GetColorScheme());
}

TEST_F(PersonalizationAppThemeProviderImplJellyTest, SetColorScheme) {
  SetThemeObserver();
  theme_provider_remote()->FlushForTesting();
  auto color_scheme = ash::ColorScheme::kExpressive;
  EXPECT_NE(color_scheme, GetColorScheme());

  theme_provider()->SetColorScheme(color_scheme);

  EXPECT_EQ(color_scheme, GetColorScheme());
}
}  // namespace ash::personalization_app
