// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/picture_in_picture/document_picture_in_picture_mixin_test_base.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/exclusive_access/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/widget/widget.h"

namespace {

using content::EvalJs;

// Native focus and pointer capture require the serialized interactive runner.
class DocumentPipExclusiveAccessInteractiveUiTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  DocumentPipExclusiveAccessInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{blink::features::kDocumentPictureInPictureAPI, true},
         {features::kDocumentPipStandaloneWindow, GetParam()}});
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    ExclusiveAccessBubbleViews::set_skip_presentation_delay_for_testing(true);
    HostContentSettingsMapFactory::GetForProfile(browser()->GetProfile())
        ->SetDefaultContentSetting(ContentSettingsType::KEYBOARD_LOCK,
                                   CONTENT_SETTING_ALLOW);
  }

  void TearDownOnMainThread() override {
    PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
    ExclusiveAccessBubbleViews::set_skip_presentation_delay_for_testing(false);
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* child() {
    return PictureInPictureWindowManager::GetInstance()->GetChildWebContents();
  }

  DocumentPipHost* GetDocumentPipHost() {
    return DocumentPipHost::FromWebContents(
        browser()->GetTabStripModel()->GetActiveWebContents());
  }

  ExclusiveAccessManager* GetPipExclusiveAccessManager() {
    if (GetParam()) {
      return GetDocumentPipHost()->GetExclusiveAccessManagerForTesting();
    }
    ExclusiveAccessManager* manager = nullptr;
    GlobalBrowserCollection::GetInstance()->ForEach(
        [&](BrowserWindowInterface* window) {
          if (window->GetTabStripModel()->GetActiveWebContents() == child()) {
            manager = ExclusiveAccessManager::From(window);
          }
          return !manager;
        });
    return manager;
  }

  void OpenAndFocusPipWindow() {
    ASSERT_NO_FATAL_FAILURE(pip_mixin_.NavigateToURLAndEnterPictureInPicture(
        browser(), gfx::Size(400, 300)));
    ASSERT_TRUE(child());
    auto* widget = views::Widget::GetWidgetForNativeWindow(
        child()->GetTopLevelNativeWindow());
    ASSERT_TRUE(widget);
    widget->Activate();
    views::test::WaitForWidgetActive(widget, true);
    child()->Focus();
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return child()->GetRenderWidgetHostView()->HasFocus(); }));
    content::ReadyForInputObserver(child()).Wait();
    // Input readiness releases paint holding; flush the input channel so the
    // renderer has also bound the host used by requestPointerLock().
    child()
        ->GetRenderWidgetHostView()
        ->GetRenderWidgetHost()
        ->FlushForTesting();
    ASSERT_TRUE(child()->GetRenderWidgetHostView()->HasFocus());
    ASSERT_TRUE(GetPipExclusiveAccessManager());
  }

  void LockPointer(bool allow_silent_relock = false) {
    ASSERT_EQ(true, EvalJs(child(), R"(
      (async () => {
        const changed = new Promise(resolve => {
          document.addEventListener('pointerlockchange', resolve, {once: true});
        });
        await document.body.requestPointerLock();
        await changed;
        return document.pointerLockElement === document.body;
      })()
    )",
                           allow_silent_relock
                               ? content::EXECUTE_SCRIPT_NO_USER_GESTURE
                               : content::EXECUTE_SCRIPT_DEFAULT_OPTIONS));
    ASSERT_TRUE(child()->GetRenderWidgetHostView()->IsPointerLocked());
    auto* manager = GetPipExclusiveAccessManager();
    ASSERT_TRUE(manager);
    ASSERT_TRUE(manager->pointer_lock_controller()->IsPointerLocked());
    if (manager->pointer_lock_controller()->IsPointerLockedSilently()) {
      ASSERT_TRUE(allow_silent_relock);
      ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
                manager->GetExclusiveAccessExitBubbleType());
      return;
    }
    ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_POINTERLOCK_EXIT_INSTRUCTION,
              manager->GetExclusiveAccessExitBubbleType());
    ASSERT_TRUE(manager->context()->IsExclusiveAccessBubbleDisplayed());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DocumentPictureInPictureMixinTestBase pip_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_P(DocumentPipExclusiveAccessInteractiveUiTest,
                       PointerLockBubbleAndEscape) {
  ASSERT_NO_FATAL_FAILURE(OpenAndFocusPipWindow());
  ASSERT_NO_FATAL_FAILURE(LockPointer());
  ASSERT_TRUE(content::ExecJs(child(), R"(
    var unlocked = new Promise(resolve => {
      document.addEventListener('pointerlockchange', resolve, {once: true});
    });
  )"));
  // SimulateKeyPress skips browser prehandling, which handles Esc for locks.
  for (auto type : {ui::EventType::kKeyPressed, ui::EventType::kKeyReleased}) {
    input::NativeWebKeyboardEvent event(
        ui::KeyEvent(type, ui::VKEY_ESCAPE, ui::EF_NONE));
    event.skip_if_unhandled = false;
    child()
        ->GetRenderWidgetHostView()
        ->GetRenderWidgetHost()
        ->ForwardKeyboardEvent(event);
  }
  EXPECT_EQ(true, EvalJs(child(),
                         "unlocked.then(() => document.pointerLockElement === "
                         "null)"));
  auto* manager = GetPipExclusiveAccessManager();
  EXPECT_FALSE(manager->pointer_lock_controller()->IsPointerLocked());
  EXPECT_FALSE(child()->GetRenderWidgetHostView()->IsPointerLocked());
  EXPECT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
            manager->GetExclusiveAccessExitBubbleType());
  EXPECT_TRUE(base::test::RunUntil(
      [&] { return !manager->context()->IsExclusiveAccessBubbleDisplayed(); }));
}

