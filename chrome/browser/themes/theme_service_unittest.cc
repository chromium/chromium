// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include <cmath>

#include "base/containers/fixed_flat_map.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_service_test_utils.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/search/ntp_features.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/test_native_theme.h"
#include "ui/views/views_features.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"
#include "ui/linux/linux_ui_getter.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

namespace {

enum class ContrastMode { kNonHighContrast, kHighContrast };
enum class SystemTheme { kDefault, kCustom };

// A class that ensures any installed extension is uninstalled before it goes
// out of scope.  This ensures the temporary directory used to load the
// extension is unlocked and can be deleted.
class ThemeScoper {
 public:
  ThemeScoper() = default;
  ThemeScoper(extensions::ExtensionService* extension_service,
              extensions::ExtensionRegistry* extension_registry)
      : extension_service_(extension_service),
        extension_registry_(extension_registry) {}
  ThemeScoper(ThemeScoper&&) noexcept = default;
  ThemeScoper& operator=(ThemeScoper&&) = default;
  ~ThemeScoper() {
    if (!extension_id_.empty() &&
        extension_registry_->GetInstalledExtension(extension_id_)) {
      extension_service_->UninstallExtension(
          extension_id_, extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
    }
  }

  std::string extension_id() const { return extension_id_; }
  void set_extension_id(std::string extension_id) {
    extension_id_ = std::move(extension_id);
  }

  base::FilePath GetTempPath() {
    return temp_dir_.CreateUniqueTempDir() ? temp_dir_.GetPath()
                                           : base::FilePath();
  }

 private:
  raw_ptr<extensions::ExtensionService> extension_service_ = nullptr;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_ = nullptr;
  std::string extension_id_;
  base::ScopedTempDir temp_dir_;
};

#if BUILDFLAG(IS_LINUX)
class LinuxUiGetterImpl : public ui::LinuxUiGetter {
 public:
  explicit LinuxUiGetterImpl(bool use_system_theme)
      : linux_ui_theme_(use_system_theme ? ui::GetDefaultLinuxUiTheme()
                                         : nullptr) {}
  ~LinuxUiGetterImpl() override = default;
  ui::LinuxUiTheme* GetForWindow(aura::Window* window) override {
    return linux_ui_theme_;
  }
  ui::LinuxUiTheme* GetForProfile(Profile* profile) override {
    return linux_ui_theme_;
  }

 private:
  const raw_ptr<ui::LinuxUiTheme> linux_ui_theme_;
};
#endif

}  // namespace

namespace theme_service_internal {

class ThemeServiceTest : public extensions::ExtensionServiceTestBase {
 public:
  ThemeServiceTest() = default;
  ~ThemeServiceTest() override = default;

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams());
    service_->Init();
    registry_ = extensions::ExtensionRegistry::Get(profile());
    ASSERT_TRUE(registry_);
    theme_service_ = ThemeServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(theme_service_);
    pref_service_ = profile_->GetPrefs();
    ASSERT_TRUE(pref_service_);
  }

