// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <array>
#include <string>
#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/document_picture_in_picture_mixin_test_base.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/win/hwnd_util.h"
#include "ui/views/window/non_client_view.h"

namespace {

// SendMessage returns once menu handling finishes, so a suppressed menu needs
// no timeout. A native timer detects and dismisses menus in TrackPopupMenu's
// modal loop, which does not necessarily run Chromium tasks.
bool SystemMenuOpensAfter(base::OnceClosure send_messages) {
  bool menu_opened = false;
  static thread_local bool* g_menu_opened = nullptr;
  base::AutoReset<bool*> reset(&g_menu_opened, &menu_opened);
  const UINT_PTR timer =
      ::SetTimer(nullptr, 0, 10, [](HWND, UINT, UINT_PTR, DWORD) {
        if (!g_menu_opened) {
          return;
        }
        GUITHREADINFO info = {sizeof(info)};
        if (::GetGUIThreadInfo(::GetCurrentThreadId(), &info) &&
            (info.flags & GUI_INMENUMODE)) {
          *g_menu_opened = true;
          ::EndMenu();
        }
        if (views::MenuController::GetActiveInstance()) {
          *g_menu_opened = true;
          views::MenuController::CancelAllActive();
        }
      });
  EXPECT_NE(0u, timer);
  if (!timer) {
    return false;
  }
  std::move(send_messages).Run();
  ::KillTimer(nullptr, timer);
  if (views::MenuController::GetActiveInstance()) {
    menu_opened = true;
    views::MenuController::CancelAllActive();
  }
  return menu_opened;
}

// Sets keys in this thread's key-state table, which GetKeyState() reads, and
// restores the table on destruction. SendMessage() does not update the table,
// so a test that sends the messages for Alt+Space or a right-click must set the
// key state those code paths read.
class ScopedKeyState {
 public:
  ScopedKeyState() { CHECK(::GetKeyboardState(saved_state_.data())); }
  ScopedKeyState(const ScopedKeyState&) = delete;
  ScopedKeyState& operator=(const ScopedKeyState&) = delete;
  ~ScopedKeyState() { ::SetKeyboardState(saved_state_.data()); }

  void Set(int virtual_key, bool down) {
    std::array<BYTE, 256> state;
    CHECK(::GetKeyboardState(state.data()));
    state[virtual_key] = static_cast<BYTE>(down ? (state[virtual_key] | 0x80)
                                                : (state[virtual_key] & ~0x80));
    CHECK(::SetKeyboardState(state.data()));
  }

