// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/web_contents.h"
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
        {blink::features::kPermissionElement,
         blink::features::kDisablePepcSecurityForTesting},
        {});
  }

  PermissionElementBrowserTest(const PermissionElementBrowserTest&) = delete;
  PermissionElementBrowserTest& operator=(const PermissionElementBrowserTest&) =
      delete;

  ~PermissionElementBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    console_observer_ =
        std::make_unique<content::WebContentsConsoleObserver>(web_contents());
    ASSERT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(),
        embedded_test_server()->GetURL("/permissions/permission_element.html"),
        1));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void WaitForResolveEvent(const std::string& id) {
    ExpectConsoleMessage(id + "-resolve");
  }

  void WaitForDismissEvent(const std::string& id) {
    ExpectConsoleMessage(id + "-dismiss");
  }

  void ExpectNoEvents() { EXPECT_EQ(0u, console_observer_->messages().size()); }

  void ExpectConsoleMessage(const std::string& expected_message,
                            std::optional<blink::mojom::ConsoleMessageLevel>
                                log_level = std::nullopt) {
    EXPECT_TRUE(console_observer_->Wait());

    EXPECT_EQ(1u, console_observer_->messages().size());
    EXPECT_EQ(expected_message, console_observer_->GetMessageAt(0));
    if (log_level) {
      EXPECT_EQ(log_level.value(), console_observer_->messages()[0].log_level);
    }

    // WebContentsConsoleObserver::Wait() will only wait until there is at least
    // one message. We need to reset the |console_observer_| in order to be able
    // to wait for the next message.
    console_observer_ =
        std::make_unique<content::WebContentsConsoleObserver>(web_contents());
  }

  void SkipInvalidElementMessage() {
    ExpectConsoleMessage(
        "The permission type 'invalid microphone' is not supported by the "
        "permission element.");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::WebContentsConsoleObserver> console_observer_;
};

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       RequestInvalidPermissionType) {
  ExpectConsoleMessage(
      "The permission type 'invalid microphone' is not supported by the "
      "permission element.",
      blink::mojom::ConsoleMessageLevel::kError);
}

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       RequestPermissionDispatchResolveEvent) {
  SkipInvalidElementMessage();

  permissions::PermissionRequestManager::AutoResponseType responses[] = {
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL,
      permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ONCE,
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL};

  std::string permission_ids[] = {"geolocation", "microphone", "camera",
                                  "camera-microphone"};

  for (const auto& response : responses) {
    permissions::PermissionRequestManager::FromWebContents(web_contents())
        ->set_auto_response_for_test(response);
    for (const auto& id : permission_ids) {
      permissions::PermissionRequestObserver observer(web_contents());
      ClickElementWithId(web_contents(), id);
      observer.Wait();
      WaitForResolveEvent(id);
    }
  }
}

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       RequestPermissionDispatchDismissEvent) {
  SkipInvalidElementMessage();
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::DISMISS);
  std::string permission_ids[] = {"geolocation", "microphone", "camera",
                                  "camera-microphone"};
  for (const auto& id : permission_ids) {
    permissions::PermissionRequestObserver observer(web_contents());
    ClickElementWithId(web_contents(), id);
    observer.Wait();
    WaitForDismissEvent(id);
  }
}

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       ClickingScrimViewDispatchDismissEvent) {
  SkipInvalidElementMessage();
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::NONE);
  std::string permission_ids[] = {"microphone", "camera", "camera-microphone"};
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

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest, TabSwitchingClosesPrompt) {
  SkipInvalidElementMessage();
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::NONE);

  permissions::PermissionRequestObserver observer(web_contents());
  ClickElementWithId(web_contents(), "camera");
  observer.Wait();

  std::unique_ptr<content::WebContents> new_tab = content::WebContents::Create(
      content::WebContents::CreateParams(browser()->profile()));
  browser()->tab_strip_model()->AppendWebContents(std::move(new_tab),
                                                  /*foreground*/ false);

  ExpectNoEvents();
  browser()->tab_strip_model()->ActivateTabAt(1);
  WaitForDismissEvent("camera");
}

class PermissionElementWithSecurityBrowserTest : public InProcessBrowserTest {
 public:
  PermissionElementWithSecurityBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kPermissionElement}, {});
  }

  PermissionElementWithSecurityBrowserTest(
      const PermissionElementWithSecurityBrowserTest&) = delete;
  PermissionElementWithSecurityBrowserTest& operator=(
      const PermissionElementWithSecurityBrowserTest&) = delete;

  ~PermissionElementWithSecurityBrowserTest() override = default;

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

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionElementWithSecurityBrowserTest,
                       JsClickingDisabledWithoutFeature) {
  permissions::PermissionRequestObserver permission_observer(web_contents());
  content::WebContentsConsoleObserver console_observer(web_contents());

  // Clicking via JS should be disabled.
  ClickElementWithId(web_contents(), "microphone");
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(console_observer.messages().size(), 1u);
  EXPECT_EQ(
      console_observer.GetMessageAt(0u),
      "The permission element can only be activated by actual user clicks.");
  EXPECT_FALSE(permission_observer.request_shown());

  // Also attempt clicking by creating a MouseEvent.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      content::JsReplace("document.getElementById($1).dispatchEvent(new "
                         "MouseEvent('click'));",
                         "microphone")));

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(console_observer.messages().size(), 2u);
  EXPECT_EQ(
      console_observer.GetMessageAt(1u),
      "The permission element can only be activated by actual user clicks.");
  EXPECT_FALSE(permission_observer.request_shown());

  // Now generate a legacy microphone permission request and wait until it is
  // observed. Then verify that no other requests have arrived.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "const stream = navigator.mediaDevices.getUserMedia({audio: true});"));
  permission_observer.Wait();
  EXPECT_TRUE(permission_observer.request_shown());
  EXPECT_EQ(console_observer.messages().size(), 2u);

  // Verify that we have observed the non-PEPC initiated request.
  EXPECT_EQ(
      permissions::PermissionRequestManager::FromWebContents(web_contents())
          ->Requests()
          .size(),
      1U);
  EXPECT_FALSE(
      permissions::PermissionRequestManager::FromWebContents(web_contents())
          ->Requests()[0]
          ->IsEmbeddedPermissionElementInitiated());
}
