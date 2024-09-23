// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_EVENT_REWRITER_CONTROLLER_IMPL_H_
#define ASH_EVENTS_EVENT_REWRITER_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/events/peripheral_customization_event_rewriter.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/env_observer.h"
#include "ui/events/ash/event_rewriter_ash.h"

namespace ui {
class EventRewriter;
}  // namespace ui

namespace ash {

class AccessibilityEventRewriter;
class DisableTrackpadEventRewriter;
class FilterKeysEventRewriter;
class KeyboardDrivenEventRewriter;
class PrerewrittenEventForwarder;

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
  void Initialize(ui::EventRewriterAsh::Delegate* event_rewriter_delegate,
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
  // mapping in EventRewriterAsh.
  void SetAltDownRemappingEnabled(bool enabled);

  ui::EventRewriterAsh::Delegate* event_rewriter_ash_delegate() {
    return event_rewriter_ash_delegate_;
  }

  PeripheralCustomizationEventRewriter*
  peripheral_customization_event_rewriter() {
    return peripheral_customization_event_rewriter_;
  }

  PrerewrittenEventForwarder* prerewritten_event_forwarder() {
    return prerewritten_event_forwarder_;
  }

 private:
  // The |EventRewriter|s managed by this controller.
  std::vector<std::unique_ptr<ui::EventRewriter>> rewriters_;

  // Owned by |rewriters_|.
  raw_ptr<AccessibilityEventRewriter> accessibility_event_rewriter_ = nullptr;
  raw_ptr<DisableTrackpadEventRewriter> disable_trackpad_event_rewriter_ =
      nullptr;
  raw_ptr<FilterKeysEventRewriter> filter_keys_event_rewriter_ = nullptr;
  raw_ptr<PeripheralCustomizationEventRewriter>
      peripheral_customization_event_rewriter_ = nullptr;
  raw_ptr<PrerewrittenEventForwarder> prerewritten_event_forwarder_ = nullptr;
  raw_ptr<KeyboardDrivenEventRewriter> keyboard_driven_event_rewriter_ =
      nullptr;
  raw_ptr<ui::EventRewriterAsh> event_rewriter_ash_ = nullptr;
  raw_ptr<ui::EventRewriterAsh::Delegate> event_rewriter_ash_delegate_ =
      nullptr;
};

}  // namespace ash

#endif  // ASH_EVENTS_EVENT_REWRITER_CONTROLLER_IMPL_H_
