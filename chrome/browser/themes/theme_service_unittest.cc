// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/containers/fixed_flat_map.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
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

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

namespace {

enum class ContrastMode { kNonHighContrast, kHighContrast };

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

  static constexpr const auto kMap =
      base::MakeFixedFlatMap<int, const char*>({CHROME_COLOR_IDS});

#undef E
  constexpr char kPrefix[] = "ThemeProperties::";

  std::string id_str = kMap.find(id)->second;
  if (base::StartsWith(id_str, kPrefix))
    return id_str.substr(strlen(kPrefix));
  return id_str;
}

std::pair<PrintableSkColor, PrintableSkColor> GetOriginalAndRedirected(
    const ui::ThemeProvider& theme_provider,
    int color_id,
    ui::NativeTheme::ColorScheme color_scheme,
    ContrastMode contrast_mode) {
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  const bool high_contrast = contrast_mode == ContrastMode::kHighContrast;
#if defined(OS_WIN)
  if (high_contrast)
    color_scheme = ui::NativeTheme::ColorScheme::kPlatformHighContrast;
  native_theme->set_forced_colors(high_contrast);
#endif
  native_theme->set_preferred_contrast(
      high_contrast ? ui::NativeTheme::PreferredContrast::kMore
                    : ui::NativeTheme::PreferredContrast::kNoPreference);
  native_theme->set_use_dark_colors(color_scheme ==
                                    ui::NativeTheme::ColorScheme::kDark);

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
    if (service_->IsExtensionEnabled(extension_id)) {
      extensions::TestExtensionRegistryObserver observer(registry_);
      installer->Load(path);
      observer.WaitForExtensionLoaded();
    } else {
      content::WindowedNotificationObserver observer(
          extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
          content::Source<Profile>(profile()));
      installer->Load(path);
      observer.Wait();
    }

    // Let the ThemeService finish creating the theme pack.
    base::RunLoop().RunUntilIdle();
  }

  void set_theme_supplier(ThemeService* theme_service,
                          scoped_refptr<CustomThemeSupplier> theme_supplier) {
    theme_service->theme_supplier_ = theme_supplier;
  }

  SkColor GetOmniboxColor(ThemeService* theme_service,
                          int id,
                          bool incognito) const {
    bool has_custom_color;
    absl::optional<SkColor> color =
        theme_service->theme_helper_.GetOmniboxColor(
            id, incognito, theme_service->GetThemeSupplier(),
            &has_custom_color);
    EXPECT_TRUE(color);
    return color.value_or(gfx::kPlaceholderColor);
  }

  bool IsExtensionDisabled(const std::string& id) const {
    return registry_->GetExtensionById(id,
                                       extensions::ExtensionRegistry::DISABLED);
  }

  // Returns the separator color as the opaque result of blending it atop the
  // frame color (which is the color we use when calculating the contrast of the
  // separator with the tab and frame colors).
  static SkColor GetSeparatorColor(SkColor tab_color, SkColor frame_color) {
    return color_utils::GetResultingPaintColor(
        ThemeHelper::GetSeparatorColor(tab_color, frame_color), frame_color);
  }

 protected:
  ui::TestNativeTheme native_theme_;
  raw_ptr<extensions::ExtensionRegistry> registry_ = nullptr;
  raw_ptr<ThemeService> theme_service_ = nullptr;
};

class IncognitoThemeServiceTest : public ThemeServiceTest,
                                  public testing::WithParamInterface<bool> {
 public:
  IncognitoThemeServiceTest() {
    bool flag_enabled = GetParam();
    if (flag_enabled) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kIncognitoBrandConsistencyForDesktop,
                                views::features::
                                    kInheritNativeThemeFromParentWidget},
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{}, /*disabled_features=*/{
              features::kIncognitoBrandConsistencyForDesktop,
              views::features::kInheritNativeThemeFromParentWidget});
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    IncognitoThemeServiceTestWithIncognitoBrandConsistencyFlag,
    IncognitoThemeServiceTest,
    testing::Values(false, true));

