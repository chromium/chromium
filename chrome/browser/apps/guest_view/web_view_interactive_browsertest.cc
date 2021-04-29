// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/text_input_test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/buildflags.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"

using extensions::AppWindow;
using extensions::ExtensionsAPIClient;
using guest_view::GuestViewBase;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;

#if defined(OS_MAC)
// This class observes the RenderWidgetHostViewCocoa corresponding to the outer
// most WebContents provided for newly added subviews. The added subview
// corresponds to a NSPopUpButtonCell which will be removed shortly after being
// shown.
class NewSubViewAddedObserver : content::RenderWidgetHostViewCocoaObserver {
 public:
  explicit NewSubViewAddedObserver(content::WebContents* web_contents)
      : content::RenderWidgetHostViewCocoaObserver(web_contents) {}

  NewSubViewAddedObserver(const NewSubViewAddedObserver&) = delete;
  NewSubViewAddedObserver& operator=(const NewSubViewAddedObserver&) = delete;
  ~NewSubViewAddedObserver() override {}

  void WaitForNextSubView() {
    if (did_receive_rect_)
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  const gfx::Rect& view_bounds_in_screen() const { return bounds_; }

 private:
  void DidAddSubviewWillBeDismissed(
      const gfx::Rect& bounds_in_root_view) override {
    did_receive_rect_ = true;
    bounds_ = bounds_in_root_view;
    if (run_loop_)
      run_loop_->Quit();
  }

  bool did_receive_rect_ = false;
  gfx::Rect bounds_;
  std::unique_ptr<base::RunLoop> run_loop_;
};
#endif  // OS_MAC

class WebViewInteractiveTest : public extensions::PlatformAppBrowserTest {
 public:
  WebViewInteractiveTest()
      : guest_web_contents_(nullptr),
        embedder_web_contents_(nullptr),
        corner_(gfx::Point()),
        mouse_click_result_(false),
        first_click_(true) {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
    // Some bots are flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  TestGuestViewManager* GetGuestViewManager() {
    TestGuestViewManager* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated may and will get called
    // before a guest is created.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                  browser()->profile())));
    }
    return manager;
  }

  void MoveMouseInsideWindowWithListener(gfx::Point point,
                                         const std::string& message) {
    ExtensionTestMessageListener move_listener(message, false);
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
        gfx::Point(corner_.x() + point.x(), corner_.y() + point.y())));
    ASSERT_TRUE(move_listener.WaitUntilSatisfied());
  }

  void SendMouseClickWithListener(ui_controls::MouseButton button,
                                  const std::string& message) {
    ExtensionTestMessageListener listener(message, false);
    SendMouseClick(button);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  void SendMouseClick(ui_controls::MouseButton button) {
    SendMouseEvent(button, ui_controls::DOWN);
    SendMouseEvent(button, ui_controls::UP);
  }

  void MoveMouseInsideWindow(const gfx::Point& point) {
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
        gfx::Point(corner_.x() + point.x(), corner_.y() + point.y())));
  }

  gfx::NativeWindow GetPlatformAppWindow() {
    const extensions::AppWindowRegistry::AppWindowList& app_windows =
        extensions::AppWindowRegistry::Get(browser()->profile())->app_windows();
    return (*app_windows.begin())->GetNativeWindow();
  }

  void SendKeyPressToPlatformApp(ui::KeyboardCode key) {
    ASSERT_EQ(1U, GetAppWindowCount());
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), key, false, false, false, false));
  }

  void SendCopyKeyPressToPlatformApp() {
    ASSERT_EQ(1U, GetAppWindowCount());
#if defined(OS_MAC)
    // Send Cmd+C on MacOSX.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_C, false, false, false, true));
#else
    // Send Ctrl+C on Windows and Linux/ChromeOS.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_C, true, false, false, false));
#endif
  }

  void SendStartOfLineKeyPressToPlatformApp() {
#if defined(OS_MAC)
    // Send Cmd+Left on MacOSX.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_LEFT, false, false, false, true));
#else
    // Send Ctrl+Left on Windows and Linux/ChromeOS.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_LEFT, true, false, false, false));
#endif
  }

  void SendBackShortcutToPlatformApp() {
#if defined(OS_MAC)
    // Send Cmd+[ on MacOSX.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_OEM_4, false, false, false, true));
#else
    // Send browser back key on Linux/Windows.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_BROWSER_BACK,
        false, false, false, false));
#endif
  }

  void SendForwardShortcutToPlatformApp() {
#if defined(OS_MAC)
    // Send Cmd+] on MacOSX.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_OEM_6, false, false, false, true));
#else
    // Send browser back key on Linux/Windows.
    ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
        GetPlatformAppWindow(), ui::VKEY_BROWSER_FORWARD,
        false, false, false, false));
