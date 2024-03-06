// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/dark_mode_support.h"

#include <windows.h>

#include "base/check.h"
#include "base/native_library.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"

namespace {

// APIs for controlling how an app and window respond to system-level
// dark/light modes.

// Available on Wwindows build base::win::Version::WIN10_19H1 and up.
enum class PreferredAppMode {
  kDefault,
  kAllowDark,
  kForceDark,
  kForceLight,
  kMax
};

// The following APIs and code was based on information from here:
// https://github.com/ysc3839/win32-darkmode

// Only available on Windows build base::win::Version::WIN10_RS5.
// NOLINTNEXTLINE(readability/casting)
using UxThemeAllowDarkModeForAppFunc = bool(WINAPI*)(bool allow);

// Available on Windows build base::win::Version::WIN10_19H1 and up.
using UxThemeSetPreferredAppModeFunc =
    // NOLINTNEXTLINE(readability/casting)
    PreferredAppMode(WINAPI*)(PreferredAppMode app_mode);

// Available on Windows build base::win::Version::WIN10_RS5 and up.
// NOLINTNEXTLINE(readability/casting)
using UxThemeAllowDarkModeForWindowFunc = bool(WINAPI*)(HWND hwnd, bool allow);

// The following two ordinals are mutually exclusive and represent a difference
// between base::win::Version::WIN10_RS5 and base::win::Version::WIN10_19H1.
constexpr WORD kUxThemeAllowDarkModeForAppOrdinal = 135;
constexpr WORD kUxThemeSetPreferredAppModeOrdinal = 135;
constexpr WORD kUxThemeAllowDarkModeForWindowOrdinal = 133;

struct DarkModeSupport {
  UxThemeAllowDarkModeForAppFunc allow_dark_mode_for_app = nullptr;
  UxThemeSetPreferredAppModeFunc set_preferred_app_mode = nullptr;
  UxThemeAllowDarkModeForWindowFunc allow_dark_mode_for_window = nullptr;
};

const DarkModeSupport& GetDarkModeSupport() {
  static const DarkModeSupport dark_mode_support =
      [] {
        DarkModeSupport dark_mode_support;
        auto* os_info = base::win::OSInfo::GetInstance();
        // Dark mode only works on WIN10_RS5 and up. uxtheme.dll depends on
        // GDI32.dll which is not available under win32k lockdown sandbox.
        if (os_info->version() >= base::win::Version::WIN10_RS5 &&
            base::win::IsUser32AndGdi32Available()) {
          base::NativeLibraryLoadError error;
          HMODULE ux_theme_lib = base::PinSystemLibrary(L"uxtheme.dll", &error);
          DCHECK(!error.code);
          if (os_info->version() >= base::win::Version::WIN10_19H1) {
            dark_mode_support.set_preferred_app_mode =
                reinterpret_cast<UxThemeSetPreferredAppModeFunc>(
                    ::GetProcAddress(
                        ux_theme_lib,
                        MAKEINTRESOURCEA(kUxThemeSetPreferredAppModeOrdinal)));
          } else {
            dark_mode_support.allow_dark_mode_for_app =
                reinterpret_cast<UxThemeAllowDarkModeForAppFunc>(
                    ::GetProcAddress(
                        ux_theme_lib,
                        MAKEINTRESOURCEA(kUxThemeAllowDarkModeForAppOrdinal)));
          }
          dark_mode_support.allow_dark_mode_for_window =
              reinterpret_cast<UxThemeAllowDarkModeForWindowFunc>(
                  ::GetProcAddress(
                      ux_theme_lib,
                      MAKEINTRESOURCEA(kUxThemeAllowDarkModeForWindowOrdinal)));
        }
        return dark_mode_support;
      }();
  return dark_mode_support;
}

}  // namespace

namespace base::win {

bool IsDarkModeAvailable() {
  auto& dark_mode_support = GetDarkModeSupport();
  return (dark_mode_support.allow_dark_mode_for_app ||
          dark_mode_support.set_preferred_app_mode) &&
         dark_mode_support.allow_dark_mode_for_window;
}

void AllowDarkModeForApp(bool allow) {
  if (!IsDarkModeAvailable())
    return;
  auto& dark_mode_support = GetDarkModeSupport();
  if (dark_mode_support.set_preferred_app_mode) {
    dark_mode_support.set_preferred_app_mode(
        allow ? PreferredAppMode::kAllowDark : PreferredAppMode::kDefault);
  } else {
    dark_mode_support.allow_dark_mode_for_app(allow);
  }
}

bool AllowDarkModeForWindow(HWND hwnd, bool allow) {
  if (!IsDarkModeAvailable())
    return false;
  return GetDarkModeSupport().allow_dark_mode_for_window(hwnd, allow);
}

}  // namespace base::win
