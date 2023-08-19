// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

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

constexpr const char16_t kMenuItemName[] = u"Listen to selected text";

using AssistiveTechnologyType = crosapi::mojom::AssistiveTechnologyType;

gfx::Rect GetControlBoundsInRoot(content::WebContents* web_contents,
                                 const std::string& field_id) {
  // Use var instead of const or let so that this can be executed many
  // times within a context on different elements without Javascript
  // variable redeclaration errors.
  content::ExecJs(web_contents, base::StringPrintf(R"(
      var element = document.getElementById('%s');
      var bounds = element.getBoundingClientRect();
    )",
                                                   field_id.c_str()));
  int top = content::EvalJs(web_contents, "bounds.top").ExtractInt();
  int left = content::EvalJs(web_contents, "bounds.left").ExtractInt();
  int width = content::EvalJs(web_contents, "bounds.width").ExtractInt();
  int height = content::EvalJs(web_contents, "bounds.height").ExtractInt();
  gfx::Rect rect(left, top, width, height);

  content::RenderWidgetHostView* view = web_contents->GetRenderWidgetHostView();
  gfx::Rect view_bounds_in_screen = view->GetViewBounds();
  gfx::Point origin = rect.origin();
  origin.Offset(view_bounds_in_screen.x(), view_bounds_in_screen.y());
  gfx::Rect rect_in_screen(origin.x(), origin.y(), rect.width(), rect.height());
  return rect_in_screen;
}

// Waits for a context menu to be shown.
class ContextMenuWaiter {
 public:
  ContextMenuWaiter() {
    RenderViewContextMenu::RegisterMenuShownCallbackForTesting(
        base::BindOnce(&ContextMenuWaiter::MenuShown, base::Unretained(this)));
  }
  ContextMenuWaiter(const ContextMenuWaiter&) = delete;
  ContextMenuWaiter& operator=(const ContextMenuWaiter&) = delete;

  ~ContextMenuWaiter() = default;

  RenderViewContextMenu* WaitForMenuShown() {
    waiter_.Run();
    return context_menu_;
  }

 private:
  void MenuShown(RenderViewContextMenu* context_menu) {
    waiter_.Quit();
    context_menu_ = context_menu;
  }

  base::RunLoop waiter_;
  raw_ptr<RenderViewContextMenu> context_menu_ = nullptr;
};

}  // namespace

class EmbeddedA11yManagerLacrosTest : public InProcessBrowserTest {
 public:
  EmbeddedA11yManagerLacrosTest() = default;
  ~EmbeddedA11yManagerLacrosTest() override = default;
  EmbeddedA11yManagerLacrosTest(const EmbeddedA11yManagerLacrosTest&) = delete;
  EmbeddedA11yManagerLacrosTest& operator=(
      const EmbeddedA11yManagerLacrosTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kApiAccessibilityServicePrivate);
  }

  void SetUp() override {
    // Start unique Ash instance for AccessibilityServicePrivate enabled.
    StartUniqueAshChrome(
        /*enabled_features=*/{"ApiAccessibilityServicePrivate"},
        /*disabled_features=*/{}, /*additional_cmdline_switches=*/{},
        "crbug/1459275 Switch to shared ash when the "
        "AccessibilityServicePrivate API is enabled by default.");
    InProcessBrowserTest::SetUp();
  }

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

  RenderViewContextMenu* LoadTestPageAndSelectTextAndRightClick() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    CHECK(ui_test_utils::NavigateToURL(
        browser(), GURL(("data:text/html;charset=utf-8,<p "
                         "id='selected'>This is some selected text</p>"))));
    EXPECT_TRUE(content::WaitForLoadStop(web_contents));

    content::BoundingBoxUpdateWaiter bounding_box_waiter(web_contents);

    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::Widget* widget = browser_view->GetWidget();
    aura::Window* window = widget->GetNativeWindow();

    ui::test::EventGenerator generator(window->GetRootWindow());
    const gfx::Rect text_bounds =
        GetControlBoundsInRoot(web_contents, "selected");
    generator.MoveMouseTo(text_bounds.CenterPoint());
    generator.ClickLeftButton();

    // Set selection with ctrl+a.
    generator.PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
    bounding_box_waiter.Wait();

    ContextMenuWaiter menu_waiter;
    generator.PressRightButton();

    RenderViewContextMenu* menu = menu_waiter.WaitForMenuShown();
    CHECK(menu);
    return menu;
  }

  int num_context_clicks() const { return num_context_clicks_; }

 private:
  void OnExtensionChanged() {
    if (waiter_ && waiter_->running()) {
      waiter_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> waiter_;
  int num_context_clicks_ = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
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

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       DoesNotShowContextMenuWhenSelectToSpeakDisabled) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_TRUE(profile);

  extensions::service_worker_test_utils::TestRegistrationObserver
      service_worker_observer(profile);

  SetEnabledAndWaitForExtensionLoaded(
      profile, AssistiveTechnologyType::kSwitchAccess,
      extension_misc::kEmbeddedA11yHelperExtensionId);

  service_worker_observer.WaitForWorkerStart();

  RenderViewContextMenu* menu = LoadTestPageAndSelectTextAndRightClick();

  const ui::SimpleMenuModel& menu_model = menu->menu_model();
  bool found = false;
  for (size_t i = 0; i < menu_model.GetItemCount(); i++) {
    if (menu_model.GetLabelAt(i) == kMenuItemName) {
      found = true;
      break;
    }
  }
  ASSERT_FALSE(found);

  SetDisabledAndWaitForExtensionUnloaded(
      profile, AssistiveTechnologyType::kSwitchAccess,
      extension_misc::kEmbeddedA11yHelperExtensionId);
}

IN_PROC_BROWSER_TEST_F(EmbeddedA11yManagerLacrosTest,
                       TriggerSelectToSpeakFromContextMenu) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_TRUE(profile);

  extensions::service_worker_test_utils::TestRegistrationObserver
      service_worker_observer(profile);

  SetEnabledAndWaitForExtensionLoaded(
      profile, AssistiveTechnologyType::kSelectToSpeak,
      extension_misc::kEmbeddedA11yHelperExtensionId);

  service_worker_observer.WaitForWorkerStart();

  RenderViewContextMenu* menu = LoadTestPageAndSelectTextAndRightClick();

  const ui::SimpleMenuModel& menu_model = menu->menu_model();
  int found_index = -1;
  for (size_t i = 0; i < menu_model.GetItemCount(); i++) {
    if (menu_model.GetLabelAt(i) == kMenuItemName) {
      found_index = i;
      break;
    }
  }
  ASSERT_GE(found_index, 0);

  base::RunLoop run_loop;
  EmbeddedA11yManagerLacros::GetInstance()->AddSpeakSelectedTextCallbackForTest(
      run_loop.QuitClosure());

  int command_id = menu_model.GetCommandIdAt(found_index);
  menu->ExecuteCommand(command_id, /*flags=*/0);

  // Block until the callback is received.
  run_loop.Run();

  SetDisabledAndWaitForExtensionUnloaded(
      profile, AssistiveTechnologyType::kSelectToSpeak,
      extension_misc::kEmbeddedA11yHelperExtensionId);
}
