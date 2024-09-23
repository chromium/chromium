// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_PERIPHERAL_CUSTOMIZATION_EVENT_REWRITER_H_
#define ASH_EVENTS_PERIPHERAL_CUSTOMIZATION_EVENT_REWRITER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_rewriter.h"

namespace ash {

class InputDeviceSettingsController;

// PeripheralCustomizationEventRewriter recognizes and rewrites events from mice
// and graphics tablets to arbitrary `ui::KeyEvent`s configured by the user via
// the Settings SWA.
class ASH_EXPORT PeripheralCustomizationEventRewriter
    : public ui::EventRewriter {
 public:
  using ButtonRemappingList =
      std::vector<std::pair<mojom::ButtonPtr, mojom::RemappingActionPtr>>;

  enum class DeviceType { kMouse, kGraphicsTablet };

  struct DeviceIdButton {
    int device_id;
    mojom::ButtonPtr button;

    DeviceIdButton(int device_id, mojom::ButtonPtr button);
    DeviceIdButton(DeviceIdButton&& device_id_button);
    ~DeviceIdButton();

    DeviceIdButton& operator=(DeviceIdButton&& device_id_button);
    friend bool operator<(const DeviceIdButton& left,
                          const DeviceIdButton& right);
  };

  struct RemappingActionResult {
    raw_ref<mojom::RemappingAction> remapping_action;
    InputDeviceSettingsMetricsManager::PeripheralCustomizationMetricsType
        peripheral_kind;

    RemappingActionResult(
        mojom::RemappingAction& remapping_action,
        InputDeviceSettingsMetricsManager::PeripheralCustomizationMetricsType
            peripheral_kind);
    RemappingActionResult(RemappingActionResult&& result);
    ~RemappingActionResult();
  };

  explicit PeripheralCustomizationEventRewriter(
      InputDeviceSettingsController* input_device_settings_controller);
  PeripheralCustomizationEventRewriter(
      const PeripheralCustomizationEventRewriter&) = delete;
  PeripheralCustomizationEventRewriter& operator=(
      const PeripheralCustomizationEventRewriter&) = delete;
  ~PeripheralCustomizationEventRewriter() override;

  // Starts observing and blocking mouse events for `device_id`. Notifies
  // observers via `OnMouseButtonPressed` whenever an event
  void StartObservingMouse(
      int device_id,
      mojom::CustomizationRestriction customization_restriction);

  // Starts observing and blocking graphics tablet events for `device_id`.
  // Notifies observers via `OnGraphicsTabletButtonPressed` whenever an event is
  // received.
  void StartObservingGraphicsTablet(
      int device_id,
      mojom::CustomizationRestriction customization_restriction);

  // Stops observing for all devices of every type.
  void StopObserving();

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  const base::flat_map<int, mojom::CustomizationRestriction>&
  mice_to_observe() {
    return mice_to_observe_;
  }
  const base::flat_map<int, mojom::CustomizationRestriction>&
  graphics_tablets_to_observe() {
    return graphics_tablets_to_observe_;
  }

 private:
  // Notifies observers if the given `mouse_event` is a remappable button for
  // the given `device_type`. Returns true if the event should be discarded.
  bool NotifyMouseEventObserving(const ui::MouseEvent& mouse_event,
                                 DeviceType device_type);
  // Notifies observers if the given `mouse_wheel_event` is a remappable button
  // for the given `device_type`. Returns true if the event should be discarded.
  bool NotifyMouseWheelEventObserving(
      const ui::MouseWheelEvent& mouse_wheel_event,
      DeviceType device_type);
  // Notifies observers if the given `key_event` is a remappable button for
  // the given `device_type`. Returns true if the event should be discarded.
  bool NotifyKeyEventObserving(const ui::KeyEvent& key_event,
                               DeviceType device_type);

  // Returns if the button is customizable.
  bool IsButtonCustomizable(const ui::KeyEvent& key_event);

  // Rewrites the given event that came from `button` within the
  // `rewritten_event` param. Returns true if the original event should be
  // discarded.
  bool RewriteEventFromButton(
      const ui::Event& event,
      const mojom::Button& button,
      std::vector<std::unique_ptr<ui::Event>>& rewritten_event);

  ui::EventDispatchDetails RewriteMouseEvent(const ui::MouseEvent& mouse_event,
                                             const Continuation continuation);
  ui::EventDispatchDetails RewriteMouseWheelEvent(
      const ui::MouseWheelEvent& mouse_event,
      const Continuation continuation);
  ui::EventDispatchDetails RewriteKeyEvent(const ui::KeyEvent& key_event,
                                           const Continuation continuation);

  std::optional<DeviceType> GetDeviceTypeToObserve(int device_id);

  std::optional<RemappingActionResult> GetRemappingAction(
      int device_id,
      const mojom::Button& button);

  void UpdatePressedButtonMap(
      mojom::ButtonPtr button,
      const ui::Event& original_event,
      const std::vector<std::unique_ptr<ui::Event>>& rewritten_event);
  void UpdatePressedButtonMapFlags(const ui::KeyEvent& key_event);

  // Removes the set of remapped modifiers from the event that should be
  // discarded.
  void RemoveRemappedModifiers(ui::Event& event);

  // Applies all remapped modifiers.
  void ApplyRemappedModifiers(ui::Event& event);

  std::unique_ptr<ui::Event> CloneEvent(const ui::Event& event);

  base::flat_map<int, mojom::CustomizationRestriction> mice_to_observe_;
  base::flat_map<int, mojom::CustomizationRestriction>
      graphics_tablets_to_observe_;

  // Maintains a list of currently pressed buttons and the flags that should
  // be applied to other events processed.
  base::flat_map<DeviceIdButton, int> device_button_to_flags_;

  raw_ptr<InputDeviceSettingsController> input_device_settings_controller_;

  // Emit all metrics.
  std::unique_ptr<InputDeviceSettingsMetricsManager> metrics_manager_;
};

}  // namespace ash

#endif  // ASH_EVENTS_PERIPHERAL_CUSTOMIZATION_EVENT_REWRITER_H_
