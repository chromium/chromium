// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/increased_contrast_theme_supplier.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/test_native_theme.h"
#include "ui/views/views_features.h"

#if BUILDFLAG(USE_GTK)
#include "ui/gtk/gtk_ui_factory.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/linux_ui/linux_ui.h"
#endif  // BUILDFLAG(USE_GTK)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

namespace {

enum class ContrastMode { kNonHighContrast, kHighContrast };
enum class SystemTheme { kDefault, kCustom };

// Struct to distinguish SkColor (aliased to uint32_t) for printing.
struct PrintableSkColor {
  bool operator==(const PrintableSkColor& other) const {
    return color == other.color;
  }

  bool operator!=(const PrintableSkColor& other) const {
    return !operator==(other);
  }

  const SkColor color;
};

std::ostream& operator<<(std::ostream& os, PrintableSkColor printable_color) {
  SkColor color = printable_color.color;
  return os << base::StringPrintf("SkColorARGB(0x%02x, 0x%02x, 0x%02x, 0x%02x)",
                                  SkColorGetA(color), SkColorGetR(color),
                                  SkColorGetG(color), SkColorGetB(color));
}

std::string ColorIdToString(int id) {
#define E(color_id, theme_property_id, ...) \
  {theme_property_id, #theme_property_id},
#define E_CPONLY(color_id)

  static constexpr const auto kMap =
      base::MakeFixedFlatMap<int, const char*>({CHROME_COLOR_IDS});

#undef E
#undef E_CPONLY
  constexpr char kPrefix[] = "ThemeProperties::";

  std::string id_str = kMap.find(id)->second;
  if (base::StartsWith(id_str, kPrefix))
    return id_str.substr(strlen(kPrefix));
  return id_str;
}

std::pair<PrintableSkColor, PrintableSkColor> GetOriginalAndRedirected(
    const ui::ThemeProvider& theme_provider,
    int color_id) {
  PrintableSkColor original{theme_provider.GetColor(color_id)};

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kColorProviderRedirectionForThemeProvider);
  PrintableSkColor redirected{theme_provider.GetColor(color_id)};

  return std::make_pair(original, redirected);
}

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

}  // namespace

namespace theme_service_internal {

class ThemeServiceTest : public extensions::ExtensionServiceTestBase {
 public:
  ThemeServiceTest() = default;
  ~ThemeServiceTest() override = default;

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    extensions::ExtensionServiceTestBase::ExtensionServiceInitParams params =
        CreateDefaultInitParams();
    params.pref_file = base::FilePath();
    InitializeExtensionService(params);
    service_->Init();
    registry_ = extensions::ExtensionRegistry::Get(profile());
    ASSERT_TRUE(registry_);
    theme_service_ = ThemeServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(theme_service_);
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

  void set_theme_supplier(ThemeService* theme_service,
                          scoped_refptr<CustomThemeSupplier> theme_supplier) {
    theme_service->theme_supplier_ = theme_supplier;
  }

  SkColor GetColor(ThemeService* theme_service, int id, bool incognito) const {
    return theme_service->theme_helper_.GetColor(
        id, incognito, theme_service->GetThemeSupplier());
  }

  bool IsExtensionDisabled(const std::string& id) const {
    return registry_->GetExtensionById(id,
                                       extensions::ExtensionRegistry::DISABLED);
  }