#endif
  }

  void SendMouseEvent(ui_controls::MouseButton button,
                      ui_controls::MouseButtonState state) {
    if (first_click_) {
      mouse_click_result_ = ui_test_utils::SendMouseEventsSync(button,
                                                                state);
      first_click_ = false;
    } else {
      ASSERT_EQ(mouse_click_result_, ui_test_utils::SendMouseEventsSync(
          button, state));
    }
  }

  enum TestServer {
    NEEDS_TEST_SERVER,
    NO_TEST_SERVER
  };

  std::unique_ptr<ExtensionTestMessageListener> RunAppHelper(
      const std::string& test_name,
      const std::string& app_location,
      TestServer test_server,
      content::WebContents** embedder_web_contents) {
    // For serving guest pages.
    if ((test_server == NEEDS_TEST_SERVER) && !StartEmbeddedTestServer()) {
      LOG(ERROR) << "FAILED TO START TEST SERVER.";
      return nullptr;
    }

    LoadAndLaunchPlatformApp(app_location.c_str(), "Launched");
    if (!ui_test_utils::ShowAndFocusNativeWindow(GetPlatformAppWindow())) {
      LOG(ERROR) << "UNABLE TO FOCUS TEST WINDOW.";
      return nullptr;
    }

    // Flush any pending events to make sure we start with a clean slate.
    content::RunAllPendingInMessageLoop();

    *embedder_web_contents = GetFirstAppWindowWebContents();

    std::unique_ptr<ExtensionTestMessageListener> done_listener(
        new ExtensionTestMessageListener("TEST_PASSED", false));
    done_listener->set_failure_message("TEST_FAILED");
    if (!content::ExecuteScript(
            *embedder_web_contents,
            base::StringPrintf("runTest('%s')", test_name.c_str()))) {
      LOG(ERROR) << "UNABLE TO START TEST";
      return nullptr;
    }

    return done_listener;
  }

  void TestHelper(const std::string& test_name,
                  const std::string& app_location,
                  TestServer test_server) {
    content::WebContents* embedder_web_contents = NULL;
    std::unique_ptr<ExtensionTestMessageListener> done_listener(RunAppHelper(
        test_name, app_location, test_server, &embedder_web_contents));

    ASSERT_TRUE(done_listener);
    ASSERT_TRUE(done_listener->WaitUntilSatisfied());

    embedder_web_contents_ = embedder_web_contents;
    guest_web_contents_ = GetGuestViewManager()->WaitForSingleGuestCreated();
  }

  void SendMessageToEmbedder(const std::string& message) {
    ASSERT_TRUE(content::ExecuteScript(
        GetFirstAppWindowWebContents(),
        base::StringPrintf("onAppMessage('%s');", message.c_str())));
  }

  void SetupTest(const std::string& app_name,
                 const std::string& guest_url_spec) {
    ASSERT_TRUE(StartEmbeddedTestServer());
    GURL::Replacements replace_host;
    replace_host.SetHostStr("localhost");

    GURL guest_url = embedded_test_server()->GetURL(guest_url_spec);
    guest_url = guest_url.ReplaceComponents(replace_host);

    ui_test_utils::UrlLoadObserver guest_observer(
        guest_url, content::NotificationService::AllSources());

    LoadAndLaunchPlatformApp(app_name.c_str(), "connected");

    guest_observer.Wait();
    content::Source<content::NavigationController> source =
        guest_observer.source();
    EXPECT_TRUE(source->GetWebContents()
                    ->GetMainFrame()
                    ->GetProcess()
                    ->IsForGuestsOnly());

    guest_web_contents_ = source->GetWebContents();
    embedder_web_contents_ =
        GuestViewBase::FromWebContents(guest_web_contents_)->
            embedder_web_contents();

    gfx::Rect offset = embedder_web_contents_->GetContainerBounds();
    corner_ = offset.origin();
  }

  content::WebContents* guest_web_contents() {
    return guest_web_contents_;
  }

  content::WebContents* embedder_web_contents() {
    return embedder_web_contents_;
  }

  gfx::Point corner() {
    return corner_;
  }

  void SimulateRWHMouseClick(content::RenderWidgetHost* rwh,
                             blink::WebMouseEvent::Button button,
                             int x,
                             int y) {
    blink::WebMouseEvent mouse_event(
        blink::WebInputEvent::Type::kMouseDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    mouse_event.button = button;
    mouse_event.SetPositionInWidget(x, y);
    // Needed for the WebViewTest.ContextMenuPositionAfterCSSTransforms
    gfx::Rect rect = rwh->GetView()->GetViewBounds();
    mouse_event.SetPositionInScreen(x + rect.x(), y + rect.y());
    rwh->ForwardMouseEvent(mouse_event);
    mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
    rwh->ForwardMouseEvent(mouse_event);
  }

  class PopupCreatedObserver {
   public:
    PopupCreatedObserver() = default;
    PopupCreatedObserver(const PopupCreatedObserver&) = delete;
    PopupCreatedObserver& operator=(const PopupCreatedObserver&) = delete;

    ~PopupCreatedObserver() = default;

    void Wait(int wait_retry_left = 10) {
      if (wait_retry_left <= 0) {
        LOG(ERROR) << "Wait failed";
        return;
      }
      if (CountWidgets() == initial_widget_count_ + 1 &&
          last_render_widget_host_->GetView()->GetNativeView()) {
        gfx::Rect popup_bounds =
            last_render_widget_host_->GetView()->GetViewBounds();
        if (!popup_bounds.size().IsEmpty()) {
          if (message_loop_.get())
            message_loop_->Quit();
          return;
        }
      }

      // If we haven't seen any new widget or we get 0 size widget, we need to
      // schedule waiting.
      ScheduleWait(wait_retry_left - 1);

      if (!message_loop_.get()) {
        message_loop_ = new content::MessageLoopRunner;
        message_loop_->Run();
      }
    }

    void Init() { initial_widget_count_ = CountWidgets(); }

    // Returns the last widget created.
    content::RenderWidgetHost* last_render_widget_host() {
      return last_render_widget_host_;
    }

   private:
    void ScheduleWait(int wait_retry_left) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&PopupCreatedObserver::Wait, base::Unretained(this),
                         wait_retry_left),
          base::TimeDelta::FromMilliseconds(200));
    }

    size_t CountWidgets() {
      std::unique_ptr<content::RenderWidgetHostIterator> widgets(
          content::RenderWidgetHost::GetRenderWidgetHosts());
      size_t num_widgets = 0;
      while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
        if (content::RenderViewHost::From(widget))
          continue;
        ++num_widgets;
        last_render_widget_host_ = widget;
      }
      return num_widgets;
    }

    size_t initial_widget_count_ = 0;
    content::RenderWidgetHost* last_render_widget_host_ = nullptr;
    scoped_refptr<content::MessageLoopRunner> message_loop_;
  };

  void PopupTestHelper(const gfx::Point& padding) {
    PopupCreatedObserver popup_observer;
    popup_observer.Init();
    // Press alt+DOWN to open popup.
    bool alt = true;
    content::SimulateKeyPress(guest_web_contents(), ui::DomKey::ARROW_DOWN,
                              ui::DomCode::ARROW_DOWN, ui::VKEY_DOWN, false,
                              false, alt, false);
    popup_observer.Wait();

    content::RenderWidgetHost* popup_rwh =
        popup_observer.last_render_widget_host();
    gfx::Rect popup_bounds = popup_rwh->GetView()->GetViewBounds();

    content::RenderViewHost* embedder_rvh =
        GetFirstAppWindowWebContents()->GetMainFrame()->GetRenderViewHost();
    gfx::Rect embedder_bounds =
        embedder_rvh->GetWidget()->GetView()->GetViewBounds();
    gfx::Vector2d diff = popup_bounds.origin() - embedder_bounds.origin();
    LOG(INFO) << "DIFF: x = " << diff.x() << ", y = " << diff.y();

    const int left_spacing = 40 + padding.x();  // div.style.paddingLeft = 40px.
    // div.style.paddingTop = 60px + (input box height = 26px).
    const int top_spacing = 60 + 26 + padding.y();

    // If the popup is placed within |threshold_px| of the expected position,
    // then we consider the test as a pass.
    const int threshold_px = 10;

    EXPECT_LE(std::abs(diff.x() - left_spacing), threshold_px);
    EXPECT_LE(std::abs(diff.y() - top_spacing), threshold_px);

    // Close the popup.
    content::SimulateKeyPress(guest_web_contents(), ui::DomKey::ESCAPE,
                              ui::DomCode::ESCAPE, ui::VKEY_ESCAPE, false,
                              false, false, false);
  }

  void FullscreenTestHelper(const std::string& test_name,
                            const std::string& test_dir) {
    TestHelper(test_name, test_dir, NO_TEST_SERVER);
    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    ASSERT_TRUE(embedder_web_contents);
    ASSERT_TRUE(guest_web_contents());
    // Click the guest to request fullscreen.
    ExtensionTestMessageListener passed_listener(
        "FULLSCREEN_STEP_PASSED", false);
    passed_listener.set_failure_message("TEST_FAILED");
    content::SimulateMouseClickAt(guest_web_contents(), 0,
                                  blink::WebMouseEvent::Button::kLeft,
                                  gfx::Point(20, 20));
    ASSERT_TRUE(passed_listener.WaitUntilSatisfied());
  }

 protected:
  TestGuestViewManagerFactory factory_;
  content::WebContents* guest_web_contents_;
  content::WebContents* embedder_web_contents_;
  gfx::Point corner_;
  bool mouse_click_result_;
  bool first_click_;
};

class WebViewImeInteractiveTest : public WebViewInteractiveTest {
 protected:
  // This class observes all the composition range updates associated with the
  // TextInputManager of the provided WebContents. The WebContents should be an
  // outer most WebContents.
  class CompositionRangeUpdateObserver {
   public:
    explicit CompositionRangeUpdateObserver(content::WebContents* web_contents)
        : tester_(web_contents) {
      tester_.SetOnImeCompositionRangeChangedCallback(base::BindRepeating(
          &CompositionRangeUpdateObserver::OnCompositionRangeUpdated,
          base::Unretained(this)));
    }
    CompositionRangeUpdateObserver(const CompositionRangeUpdateObserver&) =
        delete;
    CompositionRangeUpdateObserver& operator=(
        const CompositionRangeUpdateObserver&) = delete;
    ~CompositionRangeUpdateObserver() = default;