  ThemeScoper LoadUnpackedTheme(const std::string& source_file_path =
                                    "extensions/theme_minimal/manifest.json") {
    ThemeScoper scoper(service_, registry_);
    test::ThemeServiceChangedWaiter waiter(theme_service_);
    base::FilePath temp_dir = scoper.GetTempPath();
    base::FilePath dst_manifest_path = temp_dir.AppendASCII("manifest.json");
    base::FilePath test_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    base::FilePath src_manifest_path =
        test_data_dir.AppendASCII(source_file_path);
    EXPECT_TRUE(base::CopyFile(src_manifest_path, dst_manifest_path));

    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service_));
    extensions::TestExtensionRegistryObserver observer(registry_);
    installer->Load(temp_dir);
    std::string extenson_id = observer.WaitForExtensionLoaded()->id();
    scoper.set_extension_id(extenson_id);

    waiter.WaitForThemeChanged();

    // Make sure RegisterClient calls for storage are finished to avoid flaky
    // crashes in QuotaManagerImpl::RegisterClient on test shutdown.
    // TODO(crbug.com/1182630) : Remove this when 1182630 is fixed.
    extensions::util::GetStoragePartitionForExtensionId(extenson_id, profile());
    task_environment()->RunUntilIdle();

    return scoper;
  }

  // Update the theme with |extension_id|.
  void UpdateUnpackedTheme(const std::string& extension_id) {
    const base::FilePath& path =
        registry_->GetInstalledExtension(extension_id)->path();

    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service_));

    extensions::TestExtensionRegistryObserver observer(registry_);
    installer->Load(path);
    observer.WaitForExtensionInstalled();

    // Let the ThemeService finish creating the theme pack.
    base::RunLoop().RunUntilIdle();
  }

  bool IsExtensionDisabled(const std::string& id) const {
    return registry_->GetExtensionById(id,
                                       extensions::ExtensionRegistry::DISABLED);
  }

 protected:
  ui::TestNativeTheme test_native_theme_;
  raw_ptr<extensions::ExtensionRegistry> registry_ = nullptr;
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<ThemeService> theme_service_ = nullptr;
};

