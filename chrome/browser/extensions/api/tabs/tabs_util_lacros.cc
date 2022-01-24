// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_util.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/lacros/window_properties.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/platform_window/extensions/pinned_mode_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace extensions {
namespace tabs_util {

void SetLockedFullscreenState(Browser* browser, bool pinned) {
  aura::Window* window = browser->window()->GetNativeWindow();
  DCHECK(window);

  const chromeos::WindowPinType previous_type =
      window->GetProperty(lacros::kWindowPinTypeKey);
  DCHECK_NE(previous_type, chromeos::WindowPinType::kTrustedPinned)
      << "Extensions only set Trusted Pinned";

  bool previous_pinned =
      previous_type == chromeos::WindowPinType::kTrustedPinned;
  // As this gets triggered from extensions, we might encounter this case.
  if (previous_pinned == pinned)
    return;

  window->SetProperty(lacros::kWindowPinTypeKey,
                      pinned ? chromeos::WindowPinType::kTrustedPinned
                             : chromeos::WindowPinType::kNone);

  auto* pinned_mode_extension =
      views::DesktopWindowTreeHostLinux::From(window->GetHost())
          ->GetPinnedModeExtension();
  if (pinned) {
    pinned_mode_extension->Pin(/*trusted=*/true);
  } else {
    pinned_mode_extension->Unpin();
  }

  // Update the set of available browser commands.
  browser->command_controller()->LockedFullscreenStateChanged();

  // Wipe the clipboard in browser and detach any dev tools.
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  content::DevToolsAgentHost::DetachAllClients();
}

bool IsScreenshotRestricted(content::WebContents* web_contents) {
  return false;
}

}  // namespace tabs_util
}  // namespace extensions
