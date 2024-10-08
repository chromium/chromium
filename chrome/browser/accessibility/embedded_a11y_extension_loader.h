// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_EMBEDDED_A11Y_EXTENSION_LOADER_H_
#define CHROME_BROWSER_ACCESSIBILITY_EMBEDDED_A11Y_EXTENSION_LOADER_H_

#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"

namespace extensions {
class ComponentLoader;
}

class Profile;

///////////////////////////////////////////////////////////////////////////////
// EmbeddedA11yExtensionLoader
//
// A class that manages the installation and uninstallation of the
// Accessibility helper extension on every profile (including guest and
// incognito) for Chrome Accessibility services and features on all platforms
// except Lacros, where it just informs EmbeddedA11yHelperLacros.
//
class EmbeddedA11yExtensionLoader : public ProfileObserver,
                                    public ProfileManagerObserver {
 public:
  // Simple struct to hold information about each extension installed on all
  // profiles.
  struct ExtensionInfo {
    ExtensionInfo(const std::string& extension_id,
                  const std::string& extension_path,
                  const base::FilePath::CharType* extension_manifest_file,
                  bool should_localize);
    ExtensionInfo(const ExtensionInfo& other);
    ExtensionInfo(ExtensionInfo&&);
    ExtensionInfo& operator=(const ExtensionInfo&);
    ExtensionInfo& operator=(ExtensionInfo&&);
    ~ExtensionInfo();

    // The id of the extension.
    const std::string extension_id;

    // The path to the extension manifest file.
    const std::string extension_path;

    // The name of the extension manifest file.
    const base::FilePath::CharType* extension_manifest_file;

    // Whether the extension should be localized or not.
    bool should_localize;
  };
  // Gets the current instance of EmbeddedA11yExtensionLoader. There
  // should be one of these across all profiles.
  static EmbeddedA11yExtensionLoader* GetInstance();

  EmbeddedA11yExtensionLoader();
  ~EmbeddedA11yExtensionLoader() override;
  EmbeddedA11yExtensionLoader(EmbeddedA11yExtensionLoader&) = delete;
  EmbeddedA11yExtensionLoader& operator=(EmbeddedA11yExtensionLoader&) = delete;

  // Should be called when the browser starts up.
  void Init();

  // Install an extension.
  // `manifest_name` must live for the duration of the program. (e.g. be
  // statically allocated)
  void InstallExtensionWithId(const std::string& extension_id,
                              const std::string& extension_path,
                              const base::FilePath::CharType* manifest_name,
                              bool should_localize);
  void RemoveExtensionWithId(const std::string& extension_id);

  // We can't use extensions::ExtensionHostTestHelper as those require a
  // background page, and these extensions do not have background pages.
  void AddExtensionChangedCallbackForTest(base::RepeatingClosure callback);

  // Check whether an extension is installed or not.
  bool IsExtensionInstalled(const std::string& extension_id);

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  void UpdateAllProfiles(const std::string& extension_id);
  void UpdateProfile(Profile* profile, const std::string& extension_id);

  // Removes the helper extension with `extension_id` from the given `profile`
  // if it is installed.
  void MaybeRemoveExtension(Profile* profile, const std::string& extension_id);

  // Installs the helper extension with `extension_id` from the given `profile`
  // if it isn't yet installed.
  void MaybeInstallExtension(Profile* profile,
                             const std::string& extension_id,
                             const std::string& extension_path,
                             const base::FilePath::CharType* manifest_name,
                             bool should_localize);

  // Installs the helper extension with the given `extension_id`, `manifest` and
  // `path` using the given `component_loader` for some profile.
  void InstallExtension(extensions::ComponentLoader* component_loader,
                        const base::FilePath& path,
                        const std::string& extension_id,
                        std::optional<base::Value::Dict> manifest);

  bool initialized_ = false;
  // A map to store all accessibility helper extensions installed.
  std::map<std::string, ExtensionInfo> extension_map_;

  base::RepeatingClosure extension_installation_changed_callback_for_test_;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::WeakPtrFactory<EmbeddedA11yExtensionLoader> weak_ptr_factory_{this};

  friend struct base::DefaultSingletonTraits<EmbeddedA11yExtensionLoader>;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_EMBEDDED_A11Y_EXTENSION_LOADER_H_