    // Wait until a composition range update with a range length equal to
    // |length| is received.
    void WaitForCompositionRangeLength(uint32_t length) {
      if (last_composition_range_length_.has_value() &&
          last_composition_range_length_.value() == length)
        return;
      expected_length_ = length;
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }

   private:
    void OnCompositionRangeUpdated() {
      uint32_t length = std::numeric_limits<uint32_t>::max();
      if (tester_.GetLastCompositionRangeLength(&length)) {
        last_composition_range_length_ = length;
      }
      if (last_composition_range_length_.value() == expected_length_)
        run_loop_->Quit();
    }

    content::TextInputManagerTester tester_;
    std::unique_ptr<base::RunLoop> run_loop_;
    base::Optional<uint32_t> last_composition_range_length_;
    uint32_t expected_length_ = 0;
  };
};

class WebViewNewWindowInteractiveTest : public WebViewInteractiveTest {};
class WebViewFocusInteractiveTest : public WebViewInteractiveTest {};
class WebViewPointerLockInteractiveTest : public WebViewInteractiveTest {};

// The following class of tests do not work for OOPIF <webview>.
// TODO(ekaramad): Make this tests work with OOPIF and replace the test classes
// with WebViewInteractiveTest (see crbug.com/582562).
class DISABLED_WebViewPopupInteractiveTest : public WebViewInteractiveTest {};

// ui_test_utils::SendMouseMoveSync doesn't seem to work on OS_MAC, and
// likely won't work on many other platforms as well, so for now this test
// is for Windows and Linux only. As of Sept 17th, 2013 this test is disabled
// on Windows due to flakines, see http://crbug.com/293445.

// Disabled on Linux Aura because pointer lock does not work on Linux Aura.
// crbug.com/341876

// Timeouts flakily: crbug.com/1003345
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(WebViewPointerLockInteractiveTest,
                       DISABLED_PointerLock) {
  SetupTest("web_view/pointer_lock",
            "/extensions/platform_apps/web_view/pointer_lock/guest.html");

  // Move the mouse over the Lock Pointer button.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      gfx::Point(corner().x() + 75, corner().y() + 25)));

  // Click the Lock Pointer button. The first two times the button is clicked
  // the permission API will deny the request (intentional).
  ExtensionTestMessageListener exception_listener("request exception", false);
  SendMouseClickWithListener(ui_controls::LEFT, "lock error");
  ASSERT_TRUE(exception_listener.WaitUntilSatisfied());
  SendMouseClickWithListener(ui_controls::LEFT, "lock error");

  // Click the Lock Pointer button, locking the mouse to lockTarget1.
  SendMouseClickWithListener(ui_controls::LEFT, "locked");

  // Attempt to move the mouse off of the lock target, and onto lockTarget2,
  // (which would trigger a test failure).
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      gfx::Point(corner().x() + 74, corner().y() + 74)));
  MoveMouseInsideWindowWithListener(gfx::Point(75, 75), "mouse-move");

#if defined(OS_WIN)
  // When the mouse is unlocked on win aura, sending a test mouse click clicks
  // where the mouse moved to while locked. I was unable to figure out why, and
  // since the issue only occurs with the test mouse events, just fix it with
  // a simple workaround - moving the mouse back to where it should be.
  // TODO(mthiesse): Fix Win Aura simulated mouse events while mouse locked.
  MoveMouseInsideWindowWithListener(gfx::Point(75, 25), "mouse-move");
#endif

  ExtensionTestMessageListener unlocked_listener("unlocked", false);
  // Send a key press to unlock the mouse.
  SendKeyPressToPlatformApp(ui::VKEY_ESCAPE);

  // Wait for page to receive (successful) mouse unlock response.
  ASSERT_TRUE(unlocked_listener.WaitUntilSatisfied());

  // After the second lock, guest.js sends a message to main.js to remove the
  // webview object. main.js then removes the div containing the webview, which
  // should unlock, and leave the mouse over the mousemove-capture-container
  // div. We then move the mouse over that div to ensure the mouse was properly
  // unlocked and that the div receieves the message.
  ExtensionTestMessageListener move_captured_listener("move-captured", false);
  move_captured_listener.set_failure_message("timeout");

  // Mouse should already be over lock button (since we just unlocked), so send
  // click to re-lock the mouse.
  SendMouseClickWithListener(ui_controls::LEFT, "deleted");

  // A mousemove event is triggered on the mousemove-capture-container element
  // when we delete the webview container (since the mouse moves onto the
  // element), but just in case, send an explicit mouse movement to be safe.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      gfx::Point(corner().x() + 50, corner().y() + 10)));

  // Wait for page to receive second (successful) mouselock response.
  bool success = move_captured_listener.WaitUntilSatisfied();
  if (!success) {
    fprintf(stderr, "TIMEOUT - retrying\n");
    // About 1 in 40 tests fail to detect mouse moves at this point (why?).
    // Sending a right click seems to fix this (why?).
    ExtensionTestMessageListener move_listener2("move-captured", false);
    SendMouseClick(ui_controls::RIGHT);
    ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
        gfx::Point(corner().x() + 51, corner().y() + 11)));
    ASSERT_TRUE(move_listener2.WaitUntilSatisfied());
  }
}

// flaky http://crbug.com/412086
IN_PROC_BROWSER_TEST_F(WebViewPointerLockInteractiveTest,
                       DISABLED_PointerLockFocus) {
  SetupTest("web_view/pointer_lock_focus",
            "/extensions/platform_apps/web_view/pointer_lock_focus/guest.html");

  // Move the mouse over the Lock Pointer button.
  ASSERT_TRUE(ui_test_utils::SendMouseMoveSync(
      gfx::Point(corner().x() + 75, corner().y() + 25)));

  // Click the Lock Pointer button, locking the mouse to lockTarget.
  // This will also change focus to another element
  SendMouseClickWithListener(ui_controls::LEFT, "locked");

  // Try to unlock the mouse now that the focus is outside of the BrowserPlugin
  ExtensionTestMessageListener unlocked_listener("unlocked", false);
  // Send a key press to unlock the mouse.
  SendKeyPressToPlatformApp(ui::VKEY_ESCAPE);

  // Wait for page to receive (successful) mouse unlock response.
  ASSERT_TRUE(unlocked_listener.WaitUntilSatisfied());
}

#endif  // defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests that if a <webview> is focused before navigation then the guest starts
// off focused.
// Flaky. https://crbug.com/1013552
IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest,
                       DISABLED_Focus_FocusBeforeNavigation) {
  TestHelper("testFocusBeforeNavigation", "web_view/focus", NO_TEST_SERVER);
}

// Tests that setting focus on the <webview> sets focus on the guest.
IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, Focus_FocusEvent) {
  TestHelper("testFocusEvent", "web_view/focus", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, Focus_FocusTakeFocus) {
  TestHelper("testFocusTakeFocus", "web_view/focus", NO_TEST_SERVER);

  // Compute where to click in the window to focus the guest input box.
  int clickX, clickY;
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      embedder_web_contents(),
      "domAutomationController.send(Math.floor(window.clickX));", &clickX));
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
      embedder_web_contents(),
      "domAutomationController.send(Math.floor(window.clickY));", &clickY));

  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");
  content::SimulateMouseClickAt(guest_web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(clickX, clickY));
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  // TAB back out to the embedder's input.
  next_step_listener.Reset();
  content::SimulateKeyPress(embedder_web_contents(), ui::DomKey::TAB,
                            ui::DomCode::TAB, ui::VKEY_TAB, false, false, false,
                            false);
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
}

