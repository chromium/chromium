// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EVENT_REWRITER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EVENT_REWRITER_DELEGATE_IMPL_H_

#include <memory>

#include "ash/public/cpp/accessibility_event_rewriter_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {
enum class MagnifierCommand;
enum class SwitchAccessCommand;

// Passes key events from Ash's EventRewriter to accessibility component
// extension code. Used by ChromeVox and Switch Access. Reports ChromeVox's
// unhandled key events back to Ash for continued dispatch.
// TODO(http://crbug.com/839541): Avoid reposting unhandled events.
class AccessibilityEventRewriterDelegateImpl
    : public AccessibilityEventRewriterDelegate,
      public content::WebContentsDelegate {
 public:
  AccessibilityEventRewriterDelegateImpl();
  AccessibilityEventRewriterDelegateImpl(
      const AccessibilityEventRewriterDelegateImpl&) = delete;
  AccessibilityEventRewriterDelegateImpl& operator=(
      const AccessibilityEventRewriterDelegateImpl&) = delete;
  ~AccessibilityEventRewriterDelegateImpl() override;

  // AccessibilityEventRewriterDelegate:
  void DispatchKeyEventToChromeVox(std::unique_ptr<ui::Event> event,
                                   bool capture) override;
  void DispatchMouseEvent(std::unique_ptr<ui::Event> event) override;
  void SendSwitchAccessCommand(SwitchAccessCommand command) override;
  void SendPointScanPoint(const gfx::PointF& point) override;
  void SendMagnifierCommand(MagnifierCommand command) override;

 private:
  // Reports unhandled key events to the EventRewriterController for dispatch.
  void OnUnhandledSpokenFeedbackEvent(std::unique_ptr<ui::Event> event) const;

  // WebContentsDelegate:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EVENT_REWRITER_DELEGATE_IMPL_H_
