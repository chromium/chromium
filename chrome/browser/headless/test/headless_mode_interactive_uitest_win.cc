// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <dwmapi.h>

#include <optional>
#include <vector>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "chrome/browser/headless/headless_mode_init.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/win/window_impl.h"

namespace headless {

namespace {

class HeadlessModeInteractiveUiTest : public InProcessBrowserTest {
 public:
  HeadlessModeInteractiveUiTest() = default;

  HeadlessModeInteractiveUiTest(const HeadlessModeInteractiveUiTest&) = delete;
  HeadlessModeInteractiveUiTest& operator=(
      const HeadlessModeInteractiveUiTest&) = delete;

  ~HeadlessModeInteractiveUiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InitHeadlessMode(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(headless::IsHeadlessMode());
  }

 private:
  void InitHeadlessMode(base::CommandLine* command_line) {
    command_line->AppendSwitch(::switches::kHeadless);
    auto init_headless_mode = headless::InitHeadlessMode();
    CHECK(init_headless_mode.has_value()) << init_headless_mode.error();
    headless_mode_handle_ = std::move(headless::InitHeadlessMode().value());
  }

  std::unique_ptr<HeadlessModeHandle> headless_mode_handle_;
};

// BlankWindow creates a solid top-level window covering the desktop to provide
// a clean, predictable baseline for screen captures. This prevents desktop
// wallpaper, taskbars, or console log updates from interfering with
// screenshots.
class BlankWindow : public gfx::WindowImpl {
 public:
  BlankWindow() : gfx::WindowImpl("HeadlessModeTestBlankWindow") {}
  ~BlankWindow() override { Destroy(); }

  void Init(const gfx::Rect& bounds) {
    set_window_style(WS_POPUP | WS_VISIBLE);
    gfx::WindowImpl::Init(nullptr, bounds);

    // Disable DWM window transition animations.
    BOOL disable_transition = TRUE;
    ::DwmSetWindowAttribute(hwnd(), DWMWA_TRANSITIONS_FORCEDISABLED,
                            &disable_transition, sizeof(disable_transition));
    ::UpdateWindow(hwnd());
  }

  void MoveTo(const gfx::Rect& bounds) {
    if (hwnd()) {
      ::SetWindowPos(hwnd(), nullptr, bounds.x(), bounds.y(), bounds.width(),
                     bounds.height(), SWP_NOZORDER | SWP_NOACTIVATE);
      ::UpdateWindow(hwnd());
    }
  }

  void Destroy() {
    if (hwnd()) {
      ::DestroyWindow(hwnd());
    }
  }

