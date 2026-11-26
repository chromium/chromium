// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/android/ttc_shortcut_handler_android.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/prefs.h"
#include "chrome/browser/ttc/core/shortcut.h"
#include "components/prefs/pref_service.h"
#include "ui/android/accelerator_manager_android.h"
#include "ui/android/window_android.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/events/event_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ttc/android/jni_headers/TtcShortcutHandler_jni.h"

using base::android::AttachCurrentThread;

namespace ttc {

namespace {

constexpr int kModifierMask =
    ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN;

// Reports the shortcuts to the Java TtcShortcutHandler.
class JavaDelegate : public TtcShortcutHandlerAndroid::Delegate {
 public:
  explicit JavaDelegate(const base::android::JavaRef<jobject>& java_obj)
      : java_obj_(java_obj) {}

  // TtcShortcutHandlerAndroid::Delegate:
  void OnKeyboardShortcut() override {
    Java_TtcShortcutHandler_onKeyboardShortcut(AttachCurrentThread(),
                                               java_obj_);
  }
  void OnHoldToSpeakPressed() override {
    Java_TtcShortcutHandler_onHoldToSpeakPressed(AttachCurrentThread(),
                                                 java_obj_);
  }
  void OnHoldToSpeakReleased() override {
    Java_TtcShortcutHandler_onHoldToSpeakReleased(AttachCurrentThread(),
                                                  java_obj_);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

// Registers the shortcuts with a window's ui::AcceleratorManagerAndroid.
class WindowAcceleratorRegistry
    : public TtcShortcutHandlerAndroid::AcceleratorRegistry {
 public:
  explicit WindowAcceleratorRegistry(ui::WindowAndroid* window)
      : window_(window) {
    CHECK(window_);
  }

  // TtcShortcutHandlerAndroid::AcceleratorRegistry:
  bool Register(const ui::Accelerator& accelerator,
                ui::AcceleratorTarget* target) override {
    ui::AcceleratorManagerAndroid* manager = GetAcceleratorManager();
    if (!manager) {
      return false;
    }
    manager->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::HandlerPriority::kNormalPriority,
        target);
    return true;
  }
  void Unregister(const ui::Accelerator& accelerator,
                  ui::AcceleratorTarget* target) override {
    ui::AcceleratorManagerAndroid* manager = GetAcceleratorManager();
    if (manager) {
      manager->UnregisterAccelerator(accelerator, target);
    }
  }

 private:
  ui::AcceleratorManagerAndroid* GetAcceleratorManager() {
    return ui::AcceleratorManagerAndroid::FromWindow(window_);
  }

  const raw_ptr<ui::WindowAndroid> window_;
};

}  // namespace

TtcShortcutHandlerAndroid::TtcShortcutHandlerAndroid(
    std::unique_ptr<Delegate> delegate,
    std::unique_ptr<AcceleratorRegistry> registry,
    PrefService* prefs)
    : delegate_(std::move(delegate)), registry_(std::move(registry)) {
  CHECK(delegate_);
  CHECK(registry_);
  CHECK(prefs);
  pref_registrar_.Init(prefs);
  // base::Unretained() is safe: `this` owns `pref_registrar_`.
  auto update = base::BindRepeating(
      &TtcShortcutHandlerAndroid::UpdateRegistrations, base::Unretained(this));
  pref_registrar_.Add(prefs::kTtcKeyboardShortcut, update);
  pref_registrar_.Add(prefs::kTtcHoldToSpeakShortcut, update);
  UpdateRegistrations();
}

TtcShortcutHandlerAndroid::~TtcShortcutHandlerAndroid() {
  UnregisterAll();
}

void TtcShortcutHandlerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

bool TtcShortcutHandlerAndroid::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // Key repeats never get here: Android's AcceleratorManager doesn't pass them
  // to native, so holding a shortcut down presses it once.
  if (accelerator.key_state() == ui::Accelerator::KeyState::RELEASED) {
    if (!holding_) {
      return false;
    }
    StopHolding();
    delegate_->OnHoldToSpeakReleased();
    return true;
  }
  if (accelerator == keyboard_shortcut_) {
    delegate_->OnKeyboardShortcut();
    return true;
  }
  if (accelerator == hold_to_speak_shortcut_) {
    if (!holding_) {
      StartHolding();
      delegate_->OnHoldToSpeakPressed();
    }
    return true;
  }
  return false;
}

bool TtcShortcutHandlerAndroid::CanHandleAccelerators() const {
  return true;
}

void TtcShortcutHandlerAndroid::UpdateRegistrations() {
  if (holding_) {
    // The hold ends with the shortcut it was started with.
    StopHolding();
    delegate_->OnHoldToSpeakReleased();
  }
  UnregisterAll();
  PrefService* prefs = pref_registrar_.prefs();
  keyboard_shortcut_ =
      ParseShortcut(prefs->GetString(prefs::kTtcKeyboardShortcut));
  hold_to_speak_shortcut_ =
      ParseShortcut(prefs->GetString(prefs::kTtcHoldToSpeakShortcut));
  if (hold_to_speak_shortcut_ == keyboard_shortcut_) {
    // The settings page doesn't save the same shortcut for both, but if the
    // prefs hold it anyway, it's the keyboard shortcut.
    hold_to_speak_shortcut_ = ui::Accelerator();
  }
  if (!keyboard_shortcut_.IsEmpty()) {
    Register(keyboard_shortcut_);
  }
  if (!hold_to_speak_shortcut_.IsEmpty()) {
    Register(hold_to_speak_shortcut_);
  }
}

void TtcShortcutHandlerAndroid::Register(const ui::Accelerator& accelerator) {
  if (registry_->Register(accelerator, this)) {
    registered_.push_back(accelerator);
  }
}

void TtcShortcutHandlerAndroid::UnregisterAll() {
  for (const ui::Accelerator& accelerator : registered_) {
    registry_->Unregister(accelerator, this);
  }
  registered_.clear();
}

void TtcShortcutHandlerAndroid::StartHolding() {
  holding_ = true;
  // The modifiers may be let go before the key, so the key's release is
  // registered with every combination of the shortcut's modifiers.
  //
  // If the window loses focus while the shortcut is held, e.g. to a dialog or
  // the microphone permission prompt, Android sends it a key-up flagged
  // KeyEvent.FLAG_CANCELED for each held key. AcceleratorManagerAndroid reports
  // that as a release too, so the hold ends when focus is lost.
  const int modifiers = hold_to_speak_shortcut_.modifiers() & kModifierMask;
  for (int subset = modifiers;; subset = (subset - 1) & modifiers) {
    Register(ui::Accelerator(hold_to_speak_shortcut_.key_code(), subset,
                             ui::Accelerator::KeyState::RELEASED));
    if (subset == 0) {
      break;
    }
  }
}

void TtcShortcutHandlerAndroid::StopHolding() {
  holding_ = false;
  std::erase_if(registered_, [&](const ui::Accelerator& accelerator) {
    if (accelerator.key_state() != ui::Accelerator::KeyState::RELEASED) {
      return false;
    }
    registry_->Unregister(accelerator, this);
    return true;
  });
}

static int64_t JNI_TtcShortcutHandler_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_obj,
    ui::WindowAndroid* window,
    Profile* profile) {
  // TTC settings follow the original profile, like the settings page.
  return reinterpret_cast<int64_t>(new TtcShortcutHandlerAndroid(
      std::make_unique<JavaDelegate>(java_obj),
      std::make_unique<WindowAcceleratorRegistry>(window),
      profile->GetOriginalProfile()->GetPrefs()));
}

}  // namespace ttc

DEFINE_JNI(TtcShortcutHandler)
