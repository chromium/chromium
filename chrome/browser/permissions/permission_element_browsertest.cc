// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/webrtc/media_stream_device_permission_context.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_content_scrim_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-shared.h"
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

class PermissionElementBrowserTestBase : public InProcessBrowserTest {
 public:
  PermissionElementBrowserTestBase() = default;

  PermissionElementBrowserTestBase(const PermissionElementBrowserTestBase&) =
      delete;
  PermissionElementBrowserTestBase& operator=(
      const PermissionElementBrowserTestBase&) = delete;

  ~PermissionElementBrowserTestBase() override = default;

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

  void WaitForUpdateGrantedPermissionElement(const std::string& id) {
    ExpectConsoleMessage(id + "-granted");
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

  void TestPromptPosition(
      permissions::feature_params::PermissionElementPromptPosition position) {
    auto* permission_request_manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents());

    permissions::PermissionRequestObserver observer(web_contents());
    ClickElementWithId(web_contents(), "camera");
    observer.Wait();

    EXPECT_EQ(
        permission_request_manager->view_for_testing()->GetPromptPosition(),
        position);

    permission_request_manager->Dismiss();
    permission_request_manager->FinalizeCurrentRequests();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  std::unique_ptr<content::WebContentsConsoleObserver> console_observer_;
};

class PermissionElementBrowserTest : public PermissionElementBrowserTestBase {
 public:
  PermissionElementBrowserTest() {
    feature_list_.InitWithFeatures(
        {blink::features::kPermissionElement,
         blink::features::kBypassPepcSecurityForTesting},
        {permissions::features::kPermissionElementPromptPositioning});
  }
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
                       DispatchResolveEventUpdateGrantedElement) {
  SkipInvalidElementMessage();
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);
  std::string permission_ids[] = {"microphone", "camera", "camera-microphone"};
  for (const auto& id : permission_ids) {
    permissions::PermissionRequestObserver observer(web_contents());
    ClickElementWithId(web_contents(), id);
    observer.Wait();
    WaitForResolveEvent(id);
    ASSERT_TRUE(content::ExecJs(
        web_contents(), content::JsReplace("notifyWhenGranted($1);", id)));
    WaitForUpdateGrantedPermissionElement(id);
  }
}

class PermissionServiceInterceptor : public blink::mojom::PermissionObserver {
 public:
  explicit PermissionServiceInterceptor(
      content::RenderFrameHost* render_frame_host)
      : render_frame_host_(render_frame_host) {
    OverrideBinderForTesting();
  }

  ~PermissionServiceInterceptor() override = default;

  blink::mojom::PermissionService* GetForwardingInterface() {
    return permission_service_.get();
  }

  void AddPermissionStatusObserver(blink::mojom::PermissionName permission) {
    auto descriptor = blink::mojom::PermissionDescriptor::New();
    descriptor->name = permission;
    GetForwardingInterface()->AddPageEmbeddedPermissionObserver(
        std::move(descriptor), blink::mojom::PermissionStatus::ASK,
        GetRemote());
  }

  // Blocks until getting status granted event.
  void WaitForPermissionGranted() { loop_.Run(); }

