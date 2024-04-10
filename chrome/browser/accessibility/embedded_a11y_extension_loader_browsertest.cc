// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

class EmbeddedA11yExtensionLoaderTest : public InProcessBrowserTest {
 public:
  EmbeddedA11yExtensionLoaderTest() = default;
  ~EmbeddedA11yExtensionLoaderTest() override = default;
  EmbeddedA11yExtensionLoaderTest(const EmbeddedA11yExtensionLoaderTest&) =
      delete;
  EmbeddedA11yExtensionLoaderTest& operator=(
      const EmbeddedA11yExtensionLoaderTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    auto* embedded_a11y_extension_loader =
        EmbeddedA11yExtensionLoader::GetInstance();
    embedded_a11y_extension_loader->AddExtensionChangedCallbackForTest(
        base::BindRepeating(
            &EmbeddedA11yExtensionLoaderTest::OnExtensionChanged,
            base::Unretained(this)));
  }

  void WaitForExtensionLoaded(Profile* profile,
                              const std::string& extension_id) {
    extensions::ComponentLoader* component_loader =
        extensions::ExtensionSystem::Get(profile)
            ->extension_service()
            ->component_loader();

    while (!component_loader->Exists(extension_id)) {
      waiter_ = std::make_unique<base::RunLoop>();
      waiter_->Run();
    }

    EXPECT_TRUE(component_loader->Exists(extension_id));
  }

  void WaitForExtensionUnloaded(Profile* profile,
                                const std::string& extension_id) {
    extensions::ComponentLoader* component_loader =
        extensions::ExtensionSystem::Get(profile)
            ->extension_service()
            ->component_loader();
    while (component_loader->Exists(extension_id)) {
      waiter_ = std::make_unique<base::RunLoop>();
      waiter_->Run();
    }

    EXPECT_FALSE(component_loader->Exists(extension_id));
  }

  void InstallAndWaitForExtensionLoaded(
      Profile* profile,
      const std::string& extension_id,
      const std::string& extension_path,
      const base::FilePath::CharType* manifest_name,
      bool should_localize) {
    auto* embedded_a11y_extension_loader =
        EmbeddedA11yExtensionLoader::GetInstance();
    embedded_a11y_extension_loader->InstallExtensionWithId(
        extension_id, extension_path, manifest_name, should_localize);
    WaitForExtensionLoaded(profile, extension_id);
  }

  void RemoveAndWaitForExtensionUnloaded(Profile* profile,
                                         const std::string& extension_id) {
    auto* embedded_a11y_extension_loader =
        EmbeddedA11yExtensionLoader::GetInstance();
    embedded_a11y_extension_loader->RemoveExtensionWithId(extension_id);
    WaitForExtensionUnloaded(profile, extension_id);
  }