class ThemeProviderRedirectedEquivalenceTest
    : public ThemeServiceTest,
      public testing::WithParamInterface<
          std::tuple<ui::NativeTheme::ColorScheme, ContrastMode, int>> {
 public:
  ThemeProviderRedirectedEquivalenceTest() = default;

  void SetUp() override {
    static bool added_initializer = false;
    if (!added_initializer) {
      ui::ColorProviderManager::Get().AppendColorProviderInitializer(
          base::BindRepeating(AddChromeColorMixers));
      added_initializer = true;
    }

    ThemeServiceTest::SetUp();
  }

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<
          std::tuple<ui::NativeTheme::ColorScheme, ContrastMode, int>>
          param_info) {
    auto param_tuple = param_info.param;
    return ColorSchemeToString(
               std::get<ui::NativeTheme::ColorScheme>(param_tuple)) +
           ContrastModeToString(std::get<ContrastMode>(param_tuple)) +
           "_With_" + ColorIdToString(std::get<int>(param_tuple));
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
    switch (contrast_mode) {
      case ContrastMode::kNonHighContrast:
        return "";
      case ContrastMode::kHighContrast:
        return "HighContrast";
      default:
        NOTREACHED();
        return "InvalidContrastMode";
    }
  }
};

#define E(color_id, theme_property_id, ...) theme_property_id,
static constexpr int kTestIdValues[] = {CHROME_COLOR_IDS};
#undef E