 private:
  mojo::PendingRemote<blink::mojom::PermissionObserver> GetRemote() {
    mojo::PendingRemote<blink::mojom::PermissionObserver> remote;
    observer_receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  // blink::mojom::PermissionObserver implementation.
  void OnPermissionStatusChange(
      blink::mojom::PermissionStatus status) override {
    if (status == blink::mojom::PermissionStatus::GRANTED) {
      loop_.Quit();
    }
  }

  void OverrideBinderForTesting() {
    content::CreatePermissionService(
        render_frame_host_, permission_service_.BindNewPipeAndPassReceiver());
  }

  base::RunLoop loop_;
  const raw_ptr<content::RenderFrameHost> render_frame_host_;
  mojo::Remote<blink::mojom::PermissionService> permission_service_;
  mojo::Receiver<blink::mojom::PermissionObserver> observer_receiver_{this};
};

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       CombinedPermissionAndDeviceStatusesGranted) {
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);
  PermissionServiceInterceptor permission_service(
      web_contents()->GetPrimaryMainFrame());
  MediaStreamDevicePermissionContext* camera_permission_context =
      static_cast<MediaStreamDevicePermissionContext*>(
          PermissionManagerFactory::GetForProfile(browser()->profile())
              ->GetPermissionContextForTesting(
                  ContentSettingsType::MEDIASTREAM_CAMERA));
  camera_permission_context->set_has_device_permission_for_test(
      /*has_permission=*/false);
  permission_service.AddPermissionStatusObserver(
      blink::mojom::PermissionName::VIDEO_CAPTURE);
  ClickElementWithId(web_contents(), "camera");
  // Simulate that we accept the device permission request.
  camera_permission_context->set_has_device_permission_for_test(
      /*has_permission=*/true);
  permission_service.WaitForPermissionGranted();
  camera_permission_context->set_has_device_permission_for_test(std::nullopt);
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
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    WaitForDismissEvent(id);
  }
}

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       TappingScrimViewDispatchDismissEvent) {
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
    ui::GestureEvent tap_down(
        gfx::Point().x(), gfx::Point().y(), 0, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureTapDown));
    scrim_view->OnGestureEvent(&tap_down);
    ui::GestureEvent tap_up(
        gfx::Point().x(), gfx::Point().y(), 0, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureTap));
    scrim_view->OnGestureEvent(&tap_up);
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

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest,
                       DoubleClickDoesNotTriggerTwoRequests) {
  SkipInvalidElementMessage();
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::DISMISS);

  permissions::PermissionRequestObserver observer1(web_contents());
  content::WebContentsConsoleObserver console_observer(web_contents());

  // Click the element twice.
  ClickElementWithId(web_contents(), "microphone");
  ClickElementWithId(web_contents(), "microphone");

  EXPECT_EQ(console_observer.messages().size(), 1u);
  ExpectConsoleMessage(
      "The permission element already has a request in progress.");

  // Multiple clicks on the same permission element should only trigger one
  // request.
  observer1.Wait();
  EXPECT_TRUE(observer1.request_shown());
  WaitForDismissEvent("microphone");

  // Verify that no duplicate "microphone" requests or dismiss events are
  // created.
  permissions::PermissionRequestObserver observer2(web_contents());
  ClickElementWithId(web_contents(), "camera");
  observer2.Wait();
  EXPECT_TRUE(observer2.request_shown());
  WaitForDismissEvent("camera");

  // Verify that clicking again on the same element after the prompt was
  // dismissed, results in a permission request being shown.
  permissions::PermissionRequestObserver observer3(web_contents());
  ClickElementWithId(web_contents(), "microphone");
  observer3.Wait();
  WaitForDismissEvent("microphone");
  EXPECT_TRUE(observer3.request_shown());
}

class PermissionElementWithSecurityBrowserTest
    : public PermissionElementBrowserTestBase {
 public:
  PermissionElementWithSecurityBrowserTest() {
    feature_list_.InitWithFeatures({blink::features::kPermissionElement}, {});
  }
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

class PermissionElementStandardizedBrowserZoomTest
    : public PermissionElementBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  PermissionElementStandardizedBrowserZoomTest() {
    // Also enable/disable the StandardizedBrowserZoom feature.
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          {blink::features::kPermissionElement,
           blink::features::kBypassPepcSecurityForTesting,
           blink::features::kStandardizedBrowserZoom},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {blink::features::kPermissionElement,
           blink::features::kBypassPepcSecurityForTesting},
          {blink::features::kStandardizedBrowserZoom});
    }
  }

  void WaitForFontSizeTooLargeEvent(const std::string& id) {
    auto type_attribute_value = content::EvalJs(
        web_contents(),
        content::JsReplace("document.getElementById($1).type", id));
    EXPECT_TRUE(type_attribute_value.error.empty());
    ExpectConsoleMessage("Font size of the permission element '" +
                         type_attribute_value.ExtractString() +
                         "' is too large");
  }
};

