// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_new_window_delegate.h"

#include <utility>

#include "ui/aura/window.h"

namespace ash {

TestNewWindowDelegate::TestNewWindowDelegate() = default;
TestNewWindowDelegate::~TestNewWindowDelegate() = default;

void TestNewWindowDelegate::NewTab() {}
void TestNewWindowDelegate::NewWindow(bool incognito,
                                      bool should_trigger_session_restore) {}
void TestNewWindowDelegate::NewWindowForDetachingTab(
    aura::Window* source_window,
    const ui::OSExchangeData& drop_data,
    NewWindowForDetachingTabCallback closure) {
  std::move(closure).Run(/*new_window=*/nullptr);
}
void TestNewWindowDelegate::OpenUrl(const GURL& url,
                                    bool from_user_interaction) {}
void TestNewWindowDelegate::OpenCalculator() {}
void TestNewWindowDelegate::OpenFileManager() {}
void TestNewWindowDelegate::OpenDownloadsFolder() {}
void TestNewWindowDelegate::OpenCrosh() {}
void TestNewWindowDelegate::OpenDiagnostics() {}
void TestNewWindowDelegate::OpenGetHelp() {}
void TestNewWindowDelegate::RestoreTab() {}
void TestNewWindowDelegate::ShowKeyboardShortcutViewer() {}
void TestNewWindowDelegate::ShowTaskManager() {}
void TestNewWindowDelegate::OpenFeedbackPage(
    FeedbackSource source,
    const std::string& description_template) {}
void TestNewWindowDelegate::OpenPersonalizationHub() {}

TestNewWindowDelegateProvider::TestNewWindowDelegateProvider(
    std::unique_ptr<TestNewWindowDelegate> delegate)
    : delegate_(std::move(delegate)) {}

TestNewWindowDelegateProvider::~TestNewWindowDelegateProvider() = default;

NewWindowDelegate* TestNewWindowDelegateProvider::GetInstance() {
  return delegate_.get();
}

NewWindowDelegate* TestNewWindowDelegateProvider::GetPrimary() {
  return delegate_.get();
}

}  // namespace ash