class ColorProviderTest
    : public ThemeServiceTest,
      public testing::WithParamInterface<
          std::tuple<ui::NativeTheme::ColorScheme, ContrastMode, SystemTheme>> {
 public:
  ColorProviderTest() = default;

  // ThemeServiceTest:
  void SetUp() override {
    ThemeServiceTest::SetUp();

    // Only perform mixer initialization once.
    static bool initialized_mixers = false;
    if (!initialized_mixers) {
#if BUILDFLAG(IS_LINUX)
      // Ensures LinuxUi is configured on supported linux platforms.
      // Initializing the toolkit also adds the native toolkit ColorMixers.
      ui::OzonePlatform::InitParams ozone_params;
      ozone_params.single_process = true;
      ui::OzonePlatform::InitializeForUI(ozone_params);
      auto* linux_ui = ui::GetDefaultLinuxUi();
      ASSERT_TRUE(linux_ui);
      ui::LinuxUi::SetInstance(linux_ui);
#endif  // BUILDFLAG(IS_LINUX)

      // Add the Chrome ColorMixers after native ColorMixers.
      ui::ColorProviderManager::Get().AppendColorProviderInitializer(
          base::BindRepeating(AddChromeColorMixers));

      initialized_mixers = true;
    }
#if BUILDFLAG(IS_LINUX)
    linux_ui_getter_ = std::make_unique<LinuxUiGetterImpl>(
        std::get<SystemTheme>(GetParam()) == SystemTheme::kCustom);
#endif  // BUILDFLAG(IS_LINUX)

    const auto param_tuple = GetParam();
    auto color_scheme = std::get<ui::NativeTheme::ColorScheme>(param_tuple);
    const auto contrast_mode = std::get<ContrastMode>(param_tuple);
    const auto system_theme = std::get<SystemTheme>(param_tuple);

    // ThemeService tracks the global NativeTheme instance for native UI. Update
    // this to reflect test params and propagate any updates.
    native_theme_ = ui::NativeTheme::GetInstanceForNativeUi();
#if BUILDFLAG(IS_LINUX)
    if (system_theme == SystemTheme::kCustom)
      native_theme_ = ui::GetDefaultLinuxUiTheme()->GetNativeTheme();
#endif
    original_forced_colors_ = native_theme_->InForcedColorsMode();
    original_preferred_contrast_ = native_theme_->GetPreferredContrast();
    original_should_use_dark_colors_ = native_theme_->ShouldUseDarkColors();

    const bool high_contrast = contrast_mode == ContrastMode::kHighContrast;
#if BUILDFLAG(IS_WIN)
    if (high_contrast)
      color_scheme = ui::NativeTheme::ColorScheme::kPlatformHighContrast;
    native_theme_->set_forced_colors(high_contrast);
#endif  // BUILDFLAG(IS_WIN)
    native_theme_->SetPreferredContrast(
        high_contrast ? ui::NativeTheme::PreferredContrast::kMore
                      : ui::NativeTheme::PreferredContrast::kNoPreference);
    native_theme_->set_use_dark_colors(color_scheme ==
                                       ui::NativeTheme::ColorScheme::kDark);

    // If native_theme_ has changed, call
    // NativeTheme::NotifyOnNativeThemeUpdated to notify observers that the
    // NativeTheme has been updated so that the ThemeService will know to update
    // its ThemeSupplier to match the NativeTheme. The ColorProvider cache will
    // also be reset.
    if (original_forced_colors_ != native_theme_->InForcedColorsMode() ||
        original_preferred_contrast_ != native_theme_->GetPreferredContrast() ||
        original_should_use_dark_colors_ !=
            native_theme_->ShouldUseDarkColors()) {
      native_theme_->NotifyOnNativeThemeUpdated();
    }

    // Update ThemeService to use the system theme if necessary.
    if (system_theme == SystemTheme::kCustom) {
      theme_service_->UseSystemTheme();
    } else {
      theme_service_->UseDefaultTheme();
    }
  }

  void TearDown() override {
    // Restore the original NativeTheme parameters.
    native_theme_->set_forced_colors(original_forced_colors_);
    native_theme_->SetPreferredContrast(original_preferred_contrast_);
    native_theme_->set_use_dark_colors(original_should_use_dark_colors_);
    native_theme_->NotifyOnNativeThemeUpdated();
    ThemeServiceTest::TearDown();
  }

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<
          std::tuple<ui::NativeTheme::ColorScheme, ContrastMode, SystemTheme>>
          param_info) {
    auto param_tuple = param_info.param;
    return ColorSchemeToString(
               std::get<ui::NativeTheme::ColorScheme>(param_tuple)) +
           ContrastModeToString(std::get<ContrastMode>(param_tuple)) +
           SystemThemeToString(std::get<SystemTheme>(param_tuple));
  }

  SkColor GetColor(ui::ColorId id) const {
    const auto* const color_provider =
        ui::ColorProviderManager::Get().GetColorProviderFor(
            native_theme_->GetColorProviderKey(nullptr));
    return color_provider->GetColor(id);
  }

 private:
  static std::string ColorSchemeToString(ui::NativeTheme::ColorScheme scheme) {
    switch (scheme) {
      case ui::NativeTheme::ColorScheme::kDefault:
        NOTREACHED()
            << "Cannot unit test kDefault as it depends on machine state.";
        return "InvalidColorScheme";
      case ui::NativeTheme::ColorScheme::kLight:
        return "kLight";
      case ui::NativeTheme::ColorScheme::kDark:
        return "kDark";
      case ui::NativeTheme::ColorScheme::kPlatformHighContrast:
        return "kPlatformHighContrast";
    }
  }

  static std::string ContrastModeToString(ContrastMode contrast_mode) {
    return contrast_mode == ContrastMode::kHighContrast ? "HighContrast" : "";
  }

  static std::string SystemThemeToString(SystemTheme system_theme) {
    return system_theme == SystemTheme::kCustom ? "SystemTheme" : "";
  }

  // Store the parameter values of the global NativeTheme for UI instance
  // configured during SetUp() to check if an update should be propagated and
  // to restore the NativeTheme to its original state in TearDown().
  bool original_forced_colors_ = false;
  ui::NativeTheme::PreferredContrast original_preferred_contrast_ =
      ui::NativeTheme::PreferredContrast::kNoPreference;
  bool original_should_use_dark_colors_ = false;
  raw_ptr<ui::NativeTheme> native_theme_;
#if BUILDFLAG(IS_LINUX)
  std::unique_ptr<ui::LinuxUiGetter> linux_ui_getter_;