 private:
  void OnExtensionChanged() {
    if (waiter_ && waiter_->running()) {
      waiter_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> waiter_;
};

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(EmbeddedA11yExtensionLoaderTest,
                       InstallsRemovesAndReinstallsExtension) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const auto& profiles = profile_manager->GetLoadedProfiles();
  ASSERT_GT(profiles.size(), 0u);
  Profile* profile = profiles[0];

  InstallAndWaitForExtensionLoaded(
      profile, extension_misc::kReadingModeGDocsHelperExtensionId,
      extension_misc::kReadingModeGDocsHelperExtensionPath,
      extension_misc::kReadingModeGDocsHelperManifestFilename,
      /*should_localize=*/false);
  RemoveAndWaitForExtensionUnloaded(
      profile, extension_misc::kReadingModeGDocsHelperExtensionId);
  InstallAndWaitForExtensionLoaded(
      profile, extension_misc::kReadingModeGDocsHelperExtensionId,
      extension_misc::kReadingModeGDocsHelperExtensionPath,
      extension_misc::kReadingModeGDocsHelperManifestFilename,
      /*should_localize=*/false);
  RemoveAndWaitForExtensionUnloaded(
      profile, extension_misc::kReadingModeGDocsHelperExtensionId);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(EmbeddedA11yExtensionLoaderTest,
                       InstallsOnMultipleProfiles) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  size_t num_extra_profiles = 2;
  for (size_t i = 0; i < num_extra_profiles; i++) {
    // Create an additional profile.
    base::FilePath path_profile =
        profile_manager->GenerateNextProfileDirectoryPath();
    profiles::testing::CreateProfileSync(profile_manager, path_profile);

    // Open a browser window for the profile.
    profiles::SwitchToProfile(path_profile, false);
    content::RunAllTasksUntilIdle();
  }

  EXPECT_EQ(profile_manager->GetNumberOfProfiles(), num_extra_profiles + 1);
  const auto& profiles = profile_manager->GetLoadedProfiles();

  // Install extension for Reading Mode.
  auto* embedded_a11y_extension_loader =
      EmbeddedA11yExtensionLoader::GetInstance();
  embedded_a11y_extension_loader->InstallExtensionWithId(
      extension_misc::kReadingModeGDocsHelperExtensionId,
      extension_misc::kReadingModeGDocsHelperExtensionPath,
      extension_misc::kReadingModeGDocsHelperManifestFilename,
      /*should_localize=*/false);
  for (auto* const profile : profiles) {
    WaitForExtensionLoaded(profile,
                           extension_misc::kReadingModeGDocsHelperExtensionId);
  }

  // Remove the extension.
  embedded_a11y_extension_loader->RemoveExtensionWithId(
      extension_misc::kReadingModeGDocsHelperExtensionId);
  for (auto* const profile : profiles) {
    WaitForExtensionUnloaded(
        profile, extension_misc::kReadingModeGDocsHelperExtensionId);
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(EmbeddedA11yExtensionLoaderTest,
                       InstallsOnIncognitoProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Browser* incognito =
      CreateIncognitoBrowser(profile_manager->GetLastUsedProfile());
  content::RunAllTasksUntilIdle();

  InstallAndWaitForExtensionLoaded(
      incognito->profile(), extension_misc::kReadingModeGDocsHelperExtensionId,
      extension_misc::kReadingModeGDocsHelperExtensionPath,
      extension_misc::kReadingModeGDocsHelperManifestFilename,
      /*should_localize=*/false);
  RemoveAndWaitForExtensionUnloaded(
      incognito->profile(), extension_misc::kReadingModeGDocsHelperExtensionId);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// CreateGuestBrowser() is not supported for ChromeOS out of the box.
IN_PROC_BROWSER_TEST_F(EmbeddedA11yExtensionLoaderTest,
                       InstallsOnGuestProfile) {
  Browser* guest_browser = CreateGuestBrowser();
  content::RunAllTasksUntilIdle();

  InstallAndWaitForExtensionLoaded(
      guest_browser->profile(),
      extension_misc::kReadingModeGDocsHelperExtensionId,
      extension_misc::kReadingModeGDocsHelperExtensionPath,
      extension_misc::kReadingModeGDocsHelperManifestFilename,
      /*should_localize=*/false);
  RemoveAndWaitForExtensionUnloaded(
      guest_browser->profile(),
      extension_misc::kReadingModeGDocsHelperExtensionId);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(EmbeddedA11yExtensionLoaderTest,
                       InstallsExtensionOnLacros) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const auto& profiles = profile_manager->GetLoadedProfiles();
  ASSERT_GT(profiles.size(), 0u);
  Profile* profile = profiles[0];

  InstallAndWaitForExtensionLoaded(
      profile, extension_misc::kEmbeddedA11yHelperExtensionId,
      extension_misc::kEmbeddedA11yHelperExtensionPath,
      extension_misc::kEmbeddedA11yHelperManifestFilename,
      /*should_localize=*/true);
  RemoveAndWaitForExtensionUnloaded(
      profile, extension_misc::kEmbeddedA11yHelperExtensionId);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
