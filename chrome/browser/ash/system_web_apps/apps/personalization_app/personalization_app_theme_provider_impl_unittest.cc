// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_theme_provider_impl.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/color_palette_controller.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/mojom/color_scheme.mojom-shared.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_metrics.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash::personalization_app {

namespace {

constexpr char kFakeTestEmail[] = "fakeemail@personalization";
constexpr char kTestGaiaId[] = "1234567890";
AccountId kAccountId =
    AccountId::FromUserEmailGaiaId(kFakeTestEmail, kTestGaiaId);

void AddAndLoginUser() {
  ash::FakeChromeUserManager* user_manager =
      static_cast<ash::FakeChromeUserManager*>(
          user_manager::UserManager::Get());

  user_manager->AddUser(kAccountId);
  user_manager->LoginUser(kAccountId);
  user_manager->SwitchActiveUser(kAccountId);
}

class TestThemeObserver
    : public ash::personalization_app::mojom::ThemeObserver {
 public:
  void OnColorModeChanged(bool dark_mode_enabled) override {
    dark_mode_enabled_ = dark_mode_enabled;
  }

  void OnColorModeAutoScheduleChanged(bool enabled) override {
    color_mode_auto_schedule_enabled_ = enabled;
  }

  void OnColorSchemeChanged(
      ash::style::mojom::ColorScheme color_scheme) override {
    color_scheme_ = color_scheme;
  }

  void OnSampleColorSchemesChanged(const std::vector<ash::SampleColorScheme>&
                                       sample_color_schemes) override {
    sample_color_schemes_ = sample_color_schemes;
  }

  void OnStaticColorChanged(std::optional<::SkColor> static_color) override {
    static_color_ = static_color;
  }

  void OnGeolocationPermissionForSystemServicesChanged(
      bool enabled,
      bool is_user_modifiable) override {
    geolocation_for_system_enabled_ = enabled;
    is_geolocation_user_modifiable_ = is_user_modifiable;
  }

  void OnDaylightTimeChanged(const std::u16string& sunrise_time,
                             const std::u16string& sunset_time) override {
    // no-op
  }

  mojo::PendingRemote<ash::personalization_app::mojom::ThemeObserver>
  pending_remote() {
    if (theme_observer_receiver_.is_bound()) {
      theme_observer_receiver_.reset();
    }
    return theme_observer_receiver_.BindNewPipeAndPassRemote();
  }

  std::optional<bool> is_dark_mode_enabled() {
    if (!theme_observer_receiver_.is_bound()) {
      return std::nullopt;
    }

    theme_observer_receiver_.FlushForTesting();
    return dark_mode_enabled_;
  }

  bool is_color_mode_auto_schedule_enabled() {
    if (theme_observer_receiver_.is_bound()) {
      theme_observer_receiver_.FlushForTesting();
    }
    return color_mode_auto_schedule_enabled_;
  }

  bool is_geolocation_enabled_for_system_services() {
    if (theme_observer_receiver_.is_bound()) {
      theme_observer_receiver_.FlushForTesting();
    }
    return geolocation_for_system_enabled_;
  }

  bool is_geolocation_user_modifiable() {
    if (theme_observer_receiver_.is_bound()) {
      theme_observer_receiver_.FlushForTesting();
    }
    return is_geolocation_user_modifiable_;
  }

  ash::style::mojom::ColorScheme GetColorScheme() {
    if (theme_observer_receiver_.is_bound()) {
      theme_observer_receiver_.FlushForTesting();
    }
    return color_scheme_;
  }

  std::optional<SkColor> GetStaticColor() {
    if (!theme_observer_receiver_.is_bound()) {
      return std::nullopt;
    }
    theme_observer_receiver_.FlushForTesting();
    return static_color_;
  }

 private:
  mojo::Receiver<ash::personalization_app::mojom::ThemeObserver>
      theme_observer_receiver_{this};

  bool dark_mode_enabled_ = false;
  bool color_mode_auto_schedule_enabled_ = false;
  bool geolocation_for_system_enabled_ = false;
  bool is_geolocation_user_modifiable_ = true;
  ash::style::mojom::ColorScheme color_scheme_ =
      ash::style::mojom::ColorScheme::kTonalSpot;
  std::optional<::SkColor> static_color_ = std::nullopt;
  std::vector<ash::SampleColorScheme> sample_color_schemes_;
};

}  // namespace

