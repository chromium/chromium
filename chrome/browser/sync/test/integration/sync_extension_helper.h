// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_EXTENSION_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_EXTENSION_HELPER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "extensions/common/manifest.h"

class Profile;
class SyncTest;

namespace extensions {
class Extension;
}

class SyncExtensionHelper {
 public:
  // Singleton implementation.
  static SyncExtensionHelper* GetInstance();

  SyncExtensionHelper(const SyncExtensionHelper&) = delete;
  SyncExtensionHelper& operator=(const SyncExtensionHelper&) = delete;

  // Initializes the profiles in |test| and registers them with
  // internal data structures.
  void SetupIfNecessary(SyncTest* test);

  // Installs the extension with the given name to |profile|, and returns the
  // extension ID of the new extension.
  std::string InstallExtension(Profile* profile,
                               const std::string& name,
                               extensions::Manifest::Type type);

  // Uninstalls the extension with the given name from |profile|.
  void UninstallExtension(Profile* profile, const std::string& name);

  // Returns a vector containing the names of all currently installed extensions
  // on |profile|.
  std::vector<std::string> GetInstalledExtensionNames(Profile* profile) const;

  // Enables the extension with the given name on |profile|.
  void EnableExtension(Profile* profile, const std::string& name);

  // Disables the extension with the given name on |profile|.
  void DisableExtension(Profile* profile, const std::string& name);

  // Returns true if the extension with the given name is enabled on |profile|.
  bool IsExtensionEnabled(Profile* profile, const std::string& name) const;

  // Enables the extension with the given name to run in incognito mode
  void IncognitoEnableExtension(Profile* profile, const std::string& name);

  // Disables the extension with the given name from running in incognito mode
  void IncognitoDisableExtension(Profile* profile, const std::string& name);

  // Returns true iff the extension is enabled in incognito mode on |profile|.
  bool IsIncognitoEnabled(Profile* profile, const std::string& name) const;

  // Returns true iff the extension with the given id is pending
  // install in |profile|.
  bool IsExtensionPendingInstallForSync(Profile* profile,
                                        const std::string& id) const;

  // Installs all extensions pending sync in |profile|.
  void InstallExtensionsPendingForSync(Profile* profile);

  // Returns true iff |profile1| and |profile2| have the same extensions and
  // they are all in the same state.
  static bool ExtensionStatesMatch(Profile* profile1, Profile* profile2);

  // Returns a unique extension name based in the integer |index|.
  std::string CreateFakeExtensionName(int index);

  // Converts a fake extension name back into the index used to generate it.
  // Returns true if successful, false on failure.
  bool ExtensionNameToIndex(const std::string& name, int* index);

 private:
  struct ExtensionState {
    enum EnabledState { DISABLED, PENDING, ENABLED };

    bool operator==(const ExtensionState& other) const = default;

    EnabledState enabled_state = ENABLED;
    int disable_reasons = 0;
    bool incognito_enabled = false;
  };

  using ExtensionStateMap = std::map<std::string, ExtensionState>;
  using ExtensionNameMap =
      std::map<std::string, scoped_refptr<extensions::Extension>>;
  using ProfileExtensionNameMap = std::map<Profile*, ExtensionNameMap>;
  using StringMap = std::map<std::string, std::string>;
  using TypeMap = std::map<std::string, extensions::Manifest::Type>;

  friend struct base::DefaultSingletonTraits<SyncExtensionHelper>;

  SyncExtensionHelper();
  ~SyncExtensionHelper();

  // Returns a map from |profile|'s installed extensions to their state.
  static ExtensionStateMap GetExtensionStates(Profile* profile);

  // Initializes extensions for |profile| and creates an entry in
  // |profile_extensions_| for it.
  void SetupProfile(Profile* profile);

  // Returns an extension for the given name in |profile|.  type and
  // index.  Two extensions with the name but different profiles will
  // have the same id.
  [[nodiscard]] scoped_refptr<extensions::Extension> GetExtension(
      Profile* profile,
      const std::string& name,
      extensions::Manifest::Type type);

  std::string extension_name_prefix_;
  ProfileExtensionNameMap profile_extensions_;
  StringMap id_to_name_;
  TypeMap id_to_type_;
  bool setup_completed_ = false;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_EXTENSION_HELPER_H_
