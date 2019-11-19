// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/increased_contrast_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

using extensions::ExtensionRegistry;

namespace theme_service_internal {

class ThemeServiceTest : public extensions::ExtensionServiceTestBase {
 public:
  ThemeServiceTest() : registry_(NULL) {}
  ~ThemeServiceTest() override {}

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    extensions::ExtensionServiceTestBase::ExtensionServiceInitParams params =
        CreateDefaultInitParams();
    InitializeExtensionService(params);
    service_->Init();
    registry_ = ExtensionRegistry::Get(profile_.get());
    ASSERT_TRUE(registry_);
  }

  // Moves a minimal theme to |temp_dir_path| and unpacks it from that
  // directory.
  std::string LoadUnpackedMinimalThemeAt(const base::FilePath& temp_dir) {
    return LoadUnpackedTheme(temp_dir,
                             "extensions/theme_minimal/manifest.json");
  }

  std::string LoadUnpackedTheme(const base::FilePath& temp_dir,
                                const std::string source_file_path) {
    base::FilePath dst_manifest_path = temp_dir.AppendASCII("manifest.json");
    base::FilePath test_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    base::FilePath src_manifest_path =
        test_data_dir.AppendASCII(source_file_path);
    EXPECT_TRUE(base::CopyFile(src_manifest_path, dst_manifest_path));

    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service_));
    extensions::TestExtensionRegistryObserver observer(
        ExtensionRegistry::Get(profile()));
    installer->Load(temp_dir);
    std::string extension_id = observer.WaitForExtensionLoaded()->id();

    WaitForThemeInstall();

    return extension_id;
  }

  // Update the theme with |extension_id|.
  void UpdateUnpackedTheme(const std::string& extension_id) {
    const base::FilePath& path =
        registry_->GetInstalledExtension(extension_id)->path();

    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service_));
    if (service_->IsExtensionEnabled(extension_id)) {
      extensions::TestExtensionRegistryObserver observer(
          ExtensionRegistry::Get(profile()));
      installer->Load(path);
      observer.WaitForExtensionLoaded();
    } else {
      content::WindowedNotificationObserver observer(
          extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
          content::Source<Profile>(profile_.get()));
      installer->Load(path);
      observer.Wait();
    }

    // Let the ThemeService finish creating the theme pack.
    base::RunLoop().RunUntilIdle();
  }

  const CustomThemeSupplier* get_theme_supplier(ThemeService* theme_service) {
    return theme_service->get_theme_supplier();
  }

  void set_theme_supplier(ThemeService* theme_service,
                          scoped_refptr<CustomThemeSupplier> theme_supplier) {
    theme_service->theme_supplier_ = theme_supplier;
  }

  SkColor GetOmniboxColor(ThemeService* theme_service,
                          int id,
                          bool incognito) const {
    bool has_custom_color;
    base::Optional<SkColor> color =
        theme_service->GetOmniboxColor(id, incognito, &has_custom_color);
    EXPECT_TRUE(color);
    return color.value_or(gfx::kPlaceholderColor);
  }

  void WaitForThemeInstall() {
    content::WindowedNotificationObserver theme_change_observer(
        chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
        content::Source<ThemeService>(
            ThemeServiceFactory::GetForProfile(profile())));
    theme_change_observer.Wait();
  }

  // Returns the separator color as the opaque result of blending it atop the
  // frame color (which is the color we use when calculating the contrast of the
  // separator with the tab and frame colors).
  static SkColor GetSeparatorColor(SkColor tab_color, SkColor frame_color) {
    return color_utils::GetResultingPaintColor(
        ThemeService::GetSeparatorColor(tab_color, frame_color), frame_color);
  }

 protected:
  ExtensionRegistry* registry_;
};

