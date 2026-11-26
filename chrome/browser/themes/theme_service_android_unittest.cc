// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_android.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/android/chrome_window_color_provider_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "chrome/test/base/testing_profile.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/themes/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"

namespace {

class WindowColorProviderObserver : public ui::ColorProviderSourceObserver {
 public:
  explicit WindowColorProviderObserver(ui::ColorProviderSource* source)
      : ui::ColorProviderSourceObserver(source) {}

  void OnColorProviderChanged() override { ++change_count_; }

  int change_count() const { return change_count_; }

 private:
  int change_count_ = 0;
};

class WebContentsColorProviderObserver : public content::WebContentsObserver {
 public:
  explicit WebContentsColorProviderObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void OnColorProviderChanged() override { ++change_count_; }

  int change_count() const { return change_count_; }

 private:
  int change_count_ = 0;
};

SkColor ExpectedPrimaryColor(
    ui::ColorProviderKey::ColorMode color_mode,
    std::optional<SkColor> user_color,
    ui::ColorProviderKey::UserColorSource source,
    std::optional<ui::ColorProviderKey::SchemeVariant> variant = std::nullopt) {
  ui::ColorProviderKey key;
  key.color_mode = color_mode;
  key.user_color = user_color;
  key.user_color_source = source;
  key.scheme_variant = variant;
  return ui::ColorProviderManager::Get().GetColorProviderFor(key)->GetColor(
      ui::kColorSysPrimary);
}

class ThemeServiceAndroidTest : public testing::TestWithParam<bool> {
 public:
  ThemeServiceAndroidTest()
      : scoped_feature_list_(
            GetParam() ? std::vector<base::test::FeatureRef>{kCacheThemePrefs}
                       : std::vector<base::test::FeatureRef>{},
            GetParam()
                ? std::vector<base::test::FeatureRef>{}
                : std::vector<base::test::FeatureRef>{kCacheThemePrefs}) {}

 protected:
  void SetUp() override {
    ui::ColorProviderManager::ResetForTesting();
    ui::ColorProviderManager::Get().AppendColorProviderInitializer(
        base::BindRepeating(AddChromeColorMixers));

    os_settings_provider_.SetPreferredColorScheme(
        ui::NativeTheme::PreferredColorScheme::kLight);
    os_settings_provider_.SetPreferredColorSource(
        ui::ColorProviderKey::UserColorSource::kAccent);

    profile_ = TestingProfile::Builder().Build();
    theme_service_ = ThemeServiceFactory::GetForProfile(profile_.get());
    ASSERT_NE(theme_service_, nullptr);

    theme_service_->SetBrowserColorScheme(
        ThemeService::BrowserColorScheme::kSystem);
    FlushPostedTasks();
  }

  void TearDown() override {
    if (theme_service_) {
      theme_service_->SetBrowserColorScheme(
          ThemeService::BrowserColorScheme::kSystem);
      FlushPostedTasks();
    }
    theme_service_ = nullptr;
    profile_.reset();
    ui::ColorProviderManager::ResetForTesting();
  }

  void FlushPostedTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  ui::MockOsSettingsProvider os_settings_provider_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ThemeService> theme_service_ = nullptr;
};

TEST_P(ThemeServiceAndroidTest, DeviceAndUserColorThemes) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  ChromeWindowColorProviderDelegate::AttachToWindow(
      window->get(), profile_.get(), /*is_incognito_window=*/false,
      /*is_night_mode=*/false);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  web_contents->SetColorProviderSource(window->get());

  // 1. Default theme follows the device accent color via UsingDeviceTheme().
  os_settings_provider_.SetAccentColor(SK_ColorMAGENTA);
  SkColor os_primary = ExpectedPrimaryColor(
      ui::ColorProviderKey::ColorMode::kLight, SK_ColorMAGENTA,
      ui::ColorProviderKey::UserColorSource::kAccent);
  EXPECT_EQ(window->get()->GetColorProvider()->GetColor(ui::kColorSysPrimary),
            os_primary);
  EXPECT_EQ(web_contents->GetColorProvider().GetColor(ui::kColorSysPrimary),
            os_primary);

