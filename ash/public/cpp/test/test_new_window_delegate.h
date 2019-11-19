// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_NEW_WINDOW_DELEGATE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_NEW_WINDOW_DELEGATE_H_

#include "ash/public/cpp/new_window_delegate.h"
#include "base/macros.h"

namespace ash {

// NewWindowDelegate implementation which does nothing.
class ASH_PUBLIC_EXPORT TestNewWindowDelegate : public NewWindowDelegate {
 public:
  TestNewWindowDelegate();
  ~TestNewWindowDelegate() override;

 private:
  // NewWindowDelegate:
  void NewTab() override;
  void NewTabWithUrl(const GURL& url, bool from_user_interaction) override;
  void NewWindow(bool incognito) override;
  void OpenFileManager() override;
  void OpenCrosh() override;
  void OpenGetHelp() override;
  void RestoreTab() override;
  void ShowKeyboardShortcutViewer() override;
  void ShowTaskManager() override;
  void OpenFeedbackPage(bool from_assistant) override;

  DISALLOW_COPY_AND_ASSIGN(TestNewWindowDelegate);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_NEW_WINDOW_DELEGATE_H_
