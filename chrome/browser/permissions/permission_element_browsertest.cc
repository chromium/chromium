// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

// Simulates a click on an element with the given |id|.
void ClickElementWithId(content::WebContents* web_contents,
                        const std::string& id) {
  ASSERT_TRUE(
      content::ExecJs(web_contents, content::JsReplace("clickById($1)", id)));
}

}  // namespace

class PermissionElementBrowserTest : public InProcessBrowserTest {
 public:
  PermissionElementBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kPermissionElement,
         blink::features::kDisablePepcSecurityForTesting},
        {});
  }

  PermissionElementBrowserTest(const PermissionElementBrowserTest&) = delete;
  PermissionElementBrowserTest& operator=(const PermissionElementBrowserTest&) =
      delete;

  ~PermissionElementBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(),
        embedded_test_server()->GetURL("/permissions/permission_element.html"),
        1));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void WaitForResolveEvent(const std::string& id) {
    EXPECT_EQ(true, content::EvalJs(
                        web_contents(),
                        content::JsReplace("waitForResolveEvent($1)", id)));
  }

  void WaitForDismissEvent(const std::string& id) {
    EXPECT_EQ(true, content::EvalJs(
                        web_contents(),
                        content::JsReplace("waitForDismissEvent($1)", id)));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       RequestInvalidPermissionType) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  ASSERT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/permission_element.html"),
      1));
  ASSERT_TRUE(console_observer.Wait());
  ASSERT_EQ(1u, console_observer.messages().size());
  EXPECT_EQ(
      "The permission type 'invalid microphone' is not supported by the "
      "permission element.",
      console_observer.GetMessageAt(0));
  EXPECT_EQ(blink::mojom::ConsoleMessageLevel::kError,
            console_observer.messages()[0].log_level);
}

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       RequestPermissionDispatchResolveEvent) {
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);
  // TODO(crbug.com/1462930): add "camera-microphone" id, after we make sure
  // embedded permission request will be routed to PermissionRequestManager
  // regardless of the stored permission status.
  std::string permission_ids[] = {"geolocation", "microphone", "camera"};
  for (const auto& id : permission_ids) {
    permissions::PermissionRequestObserver observer(web_contents());
    ClickElementWithId(web_contents(), id);
    observer.Wait();
    WaitForResolveEvent(id);
  }
}

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       RequestPermissionDispatchDismissEvent) {
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);
  // TODO(crbug.com/1462930): add "camera-microphone" id, after we make sure
  // embedded permission request will be routed to PermissionRequestManager
  // regardless of the stored permission status.
  std::string permission_ids[] = {"geolocation", "microphone", "camera"};
  for (const auto& id : permission_ids) {
    permissions::PermissionRequestObserver observer(web_contents());
    ClickElementWithId(web_contents(), id);
    observer.Wait();
    WaitForDismissEvent(id);
  }
}

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       ClickingScrimViewDispatchDismissEvent) {
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::NONE);
  std::string permission_ids[] = {"microphone", "camera"};
  for (const auto& id : permission_ids) {
    views::NamedWidgetShownWaiter waiter(
        views::test::AnyWidgetTestPasskey{},
        "EmbeddedPermissionPromptContentScrimWidget");
    ClickElementWithId(web_contents(), id);
    auto* scrim_view = static_cast<EmbeddedPermissionPromptContentScrimView*>(
        waiter.WaitIfNeededAndGet()->GetContentsView());
    scrim_view->OnMousePressed(
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    scrim_view->OnMouseReleased(
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    WaitForDismissEvent(id);
  }
}
