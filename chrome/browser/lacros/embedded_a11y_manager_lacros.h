// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_EMBEDDED_A11Y_MANAGER_LACROS_H_
#define CHROME_BROWSER_LACROS_EMBEDDED_A11Y_MANAGER_LACROS_H_

#include "base/memory/singleton.h"
#include "base/values.h"
#include "chromeos/lacros/crosapi_pref_observer.h"

// Watches the state of accessibility features in Ash that need helper
// extensions installed in lacros.
// TODO(b:271633121): Manage extensions in Lacros that support Accessibility
// features running in Ash. Install and uninstall the extensions on every
// profile (including guest and incognito) depending on which Ash accessibility
// features are running.
class EmbeddedA11yManagerLacros {
 public:
  // Gets the current instance of EmbeddedA11yManagerLacros. There should be one
  // of these across all Lacros profiles.
  // TODO(b:271633121): Use this instance from a EmbeddedA11yHelperPrivate API
  // to send a Select to Speak context menu click from extension back through
  // crosapi to Ash.
  static EmbeddedA11yManagerLacros* GetInstance();

  EmbeddedA11yManagerLacros(EmbeddedA11yManagerLacros&) = delete;
  EmbeddedA11yManagerLacros& operator=(EmbeddedA11yManagerLacros&) = delete;

  // Starts to observe Ash accessibility feature state and profiles.
  // Should be called when Lacros starts up.
  void Init();

  // We can't use extensions::ExtensionHostTestHelper as those require a
  // background page, and these extensions do not have background pages.
  void AddPrefChangedCallbackForTest(base::RepeatingCallback<void()> callback);

  bool chromevox_enabled() { return chromevox_enabled_; }
  bool select_to_speak_enabled() { return select_to_speak_enabled_; }
  bool switch_access_enabled() { return switch_access_enabled_; }

 private:
  EmbeddedA11yManagerLacros();
  ~EmbeddedA11yManagerLacros();

  void OnChromeVoxEnabledChanged(base::Value value);
  void OnSelectToSpeakEnabledChanged(base::Value value);
  void OnSwitchAccessEnabledChanged(base::Value value);

  // Observers for Ash feature state.
  std::unique_ptr<CrosapiPrefObserver> chromevox_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> select_to_speak_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> switch_access_enabled_observer_;

  // The current state of Ash features.
  bool chromevox_enabled_ = false;
  bool select_to_speak_enabled_ = false;
  bool switch_access_enabled_ = false;

  base::RepeatingCallback<void()> pref_changed_callback_for_test_;

  base::WeakPtrFactory<EmbeddedA11yManagerLacros> weak_ptr_factory_{this};

  friend struct base::DefaultSingletonTraits<EmbeddedA11yManagerLacros>;
};

#endif  // CHROME_BROWSER_LACROS_EMBEDDED_A11Y_MANAGER_LACROS_H_
