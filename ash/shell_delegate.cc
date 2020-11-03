// Copyright 2019 The Chromium Authors. All rights reserved.
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

aura::Window* ShellDelegate::CreateBrowserForTabDrop(
    aura::Window* source_window,
    const ui::OSExchangeData& drop_data) {
  return nullptr;
}

media_session::mojom::MediaSessionService*
ShellDelegate::GetMediaSessionService() {
  return nullptr;
}

}  // namespace ash