// Flaky on Mac and Linux - https://crbug.com/707648
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_Focus_FocusTracksEmbedder DISABLED_Focus_FocusTracksEmbedder
#else
#define MAYBE_Focus_FocusTracksEmbedder Focus_FocusTracksEmbedder
#endif

IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest,
                       MAYBE_Focus_FocusTracksEmbedder) {
  content::WebContents* embedder_web_contents = NULL;

  std::unique_ptr<ExtensionTestMessageListener> done_listener(
      RunAppHelper("testFocusTracksEmbedder", "web_view/focus", NO_TEST_SERVER,
                   &embedder_web_contents));
  EXPECT_TRUE(done_listener->WaitUntilSatisfied());

  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");
  EXPECT_TRUE(content::ExecuteScript(
                  embedder_web_contents,
                  "window.runCommand('testFocusTracksEmbedderRunNextStep');"));

  // Blur the embedder.
  embedder_web_contents->GetMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->Blur();
  // Ensure that the guest is also blurred.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, Focus_AdvanceFocus) {
  content::WebContents* embedder_web_contents = NULL;

  {
    std::unique_ptr<ExtensionTestMessageListener> done_listener(
        RunAppHelper("testAdvanceFocus", "web_view/focus", NO_TEST_SERVER,
                     &embedder_web_contents));
    EXPECT_TRUE(done_listener->WaitUntilSatisfied());
  }

  {
    ExtensionTestMessageListener listener("button1-focused", false);
    listener.set_failure_message("TEST_FAILED");

    // In oopif-webview, the click it directly routed to the guest.
    content::WebContents* guest =
        GetGuestViewManager()->WaitForSingleGuestCreated();

    SimulateRWHMouseClick(
        guest->GetMainFrame()->GetRenderViewHost()->GetWidget(),
        blink::WebMouseEvent::Button::kLeft, 200, 20);
    content::SimulateKeyPress(embedder_web_contents, ui::DomKey::TAB,
                              ui::DomCode::TAB, ui::VKEY_TAB, false, false,
                              false, false);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  {
    // Wait for button1 to be focused again, this means we were asked to
    // move the focus to the next focusable element.
    ExtensionTestMessageListener listener("button1-advance-focus", false);
    listener.set_failure_message("TEST_FAILED");
    content::SimulateKeyPress(embedder_web_contents, ui::DomKey::TAB,
                              ui::DomCode::TAB, ui::VKEY_TAB, false, false,
                              false, false);
    content::SimulateKeyPress(embedder_web_contents, ui::DomKey::TAB,
                              ui::DomCode::TAB, ui::VKEY_TAB, false, false,
                              false, false);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }
}

// Tests that blurring <webview> also blurs the guest.
IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, Focus_BlurEvent) {
  TestHelper("testBlurEvent", "web_view/focus", NO_TEST_SERVER);
}

// Tests that a <webview> can't steal focus from the embedder.
IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest,
                       FrameInGuestWontStealFocus) {
  LoadAndLaunchPlatformApp("web_view/simple", "WebViewTest.LAUNCHED");

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  content::WebContents* guest_web_contents =
      GetGuestViewManager()->WaitForSingleGuestCreated();

  content::MainThreadFrameObserver embedder_observer(
      embedder_web_contents->GetMainFrame()->GetView()->GetRenderWidgetHost());
  content::MainThreadFrameObserver guest_observer(
      guest_web_contents->GetMainFrame()->GetView()->GetRenderWidgetHost());

  // Embedder should be focused initially.
  EXPECT_EQ(content::GetFocusedWebContents(guest_web_contents),
            embedder_web_contents);

  // Try to focus an iframe in the guest.
  EXPECT_TRUE(content::ExecuteScript(
      guest_web_contents,
      "document.body.appendChild(document.createElement('iframe')); "
      "document.querySelector('iframe').focus()"));
  embedder_observer.Wait();
  guest_observer.Wait();

  // Embedder should still be focused.
  EXPECT_EQ(content::GetFocusedWebContents(guest_web_contents),
            embedder_web_contents);

  // Try to focus the guest from the embedder.
  EXPECT_TRUE(content::ExecuteScript(
      embedder_web_contents, "document.querySelector('webview').focus()"));
  embedder_observer.Wait();
  guest_observer.Wait();

  // Guest should be focused.
  EXPECT_EQ(content::GetFocusedWebContents(guest_web_contents),
            guest_web_contents);

  // Try to focus an iframe in the embedder.
  EXPECT_TRUE(content::ExecuteScript(
      embedder_web_contents,
      "document.body.appendChild(document.createElement('iframe')); "
      "document.querySelector('iframe').focus()"));
  embedder_observer.Wait();
  guest_observer.Wait();

  // Embedder is allowed to steal focus from guest.
  EXPECT_EQ(content::GetFocusedWebContents(guest_web_contents),
            embedder_web_contents);
}

// Tests that guests receive edit commands and respond appropriately.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, EditCommands) {
  LoadAndLaunchPlatformApp("web_view/edit_commands", "connected");

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetPlatformAppWindow()));

  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();

  ExtensionTestMessageListener copy_listener("copy", false);
  SendCopyKeyPressToPlatformApp();

  // Wait for the guest to receive a 'copy' edit command.
  ASSERT_TRUE(copy_listener.WaitUntilSatisfied());
}

// Tests that guests receive edit commands and respond appropriately.
// Flaky test - crbug.com/859478
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, DISABLED_EditCommandsNoMenu) {
  SetupTest("web_view/edit_commands_no_menu",
      "/extensions/platform_apps/web_view/edit_commands_no_menu/"
      "guest.html");

  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetPlatformAppWindow()));

  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();

  ExtensionTestMessageListener start_of_line_listener("StartOfLine", false);
  SendStartOfLineKeyPressToPlatformApp();
#if defined(OS_MAC)
  // On macOS, sending an accelerator [key-down] will also cause the subsequent
  // key-up to be swallowed. The implementation of guest.html is waiting for a
  // key-up to send the caret-position message. So we send a key-down/key-up of
  // a character that otherwise has no effect.
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_UP, false, false, false, false));
#endif
  // Wait for the guest to receive a 'copy' edit command.
  ASSERT_TRUE(start_of_line_listener.WaitUntilSatisfied());
}

// There is a problem of missing keyup events with the command key after
// the NSEvent is sent to NSApplication in ui/base/test/ui_controls_mac.mm .
// This test is disabled on only the Mac until the problem is resolved.
// See http://crbug.com/425859 for more information.
#if defined(OS_MAC)
#define MAYBE_NewWindow_OpenInNewTab DISABLED_NewWindow_OpenInNewTab
#else
#define MAYBE_NewWindow_OpenInNewTab NewWindow_OpenInNewTab
#endif
// Tests that Ctrl+Click/Cmd+Click on a link fires up the newwindow API.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, MAYBE_NewWindow_OpenInNewTab) {
  content::WebContents* embedder_web_contents = nullptr;

  ExtensionTestMessageListener loaded_listener("Loaded", false);
  std::unique_ptr<ExtensionTestMessageListener> done_listener(
      RunAppHelper("testNewWindowOpenInNewTab", "web_view/newwindow",
                   NEEDS_TEST_SERVER, &embedder_web_contents));

  EXPECT_TRUE(loaded_listener.WaitUntilSatisfied());
