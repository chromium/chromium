// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_delegate.h"

namespace ash {

bool ShellDelegate::AllowDefaultTouchActions(gfx::NativeWindow window) {
  return true;
}

bool ShellDelegate::ShouldWaitForTouchPressAck(gfx::NativeWindow window) {
  return false;
}

bool ShellDelegate::IsTabDrag(const ui::OSExchangeData& drop_data) {
  return false;
}

media_session::MediaSessionService* ShellDelegate::GetMediaSessionService() {
  return nullptr;
}

bool ShellDelegate::IsUiDevToolsStarted() const {
  return false;
}

int ShellDelegate::GetUiDevToolsPort() const {
  return -1;
}

const GURL& ShellDelegate::GetLastCommittedURLForWindowIfAny(
    aura::Window* window) {
  return GURL::EmptyGURL();
}

void ShellDelegate::ShouldExitFullscreenBeforeLock(
    ShellDelegate::ShouldExitFullscreenCallback callback) {
  std::move(callback).Run(false);
}

DeskProfilesDelegate* ShellDelegate::GetDeskProfilesDelegate() {
  return nullptr;
}

bool ShellDelegate::IsNoFirstRunSwitchOn() const {
  return false;
}

}  // namespace ash
