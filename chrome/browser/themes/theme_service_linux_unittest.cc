// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include <cmath>

#include "base/containers/fixed_flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/test/theme_service_changed_waiter.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_service_test_utils.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/test_native_theme.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/linux_ui/linux_ui_factory.h"
#include "ui/views/views_features.h"

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

  SkColor GetColor(ThemeService* theme_service, int id, bool incognito) const {
    return theme_service->theme_helper_.GetColor(
        id, incognito, theme_service->GetThemeSupplier());
  }

 protected:
  ui::TestNativeTheme test_native_theme_;
  raw_ptr<extensions::ExtensionRegistry> registry_ = nullptr;
  raw_ptr<ThemeService> theme_service_ = nullptr;
};

class ThemeProviderRedirectedEquivalenceLinuxTest : public ThemeServiceTest {
 public:
  ThemeProviderRedirectedEquivalenceLinuxTest() = default;

  // ThemeServiceTest:
  void SetUp() override {
    ThemeServiceTest::SetUp();

    // Only perform mixer initialization once.
    static bool initialized_mixers = false;
    if (!initialized_mixers) {
      // Ensures the toolkit is configured on supported linux platforms.
      // Initializing the toolkit also adds the native toolkit ColorMixers.
      ui::OzonePlatform::InitParams ozone_params;
      ozone_params.single_process = true;
      ui::OzonePlatform::InitializeForUI(ozone_params);
      auto linux_ui = CreateLinuxUi();
      linux_ui_ = linux_ui.get();
      ASSERT_TRUE(linux_ui_);
      views::LinuxUI::SetInstance(std::move(linux_ui));

      // Add the Chrome ColorMixers after native ColorMixers.
      ui::ColorProviderManager::Get().AppendColorProviderInitializer(
          base::BindRepeating(AddChromeColorMixers));

      initialized_mixers = true;
    }
    // Always use system theme.
    views::LinuxUI::instance()->SetUseSystemThemeCallback(
        base::BindRepeating([](aura::Window* window) { return true; }));
    theme_service_->UseSystemTheme();
  }

  void TearDown() override {
    // Restore the original NativeTheme parameters.
    ThemeServiceTest::TearDown();
  }

 protected:
  views::LinuxUI* linux_ui_;
};

// TODO(crbug.com/1310397): There're mismatched colors in some Linux themes.
// Enable this test after fixing them.
// TODO(crbug.com/1323745): Fix consecutive failures on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_GetColor DISABLED_GetColor
#else
#define MAYBE_GetColor GetColor
#endif  // BUILDFLAG(IS_LINUX)
TEST_F(ThemeProviderRedirectedEquivalenceLinuxTest, MAYBE_GetColor) {
  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile());

  static constexpr const auto ignored_color_ids = base::MakeFixedFlatSet<
      ui::ColorId>(
      {// Ignore IPH colors due to behavior change.
       // Original: always Google blue, redirected: follow the accent color.
       ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_BACKGROUND,
       ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_CLOSE_BUTTON_INK_DROP,
       ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_DEFAULT_BUTTON_FOREGROUND});

  std::vector<std::string> themes =
      linux_ui_->GetAvailableSystemThemeNamesForTest();
  for (const std::string& theme : themes) {
    linux_ui_->SetSystemThemeByNameForTest(theme);
    for (auto color_id : theme_service::test::kTestColorIds) {
      if (ignored_color_ids.contains(color_id))
        continue;
      std::string error_message =
          base::StrCat({"GTK theme ", theme, ": ",
                        theme_service::test::ColorIdToString(color_id),
                        " has mismatched values"});
      theme_service::test::TestOriginalAndRedirectedColorMatched(
          theme_provider, color_id, error_message);
    }
  }
}

}  // namespace theme_service_internal