#endif
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ColorProviderTest,
    ::testing::Combine(::testing::Values(ui::NativeTheme::ColorScheme::kLight,
                                         ui::NativeTheme::ColorScheme::kDark),
                       ::testing::Values(ContrastMode::kNonHighContrast,
                                         ContrastMode::kHighContrast),
                       ::testing::Values(SystemTheme::kDefault,
                                         SystemTheme::kCustom)),
    ColorProviderTest::ParamInfoToString);

// Installs then uninstalls a theme and makes sure that the ThemeService
// reverts to the default theme after the uninstall.
TEST_F(ThemeServiceTest, ThemeInstallUninstall) {
  ThemeScoper scoper = LoadUnpackedTheme();
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_TRUE(theme_service_->UsingExtensionTheme());
  EXPECT_EQ(scoper.extension_id(), theme_service_->GetThemeID());

  // Now uninstall the extension, should revert to the default theme.
  service_->UninstallExtension(
      scoper.extension_id(), extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
  EXPECT_TRUE(theme_service_->UsingDefaultTheme());
  EXPECT_FALSE(theme_service_->UsingExtensionTheme());
}

// Test that a theme extension is disabled when not in use. A theme may be
// installed but not in use if it there is an infobar to revert to the previous
// theme.
TEST_F(ThemeServiceTest, DisableUnusedTheme) {
  // 1) Installing a theme should disable the previously active theme.
  ThemeScoper scoper1 = LoadUnpackedTheme();
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_EQ(scoper1.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(scoper1.extension_id()));

  // Create theme reinstaller to prevent the current theme from being
  // uninstalled.
  std::unique_ptr<ThemeService::ThemeReinstaller> reinstaller =
      theme_service_->BuildReinstallerForCurrentTheme();

  ThemeScoper scoper2 = LoadUnpackedTheme();
  EXPECT_EQ(scoper2.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(scoper2.extension_id()));
  EXPECT_TRUE(IsExtensionDisabled(scoper1.extension_id()));

  // 2) Enabling a disabled theme extension should swap the current theme.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service_);
    service_->EnableExtension(scoper1.extension_id());
    waiter.WaitForThemeChanged();
  }
  EXPECT_EQ(scoper1.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(scoper1.extension_id()));
  EXPECT_TRUE(IsExtensionDisabled(scoper2.extension_id()));

  // 3) Using RevertToExtensionTheme() with a disabled theme should enable and
  // set the theme. This is the case when the user reverts to the previous theme
  // via an infobar.
  {
    test::ThemeServiceChangedWaiter waiter(theme_service_);
    theme_service_->RevertToExtensionTheme(scoper2.extension_id());
    waiter.WaitForThemeChanged();
  }
  EXPECT_EQ(scoper2.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(scoper2.extension_id()));
  EXPECT_TRUE(IsExtensionDisabled(scoper1.extension_id()));

  // 4) Disabling the current theme extension should revert to the default theme
  // and disable any installed theme extensions.
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  service_->DisableExtension(scoper2.extension_id(),
                             extensions::disable_reason::DISABLE_USER_ACTION);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(theme_service_->UsingDefaultTheme());
  EXPECT_FALSE(service_->IsExtensionEnabled(scoper1.extension_id()));
  EXPECT_FALSE(service_->IsExtensionEnabled(scoper2.extension_id()));
}