#if defined(OS_MAC)
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_RETURN,
      false, false, false, true /* cmd */));
#else
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_RETURN,
      true /* ctrl */, false, false, false));
#endif

  // Wait for the embedder to receive a 'newwindow' event.
  ASSERT_TRUE(done_listener->WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(DISABLED_WebViewPopupInteractiveTest,
                       PopupPositioningBasic) {
  TestHelper("testBasic", "web_view/popup_positioning", NO_TEST_SERVER);
  ASSERT_TRUE(guest_web_contents());
  PopupTestHelper(gfx::Point());

  // TODO(lazyboy): Move the embedder window to a random location and
  // make sure we keep rendering popups correct in webview.
}

// Flaky on ChromeOS and Linux: http://crbug.com/526886
// TODO(crbug.com/807446): Flaky on Mac.
// TODO(crbug.com/809383): Flaky on Windows.
// Tests that moving browser plugin (without resize/UpdateRects) correctly
// repositions popup.
IN_PROC_BROWSER_TEST_F(DISABLED_WebViewPopupInteractiveTest,
                       PopupPositioningMoved) {
  TestHelper("testMoved", "web_view/popup_positioning", NO_TEST_SERVER);
  ASSERT_TRUE(guest_web_contents());
  PopupTestHelper(gfx::Point(20, 0));
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Navigation) {
  TestHelper("testNavigation", "web_view/navigation", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, Navigation_BackForwardKeys) {
  LoadAndLaunchPlatformApp("web_view/navigation", "Launched");
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetPlatformAppWindow()));
  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);

  ExtensionTestMessageListener done_listener(
      "TEST_PASSED", false);
  done_listener.set_failure_message("TEST_FAILED");
  ExtensionTestMessageListener ready_back_key_listener(
      "ReadyForBackKey", false);
  ExtensionTestMessageListener ready_forward_key_listener(
      "ReadyForForwardKey", false);

  EXPECT_TRUE(content::ExecuteScript(
                  embedder_web_contents,
                  "runTest('testBackForwardKeys')"));

  ASSERT_TRUE(ready_back_key_listener.WaitUntilSatisfied());
  SendBackShortcutToPlatformApp();

  ASSERT_TRUE(ready_forward_key_listener.WaitUntilSatisfied());
  SendForwardShortcutToPlatformApp();

  ASSERT_TRUE(done_listener.WaitUntilSatisfied());
}

// Trips over a DCHECK in content::MouseLockDispatcher::OnLockMouseACK; see
// https://crbug.com/761783.
#if defined(OS_WIN)
#define MAYBE_PointerLock_PointerLockLostWithFocus \
  PointerLock_PointerLockLostWithFocus
#else
#define MAYBE_PointerLock_PointerLockLostWithFocus \
  DISABLED_PointerLock_PointerLockLostWithFocus
#endif
IN_PROC_BROWSER_TEST_F(WebViewPointerLockInteractiveTest,
                       MAYBE_PointerLock_PointerLockLostWithFocus) {
  TestHelper("testPointerLockLostWithFocus",
             "web_view/pointerlock",
             NO_TEST_SERVER);
}

// Disable this on mac, throws an assertion failure on teardown which
// will result in flakiness:
//
// "not is fullscreen state"
// "*** Assertion failure in -[_NSWindowFullScreenTransition
//     transitionedWindowFrame],"
// See similar bug: http://crbug.com/169820.
//
// In addition to the above, these tests are flaky on some platforms:
// http://crbug.com/468660
#if defined(OS_WIN)
#define MAYBE_FullscreenAllow_EmbedderHasPermission \
  FullscreenAllow_EmbedderHasPermission
#else
#define MAYBE_FullscreenAllow_EmbedderHasPermission \
  DISABLED_FullscreenAllow_EmbedderHasPermission
#endif
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       MAYBE_FullscreenAllow_EmbedderHasPermission) {
  FullscreenTestHelper("testFullscreenAllow",
                       "web_view/fullscreen/embedder_has_permission");
}

#if defined(OS_WIN)
#define MAYBE_FullscreenDeny_EmbedderHasPermission \
  FullscreenDeny_EmbedderHasPermission
#else
#define MAYBE_FullscreenDeny_EmbedderHasPermission \
  DISABLED_FullscreenDeny_EmbedderHasPermission
#endif
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       MAYBE_FullscreenDeny_EmbedderHasPermission) {
  FullscreenTestHelper("testFullscreenDeny",
                       "web_view/fullscreen/embedder_has_permission");
}

#if defined(OS_WIN)
#define MAYBE_FullscreenAllow_EmbedderHasNoPermission \
  FullscreenAllow_EmbedderHasNoPermission
#else
#define MAYBE_FullscreenAllow_EmbedderHasNoPermission \
  DISABLED_FullscreenAllow_EmbedderHasNoPermission
#endif
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       MAYBE_FullscreenAllow_EmbedderHasNoPermission) {
  FullscreenTestHelper("testFullscreenAllow",
                       "web_view/fullscreen/embedder_has_no_permission");
}

#if defined(OS_WIN)
#define MAYBE_FullscreenDeny_EmbedderHasNoPermission \
  FullscreenDeny_EmbedderHasNoPermission
#else
#define MAYBE_FullscreenDeny_EmbedderHasNoPermission \
  DISABLED_FullscreenDeny_EmbedderHasNoPermission
#endif
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest,
                       MAYBE_FullscreenDeny_EmbedderHasNoPermission) {
  FullscreenTestHelper("testFullscreenDeny",
                       "web_view/fullscreen/embedder_has_no_permission");
}

// This test exercies the following scenario:
// 1. An <input> in guest has focus.
// 2. User takes focus to embedder by clicking e.g. an <input> in embedder.
// 3. User brings back the focus directly to the <input> in #1.
//
// Now we need to make sure TextInputTypeChanged fires properly for the guest's
// view upon step #3. We simply read the input type's state after #3 to
// make sure it's not TEXT_INPUT_TYPE_NONE.
IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, Focus_FocusRestored) {
  TestHelper("testFocusRestored", "web_view/focus", NO_TEST_SERVER);
  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);
  ASSERT_TRUE(guest_web_contents());

  // 1) We click on the guest so that we get a focus event.
  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");
  {
    content::SimulateMouseClickAt(guest_web_contents(), 0,
                                  blink::WebMouseEvent::Button::kLeft,
                                  gfx::Point(10, 10));
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testFocusRestoredRunNextStep', 1);"));
  }
  // Wait for the next step to complete.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  // 2) We click on the embedder so the guest's focus goes away and it observes
  // a blur event.
  next_step_listener.Reset();
  {
    content::SimulateMouseClickAt(embedder_web_contents, 0,
                                  blink::WebMouseEvent::Button::kLeft,
                                  gfx::Point(200, 20));
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testFocusRestoredRunNextStep', 2);"));
  }
  // Wait for the next step to complete.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  // 3) We click on the guest again to bring back focus directly to the previous
  // input element, then we ensure text_input_type is properly set.
  next_step_listener.Reset();
  {
    content::SimulateMouseClickAt(guest_web_contents(), 0,
                                  blink::WebMouseEvent::Button::kLeft,
                                  gfx::Point(10, 10));
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testFocusRestoredRunNextStep', 3)"));
  }
  // Wait for the next step to complete.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  // |text_input_client| is not available for mac and android.