INSTANTIATE_TEST_SUITE_P(
    ,
    ThemeProviderRedirectedEquivalenceTest,
    ::testing::Combine(::testing::Values(ui::NativeTheme::ColorScheme::kLight,
                                         ui::NativeTheme::ColorScheme::kDark),
                       ::testing::Values(ContrastMode::kNonHighContrast,
                                         ContrastMode::kHighContrast),
                       ::testing::ValuesIn(kTestIdValues)),
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

TEST_F(ThemeServiceTest, IncognitoTest) {
  // This test relies on incognito being meaningfully different than default,
  // which is not currently true in dark mode.
  ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(false);

  // Should get the same ThemeService for incognito and original profiles.
  ThemeService* otr_theme_service = ThemeServiceFactory::GetForProfile(
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_EQ(theme_service_, otr_theme_service);

#if !defined(OS_MAC)
  // Should get a different ThemeProvider for incognito and original profiles.
  const ui::ThemeProvider& provider =
      ThemeService::GetThemeProviderForProfile(profile());
  const ui::ThemeProvider& otr_provider =
      ThemeService::GetThemeProviderForProfile(
          profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_NE(&provider, &otr_provider);
  // And (some) colors should be different.
  EXPECT_NE(provider.GetColor(ThemeProperties::COLOR_TOOLBAR),
            otr_provider.GetColor(ThemeProperties::COLOR_TOOLBAR));
#endif
}

TEST_P(IncognitoThemeServiceTest, IncognitoCustomColor_WithAutoGeneratedTheme) {
  theme_service_->BuildAutogeneratedThemeFromColor(
      SkColorSetRGB(100, 100, 100));

  // Should get the same ThemeService for incognito and original profiles.
  ThemeService* otr_theme_service = ThemeServiceFactory::GetForProfile(
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_EQ(theme_service_, otr_theme_service);

  const ui::ThemeProvider& otr_provider =
      ThemeService::GetThemeProviderForProfile(
          profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  SkColor default_incognito_color = ThemeProperties::GetDefaultColor(
      ThemeProperties::COLOR_FRAME_ACTIVE, /*incognito= */ true,
      /*dark_mode= */ false);
  bool is_brand_consistency_flag_enabled = GetParam();

  if (is_brand_consistency_flag_enabled) {
    EXPECT_EQ(default_incognito_color,
              otr_provider.GetColor(ThemeProperties::COLOR_FRAME_ACTIVE));
  } else {
    EXPECT_NE(default_incognito_color,
              otr_provider.GetColor(ThemeProperties::COLOR_FRAME_ACTIVE));
  }
}

TEST_P(IncognitoThemeServiceTest, IncognitoCustomColor_WithExtensionOverride) {
  ThemeScoper scoper = LoadUnpackedTheme();
  EXPECT_TRUE(theme_service_->UsingExtensionTheme());

  // Should get the same ThemeService for incognito and original profiles.
  ThemeService* otr_theme_service = ThemeServiceFactory::GetForProfile(
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_EQ(theme_service_, otr_theme_service);

  const ui::ThemeProvider& otr_provider =
      ThemeService::GetThemeProviderForProfile(
          profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  SkColor default_incognito_color = ThemeProperties::GetDefaultColor(
      ThemeProperties::COLOR_TOOLBAR, /*incognito=*/true,
      /*dark_mode=*/false);
  bool is_brand_consistency_flag_enabled = GetParam();

  if (is_brand_consistency_flag_enabled) {
    EXPECT_EQ(default_incognito_color,
              otr_provider.GetColor(ThemeProperties::COLOR_TOOLBAR));
  } else {
    EXPECT_NE(default_incognito_color,
              otr_provider.GetColor(ThemeProperties::COLOR_TOOLBAR));
  }
}

TEST_F(ThemeServiceTest, GetColorForToolbarButton) {
  // This test relies on toolbar buttons having no tint, which is not currently
  // true in dark mode.
  ui::NativeTheme::GetInstanceForNativeUi()->set_use_dark_colors(false);

  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile());
  SkColor default_toolbar_button_color =
      theme_provider.GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  EXPECT_FALSE(theme_provider.HasCustomColor(
      ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON));

  ThemeScoper scoper1 = LoadUnpackedTheme(
      "extensions/theme_test_toolbar_button_color/manifest.json");

  // Should get a new color after installing a theme.
  SkColor toolbar_button_explicit_color =
      theme_provider.GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  EXPECT_NE(toolbar_button_explicit_color, default_toolbar_button_color);
  EXPECT_TRUE(theme_provider.HasCustomColor(
      ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON));

  ThemeScoper scoper2 = LoadUnpackedTheme(
      "extensions/theme_test_toolbar_button_tint/manifest.json");

  // Should get the color based on a tint.
  SkColor toolbar_button_tinted_color =
      theme_provider.GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  EXPECT_NE(toolbar_button_tinted_color, default_toolbar_button_color);
  EXPECT_NE(toolbar_button_tinted_color, toolbar_button_explicit_color);
  EXPECT_TRUE(theme_provider.HasCustomColor(
      ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON));
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
          {TP::COLOR_OMNIBOX_TEXT, TP::COLOR_OMNIBOX_BACKGROUND},
          {TP::COLOR_OMNIBOX_TEXT, TP::COLOR_OMNIBOX_BACKGROUND_HOVERED},
          {TP::COLOR_OMNIBOX_TEXT, TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_TEXT, TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED},
          {TP::COLOR_OMNIBOX_TEXT_DIMMED, TP::COLOR_OMNIBOX_BACKGROUND},
          {TP::COLOR_OMNIBOX_SELECTED_KEYWORD, TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED,
           TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED, TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED,
           TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED},
          {TP::COLOR_OMNIBOX_RESULTS_ICON, TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_RESULTS_ICON,
           TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED},
          {TP::COLOR_OMNIBOX_RESULTS_ICON_SELECTED,
           TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED},
          {TP::COLOR_OMNIBOX_RESULTS_URL, TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_RESULTS_URL_SELECTED,
           TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED},
          {TP::COLOR_OMNIBOX_BUBBLE_OUTLINE, TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE,
           TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT,
           TP::COLOR_OMNIBOX_BACKGROUND},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT,
           TP::COLOR_OMNIBOX_BACKGROUND_HOVERED},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT,
           TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_SECURE,
           TP::COLOR_OMNIBOX_BACKGROUND},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_SECURE,
           TP::COLOR_OMNIBOX_BACKGROUND_HOVERED},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_SECURE,
           TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS,
           TP::COLOR_OMNIBOX_BACKGROUND},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS,
           TP::COLOR_OMNIBOX_RESULTS_BG},
          {TP::COLOR_OMNIBOX_TEXT_DIMMED, TP::COLOR_OMNIBOX_BACKGROUND_HOVERED},
          {TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED,
           TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED},
          {TP::COLOR_OMNIBOX_RESULTS_URL, TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED},
          {TP::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS,
           TP::COLOR_OMNIBOX_BACKGROUND_HOVERED},
      };
      auto check_sufficient_contrast = [&](int id1, int id2) {
        const float contrast = color_utils::GetContrastRatio(
            GetOmniboxColor(theme_service_, id1, dark),
            GetOmniboxColor(theme_service_, id2, dark));
        EXPECT_GE(contrast, color_utils::kMinimumReadableContrastRatio);
      };
      for (const int* ids : contrasting_ids)
        check_sufficient_contrast(ids[0], ids[1]);
      if (high_contrast)
        check_sufficient_contrast(TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED,
                                  TP::COLOR_OMNIBOX_RESULTS_BG);
    }
  }
}