  BOOL ProcessWindowMessage(HWND hwnd,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id = 0) override {
    if (message == WM_ERASEBKGND) {
      HDC hdc = reinterpret_cast<HDC>(w_param);
      base::win::ScopedGDIObject<HBRUSH> brush(
          ::CreateSolidBrush(RGB(240, 240, 240)));
      RECT rect;
      ::GetClientRect(hwnd, &rect);
      ::FillRect(hdc, &rect, brush.get());
      result = 1;
      return TRUE;
    }
    return FALSE;
  }
};

// Captures a screenshot of the entire desktop screen using Windows GDI.
// The CAPTUREBLT flag ensures post-DWM composited layered windows are captured.
bool CaptureDesktopScreen(SkBitmap* bitmap) {
  int width = ::GetSystemMetrics(SM_CXSCREEN);
  int height = ::GetSystemMetrics(SM_CYSCREEN);

  base::win::ScopedGetDC screen_dc(nullptr);
  if (!screen_dc) {
    PLOG(ERROR) << "Failed to get screen DC";
    return false;
  }

  base::win::ScopedCreateDC mem_dc(::CreateCompatibleDC(screen_dc));
  if (!mem_dc.is_valid()) {
    PLOG(ERROR) << "Failed to create compatible DC";
    return false;
  }

  base::win::ScopedGDIObject<HBITMAP> compatible_bitmap(
      ::CreateCompatibleBitmap(screen_dc, width, height));
  if (!compatible_bitmap.is_valid()) {
    PLOG(ERROR) << "Failed to create compatible bitmap";
    return false;
  }

  base::win::ScopedSelectObject select_object(mem_dc.Get(),
                                              compatible_bitmap.get());

  // Move the mouse cursor to the bottom right of the screen to prevent it from
  // interfering with the screenshots.
  ::SetCursorPos(width, height);

  // BitBlt with CAPTUREBLT grabs the composited screen output from DWM.
  if (!::BitBlt(mem_dc.Get(), 0, 0, width, height, screen_dc, 0, 0,
                SRCCOPY | CAPTUREBLT)) {
    PLOG(ERROR) << "BitBlt failed to capture desktop screen";
    return false;
  }

  BITMAP bmp;
  ::GetObject(compatible_bitmap.get(), sizeof(BITMAP), &bmp);

  BITMAPINFOHEADER bih = {0};
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = bmp.bmWidth;
  bih.biHeight = -bmp.bmHeight;  // top-down format
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;

  bitmap->allocN32Pixels(bmp.bmWidth, bmp.bmHeight);
  if (::GetDIBits(screen_dc, compatible_bitmap.get(), 0, bmp.bmHeight,
                  bitmap->getPixels(), reinterpret_cast<BITMAPINFO*>(&bih),
                  DIB_RGB_COLORS) == 0) {
    PLOG(ERROR) << "GetDIBits failed to retrieve desktop pixels";
    return false;
  }

  return true;
}

// Since there is no way to hook up to DWM events directly to know when the DWM
// composition is fully complete and presented on screen, we have to resort to
// delaying the test before capturing the screen image.
void RunLoopForMilliseconds(int milliseconds) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(milliseconds));
  run_loop.Run();
}

// To save screenshots to a specific directory when running the test locally,
// set the ISOLATED_OUTDIR environment variable before running the command.
// Otherwise, they will be saved to the system temporary directory.
base::FilePath GetTestArtifactsDir() {
  const char* var = std::getenv("ISOLATED_OUTDIR");
  if (var) {
    return base::FilePath::FromUTF8Unsafe(var);
  }
  base::FilePath temp_dir;
  if (base::GetTempDir(&temp_dir)) {
    return temp_dir;
  }
  return base::FilePath();
}

void SaveBitmapToFile(const SkBitmap& bitmap, const std::string& filename) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath artifacts_dir = GetTestArtifactsDir();
  if (artifacts_dir.empty()) {
    LOG(ERROR) << "Failed to resolve test artifacts directory.";
    return;
  }
  base::FilePath file_path = artifacts_dir.AppendASCII(filename);
  std::optional<std::vector<uint8_t>> png_data =
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false);
  if (!png_data) {
    LOG(ERROR) << "Failed to encode bitmap to PNG.";
    return;
  }
  if (!base::WriteFile(file_path, png_data.value())) {
    LOG(ERROR) << "Failed to write PNG file to " << file_path.value();
  } else {
    LOG(INFO) << "Saved screenshot to " << file_path.value();
  }
}