IN_PROC_BROWSER_TEST_P(PermissionElementStandardizedBrowserZoomTest,
                       BrowserZoomDoesNotAffectValidation) {
  SkipInvalidElementMessage();
  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents());

  // 2x zoom is enough since the font-size is already set to xxx-large which is
  // the upper bound.
  zoom_controller->SetZoomLevel(2);

  for (const auto& id : {"camera", "microphone", "camera-microphone"}) {
    // The permission element still works.
    ClickElementWithId(web_contents(), id);
    WaitForResolveEvent(id);
    ExpectNoEvents();

    // Now set the CSS "zoom" to 2x.
    ASSERT_TRUE(content::ExecJs(
        web_contents(),
        content::JsReplace("document.getElementById($1).style.zoom = 2;", id)));
    WaitForFontSizeTooLargeEvent(id);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         PermissionElementStandardizedBrowserZoomTest,
                         testing::Bool());

// Test fixture identical with |PermissionElementBrowserTest| but with simulated
// different DPI devices.
class PermissionElementHighDPITest : public PermissionElementBrowserTest,
                                     public testing::WithParamInterface<float> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PermissionElementBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::StringPrintf("%f", GetParam()));
  }
};

// Ensure that the margin limit of 4px is applied regardless of device DPI.
IN_PROC_BROWSER_TEST_P(PermissionElementHighDPITest, TestMargins) {
  SkipInvalidElementMessage();
  for (const auto& property :
       {"marginTop", "marginBottom", "marginLeft", "marginRight"}) {
    for (const auto& id : {"camera", "microphone", "camera-microphone"}) {
      EXPECT_EQ(
          "4px",
          content::EvalJs(
              web_contents(),
              base::StrCat({content::JsReplace(
                                "getComputedStyle(document.getElementById("
                                "$1)).",
                                id),
                            property})));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         PermissionElementHighDPITest,
                         testing::Values(1.f, 1.25f, 1.5f, 2.f, 3.f));

class PermissionElementNearElementBrowserTest
    : public PermissionElementBrowserTestBase {
 public:
  PermissionElementNearElementBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kPermissionElement, {}},
         {blink::features::kBypassPepcSecurityForTesting, {}},
         {permissions::features::kPermissionElementPromptPositioning,
          {{"PermissionElementPromptPositioningParam", "near_element"}}}},
        {});
  }
};

class PermissionElementWindowMiddleBrowserTest
    : public PermissionElementBrowserTestBase {
 public:
  PermissionElementWindowMiddleBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kPermissionElement, {}},
         {blink::features::kBypassPepcSecurityForTesting, {}},
         {permissions::features::kPermissionElementPromptPositioning,
          {{"PermissionElementPromptPositioningParam", "window_middle"}}}},
        {});
  }
};

class PermissionElementLegacyPromptBrowserTest
    : public PermissionElementBrowserTestBase {
 public:
  PermissionElementLegacyPromptBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kPermissionElement, {}},
         {blink::features::kBypassPepcSecurityForTesting, {}},
         {permissions::features::kPermissionElementPromptPositioning,
          {{"PermissionElementPromptPositioningParam", "legacy_prompt"}}}},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(PermissionElementBrowserTest, DefaultPromptPosition) {
  TestPromptPosition(permissions::feature_params::
                         PermissionElementPromptPosition::kWindowMiddle);
}

IN_PROC_BROWSER_TEST_F(PermissionElementNearElementBrowserTest,
                       PromptPosition) {
  TestPromptPosition(permissions::feature_params::
                         PermissionElementPromptPosition::kNearElement);
}

IN_PROC_BROWSER_TEST_F(PermissionElementWindowMiddleBrowserTest,
                       PromptPosition) {
  TestPromptPosition(permissions::feature_params::
                         PermissionElementPromptPosition::kWindowMiddle);
}

IN_PROC_BROWSER_TEST_F(PermissionElementLegacyPromptBrowserTest,
                       PromptPosition) {
  TestPromptPosition(permissions::feature_params::
                         PermissionElementPromptPosition::kLegacyPrompt);
}