class PersonalizationAppThemeProviderImplTest : public ChromeAshTestBase {
 public:
  PersonalizationAppThemeProviderImplTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
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

  std::optional<bool> is_dark_mode_enabled() {
    if (theme_provider_remote_.is_bound()) {
      theme_provider_remote_.FlushForTesting();
    }
    return test_theme_observer_.is_dark_mode_enabled();
  }

  bool is_color_mode_auto_schedule_enabled() {
    if (theme_provider_remote_.is_bound()) {
      theme_provider_remote_.FlushForTesting();
    }
    return test_theme_observer_.is_color_mode_auto_schedule_enabled();
  }

  // Depending on the `managed` argument, sets the value of the
  // `kUserGeolocationAccessLevel` pref either in `PrefStoreType::MANAGED_STORE`
  // or in `PrefStoreType::USER_STORE` PrefStore.
  void SetGeolocationPref(bool enabled, bool managed) {
    GeolocationAccessLevel level;
    if (enabled) {
      level = GeolocationAccessLevel::kOnlyAllowedForSystem;
    } else {
      level = GeolocationAccessLevel::kDisallowed;
    }

    if (managed) {
      profile()->GetTestingPrefService()->SetManagedPref(
          ash::prefs::kUserGeolocationAccessLevel,
          base::Value(static_cast<int>(level)));
    } else {
      profile()->GetTestingPrefService()->SetUserPref(
          ash::prefs::kUserGeolocationAccessLevel,
          base::Value(static_cast<int>(level)));
    }
  }

  bool is_geolocation_enabled_for_system_services() {
    if (theme_provider_remote_.is_bound()) {
      theme_provider_remote_.FlushForTesting();
    }
    return test_theme_observer_.is_geolocation_enabled_for_system_services();
  }

  bool is_geolocation_user_modifiable() {
    if (theme_provider_remote_.is_bound()) {
      theme_provider_remote_.FlushForTesting();
    }
    return test_theme_observer_.is_geolocation_user_modifiable();
  }

  ash::style::mojom::ColorScheme GetColorScheme() {
    if (theme_provider_remote_.is_bound()) {
      theme_provider_remote_.FlushForTesting();
    }
    return test_theme_observer_.GetColorScheme();
  }

  std::optional<SkColor> GetStaticColor() {
    if (theme_provider_remote_.is_bound()) {
      theme_provider_remote_.FlushForTesting();
    }
    return test_theme_observer_.GetStaticColor();
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  user_manager::ScopedUserManager scoped_user_manager_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<TestingProfile> profile_;
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

TEST_F(PersonalizationAppThemeProviderImplTest,
       EnableGeolocationForSystemServices) {
  SetThemeObserver();

  // Check default geolocation state.
  EXPECT_TRUE(is_geolocation_enabled_for_system_services());
  EXPECT_TRUE(is_geolocation_user_modifiable());

  // Check consumer scenarios:
  SetGeolocationPref(/*enabled=*/false, /*managed=*/false);
  // theme_provider()->EnableGeolocationForSystemServices();
  EXPECT_FALSE(is_geolocation_enabled_for_system_services());
  EXPECT_TRUE(is_geolocation_user_modifiable());
  SetGeolocationPref(/*enabled=*/true, /*managed=*/false);
  EXPECT_TRUE(is_geolocation_enabled_for_system_services());
  EXPECT_TRUE(is_geolocation_user_modifiable());

  // Check managed scenarios:
  SetGeolocationPref(/*enabled=*/false, /*managed=*/true);
  EXPECT_FALSE(is_geolocation_enabled_for_system_services());
  EXPECT_FALSE(is_geolocation_user_modifiable());
  SetGeolocationPref(/*enabled=*/true, /*managed=*/true);
  EXPECT_TRUE(is_geolocation_enabled_for_system_services());
  EXPECT_FALSE(is_geolocation_user_modifiable());
}

class PersonalizationAppThemeProviderImplJellyTest
    : public PersonalizationAppThemeProviderImplTest {
 public:
  PersonalizationAppThemeProviderImplJellyTest() {
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kJelly);
  }

  PersonalizationAppThemeProviderImplJellyTest(
      const PersonalizationAppThemeProviderImplJellyTest&) = delete;
  PersonalizationAppThemeProviderImplJellyTest& operator=(
      const PersonalizationAppThemeProviderImplJellyTest&) = delete;

  void SetUp() override {
    PersonalizationAppThemeProviderImplTest::SetUp();
    AddAndLoginUser();
    GetSessionControllerClient()->AddUserSession(kAccountId, kFakeTestEmail);
  }

 protected:
  PrefService* GetUserPrefService() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        kAccountId);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PersonalizationAppThemeProviderImplJellyTest, SetStaticColor) {
  SetThemeObserver();
  theme_provider_remote()->FlushForTesting();
  SkColor color = SK_ColorMAGENTA;
  EXPECT_NE(color,
            GetUserPrefService()->GetUint64(prefs::kDynamicColorSeedColor));

  theme_provider()->SetStaticColor(color);

  EXPECT_EQ(color,
            GetUserPrefService()->GetUint64(prefs::kDynamicColorSeedColor));
}

