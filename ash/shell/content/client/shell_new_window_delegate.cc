// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/client/shell_new_window_delegate.h"

#include "ash/public/cpp/keyboard_shortcut_viewer.h"
#include "ash/shell/content/client/shell_browser_main_parts.h"
#include "ash/shell/content/embedded_browser.h"

namespace ash {
namespace shell {

ShellNewWindowDelegate::ShellNewWindowDelegate() = default;
ShellNewWindowDelegate::~ShellNewWindowDelegate() = default;

void ShellNewWindowDelegate::NewTab() {
  EmbeddedBrowser::Create(
      ash::shell::ShellBrowserMainParts::GetBrowserContext(),
      GURL("https://www.google.com"));
}

void ShellNewWindowDelegate::NewTabWithUrl(const GURL& url,
                                           bool from_user_interaction) {
  EmbeddedBrowser::Create(
      ash::shell::ShellBrowserMainParts::GetBrowserContext(), url);
}

void ShellNewWindowDelegate::NewWindow(bool incognito) {
  EmbeddedBrowser::Create(
      ash::shell::ShellBrowserMainParts::GetBrowserContext(),
      GURL("https://www.google.com"));
}

void ShellNewWindowDelegate::OpenFileManager() {}

void ShellNewWindowDelegate::OpenCrosh() {}

void ShellNewWindowDelegate::OpenGetHelp() {}

void ShellNewWindowDelegate::RestoreTab() {}

void ShellNewWindowDelegate::ShowKeyboardShortcutViewer() {
  ash::ToggleKeyboardShortcutViewer();
}

void ShellNewWindowDelegate::ShowTaskManager() {}

void ShellNewWindowDelegate::OpenFeedbackPage(bool from_assistant) {}

}  // namespace shell
}  // namespace ash
