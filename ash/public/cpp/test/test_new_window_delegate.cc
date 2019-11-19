// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_new_window_delegate.h"

namespace ash {

TestNewWindowDelegate::TestNewWindowDelegate() = default;
TestNewWindowDelegate::~TestNewWindowDelegate() = default;

void TestNewWindowDelegate::NewTab() {}
void TestNewWindowDelegate::NewTabWithUrl(const GURL& url,
                                          bool from_user_interaction) {}
void TestNewWindowDelegate::NewWindow(bool incognito) {}
void TestNewWindowDelegate::OpenFileManager() {}
void TestNewWindowDelegate::OpenCrosh() {}
void TestNewWindowDelegate::OpenGetHelp() {}
void TestNewWindowDelegate::RestoreTab() {}
void TestNewWindowDelegate::ShowKeyboardShortcutViewer() {}
void TestNewWindowDelegate::ShowTaskManager() {}
void TestNewWindowDelegate::OpenFeedbackPage(bool from_assistant) {}

}  // namespace ash