TEST_F(PersonalizationAppThemeProviderImplJellyTest,
       ObserveStaticColorChanges) {
  SetThemeObserver();
  theme_provider_remote()->FlushForTesting();
  SkColor color = SK_ColorMAGENTA;
  EXPECT_NE(color,
            GetUserPrefService()->GetUint64(prefs::kDynamicColorSeedColor));

  // The static color is set via the UserPrefService in
  // ColorPaletteController, and the pref listener is set via the
  // ProfilePrefService in the ThemeProvider. In real life, these point to the
  // same object, but not in this test. We have to set the pref in both places
  // for the pref listener to work. Only the profile prefs will trigger the
  // listener, and only the UserPrefService holds the information about which
  // pref was updated.
  theme_provider()->SetStaticColor(color);
  profile()->GetPrefs()->SetUint64(prefs::kDynamicColorSeedColor, color);

  EXPECT_EQ(color, GetStaticColor());
}

TEST_F(PersonalizationAppThemeProviderImplJellyTest, SetColorScheme) {
  SetThemeObserver();
  theme_provider_remote()->FlushForTesting();
  auto color_scheme = ash::style::mojom::ColorScheme::kExpressive;
  EXPECT_NE((int)color_scheme,
            GetUserPrefService()->GetInteger(prefs::kDynamicColorColorScheme));

  theme_provider()->SetColorScheme(color_scheme);

  EXPECT_EQ((int)color_scheme,
            GetUserPrefService()->GetInteger(prefs::kDynamicColorColorScheme));
}

TEST_F(PersonalizationAppThemeProviderImplJellyTest,
       ObserveColorSchemeChanges) {
  SetThemeObserver();
  theme_provider_remote()->FlushForTesting();
  auto color_scheme = ash::style::mojom::ColorScheme::kExpressive;
  EXPECT_NE((int)color_scheme,
            GetUserPrefService()->GetInteger(prefs::kDynamicColorColorScheme));

  // The color scheme is set via the UserPrefService in
  // ColorPaletteController, and the pref listener is set via the
  // ProfilePrefService in the ThemeProvider. In real life, these point to the
  // same object, but not in this test. We have to set the pref in both places
  // for the pref listener to work. Only the profile prefs will trigger the
  // listener, and only the UserPrefService holds the information about which
  // pref was updated.
  theme_provider()->SetColorScheme(color_scheme);
  profile()->GetPrefs()->SetInteger(prefs::kDynamicColorColorScheme,
                                    (int)color_scheme);

  EXPECT_EQ(color_scheme, GetColorScheme());
}

TEST_F(PersonalizationAppThemeProviderImplJellyTest,
       GenerateSampleColorSchemes) {
  SetThemeObserver();
  theme_provider_remote()->FlushForTesting();
  base::MockOnceCallback<void(const std::vector<ash::SampleColorScheme>&)>
      generate_sample_color_schemes_callback;

  // Matcher for the vector in the callback.
  auto matcher = testing::UnorderedElementsAre(
      testing::Field(&SampleColorScheme::scheme,
                     ash::style::mojom::ColorScheme::kTonalSpot),
      testing::Field(&SampleColorScheme::scheme,
                     ash::style::mojom::ColorScheme::kNeutral),
      testing::Field(&SampleColorScheme::scheme,
                     ash::style::mojom::ColorScheme::kVibrant),
      testing::Field(&SampleColorScheme::scheme,
                     ash::style::mojom::ColorScheme::kExpressive));

  base::RunLoop run_loop;
  EXPECT_CALL(generate_sample_color_schemes_callback, Run(matcher))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  theme_provider()->GenerateSampleColorSchemes(
      generate_sample_color_schemes_callback.Get());
  run_loop.Run();
}

}  // namespace ash::personalization_app
