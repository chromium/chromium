// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/window_enumerator.h"

#include <windows.h>

#include <string>
#include <vector>

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

TEST(WindowEnumeratorTest, EnumerateTopLevelWindows) {
  EnumerateChildWindows(
      ::GetDesktopWindow(), base::BindLambdaForTesting([&](HWND hwnd) {
        const std::wstring window_class = GetWindowClass(hwnd);
        EXPECT_EQ(window_class, [&] {
          constexpr int kMaxWindowClassNameLength = 256;
          wchar_t buffer[kMaxWindowClassNameLength + 1] = {0};
          const int name_len = ::GetClassName(hwnd, buffer, std::size(buffer));
          if (name_len <= 0 || name_len > kMaxWindowClassNameLength) {
            return std::wstring();
          }
          return std::wstring(&buffer[0], static_cast<size_t>(name_len));
        }());

        EXPECT_EQ(IsTopmostWindow(hwnd),
                  (::GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0);

        EXPECT_EQ(IsSystemDialog(hwnd), window_class == L"#32770");

        EXPECT_EQ(IsShellWindow(hwnd),
                  window_class == L"Button" ||
                      window_class == L"Shell_TrayWnd" ||
                      window_class == L"Shell_SecondaryTrayWnd");
        EXPECT_EQ(GetWindowTextString(hwnd), [&] {
          const int num_chars = ::GetWindowTextLength(hwnd);
          if (!num_chars) {
            return std::wstring();
          }
          std::vector<wchar_t> text(static_cast<size_t>(num_chars) + 1);
          if (!::GetWindowText(hwnd, &text.front(),
                               static_cast<int>(text.size()))) {
            return std::wstring();
          }
          return std::wstring(text.begin(), --text.end());
        }());
        return false;
      }));
}

}  // namespace base::win
