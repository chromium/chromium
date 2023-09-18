// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/features.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

const base::TimeDelta kDefaultDisableTimeout = base::Milliseconds(1000);

// Simulates a click on an element with the given |id|.
void ClickElementWithId(content::WebContents* web_contents,
                        const std::string& id) {
  const int x =
      content::EvalJs(
          web_contents,
          content::JsReplace("const bounds = "
                             "document.getElementById($1)."
                             "getBoundingClientRect();"
                             "Math.floor(bounds.left + bounds.width / 2)",
                             id))
          .ExtractInt();
  const int y =
      content::EvalJs(
          web_contents,
          content::JsReplace("const bounds = "
                             "document.getElementById($1)."
                             "getBoundingClientRect();"
                             "Math.floor(bounds.top + bounds.height / 2)",
                             id))
          .ExtractInt();

  content::SimulateMouseClickAt(
      web_contents, 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(x, y));
}

}  // namespace

class PermissionElementBrowserTest : public InProcessBrowserTest {
 public:
  PermissionElementBrowserTest() {
    feature_list_.InitAndEnableFeature(
        permissions::features::kPermissionElement);
  }

  PermissionElementBrowserTest(const PermissionElementBrowserTest&) = delete;
  PermissionElementBrowserTest& operator=(const PermissionElementBrowserTest&) =
      delete;

  ~PermissionElementBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "PermissionElement");
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(),
        embedded_test_server()->GetURL("/permissions/permission_element.html"),
        1));
    // Delay a short time to make sure all <permission> elements are clickable.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), kDefaultDisableTimeout);
    run_loop.Run();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void WaitForResolvedEvent(const std::string& id) {
    EXPECT_EQ(true, content::EvalJs(
                        web_contents(),
                        content::JsReplace("waitForResolvedEvent($1)", id)));
  }

  void WaitForDismissedEvent(const std::string& id) {
    EXPECT_EQ(true, content::EvalJs(
                        web_contents(),
                        content::JsReplace("waitForDismissedEvent($1)", id)));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       RequestInvalidPermissionType) {
  content::WebContentsConsoleObserver console_observer(web_contents());
  ClickElementWithId(web_contents(), "invalid");
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
                       RequestPermissionDispatchResolvedEvent) {
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
    WaitForResolvedEvent(id);
  }
}

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       RequestPermissionDispatchDismissedEvent) {
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
    WaitForDismissedEvent(id);
  }
}