IN_PROC_BROWSER_TEST_P(DocumentPipExclusiveAccessInteractiveUiTest,
                       PageUnlockRelockAndClose) {
  ASSERT_NO_FATAL_FAILURE(OpenAndFocusPipWindow());
  ASSERT_NO_FATAL_FAILURE(LockPointer());
  ASSERT_EQ(true, EvalJs(child(), R"(
    (async () => {
      const changed = new Promise(resolve => {
        document.addEventListener('pointerlockchange', resolve, {once: true});
      });
      document.exitPointerLock();
      await changed;
      return document.pointerLockElement === null;
    })()
  )"));
  ASSERT_FALSE(GetPipExclusiveAccessManager()
                   ->pointer_lock_controller()
                   ->IsPointerLocked());
  ASSERT_NO_FATAL_FAILURE(LockPointer(/*allow_silent_relock=*/true));

  content::WebContentsDestroyedWatcher destroyed(child());
  EXPECT_TRUE(
      PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture());
  destroyed.Wait();
  EXPECT_FALSE(child());
  if (GetParam()) {
    EXPECT_FALSE(GetDocumentPipHost()->GetExclusiveAccessManagerForTesting());
    EXPECT_FALSE(GetDocumentPipHost()->GetWidget());
  }
}

IN_PROC_BROWSER_TEST_P(DocumentPipExclusiveAccessInteractiveUiTest,
                       KeyboardLockDoesNotActivateWithoutFullscreen) {
  ASSERT_NO_FATAL_FAILURE(OpenAndFocusPipWindow());
  EXPECT_EQ(true, EvalJs(child(), R"(
    (async () => {
      await navigator.keyboard.lock(['Escape']);
      return true;
    })()
  )"));
  auto* manager = GetPipExclusiveAccessManager();
  EXPECT_FALSE(child()->IsFullscreen());
  EXPECT_FALSE(child()->GetRenderWidgetHostView()->IsKeyboardLocked());
  EXPECT_FALSE(manager->keyboard_lock_controller()->IsKeyboardLockActive());
  EXPECT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
            manager->GetExclusiveAccessExitBubbleType());
  EXPECT_FALSE(manager->context()->IsExclusiveAccessBubbleDisplayed());
  EXPECT_TRUE(content::ExecJs(child(), "navigator.keyboard.unlock()"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         DocumentPipExclusiveAccessInteractiveUiTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Standalone" : "BrowserBacked";
                         });

}  // namespace