 protected:
  ui::TestNativeTheme native_theme_;
  raw_ptr<extensions::ExtensionRegistry> registry_ = nullptr;
  raw_ptr<ThemeService> theme_service_ = nullptr;
};

class ThemeProviderRedirectedEquivalenceTest
    : public ThemeServiceTest,
      public testing::WithParamInterface<
          std::tuple<ui::NativeTheme::ColorScheme, ContrastMode, SystemTheme>> {
 public:
  ThemeProviderRedirectedEquivalenceTest() = default;

  // ThemeServiceTest:
  void SetUp() override {
    ThemeServiceTest::SetUp();

    // Only perform mixer initialization once.
    static bool initialized_mixers = false;
    if (!initialized_mixers) {
#if BUILDFLAG(USE_GTK)
      // Ensures GTK is configured on supported linux platforms. Initializing
      // GTK also adds the native GTK ColorMixers.
      ui::OzonePlatform::InitParams ozone_params;
      ozone_params.single_process = true;
      ui::OzonePlatform::InitializeForUI(ozone_params);
      auto linux_ui = BuildGtkUi();
      linux_ui->SetUseSystemThemeCallback(base::BindRepeating(
          [](bool use_system_theme, aura::Window* window) {
            return use_system_theme;
          },
          std::get<SystemTheme>(GetParam()) == SystemTheme::kCustom));
      linux_ui->Initialize();
      views::LinuxUI::SetInstance(std::move(linux_ui));
#endif  // BUILDFLAG(USE_GTK)

      // Add the Chrome ColorMixers after native ColorMixers.
      ui::ColorProviderManager::Get().AppendColorProviderInitializer(
          base::BindRepeating(AddChromeColorMixers));

      initialized_mixers = true;
    }

    const auto param_tuple = GetParam();
    auto color_scheme = std::get<ui::NativeTheme::ColorScheme>(param_tuple);
    const auto contrast_mode = std::get<ContrastMode>(param_tuple);
    const auto system_theme = std::get<SystemTheme>(param_tuple);

    // ThemeService tracks the global NativeTheme instance for native UI. Update
    // this to reflect test params and propagate any updates.
    ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
    original_forced_colors_ = native_theme->InForcedColorsMode();
    original_preferred_contrast_ = native_theme->GetPreferredContrast();
    original_should_use_dark_colors_ = native_theme->ShouldUseDarkColors();

    const bool high_contrast = contrast_mode == ContrastMode::kHighContrast;
#if BUILDFLAG(IS_WIN)
    if (high_contrast)
      color_scheme = ui::NativeTheme::ColorScheme::kPlatformHighContrast;
    native_theme->set_forced_colors(high_contrast);
#endif  // BUILDFLAG(IS_WIN)
    native_theme->set_preferred_contrast(
        high_contrast ? ui::NativeTheme::PreferredContrast::kMore
                      : ui::NativeTheme::PreferredContrast::kNoPreference);
    native_theme->set_use_dark_colors(color_scheme ==
                                      ui::NativeTheme::ColorScheme::kDark);

    // If native_theme has changed, call NativeTheme::NotifyOnNativeThemeUpdated
    // to notify observers that the NativeTheme has been updated so that the
    // ThemeService will know to update its ThemeSupplier to match the
    // NativeTheme. The ColorProvider cache will also be reset.
    if (original_forced_colors_ != native_theme->InForcedColorsMode() ||
        original_preferred_contrast_ != native_theme->GetPreferredContrast() ||
        original_should_use_dark_colors_ !=
            native_theme->ShouldUseDarkColors()) {
      native_theme->NotifyOnNativeThemeUpdated();
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
    ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
    native_theme->set_forced_colors(original_forced_colors_);
    native_theme->set_preferred_contrast(original_preferred_contrast_);
    native_theme->set_use_dark_colors(original_should_use_dark_colors_);
    native_theme->NotifyOnNativeThemeUpdated();
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
};

#define E(color_id, theme_property_id, ...) theme_property_id,
#define E_CPONLY(color_id)
static constexpr int kTestIdValues[] = {CHROME_COLOR_IDS};
#undef E
#undef E_CPONLY

INSTANTIATE_TEST_SUITE_P(
    ,
    ThemeProviderRedirectedEquivalenceTest,
    ::testing::Combine(::testing::Values(ui::NativeTheme::ColorScheme::kLight,
                                         ui::NativeTheme::ColorScheme::kDark),
                       ::testing::Values(ContrastMode::kNonHighContrast,
                                         ContrastMode::kHighContrast),
                       ::testing::Values(SystemTheme::kDefault,
                                         SystemTheme::kCustom)),
    ThemeProviderRedirectedEquivalenceTest::ParamInfoToString);

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
      profile_->GetPrefs()->GetFilePath(prefs::kCurrentThemePackFilename);
  EXPECT_FALSE(path.empty());

  theme_service_->BuildAutogeneratedThemeFromColor(
      SkColorSetRGB(100, 100, 100));
  EXPECT_FALSE(theme_service_->UsingDefaultTheme());
  EXPECT_TRUE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_EQ(ThemeService::kAutogeneratedThemeID, theme_service_->GetThemeID());
  path = profile_->GetPrefs()->GetFilePath(prefs::kCurrentThemePackFilename);
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

TEST_F(ThemeServiceTest, OmniboxContrast) {
  using TP = ThemeProperties;
  for (bool dark : {false, true}) {
    native_theme_.SetDarkMode(dark);
    for (bool high_contrast : {false, true}) {
      set_theme_supplier(
          theme_service_,
          high_contrast ? base::MakeRefCounted<IncreasedContrastThemeSupplier>(
                              &native_theme_)
                        : nullptr);
      constexpr int contrasting_ids[][2] = {
          {TP::COLOR_OMNIBOX_RESULTS_ICON, TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_RESULTS_ICON,
           TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED},
          {TP::COLOR_OMNIBOX_RESULTS_ICON_SELECTED,
           TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED,
           TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED, TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED,
           TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED,
           TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE,
           TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE,
           TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_NEGATIVE_SELECTED,
           TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE,
           TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE,
           TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_POSITIVE_SELECTED,
           TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY,
           TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY,
           TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY_SELECTED,
           TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED},
          {TP::COLOR_OMNIBOX_RESULTS_URL, TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_RESULTS_URL, TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED},
          {TP::COLOR_OMNIBOX_RESULTS_URL_SELECTED,
           TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED},
          {TP::COLOR_OMNIBOX_BUBBLE_OUTLINE, TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE,
           TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT,
           TP::COLOR_OMNIBOX_BACKGROUND},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT,
           TP::COLOR_OMNIBOX_BACKGROUND_HOVERED},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_SECURE,
           TP::COLOR_OMNIBOX_BACKGROUND},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_SECURE,
           TP::COLOR_OMNIBOX_BACKGROUND_HOVERED},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS,
           TP::COLOR_OMNIBOX_BACKGROUND},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS,
           TP::COLOR_OMNIBOX_BACKGROUND_HOVERED},
          {TP::COLOR_OMNIBOX_SELECTED_KEYWORD, TP::COLOR_OMNIBOX_BACKGROUND},
          {TP::COLOR_OMNIBOX_SELECTED_KEYWORD,
           TP::COLOR_OMNIBOX_BACKGROUND_HOVERED},
          {TP::COLOR_OMNIBOX_TEXT, TP::COLOR_OMNIBOX_BACKGROUND},
          {TP::COLOR_OMNIBOX_TEXT, TP::COLOR_OMNIBOX_BACKGROUND_HOVERED},
          {TP::COLOR_OMNIBOX_TEXT, TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_TEXT, TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED},
          {TP::COLOR_OMNIBOX_TEXT_DIMMED, TP::COLOR_OMNIBOX_BACKGROUND},
          {TP::COLOR_OMNIBOX_TEXT_DIMMED, TP::COLOR_OMNIBOX_BACKGROUND_HOVERED},
      };
      auto check_sufficient_contrast = [&](int id1, int id2) {
        const float contrast =
            color_utils::GetContrastRatio(GetColor(theme_service_, id1, dark),
                                          GetColor(theme_service_, id2, dark));
        EXPECT_GE(contrast, color_utils::kMinimumReadableContrastRatio)
            << "Dark: " << dark << " High contrast: " << high_contrast
            << " ID 1: " << id1 << " ID2: " << id2;
      };
      for (const int* ids : contrasting_ids)
        check_sufficient_contrast(ids[0], ids[1]);
      if (high_contrast)
        check_sufficient_contrast(TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED,
                                  TP::COLOR_OMNIBOX_RESULTS_BG);
    }
  }
}

