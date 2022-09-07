// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/dark_mode_support.h"

#include <windows.h>

#include "base/native_library.h"
#include "base/scoped_native_library.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

namespace {

constexpr wchar_t kWndClassName[] = L"DarkModeWClass";

class DarkModeSupportTest : public testing::Test {
 protected:
  DarkModeSupportTest() {
    ux_theme_lib_ = base::ScopedNativeLibrary(
        base::LoadSystemLibrary(L"uxtheme.dll", &error_));
  }
  ~DarkModeSupportTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    // Do this here since the assert cannot live in the constructor.
    ASSERT_FALSE(error_.code)
        << "Failed to load uxtheme.dll with error " << error_.ToString();
  }

  void TearDown() override {
    // Return the process to the pre-allowed dark mode state.
    AllowDarkModeForApp(false);
    testing::Test::TearDown();
  }

 private:
  base::NativeLibraryLoadError error_;
  base::ScopedNativeLibrary ux_theme_lib_;
};

class ScopedWindowClass {
 public:
  explicit ScopedWindowClass(const WNDCLASSEX& wnd_class)
      : instance_(wnd_class.hInstance),
        wnd_class_name_(wnd_class.lpszClassName) {
    DoRegisterClass(wnd_class);
  }
  ~ScopedWindowClass() { ::UnregisterClass(wnd_class_name_, instance_); }

 private:
  void DoRegisterClass(const WNDCLASSEX& wnd_class) {
    ASSERT_TRUE(::RegisterClassEx(&wnd_class));
  }
  const HMODULE instance_;
  const wchar_t* wnd_class_name_;
};

}  // namespace

TEST_F(DarkModeSupportTest, TestIsDarkModeAvailable) {
  // The return value is irrelevant here. This only tests that this call can be
  // make without crashing or otherwise behaving badly.
  IsDarkModeAvailable();
}

TEST_F(DarkModeSupportTest, TestAllowDarkModeForApp) {
  // This call should always succeed and not crash or behave badly.
  AllowDarkModeForApp(true);
}

TEST_F(DarkModeSupportTest, TestAllowDarkModeForWindowNoCrash) {
  HINSTANCE instance = ::GetModuleHandle(nullptr);
  WNDCLASSEX wnd_class = {
      .cbSize = sizeof(wnd_class),
      .style = CS_HREDRAW | CS_VREDRAW,
      .lpfnWndProc = ::DefWindowProc,
      .hInstance = instance,
      .hIcon = ::LoadIcon(NULL, IDI_APPLICATION),
      .hCursor = ::LoadCursor(NULL, IDC_ARROW),
      .hbrBackground = static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)),
      .lpszClassName = kWndClassName,
  };
  ScopedWindowClass scoped_window_class = ScopedWindowClass(wnd_class);

  HWND hwnd =
      ::CreateWindow(kWndClassName, L"DarkModeTest", WS_OVERLAPPEDWINDOW,
                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                     nullptr, nullptr, instance, nullptr);
  ASSERT_TRUE(hwnd) << "::CreateWindow failed with error " << ::GetLastError();
  // The "dark mode" calls below shouldn't crash. This doesn't test whether or
  // not they actually work, rather only that they don't crash or otherwise
  // behave badly.
  AllowDarkModeForApp(true);
  AllowDarkModeForWindow(hwnd, true);
  ::DestroyWindow(hwnd);
}

}  // namespace base::win