// Test the ThemeService's behavior when a theme is upgraded.
TEST_F(ThemeServiceTest, ThemeUpgrade) {
  std::unique_ptr<ThemeService::ThemeReinstaller> reinstaller =
      theme_service_->BuildReinstallerForCurrentTheme();

  ThemeScoper scoper1 = LoadUnpackedTheme();
  ThemeScoper scoper2 = LoadUnpackedTheme();

  // Test the initial state.
  EXPECT_TRUE(IsExtensionDisabled(scoper1.extension_id()));
  EXPECT_EQ(scoper2.extension_id(), theme_service_->GetThemeID());

  // 1) Upgrading the current theme should not revert to the default theme.
  test::ThemeServiceChangedWaiter waiter(theme_service_);
  UpdateUnpackedTheme(scoper2.extension_id());

  // The ThemeService should have sent an theme change notification even though
  // the id of the current theme did not change.
  waiter.WaitForThemeChanged();

  EXPECT_EQ(scoper2.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(IsExtensionDisabled(scoper1.extension_id()));

  // 2) Upgrading a disabled theme should not change the current theme.
  UpdateUnpackedTheme(scoper1.extension_id());
  EXPECT_EQ(scoper2.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(IsExtensionDisabled(scoper1.extension_id()));
}

TEST_F(ThemeServiceTest, NTPLogoAlternate) {
  // TODO(https://crbug.com/1039006): Fix ScopedTempDir deletion errors on Win.

  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile());
  {
    ThemeScoper scoper =
        LoadUnpackedTheme("extensions/theme_grey_ntp/manifest.json");
    // When logo alternate is not specified and ntp is grey, logo should be
    // colorful.
    EXPECT_EQ(0, theme_provider.GetDisplayProperty(
                     ThemeProperties::NTP_LOGO_ALTERNATE));
  }

  {
    ThemeScoper scoper =
        LoadUnpackedTheme("extensions/theme_grey_ntp_white_logo/manifest.json");
    // Logo alternate should match what is specified in the manifest.
    EXPECT_EQ(1, theme_provider.GetDisplayProperty(
                     ThemeProperties::NTP_LOGO_ALTERNATE));
  }

  {
    ThemeScoper scoper = LoadUnpackedTheme(
        "extensions/theme_color_ntp_white_logo/manifest.json");
    // When logo alternate is not specified and ntp is colorful, logo should be
    // white.
    EXPECT_EQ(1, theme_provider.GetDisplayProperty(
                     ThemeProperties::NTP_LOGO_ALTERNATE));
  }

  {
    ThemeScoper scoper = LoadUnpackedTheme(
        "extensions/theme_color_ntp_colorful_logo/manifest.json");
    // Logo alternate should match what is specified in the manifest.
    EXPECT_EQ(0, theme_provider.GetDisplayProperty(
                     ThemeProperties::NTP_LOGO_ALTERNATE));
  }
}

// crbug.com/468280
TEST_F(ThemeServiceTest, UninstallThemeWhenNoReinstallers) {
  ThemeScoper scoper1 = LoadUnpackedTheme();
  ASSERT_EQ(scoper1.extension_id(), theme_service_->GetThemeID());

  ThemeScoper scoper2;
  {
    // Show an infobar.
    std::unique_ptr<ThemeService::ThemeReinstaller> reinstaller =
        theme_service_->BuildReinstallerForCurrentTheme();

    // Install another theme. The first extension shouldn't be uninstalled yet
    // as it should be possible to revert to it.
    scoper2 = LoadUnpackedTheme();
    EXPECT_TRUE(IsExtensionDisabled(scoper1.extension_id()));
    EXPECT_EQ(scoper2.extension_id(), theme_service_->GetThemeID());

    test::ThemeServiceChangedWaiter waiter(theme_service_);
    reinstaller->Reinstall();
    waiter.WaitForThemeChanged();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(IsExtensionDisabled(scoper2.extension_id()));
    EXPECT_EQ(scoper1.extension_id(), theme_service_->GetThemeID());
  }

  // extension 2 should get uninstalled as no reinstallers are in scope.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(registry_->GetInstalledExtension(scoper2.extension_id()));
}

TEST_F(ThemeServiceTest, BuildFromColorTest) {
  // Set theme from color.
  theme_service_->UseDefaultTheme();
  EXPECT_TRUE(theme_service_->UsingDefaultTheme());
  EXPECT_FALSE(theme_service_->UsingAutogeneratedTheme());
  theme_service_->BuildAutogeneratedThemeFromColor(
      SkColorSetRGB(100, 100, 100));
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_TRUE(theme_service_->UsingAutogeneratedTheme());

  // Set theme from data pack and then override it with theme from color.
  ThemeScoper scoper = LoadUnpackedTheme();
  EXPECT_EQ(scoper.extension_id(), theme_service_->GetThemeID());
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_FALSE(theme_service_->UsingAutogeneratedTheme());
  base::FilePath path =
      pref_service_->GetFilePath(prefs::kCurrentThemePackFilename);
  EXPECT_FALSE(path.empty());

  theme_service_->BuildAutogeneratedThemeFromColor(
      SkColorSetRGB(100, 100, 100));
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_TRUE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_EQ(ThemeService::kAutogeneratedThemeID, theme_service_->GetThemeID());
  path = pref_service_->GetFilePath(prefs::kCurrentThemePackFilename);
  EXPECT_TRUE(path.empty());
}

TEST_F(ThemeServiceTest, BuildFromColor_DisableExtensionTest) {
  ThemeScoper scoper = LoadUnpackedTheme();
  EXPECT_EQ(scoper.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(scoper.extension_id()));

  // Setting autogenerated theme should disable previous theme.
  theme_service_->BuildAutogeneratedThemeFromColor(
      SkColorSetRGB(100, 100, 100));
  EXPECT_TRUE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_FALSE(service_->IsExtensionEnabled(scoper.extension_id()));
}

TEST_F(ThemeServiceTest, UseDefaultTheme_DisableExtensionTest) {
  ThemeScoper scoper = LoadUnpackedTheme();
  EXPECT_EQ(scoper.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(scoper.extension_id()));

  // Resetting to default theme should disable previous theme.
  theme_service_->UseDefaultTheme();
  EXPECT_FALSE(service_->IsExtensionEnabled(scoper.extension_id()));
}

// Test that setting theme to default resets the NTP theme as well.
TEST_F(ThemeServiceTest, UseDefaultTheme_DisableNtpThemeTest) {
  // Turn on Customize Chrome Side Panel.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ntp_features::kCustomizeChromeSidePanel);

  base::Value::Dict test_background_info;
  test_background_info.Set("test_data", "foo");
  pref_service_->SetDict(prefs::kNtpCustomBackgroundDict,
                         std::move(test_background_info));

  const base::Value::Dict& background_info_with_theme =
      pref_service_->GetDict(prefs::kNtpCustomBackgroundDict);
  const base::Value* test_data = background_info_with_theme.Find("test_data");
  EXPECT_NE(test_data, nullptr);
  EXPECT_NE(test_data->GetIfString(), nullptr);

  theme_service_->UseDefaultTheme();
  const base::Value::Dict& background_info_without_theme =
      pref_service_->GetDict(prefs::kNtpCustomBackgroundDict);
  EXPECT_EQ(background_info_without_theme.Find("test_data"), nullptr);
}

TEST_P(ColorProviderTest, OmniboxContrast) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1336315): Windows platform high contrast colors are not
  // sufficiently high-contrast to pass this test.
  if (std::get<ContrastMode>(GetParam()) == ContrastMode::kHighContrast)
    return;
