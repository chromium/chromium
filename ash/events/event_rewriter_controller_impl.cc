// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/event_rewriter_controller_impl.h"

#include <utility>

#include "ash/accessibility/disable_trackpad_event_rewriter.h"
#include "ash/accessibility/filter_keys_event_rewriter.h"
#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/events/accessibility_event_rewriter.h"
#include "ash/events/keyboard_driven_event_rewriter.h"
#include "ash/events/peripheral_customization_event_rewriter.h"
#include "ash/events/prerewritten_event_forwarder.h"
#include "ash/public/cpp/accessibility_event_rewriter_delegate.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "base/command_line.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/caps_lock_event_rewriter.h"
#include "ui/events/ash/discard_key_event_rewriter.h"
#include "ui/events/ash/keyboard_device_id_event_rewriter.h"
#include "ui/events/ash/keyboard_modifier_event_rewriter.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_source.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

namespace ash {
namespace {

class KeyboardModifierEventRewriterDelegateImpl
    : public ui::KeyboardModifierEventRewriter::Delegate {
 public:
  explicit KeyboardModifierEventRewriterDelegateImpl(
      ui::EventRewriterAsh::Delegate* event_rewriter_delegate)
      : event_rewriter_delegate_(event_rewriter_delegate) {}

  std::optional<ui::mojom::ModifierKey> GetKeyboardRemappedModifierValue(
      int device_id,
      ui::mojom::ModifierKey modifier_key,
      const std::string& pref_name) const override {
    return event_rewriter_delegate_->GetKeyboardRemappedModifierValue(
        device_id, modifier_key, pref_name);
  }

  bool RewriteModifierKeys() override {
    return event_rewriter_delegate_->RewriteModifierKeys();
  }

 private:
  raw_ptr<ui::EventRewriterAsh::Delegate> event_rewriter_delegate_;
};

}  // namespace

// static
EventRewriterController* EventRewriterController::Get() {
  return Shell::HasInstance() ? Shell::Get()->event_rewriter_controller()
                              : nullptr;
}

EventRewriterControllerImpl::EventRewriterControllerImpl() {
  // Add the controller as an observer for new root windows.
  aura::Env::GetInstance()->AddObserver(this);
}

EventRewriterControllerImpl::~EventRewriterControllerImpl() {
  aura::Env::GetInstance()->RemoveObserver(this);
  // Remove the rewriters from every root window EventSource.
  for (aura::Window* window : Shell::GetAllRootWindows()) {
    auto* event_source = window->GetHost()->GetEventSource();
    for (const auto& rewriter : rewriters_) {
      event_source->RemoveEventRewriter(rewriter.get());
    }
  }
}

void EventRewriterControllerImpl::Initialize(
    ui::EventRewriterAsh::Delegate* event_rewriter_delegate,
    AccessibilityEventRewriterDelegate* accessibility_event_rewriter_delegate) {
  std::unique_ptr<KeyboardDrivenEventRewriter> keyboard_driven_event_rewriter =
      std::make_unique<KeyboardDrivenEventRewriter>();
  keyboard_driven_event_rewriter_ = keyboard_driven_event_rewriter.get();

  bool privacy_screen_supported = false;
  if (Shell::Get()->privacy_screen_controller() &&
      Shell::Get()->privacy_screen_controller()->IsSupported()) {
    privacy_screen_supported = true;
  }

  auto keyboard_device_id_event_rewriter =
      std::make_unique<ui::KeyboardDeviceIdEventRewriter>(
          Shell::Get()->keyboard_capability());

  event_rewriter_ash_delegate_ = event_rewriter_delegate;
  std::unique_ptr<ui::EventRewriterAsh> event_rewriter_ash =
      std::make_unique<ui::EventRewriterAsh>(
          event_rewriter_delegate, Shell::Get()->keyboard_capability(),
          Shell::Get()->sticky_keys_controller(), privacy_screen_supported);
  event_rewriter_ash_ = event_rewriter_ash.get();

  std::unique_ptr<PeripheralCustomizationEventRewriter>
      peripheral_customization_event_rewriter;
  if (features::IsPeripheralCustomizationEnabled() ||
      ::features::IsShortcutCustomizationEnabled()) {
    peripheral_customization_event_rewriter =
        std::make_unique<PeripheralCustomizationEventRewriter>(
            Shell::Get()->input_device_settings_controller());
    peripheral_customization_event_rewriter_ =
        peripheral_customization_event_rewriter.get();
  }

  std::unique_ptr<PrerewrittenEventForwarder> prerewritten_event_forwarder =
      std::make_unique<PrerewrittenEventForwarder>();
  prerewritten_event_forwarder_ = prerewritten_event_forwarder.get();

  std::unique_ptr<AccessibilityEventRewriter> accessibility_event_rewriter =
      std::make_unique<AccessibilityEventRewriter>(
          event_rewriter_ash.get(), accessibility_event_rewriter_delegate);
  accessibility_event_rewriter_ = accessibility_event_rewriter.get();

  // EventRewriters are notified in the order they are added.
  if (::features::IsAccessibilityDisableTrackpadEnabled()) {
    std::unique_ptr<DisableTrackpadEventRewriter>
        disable_trackpad_event_rewriter =
            std::make_unique<DisableTrackpadEventRewriter>();
    disable_trackpad_event_rewriter_ = disable_trackpad_event_rewriter.get();
    // The DisableTrackpadEventRewriter needs to be notified first, as it
    // should stop all trackpad events from propagating further into the system.
    AddEventRewriter(std::move(disable_trackpad_event_rewriter));
  }
  if (::features::IsAccessibilityFilterKeysEnabled()) {
    std::unique_ptr<FilterKeysEventRewriter> filter_keys_event_rewriter =
        std::make_unique<FilterKeysEventRewriter>();
    filter_keys_event_rewriter_ = filter_keys_event_rewriter.get();
    // The FilterKeysEventRewriter needs to be notified before any other
    // rewriters that modify key events, as it should delay or cancel all key
    // events from propagating further into the system.
    AddEventRewriter(std::move(filter_keys_event_rewriter));
  }
  AddEventRewriter(std::move(keyboard_device_id_event_rewriter));
  if (features::IsKeyboardRewriterFixEnabled()) {
    auto keyboard_modifier_event_rewriter =
        std::make_unique<ui::KeyboardModifierEventRewriter>(
            std::make_unique<KeyboardModifierEventRewriterDelegateImpl>(
                event_rewriter_delegate),
            ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine(),
            Shell::Get()->keyboard_capability(),
            ash::input_method::InputMethodManager::Get()->GetImeKeyboard());
    AddEventRewriter(std::move(keyboard_modifier_event_rewriter));
  }
  // CapsLock event rewriter must come after modifier rewriting as it can effect
  // its result. This means with the rewritter fix enabled, it must come before
  // EventRewriterAsh, but with the fix disabled, it must come after.
  if (features::IsKeyboardRewriterFixEnabled() &&
      features::IsModifierSplitEnabled()) {
    AddEventRewriter(std::make_unique<ui::CapsLockEventRewriter>(
        ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine(),
        Shell::Get()->keyboard_capability(),
        ash::input_method::InputMethodManager::Get()->GetImeKeyboard()));
  }
  if (features::IsPeripheralCustomizationEnabled() ||
      ::features::IsShortcutCustomizationEnabled()) {
    AddEventRewriter(std::move(peripheral_customization_event_rewriter));
  }
  AddEventRewriter(std::move(prerewritten_event_forwarder));
  // Accessibility rewriter is applied between modifier event rewriters and
  // EventRewriterAsh. Specifically, Search modifier is captured by the
  // accessibility rewriter, that should be the ones after modifier remapping.
  // However, accessibility rewriter wants to capture it before it is rewritten
  // into 6-pack keys, which is done in EventRewriterAsh.
  AddEventRewriter(std::move(accessibility_event_rewriter));
  AddEventRewriter(std::move(keyboard_driven_event_rewriter));
  AddEventRewriter(std::move(event_rewriter_ash));
  if (!features::IsKeyboardRewriterFixEnabled() &&
      features::IsModifierSplitEnabled()) {
    AddEventRewriter(std::make_unique<ui::CapsLockEventRewriter>(
        ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine(),
        Shell::Get()->keyboard_capability(),
        ash::input_method::InputMethodManager::Get()->GetImeKeyboard()));
  }
  if (features::IsModifierSplitEnabled()) {
    AddEventRewriter(std::make_unique<ui::DiscardKeyEventRewriter>());
  }
}

void EventRewriterControllerImpl::AddEventRewriter(
    std::unique_ptr<ui::EventRewriter> rewriter) {
  // Add the rewriters to each existing root window EventSource.
  for (aura::Window* window : Shell::GetAllRootWindows()) {
    window->GetHost()->GetEventSource()->AddEventRewriter(rewriter.get());
  }

  // In case there are any mirroring displays, their hosts' EventSources won't
  // be included above.
  const auto* mirror_window_controller =
      Shell::Get()->window_tree_host_manager()->mirror_window_controller();
  for (aura::Window* window : mirror_window_controller->GetAllRootWindows()) {
    window->GetHost()->GetEventSource()->AddEventRewriter(rewriter.get());
  }

  rewriters_.push_back(std::move(rewriter));
}

void EventRewriterControllerImpl::SetKeyboardDrivenEventRewriterEnabled(
    bool enabled) {
  keyboard_driven_event_rewriter_->set_enabled(enabled);
}

void EventRewriterControllerImpl::SetArrowToTabRewritingEnabled(bool enabled) {
  keyboard_driven_event_rewriter_->set_arrow_to_tab_rewriting_enabled(enabled);
}

void EventRewriterControllerImpl::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) {
  accessibility_event_rewriter_->OnUnhandledSpokenFeedbackEvent(
      std::move(event));
}

void EventRewriterControllerImpl::CaptureAllKeysForSpokenFeedback(
    bool capture) {
  accessibility_event_rewriter_->set_chromevox_capture_all_keys(capture);
}

void EventRewriterControllerImpl::SetSendMouseEvents(bool value) {
  accessibility_event_rewriter_->set_send_mouse_events(value);
}

void EventRewriterControllerImpl::SetAltDownRemappingEnabled(bool enabled) {
  if (event_rewriter_ash_) {
    event_rewriter_ash_->set_alt_down_remapping_enabled(enabled);
  }
}

void EventRewriterControllerImpl::OnHostInitialized(
    aura::WindowTreeHost* host) {
  for (const auto& rewriter : rewriters_)
    host->GetEventSource()->AddEventRewriter(rewriter.get());
}

}  // namespace ash
