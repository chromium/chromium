// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_ANDROID_TTC_SHORTCUT_HANDLER_ANDROID_H_
#define CHROME_BROWSER_TTC_ANDROID_TTC_SHORTCUT_HANDLER_ANDROID_H_

#include <jni.h>

#include <memory>
#include <vector>

#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

class PrefService;

namespace ttc {

// Handles TTC's keyboard shortcuts in one window: the keyboard shortcut, which
// starts or ends a session, and the hold to speak shortcut, which TTC listens
// to while it is held down. The shortcuts come from the ttc.keyboard_shortcut
// and ttc.hold_to_speak_shortcut prefs, and are registered with the window's
// ui::AcceleratorManagerAndroid, like other customizable shortcuts on Android.
// Presses and releases are reported to the Java TtcShortcutHandler.
class TtcShortcutHandlerAndroid : public ui::AcceleratorTarget {
 public:
  // Receives the shortcuts' presses and releases. Outside of tests, this is the
  // Java TtcShortcutHandler.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // The keyboard shortcut was pressed.
    virtual void OnKeyboardShortcut() = 0;
    // The hold to speak shortcut was pressed down.
    virtual void OnHoldToSpeakPressed() = 0;
    // The hold to speak shortcut was released, or stopped being the shortcut.
    virtual void OnHoldToSpeakReleased() = 0;
  };

  // Where the shortcuts are registered. Outside of tests, this is the window's
  // ui::AcceleratorManagerAndroid.
  class AcceleratorRegistry {
   public:
    virtual ~AcceleratorRegistry() = default;

    // Returns whether `accelerator` was registered for `target`.
    virtual bool Register(const ui::Accelerator& accelerator,
                          ui::AcceleratorTarget* target) = 0;
    virtual void Unregister(const ui::Accelerator& accelerator,
                            ui::AcceleratorTarget* target) = 0;
  };

  TtcShortcutHandlerAndroid(std::unique_ptr<Delegate> delegate,
                            std::unique_ptr<AcceleratorRegistry> registry,
                            PrefService* prefs);
  TtcShortcutHandlerAndroid(const TtcShortcutHandlerAndroid&) = delete;
  TtcShortcutHandlerAndroid& operator=(const TtcShortcutHandlerAndroid&) =
      delete;
  ~TtcShortcutHandlerAndroid() override;

  // Called by Java when the handler is no longer needed.
  void Destroy(JNIEnv* env);

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  // Registers the shortcuts the prefs currently hold, replacing any that were
  // registered before.
  void UpdateRegistrations();
  void Register(const ui::Accelerator& accelerator);
  void UnregisterAll();

  // While the hold to speak shortcut is held down, its key's release is
  // registered too, whichever of its modifiers are still held by then.
  void StartHolding();
  void StopHolding();

  const std::unique_ptr<Delegate> delegate_;
  const std::unique_ptr<AcceleratorRegistry> registry_;
  PrefChangeRegistrar pref_registrar_;

  ui::Accelerator keyboard_shortcut_;
  ui::Accelerator hold_to_speak_shortcut_;
  std::vector<ui::Accelerator> registered_;
  bool holding_ = false;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_ANDROID_TTC_SHORTCUT_HANDLER_ANDROID_H_
