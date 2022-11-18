// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_EVENT_REWRITER_CONTROLLER_IMPL_H_
#define ASH_EVENTS_EVENT_REWRITER_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "ui/aura/env_observer.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"

namespace ui {
class EventRewriter;
}  // namespace ui

namespace ash {

class AccessibilityEventRewriter;
class KeyboardDrivenEventRewriter;

// Owns ui::EventRewriters and ensures that they are added to each root window
// EventSource, current and future, in the order that they are added to this.
class ASH_EXPORT EventRewriterControllerImpl : public EventRewriterController,
                                               public aura::EnvObserver {
 public:
  EventRewriterControllerImpl();

  EventRewriterControllerImpl(const EventRewriterControllerImpl&) = delete;
  EventRewriterControllerImpl& operator=(const EventRewriterControllerImpl&) =
      delete;

  ~EventRewriterControllerImpl() override;

  // EventRewriterController:
  void Initialize(ui::EventRewriterChromeOS::Delegate* event_rewriter_delegate,
                  AccessibilityEventRewriterDelegate*
                      accessibility_event_rewriter_delegate) override;
  void AddEventRewriter(std::unique_ptr<ui::EventRewriter> rewriter) override;
  void SetKeyboardDrivenEventRewriterEnabled(bool enabled) override;
  void SetArrowToTabRewritingEnabled(bool enabled) override;
  void OnUnhandledSpokenFeedbackEvent(
      std::unique_ptr<ui::Event> event) override;
  void CaptureAllKeysForSpokenFeedback(bool capture) override;
  void SetSendMouseEvents(bool value) override;

  // aura::EnvObserver:
  void OnHostInitialized(aura::WindowTreeHost* host) override;

  // Enable/disable the combination of alt + other key or mouse event
  // mapping in EventRewriterChromeOS.
  void SetAltDownRemappingEnabled(bool enabled);

  ui::EventRewriterChromeOS::Delegate* event_rewriter_chromeos_delegate() {
    return event_rewriter_chromeos_delegate_;
  }

 private:
  // The |EventRewriter|s managed by this controller.
  std::vector<std::unique_ptr<ui::EventRewriter>> rewriters_;

  // Owned by |rewriters_|.
  AccessibilityEventRewriter* accessibility_event_rewriter_ = nullptr;
  KeyboardDrivenEventRewriter* keyboard_driven_event_rewriter_ = nullptr;
  ui::EventRewriterChromeOS* event_rewriter_chromeos_ = nullptr;
  ui::EventRewriterChromeOS::Delegate* event_rewriter_chromeos_delegate_ =
      nullptr;
};

}  // namespace ash

#endif  // ASH_EVENTS_EVENT_REWRITER_CONTROLLER_IMPL_H_
