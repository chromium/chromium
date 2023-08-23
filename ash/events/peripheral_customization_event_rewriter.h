// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_PERIPHERAL_CUSTOMIZATION_EVENT_REWRITER_H_
#define ASH_EVENTS_PERIPHERAL_CUSTOMIZATION_EVENT_REWRITER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_rewriter.h"

namespace ash {

// PeripheralCustomizationEventRewriter recognizes and rewrites events from mice
// and graphics tablets to arbitrary `ui::KeyEvent`s configured by the user via
// the Settings SWA.
class ASH_EXPORT PeripheralCustomizationEventRewriter
    : public ui::EventRewriter {
 public:
  using ButtonRemappingList =
      std::vector<std::pair<mojom::ButtonPtr, mojom::RemappingActionPtr>>;

  enum class DeviceType { kMouse, kGraphicsTablet };

  class Observer : public base::CheckedObserver {
   public:
    // Called when a mouse that is currently being observed presses a button
    // that is remappable on mice.
    virtual void OnMouseButtonPressed(int device_id,
                                      const mojom::Button& button) = 0;

    // Called when a graphics tablet that is currently being observed presses a
    // button that is remappable on graphics tablets.
    virtual void OnGraphicsTabletButtonPressed(int device_id,
                                               const mojom::Button& button) = 0;
  };

  PeripheralCustomizationEventRewriter();
  PeripheralCustomizationEventRewriter(
      const PeripheralCustomizationEventRewriter&) = delete;
  PeripheralCustomizationEventRewriter& operator=(
      const PeripheralCustomizationEventRewriter&) = delete;
  ~PeripheralCustomizationEventRewriter() override;

  // Starts observing and blocking mouse events for `device_id`. Notifies
  // observers via `OnMouseButtonPressed` whenever an event
  void StartObservingMouse(int device_id);

  // Starts observing and blocking graphics tablet events for `device_id`.
  // Notifies observers via `OnGraphicsTabletButtonPressed` whenever an event is
  // received.
  void StartObservingGraphicsTablet(int device_id);

  // Stops observing for all devices of every type.
  void StopObserving();

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // This is only for testing and will be removed once the controller properly
  // sends button remapping data to the rewriter.
  // TODO(dpad): Remove this function once button remapping data can be received
  // from the settings controller.
  void SetRemappingActionForTesting(int device_id,
                                    mojom::ButtonPtr button,
                                    mojom::RemappingActionPtr remapping_action);

 private:
  // Notifies observers if the given `mouse_event` is a remappable button for
  // the given `device_type`. Returns true if the event should be discarded.
  bool NotifyMouseEventObserving(const ui::MouseEvent& mouse_event,
                                 DeviceType device_type);
  // Notifies observers if the given `key_event` is a remappable button for
  // the given `device_type`. Returns true if the event should be discarded.
  bool NotifyKeyEventObserving(const ui::KeyEvent& key_event,
                               DeviceType device_type);

  // Rewrites the given event that came from `button` within the
  // `rewritten_event` param. Returns true if the original event should be
  // discarded.
  bool RewriteEventFromButton(const ui::Event& event,
                              const mojom::Button& button,
                              std::unique_ptr<ui::Event>& rewritten_event);

  ui::EventDispatchDetails RewriteMouseEvent(const ui::MouseEvent& mouse_event,
                                             const Continuation continuation);
  ui::EventDispatchDetails RewriteKeyEvent(const ui::KeyEvent& key_event,
                                           const Continuation continuation);

  absl::optional<DeviceType> GetDeviceTypeToObserve(int device_id);

  const mojom::RemappingAction* GetRemappingAction(int device_id,
                                                   const mojom::Button& button);

  // Removes the set of remapped modifiers from the event that should be
  // discarded.
  void RemoveRemappedModifiers(ui::Event& event);

  base::flat_set<int> mice_to_observe_;
  base::flat_set<int> graphics_tablets_to_observe_;
  base::ObserverList<Observer> observers_;

  // TODO(dpad): Remove once `InputDeviceSettingsController` is updated to
  // handle button remappings.
  base::flat_map<int, ButtonRemappingList> button_remappings_for_testing_;
};

}  // namespace ash

#endif  // ASH_EVENTS_PERIPHERAL_CUSTOMIZATION_EVENT_REWRITER_H_