 private:
  std::array<BYTE, 256> saved_state_;
};

class DocumentPipSystemMenuInteractiveUiTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  DocumentPipSystemMenuInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{blink::features::kDocumentPictureInPictureAPI, true},
         {features::kDesktopGlowUp, false},
         {features::kDocumentPipStandaloneWindow, standalone_enabled()},
         {features::kMenuSimplification, menu_simplification_enabled()}});
  }

  void TearDownOnMainThread() override {
    PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  bool standalone_enabled() const { return std::get<0>(GetParam()); }
  bool menu_simplification_enabled() const { return std::get<1>(GetParam()); }

  void OpenAndActivatePipWindow() {
    ASSERT_NO_FATAL_FAILURE(
        pip_mixin_.NavigateToURLAndEnterPictureInPicture(browser()));
    auto* child = pip_mixin_.window_controller()->GetChildWebContents();
    ASSERT_TRUE(child);
    pip_mixin_.WaitForPageLoad(child);
    auto* widget = GetPipWidget();
    ASSERT_TRUE(widget);
    widget->Activate();
    views::test::WaitForWidgetActive(widget, true);
  }

  views::Widget* GetPipWidget() {
    return views::Widget::GetWidgetForNativeWindow(
        pip_mixin_.window_controller()
            ->GetChildWebContents()
            ->GetTopLevelNativeWindow());
  }

  gfx::Point GetCaptionPoint() {
    auto* widget = GetPipWidget();
    views::Label* title = nullptr;
    if (standalone_enabled()) {
      title = views::AsViewClass<DocumentPipFrameView>(
                  widget->non_client_view()->frame_view())
                  ->GetWindowTitleForTesting();
    } else {
      title = views::AsViewClass<PictureInPictureBrowserFrameView>(
                  widget->non_client_view()->frame_view())
                  ->GetWindowTitleForTesting();
    }
    return title->GetBoundsInScreen().CenterPoint();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DocumentPictureInPictureMixinTestBase pip_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_P(DocumentPipSystemMenuInteractiveUiTest, AltSpace) {
  ASSERT_NO_FATAL_FAILURE(OpenAndActivatePipWindow());
  const HWND hwnd = views::HWNDForWidget(GetPipWidget());
  const LPARAM alt_space_lparam = 1 | (0x39 << 16) | (1 << 29);

  // Each message path that can open the menu is checked on its own, so one
  // path opening a menu cannot hide another that no longer does.
  {
    // The messages Windows delivers for a typed Alt+Space. HWNDMessageHandler
    // reads Alt from the key-state table rather than from lParam.
    SCOPED_TRACE("WM_SYSKEYDOWN, WM_SYSCHAR, WM_SYSKEYUP");
    EXPECT_EQ(!menu_simplification_enabled(),
              SystemMenuOpensAfter(base::BindLambdaForTesting([&] {
                ScopedKeyState key_state;
                key_state.Set(VK_MENU, true);
                ::SendMessage(hwnd, WM_SYSKEYDOWN, VK_SPACE, alt_space_lparam);
                ::SendMessage(hwnd, WM_SYSCHAR, VK_SPACE, alt_space_lparam);
                ::SendMessage(hwnd, WM_SYSKEYUP, VK_SPACE,
                              alt_space_lparam | (1 << 30) | (1u << 31));
              })));
  }
  {
    SCOPED_TRACE("WM_SYSCOMMAND, SC_KEYMENU");
    EXPECT_EQ(!menu_simplification_enabled(),
              SystemMenuOpensAfter(base::BindLambdaForTesting([&] {
                ::SendMessage(hwnd, WM_SYSCOMMAND, SC_KEYMENU, ' ');
              })));
  }
}

IN_PROC_BROWSER_TEST_P(DocumentPipSystemMenuInteractiveUiTest,
                       TitleBarRightClick) {
  ASSERT_NO_FATAL_FAILURE(OpenAndActivatePipWindow());
  const HWND hwnd = views::HWNDForWidget(GetPipWidget());
  const gfx::Point screen_px =
      display::win::GetScreenWin()->DIPToScreenPoint(GetCaptionPoint());
  const LPARAM screen_lparam = MAKELPARAM(screen_px.x(), screen_px.y());
  ASSERT_EQ(HTCAPTION, ::SendMessage(hwnd, WM_NCHITTEST, 0, screen_lparam));
  POINT client_px = {screen_px.x(), screen_px.y()};
  ASSERT_TRUE(::ScreenToClient(hwnd, &client_px));
  EXPECT_EQ(!menu_simplification_enabled(),
            SystemMenuOpensAfter(base::BindLambdaForTesting([&] {
              ScopedKeyState key_state;
              // Views captures the mouse on the press only while the button
              // is down in the key-state table.
              key_state.Set(VK_RBUTTON, true);
              ::SendMessage(hwnd, WM_NCRBUTTONDOWN, HTCAPTION, screen_lparam);
              const bool captured = ::GetCapture() == hwnd;
              // The native path (HWNDMessageHandler) and the Views path (a
              // ContextMenuController on the NonClientView) both capture the
              // mouse on the press. A synthetic WM_NCRBUTTONUP does not make
              // Windows open its menu, so check capture itself.
              EXPECT_TRUE(captured);
              key_state.Set(VK_RBUTTON, false);
              // Windows sends the release as a client message only while the
              // window has captured the mouse, and as a non-client message
              // otherwise. Send the one Windows would, so a window that fails
              // to capture is not tested through the captured path.
              if (captured) {
                ::SendMessage(hwnd, WM_RBUTTONUP, 0,
                              MAKELPARAM(client_px.x, client_px.y));
              } else {
                ::SendMessage(hwnd, WM_NCRBUTTONUP, HTCAPTION, screen_lparam);
              }
            })));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DocumentPipSystemMenuInteractiveUiTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      return std::string(std::get<0>(info.param) ? "Standalone"
                                                 : "BrowserBacked") +
             (std::get<1>(info.param) ? "Simplified" : "Native");
    });

}  // namespace