#if !defined(OS_MAC) && !defined(OS_ANDROID)
  ui::TextInputClient* text_input_client = embedder_web_contents->GetMainFrame()
                                               ->GetRenderViewHost()
                                               ->GetWidget()
                                               ->GetView()
                                               ->GetTextInputClient();
  ASSERT_TRUE(text_input_client);
  ASSERT_TRUE(text_input_client->GetTextInputType() !=
              ui::TEXT_INPUT_TYPE_NONE);
#endif
}

// ui::TextInputClient is NULL for mac and android.
#if !defined(OS_MAC) && !defined(OS_ANDROID)
#if defined(ADDRESS_SANITIZER) || defined(OS_WIN)
#define MAYBE_Focus_InputMethod DISABLED_Focus_InputMethod
#else
#define MAYBE_Focus_InputMethod Focus_InputMethod
#endif
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, MAYBE_Focus_InputMethod) {
  content::WebContents* embedder_web_contents = NULL;
  std::unique_ptr<ExtensionTestMessageListener> done_listener(
      RunAppHelper("testInputMethod", "web_view/focus", NO_TEST_SERVER,
                   &embedder_web_contents));
  ASSERT_TRUE(done_listener->WaitUntilSatisfied());

  ui::TextInputClient* text_input_client = embedder_web_contents->GetMainFrame()
                                               ->GetRenderViewHost()
                                               ->GetWidget()
                                               ->GetView()
                                               ->GetTextInputClient();
  ASSERT_TRUE(text_input_client);

  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");

  // An input element inside the <webview> gets focus and is given some
  // user input via IME.
  {
    ui::CompositionText composition;
    composition.text = u"InputTest123";
    text_input_client->SetCompositionText(composition);
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testInputMethodRunNextStep', 1);"));

    // Wait for the next step to complete.
    ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
  }

  // A composition is committed via IME.
  {
    next_step_listener.Reset();

    ui::CompositionText composition;
    composition.text = u"InputTest456";
    text_input_client->SetCompositionText(composition);
    text_input_client->ConfirmCompositionText(/* keep_selection */ false);
    EXPECT_TRUE(content::ExecuteScript(
                  embedder_web_contents,
                  "window.runCommand('testInputMethodRunNextStep', 2);"));

    // Wait for the next step to complete.
    ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
  }

  // TODO(lazyboy): http://crbug.com/457007, Add a step or a separate test to
  // check the following, currently it turns this test to be flaky:
  // If we move the focus from the first <input> to the second one after we
  // have some composition text set but *not* committed (by calling
  // text_input_client->SetCompositionText()), then it would cause IME cancel
  // and the onging composition is committed in the first <input> in the
  // <webview>, not the second one.

  // Tests ExtendSelectionAndDelete message works in <webview>.
  // https://crbug.com/971985
  {
    next_step_listener.Reset();

    // At this point we have set focus on first <input> in the <webview>,
    // and the value it contains is 'InputTest456' with caret set after 'T'.
    // Now we delete 'Test' in 'InputTest456', as the caret is after 'T':
    // delete before 1 character ('T') and after 3 characters ('est').
    text_input_client->ExtendSelectionAndDelete(1, 3);
    EXPECT_TRUE(content::ExecuteScript(
                    embedder_web_contents,
                    "window.runCommand('testInputMethodRunNextStep', 3);"));

    // Wait for the next step to complete.
    ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
  }
}
#endif

#if defined(OS_LINUX) || \
    defined(OS_CHROMEOS)  // TODO(https://crbug.com/801552): Flaky.
#define MAYBE_LongPressSelection DISABLED_LongPressSelection
#else
#define MAYBE_LongPressSelection LongPressSelection
#endif
#if !defined(OS_MAC)
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, MAYBE_LongPressSelection) {
  SetupTest("web_view/text_selection",
            "/extensions/platform_apps/web_view/text_selection/guest.html");
  ASSERT_TRUE(guest_web_contents());
  ASSERT_TRUE(embedder_web_contents());
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(GetPlatformAppWindow()));

  blink::WebInputEvent::Type context_menu_gesture_event_type =
      blink::WebInputEvent::Type::kGestureLongPress;
#if defined(OS_WIN)
  context_menu_gesture_event_type = blink::WebInputEvent::Type::kGestureLongTap;
#endif
  auto filter = std::make_unique<content::InputMsgWatcher>(
      guest_web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost(),
      context_menu_gesture_event_type);

  // Wait for guest to load (without this the events never reach the guest).
  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, message_loop_runner->QuitClosure(),
      base::TimeDelta::FromMilliseconds(200));
  message_loop_runner->Run();

  gfx::Rect guest_rect = guest_web_contents()->GetContainerBounds();
  gfx::Point embedder_origin =
      embedder_web_contents()->GetContainerBounds().origin();
  guest_rect.Offset(-embedder_origin.x(), -embedder_origin.y());

  // Mouse click is necessary for focus.
  content::SimulateMouseClickAt(embedder_web_contents(), 0,
                                blink::WebMouseEvent::Button::kLeft,
                                guest_rect.CenterPoint());

  content::SimulateLongTapAt(embedder_web_contents(), guest_rect.CenterPoint());
  EXPECT_EQ(blink::mojom::InputEventResultState::kConsumed,
            filter->GetAckStateWaitIfNecessary());

  // Give enough time for the quick menu to fire.
  message_loop_runner = new content::MessageLoopRunner;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, message_loop_runner->QuitClosure(),
      base::TimeDelta::FromMilliseconds(200));
  message_loop_runner->Run();

// TODO: Fix quick menu opening on Windows.
#if !defined(OS_WIN)
  EXPECT_TRUE(ui::TouchSelectionMenuRunner::GetInstance()->IsRunning());
#endif

  EXPECT_FALSE(guest_web_contents()->IsShowingContextMenu());
}
#endif

#if defined(OS_MAC)
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, TextSelection) {
  SetupTest("web_view/text_selection",
            "/extensions/platform_apps/web_view/text_selection/guest.html");
  ASSERT_TRUE(guest_web_contents());
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(
      GetPlatformAppWindow()));

  // Wait until guest sees a context menu.
  ExtensionTestMessageListener ctx_listener("MSG_CONTEXTMENU", false);
  ContextMenuWaiter menu_observer;
  SimulateRWHMouseClick(guest_web_contents()->GetRenderViewHost()->GetWidget(),
                        blink::WebMouseEvent::Button::kRight, 20, 20);
  menu_observer.WaitForMenuOpenAndClose();
  ASSERT_TRUE(ctx_listener.WaitUntilSatisfied());

  // Now verify that the selection text propagates properly to RWHV.
  content::RenderWidgetHostView* guest_rwhv =
      guest_web_contents()->GetRenderWidgetHostView();
  ASSERT_TRUE(guest_rwhv);
  std::string selected_text = base::UTF16ToUTF8(guest_rwhv->GetSelectedText());
  ASSERT_GE(selected_text.size(), 10u);
  ASSERT_EQ("AAAAAAAAAA", selected_text.substr(0, 10));
}