#endif

  constexpr ui::ColorId contrasting_ids[][2] = {
      {kColorOmniboxResultsIcon, kColorOmniboxResultsBackground},
      {kColorOmniboxResultsIcon, kColorOmniboxResultsBackgroundHovered},
      {kColorOmniboxResultsIconSelected,
       kColorOmniboxResultsBackgroundSelected},
      {kColorOmniboxResultsTextSelected,
       kColorOmniboxResultsBackgroundSelected},
      {kColorOmniboxResultsTextDimmed, kColorOmniboxResultsBackground},
      {kColorOmniboxResultsTextDimmed, kColorOmniboxResultsBackgroundHovered},
      {kColorOmniboxResultsTextDimmedSelected,
       kColorOmniboxResultsBackgroundSelected},
      {kColorOmniboxResultsTextNegative, kColorOmniboxResultsBackground},
      {kColorOmniboxResultsTextNegative, kColorOmniboxResultsBackgroundHovered},
      {kColorOmniboxResultsTextNegativeSelected,
       kColorOmniboxResultsBackgroundSelected},
      {kColorOmniboxResultsTextPositive, kColorOmniboxResultsBackground},
      {kColorOmniboxResultsTextPositive, kColorOmniboxResultsBackgroundHovered},
      {kColorOmniboxResultsTextPositiveSelected,
       kColorOmniboxResultsBackgroundSelected},
      {kColorOmniboxResultsTextSecondary, kColorOmniboxResultsBackground},
      {kColorOmniboxResultsTextSecondary,
       kColorOmniboxResultsBackgroundHovered},
      {kColorOmniboxResultsTextSecondarySelected,
       kColorOmniboxResultsBackgroundSelected},
      {kColorOmniboxResultsUrl, kColorOmniboxResultsBackground},
      {kColorOmniboxResultsUrl, kColorOmniboxResultsBackgroundHovered},
      {kColorOmniboxResultsUrlSelected, kColorOmniboxResultsBackgroundSelected},
      {kColorOmniboxBubbleOutline, kColorOmniboxResultsBackground},
      {kColorOmniboxBubbleOutlineExperimentalKeywordMode,
       kColorOmniboxResultsBackground},
      {kColorOmniboxSecurityChipDefault, kColorToolbarBackgroundSubtleEmphasis},
      {kColorOmniboxSecurityChipDefault,
       kColorToolbarBackgroundSubtleEmphasisHovered},
      {kColorOmniboxSecurityChipSecure, kColorToolbarBackgroundSubtleEmphasis},
      {kColorOmniboxSecurityChipSecure,
       kColorToolbarBackgroundSubtleEmphasisHovered},
      {kColorOmniboxSecurityChipDangerous,
       kColorToolbarBackgroundSubtleEmphasis},
      {kColorOmniboxSecurityChipDangerous,
       kColorToolbarBackgroundSubtleEmphasisHovered},
      {kColorOmniboxKeywordSelected, kColorToolbarBackgroundSubtleEmphasis},
      {kColorOmniboxKeywordSelected,
       kColorToolbarBackgroundSubtleEmphasisHovered},
      {kColorOmniboxText, kColorToolbarBackgroundSubtleEmphasis},
      {kColorOmniboxText, kColorToolbarBackgroundSubtleEmphasisHovered},
      {kColorOmniboxText, kColorOmniboxResultsBackground},
      {kColorOmniboxText, kColorOmniboxResultsBackgroundHovered},
      {kColorOmniboxTextDimmed, kColorToolbarBackgroundSubtleEmphasis},
      {kColorOmniboxTextDimmed, kColorToolbarBackgroundSubtleEmphasisHovered},
  };
  auto check_sufficient_contrast =
      [&](ui::ColorId id1, ui::ColorId id2,
          float expected_contrast_ratio =
              color_utils::kMinimumReadableContrastRatio) {
        const theme_service::test::PrintableSkColor color1{GetColor(id1)};
        const theme_service::test::PrintableSkColor color2{GetColor(id2)};
        const float contrast =
            color_utils::GetContrastRatio(color1.color, color2.color);
        EXPECT_GE(contrast, expected_contrast_ratio)
            << "Color 1: " << theme_service::test::ColorIdToString(id1) << " - "
            << color1
            << "\nColor 2: " << theme_service::test::ColorIdToString(id2)
            << " - " << color2;
      };
  for (const ui::ColorId* ids : contrasting_ids)
    check_sufficient_contrast(ids[0], ids[1]);
