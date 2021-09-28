// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_util.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/ui/base/window_properties.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ash/public/cpp/assistant/assistant_state.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/ui/ash/capture_mode/chrome_capture_mode_delegate.h"
#else
#include "ui/platform_window/extensions/pinned_mode_extension.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#endif

namespace {

// This is the common code for either of the two SetLockedFullscreenState calls.
// It will make sure that all the non browser related tasks will be turned off/
// cleared when the lock mode starts (or ends).
void SetLockedFullscreenStateCommon(Browser* browser,
                                    aura::Window* window,
                                    Profile* const profile,
                                    chromeos::WindowPinType new_type) {
  DCHECK(window);
  const chromeos::WindowPinType previous_type =
      window->GetProperty(chromeos::kWindowPinTypeKey);
  // As this gets triggered from extensions, we might encounter this case.
  if (previous_type == new_type)
    return;

  window->SetProperty(chromeos::kWindowPinTypeKey, new_type);

  if (browser) {
    // This should only be called when called from inside the browser and not
    // when called through Exo.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto* pinned_mode_extension =
        views::DesktopWindowTreeHostLinux::From(window->GetHost())
            ->GetPinnedModeExtension();
    if (new_type != chromeos::WindowPinType::kNone) {
      pinned_mode_extension->Pin(/*trusted=*/
                                 new_type ==
                                 chromeos::WindowPinType::kTrustedPinned);
    } else {
      pinned_mode_extension->Unpin();
    }
#endif

    // Update the set of available browser commands.
    browser->command_controller()->LockedFullscreenStateChanged();
  }

  const bool locked = new_type == chromeos::WindowPinType::kTrustedPinned;
  const bool previous_locked =
      previous_type == chromeos::WindowPinType::kTrustedPinned;
  // Make sure that we only proceed if we switch from locked to unlocked or
  // vice versa.
  if (locked == previous_locked)
    return;

  // Reset the clipboard and kill dev tools when entering or exiting locked
  // fullscreen (security concerns).
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  content::DevToolsAgentHost::DetachAllClients();

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug/1243104): This might be interesting for DLP to change.
  // Disable both screenshots and video screen captures via the capture mode
  // feature.
  ChromeCaptureModeDelegate::Get()->SetIsScreenCaptureLocked(locked);

  // Disable ARC while in the locked fullscreen mode.
  arc::ArcSessionManager* const arc_session_manager =
      arc::ArcSessionManager::Get();
  if (arc_session_manager && arc::IsArcAllowedForProfile(profile)) {
    if (locked) {
      // Disable ARC, preserve data.
      arc_session_manager->RequestDisable();
    } else {
      // Re-enable ARC if needed.
      if (arc::IsArcPlayStoreEnabledForProfile(profile))
        arc_session_manager->RequestEnable();
    }
  }

  if (assistant::IsAssistantAllowedForProfile(profile) ==
      chromeos::assistant::AssistantAllowedState::ALLOWED) {
    ash::AssistantState::Get()->NotifyLockedFullScreenStateChanged(locked);
  }
#endif
}

}  // namespace

namespace extensions {
namespace tabs_util {

void SetLockedFullscreenStateFromExo(aura::Window* window,
                                     chromeos::WindowPinType type) {
  SetLockedFullscreenStateCommon(nullptr, window,
                                 ProfileManager::GetPrimaryUserProfile(), type);
}

void SetLockedFullscreenState(Browser* browser, chromeos::WindowPinType type) {
  SetLockedFullscreenStateCommon(browser, browser->window()->GetNativeWindow(),
                                 browser->profile(), type);
}

bool IsScreenshotRestricted(content::WebContents* web_contents) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug/1243104): This might be interesting for DLP to change.
  return false;
#else
  return policy::DlpContentManager::Get()->IsScreenshotApiRestricted(
      ScreenshotArea::CreateForWindow(web_contents->GetNativeView()));
#endif
}

}  // namespace tabs_util
}  // namespace extensions