// Verifies that asking for a word lookup from a guest will lead to a returned
// mojo callback from the renderer containing the right selected word.
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, WordLookup) {
  SetupTest("web_view/text_selection",
            "/extensions/platform_apps/web_view/text_selection/guest.html");
  ASSERT_TRUE(guest_web_contents());
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(GetPlatformAppWindow()));

  content::TextInputTestLocalFrame text_input_local_frame;
  text_input_local_frame.SetUp(guest_web_contents()->GetMainFrame());

  // Lookup some string through context menu.
  ContextMenuNotificationObserver menu_observer(IDC_CONTENT_CONTEXT_LOOK_UP);
  // Simulating a mouse click at a position to highlight text in guest and
  // showing the context menu.
  SimulateRWHMouseClick(guest_web_contents()->GetRenderViewHost()->GetWidget(),
                        blink::WebMouseEvent::Button::kRight, 20, 20);
  // Wait for the response form the guest renderer.
  text_input_local_frame.WaitForGetStringForRange();

  // Sanity check.
  ASSERT_EQ("AAAA", text_input_local_frame.GetStringFromRange().substr(0, 4));
}
#endif

// Flaky on Mac: http://crbug.com/811893
// Flaky on Linux/ChromeOS/Windows: http://crbug.com/845638
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_WIN)
#define MAYBE_FocusAndVisibility DISABLED_FocusAndVisibility
#else
#define MAYBE_FocusAndVisibility FocusAndVisibility
#endif

IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest, MAYBE_FocusAndVisibility) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  LoadAndLaunchPlatformApp("web_view/focus_visibility",
                           "WebViewInteractiveTest.LOADED");
  ExtensionTestMessageListener test_init_listener(
      "WebViewInteractiveTest.WebViewInitialized", false);
  SendMessageToEmbedder("init-oopif");
  EXPECT_TRUE(test_init_listener.WaitUntilSatisfied());

  // Send several tab-keys. The button inside webview should receive focus at
  // least once.
  ExtensionTestMessageListener key_processed_listener(
      "WebViewInteractiveTest.KeyUp", false);
#if defined(OS_MAC)
  // On mac, the event listener seems one key event behind and deadlocks. Send
  // an extra tab to get things unblocked. See http://crbug.com/685281 when
  // fixed, this can be removed.
  SendKeyPressToPlatformApp(ui::VKEY_TAB);
#endif
  for (size_t i = 0; i < 4; ++i) {
    key_processed_listener.Reset();
    SendKeyPressToPlatformApp(ui::VKEY_TAB);
    EXPECT_TRUE(key_processed_listener.WaitUntilSatisfied());
  }

  // Verify that the button in the guest receives focus.
  ExtensionTestMessageListener webview_button_focused_listener(
      "WebViewInteractiveTest.WebViewButtonWasFocused", false);
  webview_button_focused_listener.set_failure_message(
      "WebViewInteractiveTest.WebViewButtonWasNotFocused");
  SendMessageToEmbedder("verify");
  EXPECT_TRUE(webview_button_focused_listener.WaitUntilSatisfied());

  // Reset the test and now make the <webview> invisible.
  ExtensionTestMessageListener reset_listener("WebViewInteractiveTest.DidReset",
                                              false);
  SendMessageToEmbedder("reset");
  EXPECT_TRUE(reset_listener.WaitUntilSatisfied());
  ExtensionTestMessageListener did_hide_webview_listener(
      "WebViewInteractiveTest.DidHideWebView", false);
  SendMessageToEmbedder("hide-webview");
  EXPECT_TRUE(did_hide_webview_listener.WaitUntilSatisfied());

  // Send the same number of keys and verify that the webview button was not
  // this time.
  for (size_t i = 0; i < 4; ++i) {
    key_processed_listener.Reset();
    SendKeyPressToPlatformApp(ui::VKEY_TAB);
    EXPECT_TRUE(key_processed_listener.WaitUntilSatisfied());
  }
  ExtensionTestMessageListener webview_button_not_focused_listener(
      "WebViewInteractiveTest.WebViewButtonWasNotFocused", false);
  webview_button_not_focused_listener.set_failure_message(
      "WebViewInteractiveTest.WebViewButtonWasFocused");
  SendMessageToEmbedder("verify");
  EXPECT_TRUE(webview_button_not_focused_listener.WaitUntilSatisfied());
}

// Flaky on MacOSX, crbug.com/817066.
// Flaky timeouts on Linux. https://crbug.com/709202
// Flaky timeouts on Win. https://crbug.com/846695
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN)
#define MAYBE_KeyboardFocusSimple DISABLED_KeyboardFocusSimple
#else
#define MAYBE_KeyboardFocusSimple KeyboardFocusSimple
#endif
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, MAYBE_KeyboardFocusSimple) {
  TestHelper("testKeyboardFocusSimple", "web_view/focus", NO_TEST_SERVER);

  EXPECT_EQ(embedder_web_contents()->GetFocusedFrame(),
            embedder_web_contents()->GetMainFrame());
  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");
  {
    gfx::Rect offset = embedder_web_contents()->GetContainerBounds();
    // Click the <input> element inside the <webview>.
    // If we wanted, we could ask the embedder to compute an appropriate point.
    MoveMouseInsideWindow(gfx::Point(offset.x() + 40, offset.y() + 40));
    SendMouseClick(ui_controls::LEFT);
  }

  // Waits for the renderer to know the input has focus.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_A, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_B, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_C, false, false, false, false));

  next_step_listener.Reset();
  EXPECT_TRUE(content::ExecuteScript(
      embedder_web_contents(),
      "window.runCommand('testKeyboardFocusRunNextStep', 'aBc');"));

  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
}

// Ensures that input is routed to the webview after the containing window loses
// and regains focus. Additionally, the webview does not process keypresses sent
// while another window is focused.
// http://crbug.com/660044.
// Flaky on MacOSX, crbug.com/817067.
// Flaky on linux, crbug.com/706830.
// Flaky on Windows, crbug.com/847201.
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN)
#define MAYBE_KeyboardFocusWindowCycle DISABLED_KeyboardFocusWindowCycle
#else
#define MAYBE_KeyboardFocusWindowCycle KeyboardFocusWindowCycle
#endif
IN_PROC_BROWSER_TEST_F(WebViewInteractiveTest, MAYBE_KeyboardFocusWindowCycle) {
  TestHelper("testKeyboardFocusWindowFocusCycle", "web_view/focus",
             NO_TEST_SERVER);

  EXPECT_EQ(embedder_web_contents()->GetFocusedFrame(),
            embedder_web_contents()->GetMainFrame());
  ExtensionTestMessageListener next_step_listener("TEST_STEP_PASSED", false);
  next_step_listener.set_failure_message("TEST_STEP_FAILED");
  {
    gfx::Rect offset = embedder_web_contents()->GetContainerBounds();
    // Click the <input> element inside the <webview>.
    // If we wanted, we could ask the embedder to compute an appropriate point.
    MoveMouseInsideWindow(gfx::Point(offset.x() + 40, offset.y() + 40));
    SendMouseClick(ui_controls::LEFT);
  }

  // Waits for the renderer to know the input has focus.
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_A, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_B, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_C, false, false, false, false));

  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("minimal", "Launched");
  extensions::AppWindow* window = GetFirstAppWindowForApp(extension->id());
  EXPECT_TRUE(content::ExecuteScript(
      embedder_web_contents(),
      "window.runCommand('monitorGuestEvent', 'focus');"));

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_F, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_O, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_O, false, true, false, false));

  // Close the other window and wait for the webview to regain focus.
  CloseAppWindow(window);
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(GetPlatformAppWindow()));
  next_step_listener.Reset();
  EXPECT_TRUE(
      content::ExecuteScript(embedder_web_contents(),
                             "window.runCommand('waitGuestEvent', 'focus');"));
  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());

  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_X, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_Y, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      GetPlatformAppWindow(), ui::VKEY_Z, false, false, false, false));

  next_step_listener.Reset();
  EXPECT_TRUE(content::ExecuteScript(
      embedder_web_contents(),
      "window.runCommand('testKeyboardFocusRunNextStep', 'aBcxYz');"));

  ASSERT_TRUE(next_step_listener.WaitUntilSatisfied());
}

