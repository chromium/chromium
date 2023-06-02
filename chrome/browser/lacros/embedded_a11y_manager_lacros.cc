// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"

#include "base/memory/singleton.h"

// static
EmbeddedA11yManagerLacros* EmbeddedA11yManagerLacros::GetInstance() {
  return base::Singleton<
      EmbeddedA11yManagerLacros,
      base::LeakySingletonTraits<EmbeddedA11yManagerLacros>>::get();
}

EmbeddedA11yManagerLacros::EmbeddedA11yManagerLacros() = default;

EmbeddedA11yManagerLacros::~EmbeddedA11yManagerLacros() = default;

void EmbeddedA11yManagerLacros::Init() {
  CHECK(!chromevox_enabled_observer_)
      << "EmbeddedA11yManagerLacros::Init should only be called once.";
  // Initial values are obtained when the observers are created, there is no
  // need to do so explicitly.
  chromevox_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
      base::BindRepeating(&EmbeddedA11yManagerLacros::OnChromeVoxEnabledChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  select_to_speak_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kAccessibilitySelectToSpeakEnabled,
      base::BindRepeating(
          &EmbeddedA11yManagerLacros::OnSelectToSpeakEnabledChanged,
          weak_ptr_factory_.GetWeakPtr()));
  switch_access_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kAccessibilitySwitchAccessEnabled,
      base::BindRepeating(
          &EmbeddedA11yManagerLacros::OnSwitchAccessEnabledChanged,
          weak_ptr_factory_.GetWeakPtr()));

  // TODO(b:271633121): Begin observing for profile changes.
}

void EmbeddedA11yManagerLacros::AddPrefChangedCallbackForTest(
    base::RepeatingCallback<void()> callback) {
  pref_changed_callback_for_test_ = std::move(callback);
}

void EmbeddedA11yManagerLacros::OnChromeVoxEnabledChanged(base::Value value) {
  CHECK(value.is_bool());
  chromevox_enabled_ = value.GetBool();
  if (pref_changed_callback_for_test_) {
    pref_changed_callback_for_test_.Run();
  }
  // TODO(b:271633121): Load or unload helper extension.
}

void EmbeddedA11yManagerLacros::OnSelectToSpeakEnabledChanged(
    base::Value value) {
  CHECK(value.is_bool());
  select_to_speak_enabled_ = value.GetBool();
  if (pref_changed_callback_for_test_) {
    pref_changed_callback_for_test_.Run();
  }
  // TODO(b:271633121): Load or unload helper extension.
}

void EmbeddedA11yManagerLacros::OnSwitchAccessEnabledChanged(
    base::Value value) {
  CHECK(value.is_bool());
  switch_access_enabled_ = value.GetBool();
  if (pref_changed_callback_for_test_) {
    pref_changed_callback_for_test_.Run();
  }
  // TODO(b:271633121): Load or unload helper extension.
}
