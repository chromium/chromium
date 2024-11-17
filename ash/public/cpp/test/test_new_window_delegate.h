// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_NEW_WINDOW_DELEGATE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_NEW_WINDOW_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"

namespace ash {

// NewWindowDelegate implementation which does nothing.
class ASH_PUBLIC_EXPORT TestNewWindowDelegate : public NewWindowDelegate {
 public:
  TestNewWindowDelegate();
  TestNewWindowDelegate(const TestNewWindowDelegate&) = delete;
  TestNewWindowDelegate& operator=(const TestNewWindowDelegate&) = delete;
  ~TestNewWindowDelegate() override;

 private:
  // NewWindowDelegate:
  void NewTab() override;
  void NewWindow(bool incognito, bool should_trigger_session_restore) override;
  void NewWindowForDetachingTab(
      aura::Window* source_window,
      const ui::OSExchangeData& drop_data,
      NewWindowForDetachingTabCallback closure) override;
  void OpenUrl(const GURL& url,
               OpenUrlFrom from,
               Disposition disposition) override;
  void OpenCalculator() override;
  void OpenFileManager() override;
  void OpenDownloadsFolder() override;
  void OpenCrosh() override;
  void OpenDiagnostics() override;
  void OpenGetHelp() override;
  void RestoreTab() override;
  void ShowShortcutCustomizationApp() override;
  void ShowTaskManager() override;
  void OpenFeedbackPage(FeedbackSource source,
                        const std::string& description_template) override;
  void OpenPersonalizationHub() override;
  void OpenCaptivePortalSignin(const GURL& url) override;
  void OpenFile(const base::FilePath& file_path) override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_NEW_WINDOW_DELEGATE_H_