  // 2. Picking a custom user color + variant overrides the OS accent on both
  // WindowAndroid (direct callers) and attached WebContents.
  WindowColorProviderObserver window_observer(window->get());
  WebContentsColorProviderObserver web_contents_observer(web_contents.get());
  theme_service_->SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kVibrant);
  EXPECT_GE(window_observer.change_count(), 1);
  EXPECT_GE(web_contents_observer.change_count(), 1);
  SkColor red_vibrant_primary =
      ExpectedPrimaryColor(ui::ColorProviderKey::ColorMode::kLight, SK_ColorRED,
                           ui::ColorProviderKey::UserColorSource::kAccent,
                           ui::ColorProviderKey::SchemeVariant::kVibrant);
  EXPECT_EQ(window->get()->GetColorProvider()->GetColor(ui::kColorSysPrimary),
            red_vibrant_primary);
  EXPECT_EQ(web_contents->GetColorProvider().GetColor(ui::kColorSysPrimary),
            red_vibrant_primary);

  // 3. Grayscale theme uses kGrayscale and suppresses scheme_variant.
  theme_service_->SetIsGrayscale(true);
  SkColor grayscale_primary = ExpectedPrimaryColor(
      ui::ColorProviderKey::ColorMode::kLight, std::nullopt,
      ui::ColorProviderKey::UserColorSource::kGrayscale);
  EXPECT_EQ(window->get()->GetColorProvider()->GetColor(ui::kColorSysPrimary),
            grayscale_primary);
  EXPECT_EQ(web_contents->GetColorProvider().GetColor(ui::kColorSysPrimary),
            grayscale_primary);

  // 4. Policy theme overrides grayscale and custom color variants.
  profile_->GetTestingPrefService()->SetManagedPref(
      themes::kPolicyThemeColor, base::Value(static_cast<int>(SK_ColorGREEN)));
  SkColor policy_primary = ExpectedPrimaryColor(
      ui::ColorProviderKey::ColorMode::kLight, SK_ColorGREEN,
      ui::ColorProviderKey::UserColorSource::kAccent);
  EXPECT_EQ(window->get()->GetColorProvider()->GetColor(ui::kColorSysPrimary),
            policy_primary);
  profile_->GetTestingPrefService()->RemoveManagedPref(
      themes::kPolicyThemeColor);

  // 5. "Default Chrome" (UseDefaultTheme() followed by UseDeviceTheme(false))
  // gives the static baseline palette rather than the OS accent color.
  theme_service_->UseDefaultTheme();
  theme_service_->UseDeviceTheme(false);
  SkColor baseline_primary = ExpectedPrimaryColor(
      ui::ColorProviderKey::ColorMode::kLight, std::nullopt,
      ui::ColorProviderKey::UserColorSource::kBaseline);
  EXPECT_EQ(window->get()->GetColorProvider()->GetColor(ui::kColorSysPrimary),
            baseline_primary);

  // 6. Re-enabling Follow Device Theme restores the OS accent color.
  theme_service_->UseDeviceTheme(true);
  EXPECT_EQ(window->get()->GetColorProvider()->GetColor(ui::kColorSysPrimary),
            os_primary);
}

TEST_P(ThemeServiceAndroidTest, ActivityNightModeAndBrowserColorScheme) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      dark_window = ui::WindowAndroid::CreateForTesting();
  ChromeWindowColorProviderDelegate::AttachToWindow(
      dark_window->get(), profile_.get(), /*is_incognito_window=*/false,
      /*is_night_mode=*/true);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      light_window = ui::WindowAndroid::CreateForTesting();
  ChromeWindowColorProviderDelegate::AttachToWindow(
      light_window->get(), profile_.get(), /*is_incognito_window=*/false,
      /*is_night_mode=*/false);

  // Under BrowserColorScheme::kSystem, each window follows its Activity's
  // cached night mode even when the global NativeTheme differs.
  EXPECT_EQ(dark_window->get()->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kDark);
  EXPECT_EQ(light_window->get()->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kLight);

  // BrowserColorScheme overrides Activity night mode on themed windows.
  theme_service_->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kDark);
  FlushPostedTasks();
  EXPECT_EQ(light_window->get()->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kDark);

  theme_service_->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);
  FlushPostedTasks();
  EXPECT_EQ(dark_window->get()->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kLight);
}