// Installs then uninstalls a theme and makes sure that the ThemeService
// reverts to the default theme after the uninstall.
TEST_F(ThemeServiceTest, ThemeInstallUninstall) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::RunLoop().RunUntilIdle();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const std::string& extension_id =
      LoadUnpackedMinimalThemeAt(temp_dir.GetPath());
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  EXPECT_TRUE(theme_service->UsingExtensionTheme());
  EXPECT_EQ(extension_id, theme_service->GetThemeID());

  // Now uninstall the extension, should revert to the default theme.
  service_->UninstallExtension(extension_id,
                               extensions::UNINSTALL_REASON_FOR_TESTING,
                               NULL);
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
  EXPECT_FALSE(theme_service->UsingExtensionTheme());
}

// Test that a theme extension is disabled when not in use. A theme may be
// installed but not in use if it there is an infobar to revert to the previous
// theme.
TEST_F(ThemeServiceTest, DisableUnusedTheme) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();

  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  // 1) Installing a theme should disable the previously active theme.
  const std::string& extension1_id =
      LoadUnpackedMinimalThemeAt(temp_dir1.GetPath());
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  EXPECT_EQ(extension1_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension1_id));

  // Create theme reinstaller to prevent the current theme from being
  // uninstalled.
  std::unique_ptr<ThemeService::ThemeReinstaller> reinstaller =
      theme_service->BuildReinstallerForCurrentTheme();

  const std::string& extension2_id =
      LoadUnpackedMinimalThemeAt(temp_dir2.GetPath());
  EXPECT_EQ(extension2_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension2_id));
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));

  // 2) Enabling a disabled theme extension should swap the current theme.
  service_->EnableExtension(extension1_id);
  WaitForThemeInstall();
  EXPECT_EQ(extension1_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension1_id));
  EXPECT_TRUE(registry_->GetExtensionById(extension2_id,
                                          ExtensionRegistry::DISABLED));

  // 3) Using RevertToExtensionTheme() with a disabled theme should enable and
  // set the theme. This is the case when the user reverts to the previous theme
  // via an infobar.
  theme_service->RevertToExtensionTheme(extension2_id);
  WaitForThemeInstall();
  EXPECT_EQ(extension2_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension2_id));
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));

  // 4) Disabling the current theme extension should revert to the default theme
  // and disable any installed theme extensions.
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  service_->DisableExtension(extension2_id,
                             extensions::disable_reason::DISABLE_USER_ACTION);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
  EXPECT_FALSE(service_->IsExtensionEnabled(extension1_id));
  EXPECT_FALSE(service_->IsExtensionEnabled(extension2_id));
}

// Test the ThemeService's behavior when a theme is upgraded.
TEST_F(ThemeServiceTest, ThemeUpgrade) {
  // Setup.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<ThemeService::ThemeReinstaller> reinstaller =
      theme_service->BuildReinstallerForCurrentTheme();

  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  const std::string& extension1_id =
      LoadUnpackedMinimalThemeAt(temp_dir1.GetPath());
  const std::string& extension2_id =
      LoadUnpackedMinimalThemeAt(temp_dir2.GetPath());

  // Test the initial state.
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));
  EXPECT_EQ(extension2_id, theme_service->GetThemeID());

  // 1) Upgrading the current theme should not revert to the default theme.
  content::WindowedNotificationObserver theme_change_observer(
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(theme_service));
  UpdateUnpackedTheme(extension2_id);

  // The ThemeService should have sent an theme change notification even though
  // the id of the current theme did not change.
  theme_change_observer.Wait();

  EXPECT_EQ(extension2_id, theme_service->GetThemeID());
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));

  // 2) Upgrading a disabled theme should not change the current theme.
  UpdateUnpackedTheme(extension1_id);
  EXPECT_EQ(extension2_id, theme_service->GetThemeID());
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));
}

TEST_F(ThemeServiceTest, IncognitoTest) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::RunLoop().RunUntilIdle();

  // Should get the same ThemeService for incognito and original profiles.
  ThemeService* otr_theme_service =
      ThemeServiceFactory::GetForProfile(profile_->GetOffTheRecordProfile());
  EXPECT_EQ(theme_service, otr_theme_service);