IN_PROC_BROWSER_TEST_F(HeadlessModeInteractiveUiTest,
                       WindowsDesktopScreenNoArtifacts) {
  int width = ::GetSystemMetrics(SM_CXSCREEN);
  int height = ::GetSystemMetrics(SM_CYSCREEN);

  // Create a blank window covering the entire screen to prevent console log
  // updates from showing in the screenshots.
  BlankWindow blank_window;
  blank_window.Init(gfx::Rect(0, 0, width, height));

  // Force DWM to fully render the blank window.
  ::DwmFlush();
  RunLoopForMilliseconds(150);

  // Capture the baseline screen state.
  SkBitmap screen_before;
  ASSERT_TRUE(CaptureDesktopScreen(&screen_before));

  // Verify the baseline screen is a uniform color in the work area.
  RECT work_area;
  ::SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0);
  SkColor baseline_color =
      screen_before.getColor(work_area.left, work_area.top);
  int non_uniform_pixels = 0;
  for (int y = work_area.top; y < work_area.bottom; ++y) {
    for (int x = work_area.left; x < work_area.right; ++x) {
      if (screen_before.getColor(x, y) != baseline_color) {
        non_uniform_pixels++;
      }
    }
  }
  // Some bots display an "Activate Windows" watermark at the bottom right of
  // the desktop, so allow some number of non-uniform pixels
  // (crbug.com/524704032).
  constexpr int kNonUniformPixelsTolerance = 3000;
  if (non_uniform_pixels > kNonUniformPixelsTolerance) {
    SaveBitmapToFile(screen_before, "screen_before.png");
  }
  EXPECT_LE(non_uniform_pixels, kNonUniformPixelsTolerance)
      << "Detected " << non_uniform_pixels
      << " non-uniform pixels in the baseline screen work area!";

  // Create a new headless browser window and wait for it to be active.
  Browser* new_browser =
      ui_test_utils::OpenNewEmptyWindowAndWaitUntilActivated(GetProfile());
  ASSERT_TRUE(new_browser);

  content::WebContents* web_contents =
      new_browser->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  content::WaitForCopyableViewInWebContents(web_contents);

  // Wait for the new browser's compositor to finish drawing a frame.
  ui::Compositor* compositor =
      BrowserView::GetBrowserViewForBrowser(new_browser)
          ->GetWidget()
          ->GetCompositor();
  ui::DrawWaiterForTest::WaitForCompositingEnded(compositor);

  // Retrieve the window bounds and inflate them to include shadows/artifacts.
  gfx::Rect window_bounds = new_browser->GetWindow()->GetBounds();
  window_bounds.Outset(20);

  // Ensure the new browser window has finished updates.
  HWND new_browser_hwnd = new_browser->GetWindow()
                              ->GetNativeWindow()
                              ->GetHost()
                              ->GetAcceleratedWidget();
  ::RedrawWindow(new_browser_hwnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ALLCHILDREN);
  ::UpdateWindow(new_browser_hwnd);
  ::DwmFlush();

  // Force DWM to recompose the desktop area by moving the baseline blank
  // window.
  blank_window.MoveTo(gfx::Rect(10, 10, width, height));
  ::DwmFlush();
  blank_window.MoveTo(gfx::Rect(0, 0, width, height));
  ::DwmFlush();
  RunLoopForMilliseconds(150);

  // Capture the desktop state after opening the headless browser.
  SkBitmap screen_after;
  ASSERT_TRUE(CaptureDesktopScreen(&screen_after));

  // Verify capturing succeeded and dimensions match.
  ASSERT_EQ(screen_before.width(), screen_after.width());
  ASSERT_EQ(screen_before.height(), screen_after.height());

  // Clip the window bounds to the screen boundaries.
  gfx::Rect screen_bounds(0, 0, screen_before.width(), screen_before.height());
  window_bounds.Intersect(screen_bounds);

  // Compare pixels inside the window region.
  int diff_pixels = 0;
  for (int y = window_bounds.y(); y < window_bounds.bottom(); ++y) {
    for (int x = window_bounds.x(); x < window_bounds.right(); ++x) {
      if (screen_before.getColor(x, y) != screen_after.getColor(x, y)) {
        diff_pixels++;
      }
    }
  }

  // Save the screen captures only if we detect a discrepancy.
  if (diff_pixels > 0) {
    SaveBitmapToFile(screen_before, "screen_before.png");
    SaveBitmapToFile(screen_after, "screen_after.png");
  }

  // There must be no different pixels if the window was correctly cloaked.
  EXPECT_EQ(diff_pixels, 0)
      << "Detected " << diff_pixels
      << " different pixels in the headless window region after it was opened!";
}

}  // namespace

}  // namespace headless
