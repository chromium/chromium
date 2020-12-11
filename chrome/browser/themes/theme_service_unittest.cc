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
#include "ui/native_theme/test_native_theme.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

namespace {

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
  extensions::ExtensionService* extension_service_ = nullptr;
  extensions::ExtensionRegistry* extension_registry_ = nullptr;
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
    scoper.set_extension_id(observer.WaitForExtensionLoaded()->id());

    WaitForThemeInstall();

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
    base::Optional<SkColor> color =
        theme_service->theme_helper_.GetOmniboxColor(
            id, incognito, theme_service->GetThemeSupplier(),
            &has_custom_color);
    EXPECT_TRUE(color);
    return color.value_or(gfx::kPlaceholderColor);
  }

  void WaitForThemeInstall() {
    content::WindowedNotificationObserver theme_change_observer(
        chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
        content::Source<ThemeService>(theme_service_));
    theme_change_observer.Wait();
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
  extensions::ExtensionRegistry* registry_ = nullptr;
  ThemeService* theme_service_ = nullptr;
};

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
  service_->EnableExtension(scoper1.extension_id());
  WaitForThemeInstall();
  EXPECT_EQ(scoper1.extension_id(), theme_service_->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(scoper1.extension_id()));
  EXPECT_TRUE(IsExtensionDisabled(scoper2.extension_id()));

  // 3) Using RevertToExtensionTheme() with a disabled theme should enable and
  // set the theme. This is the case when the user reverts to the previous theme
  // via an infobar.
  theme_service_->RevertToExtensionTheme(scoper2.extension_id());
  WaitForThemeInstall();
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
  content::WindowedNotificationObserver theme_change_observer(
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(theme_service_));
  UpdateUnpackedTheme(scoper2.extension_id());

  // The ThemeService should have sent an theme change notification even though
  // the id of the current theme did not change.
  theme_change_observer.Wait();

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
  ThemeService* otr_theme_service =
      ThemeServiceFactory::GetForProfile(profile_->GetPrimaryOTRProfile());
  EXPECT_EQ(theme_service_, otr_theme_service);

#if !defined(OS_MAC)
  // Should get a different ThemeProvider for incognito and original profiles.
  const ui::ThemeProvider& provider =
      ThemeService::GetThemeProviderForProfile(profile());
  const ui::ThemeProvider& otr_provider =
      ThemeService::GetThemeProviderForProfile(
          profile_->GetPrimaryOTRProfile());
  EXPECT_NE(&provider, &otr_provider);
  // And (some) colors should be different.
  EXPECT_NE(provider.GetColor(ThemeProperties::COLOR_TOOLBAR),
            otr_provider.GetColor(ThemeProperties::COLOR_TOOLBAR));
#endif
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

    reinstaller->Reinstall();
    WaitForThemeInstall();
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

  native_theme_.SetUsesHighContrastColors(true);
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

  native_theme_.SetUsesHighContrastColors(false);
  theme_service_->OnNativeThemeUpdated(&native_theme_);
  EXPECT_TRUE(theme_service_->UsingDefaultTheme());
  EXPECT_EQ(theme_service_->GetThemeSupplier(), nullptr);
}

}  // namespace theme_service_internal