TEST_P(ThemeServiceAndroidTest, CustomTabWebApkIncognitoAndMixedModeIsolation) {
  os_settings_provider_.SetAccentColor(SK_ColorMAGENTA);
  theme_service_->SetUserColorAndBrowserColorVariant(
      SK_ColorRED, ui::mojom::BrowserColorVariant::kVibrant);
  theme_service_->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kDark);
  FlushPostedTasks();

  SkColor light_os_primary = ExpectedPrimaryColor(
      ui::ColorProviderKey::ColorMode::kLight, SK_ColorMAGENTA,
      ui::ColorProviderKey::UserColorSource::kAccent);
  SkColor dark_os_primary = ExpectedPrimaryColor(
      ui::ColorProviderKey::ColorMode::kDark, SK_ColorMAGENTA,
      ui::ColorProviderKey::UserColorSource::kAccent);
  SkColor dark_red_primary =
      ExpectedPrimaryColor(ui::ColorProviderKey::ColorMode::kDark, SK_ColorRED,
                           ui::ColorProviderKey::UserColorSource::kAccent,
                           ui::ColorProviderKey::SchemeVariant::kVibrant);
  SkColor incognito_primary =
      ExpectedPrimaryColor(ui::ColorProviderKey::ColorMode::kDark, std::nullopt,
                           ui::ColorProviderKey::UserColorSource::kGrayscale);
  SkColor isolated_primary = ExpectedPrimaryColor(
      ui::ColorProviderKey::ColorMode::kLight, std::nullopt,
      ui::ColorProviderKey::UserColorSource::kBaseline);

  // 1. Regular ChromeTabbedActivity window applies browser theme.
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      tabbed_window = ui::WindowAndroid::CreateForTesting();
  ChromeWindowColorProviderDelegate::AttachToWindow(
      tabbed_window->get(), profile_.get(), /*is_incognito_window=*/false,
      /*is_night_mode=*/false);
  EXPECT_EQ(tabbed_window->get()->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kDark);
  EXPECT_EQ(
      tabbed_window->get()->GetColorProvider()->GetColor(ui::kColorSysPrimary),
      dark_red_primary);

  // 2. Custom Tab / Popup / WebAPK / Ephemeral CCT windows (profile = nullptr)
  // ignore browser theme and follow only their Activity night mode.
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      light_cct_window = ui::WindowAndroid::CreateForTesting();
  ChromeWindowColorProviderDelegate::AttachToWindow(
      light_cct_window->get(), /*profile=*/nullptr,
      /*is_incognito_window=*/false, /*is_night_mode=*/false);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      dark_cct_window = ui::WindowAndroid::CreateForTesting();
  ChromeWindowColorProviderDelegate::AttachToWindow(
      dark_cct_window->get(), /*profile=*/nullptr,
      /*is_incognito_window=*/false, /*is_night_mode=*/true);

  std::unique_ptr<content::WebContents> cct_tab =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
      cct_tab.get(), light_cct_window->get());
  EXPECT_EQ(cct_tab->GetColorMode(), ui::ColorProviderKey::ColorMode::kLight);
  EXPECT_EQ(cct_tab->GetColorProvider().GetColor(ui::kColorSysPrimary),
            light_os_primary);

  // Dark CCT follows its local dark mode rather than the light OS NativeTheme.
  EXPECT_EQ(dark_cct_window->get()->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kDark);
  EXPECT_EQ(dark_cct_window->get()->GetColorProvider()->GetColor(
                ui::kColorSysPrimary),
            dark_os_primary);

  // Reparenting the CCT tab into ChromeTabbedActivity adopts the browser theme.
  ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
      cct_tab.get(), tabbed_window->get());
  EXPECT_EQ(cct_tab->GetColorProvider().GetColor(ui::kColorSysPrimary),
            dark_red_primary);

  // Detaching a regular tab falls back to the default (unthemed) source.
  ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
      cct_tab.get(), nullptr);
  EXPECT_EQ(cct_tab->GetColorMode(), ui::ColorProviderKey::ColorMode::kLight);
  EXPECT_EQ(cct_tab->GetColorProvider().GetColor(ui::kColorSysPrimary),
            light_os_primary);

  // 3. Ephemeral CCT profile is not Incognito-branded and stays on the CCT
  // window's ColorProviderSource.
  Profile* ephemeral_profile = profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUnique("CCT:Ephemeral"),
      /*create_if_needed=*/true);
  std::unique_ptr<content::WebContents> ephemeral_tab =
      content::WebContentsTester::CreateTestWebContents(ephemeral_profile,
                                                        nullptr);
  ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
      ephemeral_tab.get(), light_cct_window->get());
  EXPECT_EQ(ephemeral_tab->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kLight);
  EXPECT_EQ(ephemeral_tab->GetColorProvider().GetColor(ui::kColorSysPrimary),
            light_os_primary);

  // 4. Incognito-only window (e.g. OFF_THE_RECORD tabbed window or Incognito
  // CCT) enforces dark grayscale even without an OTR profile attached.
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      incognito_window = ui::WindowAndroid::CreateForTesting();
  ChromeWindowColorProviderDelegate::AttachToWindow(
      incognito_window->get(), /*profile=*/nullptr,
      /*is_incognito_window=*/true, /*is_night_mode=*/false);
  EXPECT_EQ(incognito_window->get()->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kDark);
  EXPECT_EQ(incognito_window->get()->GetColorProvider()->GetColor(
                ui::kColorSysPrimary),
            incognito_primary);

  // 5. MIXED-mode isolation: Incognito and Incognito CCT WebContents use a
  // shared dark grayscale source whether detached or attached, so attaching,
  // reparenting, and detaching keep them isolated with zero extra updates.
  for (Profile* otr_profile : {
           profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true),
           profile_->GetOffTheRecordProfile(
               Profile::OTRProfileID::CreateUniqueForIncognitoCct(),
               /*create_if_needed=*/true),
       }) {
    std::unique_ptr<content::WebContents> otr_contents =
        content::WebContentsTester::CreateTestWebContents(otr_profile, nullptr);
    ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
        otr_contents.get(), nullptr);
    EXPECT_EQ(otr_contents->GetColorMode(),
              ui::ColorProviderKey::ColorMode::kDark);
    EXPECT_EQ(otr_contents->GetColorProvider().GetColor(ui::kColorSysPrimary),
              incognito_primary);

    WebContentsColorProviderObserver observer(otr_contents.get());
    ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
        otr_contents.get(), tabbed_window->get());
    ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
        otr_contents.get(), light_cct_window->get());
    ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
        otr_contents.get(), nullptr);
    EXPECT_EQ(observer.change_count(), 0);
    EXPECT_EQ(otr_contents->GetColorMode(),
              ui::ColorProviderKey::ColorMode::kDark);
    EXPECT_EQ(otr_contents->GetColorProvider().GetColor(ui::kColorSysPrimary),
              incognito_primary);
  }

  // 6. Enterprise Isolated profile uses a shared light baseline source whether
  // detached or attached to a Regular browser window.
  Profile* isolated_profile = profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  profile_metrics::SetBrowserProfileType(
      isolated_profile,
      profile_metrics::BrowserProfileType::kEnterpriseIsolated);
  ASSERT_TRUE(isolated_profile->IsEnterpriseIsolatedModeProfile());
  std::unique_ptr<content::WebContents> isolated_contents =
      content::WebContentsTester::CreateTestWebContents(isolated_profile,
                                                        nullptr);
  ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
      isolated_contents.get(), nullptr);
  EXPECT_EQ(isolated_contents->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kLight);
  EXPECT_EQ(
      isolated_contents->GetColorProvider().GetColor(ui::kColorSysPrimary),
      isolated_primary);

  WebContentsColorProviderObserver isolated_observer(isolated_contents.get());
  ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
      isolated_contents.get(), tabbed_window->get());
  ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
      isolated_contents.get(), nullptr);
  EXPECT_EQ(isolated_observer.change_count(), 0);
  EXPECT_EQ(isolated_contents->GetColorMode(),
            ui::ColorProviderKey::ColorMode::kLight);
  EXPECT_EQ(
      isolated_contents->GetColorProvider().GetColor(ui::kColorSysPrimary),
      isolated_primary);

  // Null WebContents safety and no-op initial attach on a windowless regular
  // WebContents.
  ChromeWindowColorProviderDelegate::InitWebContentsColorProviderSource(
      nullptr);
  ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
      nullptr, tabbed_window->get());
  std::unique_ptr<content::WebContents> windowless_regular =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  WebContentsColorProviderObserver windowless_observer(
      windowless_regular.get());
  ChromeWindowColorProviderDelegate::InitWebContentsColorProviderSource(
      windowless_regular.get());
  EXPECT_EQ(windowless_observer.change_count(), 0);
}

