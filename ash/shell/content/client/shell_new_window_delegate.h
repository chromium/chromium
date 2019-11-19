// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_CONTENT_CLIENT_SHELL_NEW_WINDOW_DELEGATE_H_
#define ASH_SHELL_CONTENT_CLIENT_SHELL_NEW_WINDOW_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/public/cpp/new_window_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/content_browser_client.h"

namespace ash {
namespace shell {

class ShellNewWindowDelegate : public ash::NewWindowDelegate {
 public:
  ShellNewWindowDelegate();
  ~ShellNewWindowDelegate() override;

  // ash::NewWindowDelegate:
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

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellNewWindowDelegate);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_CONTENT_CLIENT_SHELL_NEW_WINDOW_DELEGATE_H_
