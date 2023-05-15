// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/message_window.h"

#include <windows.h>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// To avoid conflicts with the macro from the Windows SDK...
#undef FindWindow

namespace base {

namespace {

bool HandleMessage(UINT message,
                   WPARAM wparam,
                   LPARAM lparam,
                   LRESULT* result) {
  // Return |wparam| as the result of WM_USER message.
  if (message == WM_USER) {
    *result = wparam;
    return true;
  }

  return false;
}

}  // namespace

// Checks that a window can be created.
TEST(MessageWindowTest, Create) {
  win::MessageWindow window;
  EXPECT_TRUE(window.Create(base::BindRepeating(&HandleMessage)));
}

// Checks that a named window can be created.
TEST(MessageWindowTest, CreateNamed) {
  win::MessageWindow window;
  EXPECT_TRUE(window.CreateNamed(base::BindRepeating(&HandleMessage),
                                 UTF8ToWide("test_message_window")));
}

// Verifies that the created window can receive messages.
TEST(MessageWindowTest, SendMessage) {
  win::MessageWindow window;
  EXPECT_TRUE(window.Create(base::BindRepeating(&HandleMessage)));

  EXPECT_EQ(SendMessage(window.hwnd(), WM_USER, 100, 0), 100);
}

// Verifies that a named window can be found by name.
TEST(MessageWindowTest, FindWindow) {
  std::wstring name =
      UTF8ToWide(base::Uuid::GenerateRandomV4().AsLowercaseString());
  win::MessageWindow window;
  EXPECT_TRUE(window.CreateNamed(base::BindRepeating(&HandleMessage), name));

  HWND hwnd = win::MessageWindow::FindWindow(name);
  EXPECT_TRUE(hwnd != nullptr);
  EXPECT_EQ(SendMessage(hwnd, WM_USER, 200, 0), 200);
}

}  // namespace base