#if !defined(OS_MACOSX)
  // Should get a different ThemeProvider for incognito and original profiles.
  const ui::ThemeProvider& provider =
      ThemeService::GetThemeProviderForProfile(profile_.get());
  const ui::ThemeProvider& otr_provider =
      ThemeService::GetThemeProviderForProfile(
          profile_->GetOffTheRecordProfile());
  EXPECT_NE(&provider, &otr_provider);
  // And (some) colors should be different.
  EXPECT_NE(provider.GetColor(ThemeProperties::COLOR_TOOLBAR),
            otr_provider.GetColor(ThemeProperties::COLOR_TOOLBAR));
#endif
}

TEST_F(ThemeServiceTest, GetDefaultThemeProviderForProfile) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::RunLoop().RunUntilIdle();

  SkColor default_toolbar_color =
      ThemeService::GetThemeProviderForProfile(profile_.get())
          .GetColor(ThemeProperties::COLOR_TOOLBAR);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  LoadUnpackedMinimalThemeAt(temp_dir.GetPath());

  // Should get a new color after installing a theme.
  EXPECT_NE(ThemeService::GetThemeProviderForProfile(profile_.get())
                .GetColor(ThemeProperties::COLOR_TOOLBAR),
            default_toolbar_color);

  // Should get the same color when requesting a default color.
  EXPECT_EQ(ThemeService::GetDefaultThemeProviderForProfile(profile_.get())
                .GetColor(ThemeProperties::COLOR_TOOLBAR),
            default_toolbar_color);
}

TEST_F(ThemeServiceTest, GetColorForToolbarButton) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::RunLoop().RunUntilIdle();

  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile_.get());
  SkColor default_toolbar_button_color =
      theme_provider.GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  EXPECT_FALSE(theme_provider.HasCustomColor(
      ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON));

  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  LoadUnpackedTheme(temp_dir1.GetPath(),
                    "extensions/theme_test_toolbar_button_color/manifest.json");

  // Should get a new color after installing a theme.
  SkColor toolbar_button_explicit_color =
      theme_provider.GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  EXPECT_NE(toolbar_button_explicit_color, default_toolbar_button_color);
  EXPECT_TRUE(theme_provider.HasCustomColor(
      ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON));

  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());
  LoadUnpackedTheme(temp_dir2.GetPath(),
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
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::RunLoop().RunUntilIdle();

  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile_.get());
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    LoadUnpackedTheme(temp_dir.GetPath(),
                      "extensions/theme_grey_ntp/manifest.json");
    // When logo alternate is not specified and ntp is grey, logo should be
    // colorful.
    EXPECT_EQ(0, theme_provider.GetDisplayProperty(
                     ThemeProperties::NTP_LOGO_ALTERNATE));
  }

  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    LoadUnpackedTheme(temp_dir.GetPath(),
                      "extensions/theme_grey_ntp_white_logo/manifest.json");
    // Logo alternate should match what is specified in the manifest.
    EXPECT_EQ(1, theme_provider.GetDisplayProperty(
                     ThemeProperties::NTP_LOGO_ALTERNATE));
  }

  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    LoadUnpackedTheme(temp_dir.GetPath(),
                      "extensions/theme_color_ntp_white_logo/manifest.json");
    // When logo alternate is not specified and ntp is colorful, logo should be
    // white.
    EXPECT_EQ(1, theme_provider.GetDisplayProperty(
                     ThemeProperties::NTP_LOGO_ALTERNATE));
  }

  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    LoadUnpackedTheme(temp_dir.GetPath(),
                      "extensions/theme_color_ntp_colorful_logo/manifest.json");
    // Logo alternate should match what is specified in the manifest.
    EXPECT_EQ(0, theme_provider.GetDisplayProperty(
                     ThemeProperties::NTP_LOGO_ALTERNATE));
  }
}

