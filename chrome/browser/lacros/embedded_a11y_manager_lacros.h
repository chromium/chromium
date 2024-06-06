// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_EMBEDDED_A11Y_MANAGER_LACROS_H_
#define CHROME_BROWSER_LACROS_EMBEDDED_A11Y_MANAGER_LACROS_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/singleton.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/crosapi/mojom/embedded_accessibility_helper.mojom.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#include "content/public/browser/focused_node_details.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

// Manages preferences in Lacros that support Accessibility features running in
// Ash. This class calls EmbeddedA11yExtensionLoader to install and uninstall
// the extensions on every profile (including guest and incognito) depending on
// which Ash accessibility features are running and syncs the preferences on all
// profiles.
class EmbeddedA11yManagerLacros
    : public crosapi::mojom::EmbeddedAccessibilityHelper,
      public ProfileObserver,
      public ProfileManagerObserver {
 public:
  // Gets the current instance of EmbeddedA11yManagerLacros. There should be one
  // of these across all Lacros profiles.
  static EmbeddedA11yManagerLacros* GetInstance();

  EmbeddedA11yManagerLacros(EmbeddedA11yManagerLacros&) = delete;
  EmbeddedA11yManagerLacros& operator=(EmbeddedA11yManagerLacros&) = delete;

  // crosapi::mojom::EmbeddedAccessibilityHelper:
  void ClipboardCopyInActiveGoogleDoc(const std::string& url) override;

  // Starts to observe Ash accessibility feature state and profiles.
  // Should be called when Lacros starts up.
  void Init();

  // Called when the Select to Speak context menu was clicked in Lacros,
  // and forwards the event back to Ash to inform the Select to Speak
  // accessibility feature that selected text should be spoken.
  void SpeakSelectedText();

  bool IsSelectToSpeakEnabled();

  // We can't use extensions::ExtensionHostTestHelper as those require a
  // background page, and these extensions do not have background pages.
  void AddExtensionChangedCallbackForTest(base::RepeatingClosure callback);

  void AddSpeakSelectedTextCallbackForTest(base::RepeatingClosure callback);

  void AddFocusChangedCallbackForTest(
      base::RepeatingCallback<void(gfx::Rect)> callback);

  void SetReadingModeEnabled(bool enabled);

  bool IsReadingModeEnabled();

 private:
  EmbeddedA11yManagerLacros();
  ~EmbeddedA11yManagerLacros() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  void UpdateOverscrollHistoryNavigationEnabled();

  void OnChromeVoxEnabledChanged(base::Value value);
  void OnSelectToSpeakEnabledChanged(base::Value value);
  void OnSwitchAccessEnabledChanged(base::Value value);
  void OnFocusHighlightEnabledChanged(base::Value value);
  void OnReducedAnimationsEnabledChanged(base::Value value);
  void OnOverscrollHistoryNavigationEnabledChanged(base::Value value);

  // Called when focus highlight feature is active and the focused node
  // changed.
  void OnFocusChangedInPage(const content::FocusedNodeDetails& details);

  void UpdateEmbeddedA11yHelperExtension();
  void UpdateChromeVoxHelperExtension();

  // Observers for Ash feature state.
  std::unique_ptr<CrosapiPrefObserver> chromevox_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> select_to_speak_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> switch_access_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> focus_highlight_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> reduced_animations_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver>
      overscroll_history_navigation_enabled_observer_;

  // The current state of Ash features.
  bool chromevox_enabled_ = false;
  bool select_to_speak_enabled_ = false;
  bool switch_access_enabled_ = false;
  bool reading_mode_enabled_ = false;
  std::optional<bool> overscroll_history_navigation_enabled_;

  base::RepeatingClosure speak_selected_text_callback_for_test_;
  base::RepeatingCallback<void(gfx::Rect)> focus_changed_callback_for_test_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::CallbackListSubscription focus_changed_subscription_;

  mojo::Remote<crosapi::mojom::EmbeddedAccessibilityHelperClient>
      a11y_helper_remote_;

  mojo::Receiver<crosapi::mojom::EmbeddedAccessibilityHelper>
      a11y_helper_receiver_{this};

  base::WeakPtrFactory<EmbeddedA11yManagerLacros> weak_ptr_factory_{this};

  friend struct base::DefaultSingletonTraits<EmbeddedA11yManagerLacros>;
};

#endif  // CHROME_BROWSER_LACROS_EMBEDDED_A11Y_MANAGER_LACROS_H_
