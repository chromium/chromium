// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_EMBEDDED_A11Y_MANAGER_LACROS_H_
#define CHROME_BROWSER_LACROS_EMBEDDED_A11Y_MANAGER_LACROS_H_

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
#include "mojo/public/cpp/bindings/remote.h"

namespace extensions {
class ComponentLoader;
}

class Profile;

// Manages extensions and preferences in Lacros that support Accessibility
// features running in Ash. Installs and uninstalls the extensions on every
// profile (including guest and incognito) depending on which Ash accessibility
// features are running and syncs the preferences on all profiles.
class EmbeddedA11yManagerLacros : public ProfileObserver,
                                  public ProfileManagerObserver {
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

  // Called when the Select to Speak context menu was clicked in Lacros,
  // and forwards the event back to Ash to inform the Select to Speak
  // accessibility feature that selected text should be spoken.
  void SpeakSelectedText();

  // We can't use extensions::ExtensionHostTestHelper as those require a
  // background page, and these extensions do not have background pages.
  void AddExtensionChangedCallbackForTest(base::RepeatingClosure callback);

  void AddSpeakSelectedTextCallbackForTest(base::RepeatingClosure callback);

 private:
  EmbeddedA11yManagerLacros();
  ~EmbeddedA11yManagerLacros() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  void UpdateAllProfiles();
  void UpdateProfile(Profile* profile);

  void OnChromeVoxEnabledChanged(base::Value value);
  void OnSelectToSpeakEnabledChanged(base::Value value);
  void OnSwitchAccessEnabledChanged(base::Value value);
  void OnPdfOcrAlwaysActiveChanged(base::Value value);

  // Removes the helper extension with `extension_id` from the given `profile`
  // if it is installed.
  void MaybeRemoveExtension(Profile* profile, const std::string& extension_id);

  // Installs the helper extension with `extension_id` from the given `profile`
  // if it isn't yet installed.
  void MaybeInstallExtension(Profile* profile,
                             const std::string& extension_id,
                             const std::string& extension_path,
                             const base::FilePath::CharType* manifest_name);

  // Installs the helper extension with the given `extension_id`, `manifest` and
  // `path` using the given `component_loader` for some profile.
  void InstallExtension(extensions::ComponentLoader* component_loader,
                        const base::FilePath& path,
                        const std::string& extension_id,
                        absl::optional<base::Value::Dict> manifest);

  // Observers for Ash feature state.
  std::unique_ptr<CrosapiPrefObserver> chromevox_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> select_to_speak_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> switch_access_enabled_observer_;
  std::unique_ptr<CrosapiPrefObserver> pdf_ocr_always_active_observer_;

  // The current state of Ash features.
  bool chromevox_enabled_ = false;
  bool select_to_speak_enabled_ = false;
  bool switch_access_enabled_ = false;
  bool pdf_ocr_always_active_enabled_ = false;

  base::RepeatingClosure extension_installation_changed_callback_for_test_;
  base::RepeatingClosure speak_selected_text_callback_for_test_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  mojo::Remote<crosapi::mojom::EmbeddedAccessibilityHelperClient>
      a11y_helper_remote_;

  base::WeakPtrFactory<EmbeddedA11yManagerLacros> weak_ptr_factory_{this};

  friend struct base::DefaultSingletonTraits<EmbeddedA11yManagerLacros>;
};

#endif  // CHROME_BROWSER_LACROS_EMBEDDED_A11Y_MANAGER_LACROS_H_