#if defined(OS_MAC)
// This test verifies that replacement range for IME works with <webview>s. To
// verify this, a <webview> with an <input> inside is loaded. Then the <input>
// is focused and  populated with some text. The test then sends an IPC to
// commit some text which will replace part of the previous text some new text.
IN_PROC_BROWSER_TEST_F(WebViewImeInteractiveTest,
                       CommitTextWithReplacementRange) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  LoadAndLaunchPlatformApp("web_view/ime", "WebViewImeTest.Launched");
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(GetPlatformAppWindow()));

  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();

  content::WebContents* guest_web_contents =
      GetGuestViewManager()->GetLastGuestCreated();

  // Click the <input> element inside the <webview>. In its focus handle, the
  // <input> inside the <webview> initializes its value to "A B X D".
  ExtensionTestMessageListener focus_listener("WebViewImeTest.InputFocused",
                                              false);
  content::WebContents* target_web_contents = guest_web_contents;
  WaitForHitTestData(guest_web_contents);

  // The guest page has a large input box and (50, 50) lies inside the box.
  content::SimulateMouseClickAt(target_web_contents, 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(50, 50));
  EXPECT_TRUE(focus_listener.WaitUntilSatisfied());

  // Verify the text inside the <input> is "A B X D".
  std::string value;
  ASSERT_TRUE(ExecuteScriptAndExtractString(guest_web_contents,
                                            "window.domAutomationController."
                                            "send(document.querySelector('"
                                            "input').value)",
                                            &value));
  EXPECT_EQ("A B X D", value);

  // Now commit "C" to to replace the range (4, 5).
  // For OOPIF guests, the target for IME is the RWH for the guest's main frame.
  // For BrowserPlugin-based guests, input always goes to the embedder.
  ExtensionTestMessageListener input_listener("WebViewImetest.InputReceived",
                                              false);
  content::RenderWidgetHost* target_rwh_for_input =
      target_web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost();
  content::SendImeCommitTextToWidget(target_rwh_for_input, u"C",
                                     std::vector<ui::ImeTextSpan>(),
                                     gfx::Range(4, 5), 0);
  EXPECT_TRUE(input_listener.WaitUntilSatisfied());

  // Get the input value from the guest.
  value.clear();
  ASSERT_TRUE(ExecuteScriptAndExtractString(guest_web_contents,
                                            "window.domAutomationController."
                                            "send(document.querySelector('"
                                            "input').value)",
                                            &value));
  EXPECT_EQ("A B C D", value);
}
#endif  //  OS_MAC

// This test verifies that focusing an input inside a <webview> will put the
// guest process's render widget into a monitoring mode for composition range
// changes.
IN_PROC_BROWSER_TEST_F(WebViewImeInteractiveTest, CompositionRangeUpdates) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  LoadAndLaunchPlatformApp("web_view/ime", "WebViewImeTest.Launched");
  ASSERT_TRUE(ui_test_utils::ShowAndFocusNativeWindow(GetPlatformAppWindow()));

  // Flush any pending events to make sure we start with a clean slate.
  content::RunAllPendingInMessageLoop();

  content::WebContents* guest_web_contents =
      GetGuestViewManager()->GetLastGuestCreated();

  // Click the <input> element inside the <webview>. In its focus handle, the
  // <input> inside the <webview> initializes its value to "A B X D".
  ExtensionTestMessageListener focus_listener("WebViewImeTest.InputFocused",
                                              false);
  content::WebContents* embedder_web_contents =
      guest_view::GuestViewBase::FromWebContents(guest_web_contents)
          ->embedder_web_contents();

  // Event routing in OOPIF and non-OOPIF <webview> is different. With OOPIF,
  // input is directly routed to the guest process as opposed to the non OOPIF
  // mode where input is always sent to the embedder process first (then hops
  // back to the browser and then to the guest).
  content::WebContents* target_web_contents = guest_web_contents;
  WaitForHitTestData(guest_web_contents);

  // The guest page has a large input box and (50, 50) lies inside the box.
  content::SimulateMouseClickAt(target_web_contents, 0,
                                blink::WebMouseEvent::Button::kLeft,
                                gfx::Point(50, 50));
  EXPECT_TRUE(focus_listener.WaitUntilSatisfied());

  // Clear the string as it already contains some text. Then verify the text in
  // the <input> is empty.
  std::string value;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      guest_web_contents,
      "var input = document.querySelector('input');"
      "input.value = '';"
      "window.domAutomationController.send("
      "    document.querySelector('input').value)",
      &value));
  EXPECT_EQ("", value);

  // Now set some composition text which should lead to an update in composition
  // range information.
  CompositionRangeUpdateObserver observer(embedder_web_contents);
  content::SendImeSetCompositionTextToWidget(
      target_web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost(),
      u"ABC", std::vector<ui::ImeTextSpan>(), gfx::Range::InvalidRange(), 0, 3);
  observer.WaitForCompositionRangeLength(3U);
}

#if defined(OS_MAC)
// This test verifies that drop-down lists appear correctly inside OOPIF-based
// webviews which have offset inside embedder. This is a test for all guest
// views as the logic for showing such popups is inside content/ layer. For more
// context see https://crbug.com/772840.
IN_PROC_BROWSER_TEST_F(WebViewFocusInteractiveTest,
                       DropDownPopupInCorrectPosition) {
  TestHelper("testSelectPopupPositionInMac", "web_view/shim", NO_TEST_SERVER);
  ASSERT_TRUE(guest_web_contents_);

  // This is set in javascript.
  const float distance_from_root_view_origin = 250.0;
  // Verify that the view is offset inside root view as expected.
  content::RenderWidgetHostView* guest_rwhv =
      guest_web_contents_->GetRenderWidgetHostView();
  while (guest_rwhv->TransformPointToRootCoordSpace(gfx::Point())
             .OffsetFromOrigin()
             .Length() < distance_from_root_view_origin) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  // Now trigger the popup and wait until it is displayed. The popup will get
  // dismissed after being shown.
  NewSubViewAddedObserver popup_observer(embedder_web_contents_);
  // Now send a mouse click and wait until the <select> tag is focused.
  SimulateRWHMouseClick(guest_rwhv->GetRenderWidgetHost(),
                        blink::WebMouseEvent::Button::kLeft, 5, 5);
  popup_observer.WaitForNextSubView();

  // Verify the popup bounds intersect with those of the guest. Since the popup
  // is relatively small (the width is determined by the <select> element's
  // width and the hight is a factor of font-size and number of items), the
  // intersection alone is a good indication the popup is shown properly inside
  // the screen.
  gfx::Rect guest_bounds_in_embedder(
      guest_rwhv->TransformPointToRootCoordSpace(gfx::Point()),
      guest_rwhv->GetViewBounds().size());
  EXPECT_TRUE(guest_bounds_in_embedder.Intersects(
      popup_observer.view_bounds_in_screen()))
      << "Guest bounds:" << guest_bounds_in_embedder.ToString()
      << " do not intersect with popup bounds:"
      << popup_observer.view_bounds_in_screen().ToString();
}
#endif
