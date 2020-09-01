// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_EVENT_REWRITER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_EVENT_REWRITER_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/accessibility_event_rewriter_delegate.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {
enum class SwitchAccessCommand;
}

// Passes key events from Ash's EventRewriter to accessibility component
// extension code. Used by ChromeVox and Switch Access. Reports ChromeVox's
// unhandled key events back to Ash for continued dispatch.
// TODO(http://crbug.com/839541): Avoid reposting unhandled events.
class AccessibilityEventRewriterDelegate
    : public ash::AccessibilityEventRewriterDelegate,
      public content::WebContentsDelegate,
      public wm::ActivationChangeObserver {
 public:
  AccessibilityEventRewriterDelegate();
  AccessibilityEventRewriterDelegate(
      const AccessibilityEventRewriterDelegate&) = delete;
  AccessibilityEventRewriterDelegate& operator=(
      const AccessibilityEventRewriterDelegate&) = delete;
  ~AccessibilityEventRewriterDelegate() override;

  // ash::AccessibilityEventRewriterDelegate:
  void DispatchKeyEventToChromeVox(std::unique_ptr<ui::Event> event,
                                   bool capture) override;
  void DispatchMouseEventToChromeVox(std::unique_ptr<ui::Event> event) override;
  void SendSwitchAccessCommand(ash::SwitchAccessCommand command) override;

 private:
  // Reports unhandled key events to the EventRewriterController for dispatch.
  void OnUnhandledSpokenFeedbackEvent(std::unique_ptr<ui::Event> event) const;

  // WebContentsDelegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  // wm::ActivationChangeObserver overrides.
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  bool is_arc_window_active_ = false;
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_EVENT_REWRITER_DELEGATE_H_