#if !BUILDFLAG(USE_GTK) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1336796): GTK and LaCrOS do not have a sufficiently
  // high-contrast selected row color to pass this test.
  if (std::get<ContrastMode>(GetParam()) == ContrastMode::kHighContrast) {
    check_sufficient_contrast(kColorOmniboxResultsBackgroundSelected,
                              kColorOmniboxResultsBackground,
                              color_utils::kMinimumVisibleContrastRatio);
  }
#endif
}

// Sets and unsets themes using the BrowserThemeColor policy.
TEST_F(ThemeServiceTest, PolicyThemeColorSet) {
  theme_service_->UseDefaultTheme();
  EXPECT_TRUE(theme_service_->UsingDefaultTheme());
  EXPECT_FALSE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_FALSE(theme_service_->UsingPolicyTheme());

  // Setting a blank policy color shouldn't cause any theme updates.
  pref_service_->ClearPref(prefs::kPolicyThemeColor);
  EXPECT_TRUE(theme_service_->UsingDefaultTheme());
  EXPECT_FALSE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_FALSE(theme_service_->UsingPolicyTheme());

  // Setting a valid policy color causes theme to update. The applied theme is
  // autogenerated based on the policy color.
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyThemeColor, std::make_unique<base::Value>(100));
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_TRUE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_TRUE(theme_service_->UsingPolicyTheme());
  // Policy theme is not saved in prefs.
  EXPECT_EQ(theme_service_->GetThemeID(), std::string());

  // Unsetting policy theme and setting autogenerated theme.
  profile_->GetTestingPrefService()->RemoveManagedPref(
      prefs::kPolicyThemeColor);
  theme_service_->BuildAutogeneratedThemeFromColor(
      SkColorSetRGB(100, 100, 100));
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_TRUE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_FALSE(theme_service_->UsingPolicyTheme());
  EXPECT_EQ(ThemeService::kAutogeneratedThemeID, theme_service_->GetThemeID());

  // Setting a different policy color.
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyThemeColor, std::make_unique<base::Value>(-100));
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_TRUE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_TRUE(theme_service_->UsingPolicyTheme());
  EXPECT_EQ(ThemeService::kAutogeneratedThemeID, theme_service_->GetThemeID());

  // Removing policy color reverts the theme to the one saved in prefs, or
  // the default theme if prefs are empty.
  profile_->GetTestingPrefService()->RemoveManagedPref(
      prefs::kPolicyThemeColor);
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_TRUE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_FALSE(theme_service_->UsingPolicyTheme());
  EXPECT_EQ(ThemeService::kAutogeneratedThemeID, theme_service_->GetThemeID());

  // Install extension theme.
  ThemeScoper scoper = LoadUnpackedTheme();
  EXPECT_TRUE(theme_service_->UsingExtensionTheme());
  EXPECT_FALSE(theme_service_->UsingPolicyTheme());
  EXPECT_EQ(scoper.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(scoper.extension_id()));
  EXPECT_TRUE(registry_->GetInstalledExtension(scoper.extension_id()));

  // Applying policy theme should unset the extension theme but not disable the
  // extension..
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyThemeColor, std::make_unique<base::Value>(100));
  EXPECT_FALSE(theme_service_->UsingExtensionTheme());
  EXPECT_TRUE(theme_service_->UsingPolicyTheme());
  EXPECT_EQ(scoper.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(scoper.extension_id()));
  EXPECT_TRUE(registry_->GetInstalledExtension(scoper.extension_id()));

  // Cannot set other themes while a policy theme is applied.
  theme_service_->BuildAutogeneratedThemeFromColor(
      SkColorSetRGB(100, 100, 100));
  theme_service_->UseDefaultTheme();
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_TRUE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_FALSE(theme_service_->UsingExtensionTheme());
  EXPECT_TRUE(theme_service_->UsingPolicyTheme());

  // Removing policy color unsets the policy theme and restores the extension
  // theme.
  profile_->GetTestingPrefService()->RemoveManagedPref(
      prefs::kPolicyThemeColor);
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_FALSE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_TRUE(theme_service_->UsingExtensionTheme());
  EXPECT_FALSE(theme_service_->UsingPolicyTheme());
  EXPECT_EQ(scoper.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(scoper.extension_id()));
  EXPECT_TRUE(registry_->GetInstalledExtension(scoper.extension_id()));
}

}  // namespace theme_service_internal
