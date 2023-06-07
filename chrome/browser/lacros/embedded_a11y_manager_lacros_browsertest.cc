// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Tests for EmbeddedA11yManagerLacros, ensuring it can install
// the correct accessibility helper extensions on all the profiles
// and responds to the state of the ash accessibility prefs.
//
// NOTE: Tests in this file modify Ash accessibility features. That is
// potentially a lasting side effect that can affect other tests.
// * To prevent interference with tests that are run in parallel, these tests
// are a part of lacros_chrome_browsertests_run_in_series test suite.
// * To prevent interference with following tests, they try to clean up all the
// side effects themselves, e.g. if a test sets a pref, it is also responsible
// for unsetting it.

namespace {

using AssistiveTechnologyType = crosapi::mojom::AssistiveTechnologyType;

}  // namespace

class EmbeddedA11yManagerLacrosTest : public InProcessBrowserTest {
 public:
  EmbeddedA11yManagerLacrosTest() = default;
  ~EmbeddedA11yManagerLacrosTest() override = default;
  EmbeddedA11yManagerLacrosTest(const EmbeddedA11yManagerLacrosTest&) = delete;
  EmbeddedA11yManagerLacrosTest& operator=(
      const EmbeddedA11yManagerLacrosTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    EmbeddedA11yManagerLacros::GetInstance()
        ->AddExtensionChangedCallbackForTest(base::BindRepeating(
            &EmbeddedA11yManagerLacrosTest::OnExtensionChanged,
            base::Unretained(this)));

    auto* lacros_service = chromeos::LacrosService::Get();
    if (!lacros_service ||
        !lacros_service->IsAvailable<crosapi::mojom::TestController>() ||
        lacros_service->GetInterfaceVersion<crosapi::mojom::TestController>() <
            static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                                 kSetAssistiveTechnologyEnabledMinVersion)) {
      GTEST_SKIP() << "Ash version doesn't have required test API";
    }
  }

  void OnExtensionChanged() {
    if (waiter_ && waiter_->running()) {
      waiter_->Quit();
    }
  }

  void SetFeatureEnabled(AssistiveTechnologyType at_type, bool enabled) {
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::TestController>()
        ->SetAssistiveTechnologyEnabled(at_type, enabled);
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

  void SetEnabledAndWaitForExtensionLoaded(Profile* profile,
                                           AssistiveTechnologyType at_type,
                                           const std::string& extension_id) {
    SetFeatureEnabled(at_type, true);
    WaitForExtensionLoaded(profile, extension_id);
  }

  void SetDisabledAndWaitForExtensionUnloaded(Profile* profile,
                                              AssistiveTechnologyType at_type,
                                              const std::string& extension_id) {
    SetFeatureEnabled(at_type, false);
    WaitForExtensionUnloaded(profile, extension_id);
  }

 private:
  std::unique_ptr<base::RunLoop> waiter_;
};

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       AddsAndRemovesHelperForChromeVox) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const auto& profiles = profile_manager->GetLoadedProfiles();
  ASSERT_GT(profiles.size(), 0u);
  Profile* profile = profiles[0];
  SetEnabledAndWaitForExtensionLoaded(
      profile, AssistiveTechnologyType::kChromeVox,
      extension_misc::kChromeVoxHelperExtensionId);
  SetDisabledAndWaitForExtensionUnloaded(
      profile, AssistiveTechnologyType::kChromeVox,
      extension_misc::kChromeVoxHelperExtensionId);
}

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       AddsAndRemovesHelperForSelectToSpeak) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const auto& profiles = profile_manager->GetLoadedProfiles();
  ASSERT_GT(profiles.size(), 0u);
  Profile* profile = profiles[0];
  SetEnabledAndWaitForExtensionLoaded(
      profile, AssistiveTechnologyType::kSelectToSpeak,
      extension_misc::kEmbeddedA11yHelperExtensionId);
  SetDisabledAndWaitForExtensionUnloaded(
      profile, AssistiveTechnologyType::kSelectToSpeak,
      extension_misc::kEmbeddedA11yHelperExtensionId);
}

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       AddsAndRemovesHelperForSwitchAccess) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const auto& profiles = profile_manager->GetLoadedProfiles();
  ASSERT_GT(profiles.size(), 0u);
  Profile* profile = profiles[0];

  SetEnabledAndWaitForExtensionLoaded(
      profile, AssistiveTechnologyType::kSwitchAccess,
      extension_misc::kEmbeddedA11yHelperExtensionId);
  SetDisabledAndWaitForExtensionUnloaded(
      profile, AssistiveTechnologyType::kSwitchAccess,
      extension_misc::kEmbeddedA11yHelperExtensionId);
}

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       SwitchAccessAndSelectToSpeak) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const auto& profiles = profile_manager->GetLoadedProfiles();
  ASSERT_GT(profiles.size(), 0u);
  Profile* profile = profiles[0];

  // Installed with first feature enabled.
  SetEnabledAndWaitForExtensionLoaded(
      profile, AssistiveTechnologyType::kSwitchAccess,
      extension_misc::kEmbeddedA11yHelperExtensionId);

  // Still installed with second feature enabled.
  SetFeatureEnabled(AssistiveTechnologyType::kSelectToSpeak, true);
  extensions::ComponentLoader* component_loader =
      extensions::ExtensionSystem::Get(profile)
          ->extension_service()
          ->component_loader();
  EXPECT_TRUE(
      component_loader->Exists(extension_misc::kEmbeddedA11yHelperExtensionId));

  // Not unloaded if one of the two features is still enabled.
  SetFeatureEnabled(AssistiveTechnologyType::kSwitchAccess, false);
  EXPECT_TRUE(
      component_loader->Exists(extension_misc::kEmbeddedA11yHelperExtensionId));

  // Unloads after Select to Speak is also disabled.
  SetDisabledAndWaitForExtensionUnloaded(
      profile, AssistiveTechnologyType::kSelectToSpeak,
      extension_misc::kEmbeddedA11yHelperExtensionId);
}

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
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
  SetFeatureEnabled(AssistiveTechnologyType::kSwitchAccess, true);
  for (auto* const profile : profiles) {
    WaitForExtensionLoaded(profile,
                           extension_misc::kEmbeddedA11yHelperExtensionId);
  }

  // Turn off switch access.
  SetFeatureEnabled(AssistiveTechnologyType::kSwitchAccess, false);
  for (auto* const profile : profiles) {
    WaitForExtensionUnloaded(profile,
                             extension_misc::kEmbeddedA11yHelperExtensionId);
  }
}

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       IncognitoProfileA11yLoadedFirst) {
  SetFeatureEnabled(AssistiveTechnologyType::kSelectToSpeak, true);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Browser* incognito =
      CreateIncognitoBrowser(profile_manager->GetPrimaryUserProfile());
  content::RunAllTasksUntilIdle();

  WaitForExtensionLoaded(incognito->profile(),
                         extension_misc::kEmbeddedA11yHelperExtensionId);
  SetDisabledAndWaitForExtensionUnloaded(
      incognito->profile(), AssistiveTechnologyType::kSelectToSpeak,
      extension_misc::kEmbeddedA11yHelperExtensionId);
}

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       IncognitoProfileA11yLoadedSecond) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Browser* incognito =
      CreateIncognitoBrowser(profile_manager->GetPrimaryUserProfile());
  content::RunAllTasksUntilIdle();

  SetEnabledAndWaitForExtensionLoaded(
      incognito->profile(), AssistiveTechnologyType::kSelectToSpeak,
      extension_misc::kEmbeddedA11yHelperExtensionId);
  SetDisabledAndWaitForExtensionUnloaded(
      incognito->profile(), AssistiveTechnologyType::kSelectToSpeak,
      extension_misc::kEmbeddedA11yHelperExtensionId);
}

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       GuestProfileA11yLoadedFirst) {
  SetFeatureEnabled(AssistiveTechnologyType::kSwitchAccess, true);

  Browser* guest_browser = CreateGuestBrowser();
  content::RunAllTasksUntilIdle();

  WaitForExtensionLoaded(guest_browser->profile(),
                         extension_misc::kEmbeddedA11yHelperExtensionId);

  SetDisabledAndWaitForExtensionUnloaded(
      guest_browser->profile(), AssistiveTechnologyType::kSwitchAccess,
      extension_misc::kEmbeddedA11yHelperExtensionId);
}

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       GuestProfileA11yLoadedSecond) {
  Browser* guest_browser = CreateGuestBrowser();
  content::RunAllTasksUntilIdle();

  SetDisabledAndWaitForExtensionUnloaded(
      guest_browser->profile(), AssistiveTechnologyType::kChromeVox,
      extension_misc::kChromeVoxHelperExtensionId);
  SetDisabledAndWaitForExtensionUnloaded(
      guest_browser->profile(), AssistiveTechnologyType::kChromeVox,
      extension_misc::kChromeVoxHelperExtensionId);
}