// crbug.com/468280
TEST_F(ThemeServiceTest, UninstallThemeWhenNoReinstallers) {
  // Setup.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::RunLoop().RunUntilIdle();

  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  const std::string& extension1_id =
      LoadUnpackedMinimalThemeAt(temp_dir1.GetPath());
  ASSERT_EQ(extension1_id, theme_service->GetThemeID());

  std::string extension2_id = "";
  {
    // Show an infobar.
    std::unique_ptr<ThemeService::ThemeReinstaller> reinstaller =
        theme_service->BuildReinstallerForCurrentTheme();

    // Install another theme. The first extension shouldn't be uninstalled yet
    // as it should be possible to revert to it.
    extension2_id = LoadUnpackedMinimalThemeAt(temp_dir2.GetPath());
    EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                            ExtensionRegistry::DISABLED));
    EXPECT_EQ(extension2_id, theme_service->GetThemeID());

    reinstaller->Reinstall();
    WaitForThemeInstall();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(registry_->GetExtensionById(extension2_id,
                                            ExtensionRegistry::DISABLED));
    EXPECT_EQ(extension1_id, theme_service->GetThemeID());
  }

  // extension 2 should get uninstalled as no reinstallers are in scope.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(registry_->GetExtensionById(extension2_id,
                                           ExtensionRegistry::EVERYTHING));
}

TEST_F(ThemeServiceTest, BuildFromColorTest) {
  // Set theme from color.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
  EXPECT_FALSE(theme_service->UsingAutogenerated());
  theme_service->BuildFromColor(SkColorSetRGB(100, 100, 100));
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  EXPECT_TRUE(theme_service->UsingAutogenerated());

  // Set theme from data pack and then override it with theme from color.
  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  const std::string& extension1_id =
      LoadUnpackedMinimalThemeAt(temp_dir1.GetPath());
  EXPECT_EQ(extension1_id, theme_service->GetThemeID());
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  EXPECT_FALSE(theme_service->UsingAutogenerated());
  base::FilePath path =
      profile_->GetPrefs()->GetFilePath(prefs::kCurrentThemePackFilename);
  EXPECT_FALSE(path.empty());

  theme_service->BuildFromColor(SkColorSetRGB(100, 100, 100));
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  EXPECT_TRUE(theme_service->UsingAutogenerated());
  EXPECT_EQ(ThemeService::kAutogeneratedThemeID, theme_service->GetThemeID());
  path = profile_->GetPrefs()->GetFilePath(prefs::kCurrentThemePackFilename);
  EXPECT_TRUE(path.empty());
}

TEST_F(ThemeServiceTest, BuildFromColor_DisableExtensionTest) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  const std::string& extension1_id =
      LoadUnpackedMinimalThemeAt(temp_dir1.GetPath());
  EXPECT_EQ(extension1_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension1_id));

  // Setting autogenerated theme should disable previous theme.
  theme_service->BuildFromColor(SkColorSetRGB(100, 100, 100));
  EXPECT_TRUE(theme_service->UsingAutogenerated());
  EXPECT_FALSE(service_->IsExtensionEnabled(extension1_id));
}

TEST_F(ThemeServiceTest, UseDefaultTheme_DisableExtensionTest) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  const std::string& extension1_id =
      LoadUnpackedMinimalThemeAt(temp_dir1.GetPath());
  EXPECT_EQ(extension1_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension1_id));

  // Resetting to default theme should disable previous theme.
  theme_service->UseDefaultTheme();
  EXPECT_FALSE(service_->IsExtensionEnabled(extension1_id));
}

TEST_F(ThemeServiceTest, OmniboxContrast) {
  using TP = ThemeProperties;
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  for (bool dark : {false, true}) {
    for (bool high_contrast : {false, true}) {
      set_theme_supplier(
          theme_service,
          high_contrast
              ? base::MakeRefCounted<IncreasedContrastThemeSupplier>(dark)
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
            GetOmniboxColor(theme_service, id1, dark),
            GetOmniboxColor(theme_service, id2, dark));
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
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());

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
  set_theme_supplier(theme_service,
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
    GetOmniboxColor(theme_service, id, false);
    GetOmniboxColor(theme_service, id, true);
  }
}

}  // namespace theme_service_internal