TEST_P(ThemeServiceAndroidTest,
       ThinWebViewReparentingPreservesHostWindowTheme) {
  theme_service_->SetUserColorAndBrowserColorVariant(
      SK_ColorBLUE, ui::mojom::BrowserColorVariant::kTonalSpot);
  SkColor blue_primary = ExpectedPrimaryColor(
      ui::ColorProviderKey::ColorMode::kLight, SK_ColorBLUE,
      ui::ColorProviderKey::UserColorSource::kAccent,
      ui::ColorProviderKey::SchemeVariant::kTonalSpot);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      host_window = ui::WindowAndroid::CreateForTesting();
  ChromeWindowColorProviderDelegate::AttachToWindow(
      host_window->get(), profile_.get(), /*is_incognito_window=*/false,
      /*is_night_mode=*/false);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      thin_webview_window = ui::WindowAndroid::CreateForTesting();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                        nullptr);
  // Mirror ThinWebViewHost: attach the host ChromeWindow as the
  // ColorProviderSource, then reparent ViewAndroid under ThinWebView's
  // orphaned WindowAndroid.
  ChromeWindowColorProviderDelegate::UpdateWebContentsColorProviderSource(
      web_contents.get(), host_window->get());
  thin_webview_window->get()->AddChild(web_contents->GetNativeView());

  EXPECT_EQ(web_contents->GetColorProvider().GetColor(ui::kColorSysPrimary),
            blue_primary);

  // Subsequent ThemeService updates still propagate through host_window.
  theme_service_->SetUserColorAndBrowserColorVariant(
      SK_ColorGREEN, ui::mojom::BrowserColorVariant::kTonalSpot);
  SkColor green_primary = ExpectedPrimaryColor(
      ui::ColorProviderKey::ColorMode::kLight, SK_ColorGREEN,
      ui::ColorProviderKey::UserColorSource::kAccent,
      ui::ColorProviderKey::SchemeVariant::kTonalSpot);
  EXPECT_EQ(web_contents->GetColorProvider().GetColor(ui::kColorSysPrimary),
            green_primary);
}

TEST_P(ThemeServiceAndroidTest, ProfileDestructionDetachesObservations) {
  std::unique_ptr<TestingProfile> transient_profile =
      TestingProfile::Builder().Build();
  ThemeService* transient_theme_service =
      ThemeServiceFactory::GetForProfile(transient_profile.get());
  ASSERT_NE(transient_theme_service, nullptr);
  transient_theme_service->SetUserColor(SK_ColorRED);

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  ChromeWindowColorProviderDelegate::AttachToWindow(
      window->get(), transient_profile.get(), /*is_incognito_window=*/false,
      /*is_night_mode=*/false);
  EXPECT_EQ(window->get()->GetColorProviderKey().user_color, SK_ColorRED);

  WindowColorProviderObserver observer(window->get());
  transient_profile.reset();
  EXPECT_EQ(observer.change_count(), 1);
  // Accessing ColorProviderKey after profile destruction falls back cleanly
  // without touching the destroyed Profile or ThemeService.
  EXPECT_NE(window->get()->GetColorProviderKey().user_color, SK_ColorRED);
}

INSTANTIATE_TEST_SUITE_P(,
                         ThemeServiceAndroidTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "CacheThemePrefs"
                                             : "UncachedThemePrefs";
                         });

}  // namespace