TEST_F(ThemeServiceTest, NativeIncreasedContrastChanged) {
  theme_service_->UseDefaultTheme();

  native_theme_.SetUserHasContrastPreference(true);
  theme_service_->OnNativeThemeUpdated(&native_theme_);
  EXPECT_TRUE(theme_service_->UsingDefaultTheme());
  bool using_increased_contrast =
      theme_service_->GetThemeSupplier() &&
      theme_service_->GetThemeSupplier()->get_theme_type() ==
          CustomThemeSupplier::ThemeType::INCREASED_CONTRAST;
  bool expecting_increased_contrast =
      theme_service_->theme_helper_for_testing()
          .ShouldUseIncreasedContrastThemeSupplier(&native_theme_);
  EXPECT_EQ(using_increased_contrast, expecting_increased_contrast);

  native_theme_.SetUserHasContrastPreference(false);
  theme_service_->OnNativeThemeUpdated(&native_theme_);
  EXPECT_TRUE(theme_service_->UsingDefaultTheme());
  EXPECT_EQ(theme_service_->GetThemeSupplier(), nullptr);
}

// Sets and unsets themes using the BrowserThemeColor policy.
TEST_F(ThemeServiceTest, PolicyThemeColorSet) {
  theme_service_->UseDefaultTheme();
  EXPECT_TRUE(theme_service_->UsingDefaultTheme());
  EXPECT_FALSE(theme_service_->UsingAutogeneratedTheme());
  EXPECT_FALSE(theme_service_->UsingPolicyTheme());

  // Setting a blank policy color shouldn't cause any theme updates.
  profile_->GetPrefs()->ClearPref(prefs::kPolicyThemeColor);
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

// TODO(crbug.com/1056953): Enable on Linux GTK.
#if BUILDFLAG(USE_GTK)
#define MAYBE_GetColor DISABLED_GetColor
#else
#define MAYBE_GetColor GetColor
#endif
TEST_P(ThemeProviderRedirectedEquivalenceTest, MAYBE_GetColor) {
  static constexpr const auto kTolerances = base::MakeFixedFlatMap<int, int>(
      {{ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY, 1},
       {ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SECONDARY_SELECTED, 1},
       {ThemeProperties::COLOR_STATUS_BUBBLE_INACTIVE, 1},
       {ThemeProperties::COLOR_TAB_BACKGROUND_INACTIVE_FRAME_INACTIVE, 1},
       {ThemeProperties::COLOR_TAB_GROUP_BOOKMARK_BAR_ORANGE, 1},
       {ThemeProperties::COLOR_TAB_STROKE_FRAME_INACTIVE, 1},
       {ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR_FRAME_INACTIVE, 1},
       {ThemeProperties::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE, 1}});
  auto get_tolerance = [](int id) {
    auto* it = kTolerances.find(id);
    if (it != kTolerances.end())
      return it->second;
    return 0;
  };

  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile());

  for (auto color_id : kTestIdValues) {
    // Verifies that colors with and without the ColorProvider are the same.
    auto pair = GetOriginalAndRedirected(theme_provider, color_id);
    auto original = pair.first;
    auto redirected = pair.second;
    auto tolerance = get_tolerance(color_id);
    std::string error_message =
        base::StrCat({ColorIdToString(color_id), " has mismatched values"});
    if (!tolerance) {
      EXPECT_EQ(original, redirected) << error_message;
    } else {
      EXPECT_LE(std::abs(static_cast<int>(SkColorGetA(original.color) -
                                          SkColorGetA(redirected.color))),
                tolerance)
          << error_message;
      EXPECT_LE(std::abs(static_cast<int>(SkColorGetR(original.color) -
                                          SkColorGetR(redirected.color))),
                tolerance)
          << error_message;
      EXPECT_LE(std::abs(static_cast<int>(SkColorGetG(original.color) -
                                          SkColorGetG(redirected.color))),
                tolerance)
          << error_message;
      EXPECT_LE(std::abs(static_cast<int>(SkColorGetB(original.color) -
                                          SkColorGetB(redirected.color))),
                tolerance)
          << error_message;
    }
  }
}

}  // namespace theme_service_internal
