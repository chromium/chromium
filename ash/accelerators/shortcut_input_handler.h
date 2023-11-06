// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_SHORTCUT_INPUT_HANDLER_H_
#define ASH_ACCELERATORS_SHORTCUT_INPUT_HANDLER_H_

#include "ash/ash_export.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "base/observer_list.h"
#include "ui/events/event_handler.h"

namespace ui {
class KeyEvent;
}  // namespace ui

namespace ash {

// `ShortcutInputHandler` intercepts events and forwards them to other
// components for the purposes of inputting shortcuts in ChromeOS. Utilized in
// the Peripheral Customization and Shortcut Customization features. This class
// is intended to be a singleton and used by both apps through the same
// instance.
class ASH_EXPORT ShortcutInputHandler : public ui::EventHandler {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever a key event is pressed by the user when the
    // `ShortcutInputHandler` is observing.
    virtual void OnShortcutInputEventPressed(
        const mojom::KeyEvent& key_event) = 0;

    // Called whenever a key event is released by the user when the
    // `ShortcutInputHandler` is observing.
    virtual void OnShortcutInputEventReleased(
        const mojom::KeyEvent& key_event) = 0;
  };

  ShortcutInputHandler();
  ~ShortcutInputHandler() override;
  ShortcutInputHandler(const ShortcutInputHandler&) = delete;
  ShortcutInputHandler& operator=(const ShortcutInputHandler&) = delete;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_SHORTCUT_INPUT_HANDLER_H_