// Ensure nothing DCHECKs if either of COLOR_OMNIBOX_BACKGROUND or
// COLOR_OMNIBOX_TEXT are translucent (https://crbug.com/1006102).
TEST_F(ThemeServiceTest, TranslucentOmniboxBackgroundAndText) {
  using TP = ThemeProperties;
  class TranslucentOmniboxThemeSupplier : public CustomThemeSupplier {
   public:
    TranslucentOmniboxThemeSupplier()
        : CustomThemeSupplier(ThemeType::EXTENSION) {}
    bool GetColor(int id, SkColor* color) const override {
      switch (id) {
        case TP::COLOR_OMNIBOX_BACKGROUND:
          *color = SkColorSetARGB(127, 255, 255, 255);
          return true;
        case TP::COLOR_OMNIBOX_TEXT:
          *color = SkColorSetARGB(127, 0, 0, 0);
          return true;
      }
      return CustomThemeSupplier::GetColor(id, color);
    }

   private:
    ~TranslucentOmniboxThemeSupplier() override = default;
  };
  set_theme_supplier(theme_service_,
                     base::MakeRefCounted<TranslucentOmniboxThemeSupplier>());

  constexpr int ids[] = {
      TP::COLOR_OMNIBOX_BACKGROUND,
      TP::COLOR_OMNIBOX_TEXT,
      TP::COLOR_OMNIBOX_BACKGROUND_HOVERED,
      TP::COLOR_OMNIBOX_SELECTED_KEYWORD,
      TP::COLOR_OMNIBOX_TEXT_DIMMED,
      TP::COLOR_OMNIBOX_RESULTS_BG,
      TP::COLOR_OMNIBOX_RESULTS_BG_HOVERED,
      TP::COLOR_OMNIBOX_RESULTS_BG_SELECTED,
      TP::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED,
      TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED,
      TP::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED,
      TP::COLOR_OMNIBOX_RESULTS_ICON,
      TP::COLOR_OMNIBOX_RESULTS_ICON_SELECTED,
      TP::COLOR_OMNIBOX_RESULTS_URL,
      TP::COLOR_OMNIBOX_RESULTS_URL_SELECTED,
      TP::COLOR_OMNIBOX_BUBBLE_OUTLINE,
      TP::COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE,
      TP::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT,
      TP::COLOR_OMNIBOX_SECURITY_CHIP_SECURE,
      TP::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS,
  };

  for (int id : ids) {
    GetOmniboxColor(theme_service_, id, false);
    GetOmniboxColor(theme_service_, id, true);
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

// TODO(crbug.com/1056953): Enable on Mac.
// Flaky on linux-chromeos-rel crbug.com/1273727
#if defined(OS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_GetColor DISABLED_GetColor
#else
#define MAYBE_GetColor GetColor
#endif
TEST_P(ThemeProviderRedirectedEquivalenceTest, MAYBE_GetColor) {
  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile());
  auto param_tuple = GetParam();
  auto color_scheme = std::get<ui::NativeTheme::ColorScheme>(param_tuple);
  auto contrast_mode = std::get<ContrastMode>(param_tuple);
  auto color_id = std::get<int>(param_tuple);

  // Verifies that colors with and without the ColorProvider are the same.
  auto pair = GetOriginalAndRedirected(theme_provider, color_id, color_scheme,
                                       contrast_mode);
  auto original = pair.first;
  auto redirected = pair.second;
  EXPECT_EQ(original, redirected);
}

}  // namespace theme_service_internal
