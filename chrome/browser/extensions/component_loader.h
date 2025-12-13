// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ExtensionSystem;

// For registering, loading, and unloading component extensions.
class ComponentLoader : public KeyedService {
 public:
  static ComponentLoader* Get(content::BrowserContext* context);

  ComponentLoader(const ComponentLoader&) = delete;
  ComponentLoader& operator=(const ComponentLoader&) = delete;

  ~ComponentLoader() override;

  // KeyedService:
  void Shutdown() override;

  size_t registered_extensions_count() const {
    return component_extensions_.size();
  }

  // Creates and loads all registered component extensions.
  void LoadAll();

  // Registers and possibly loads a component extension. If ExtensionService
  // has been initialized, the extension is loaded; otherwise, the load is
  // deferred until LoadAll is called. The ID of the added extension is
  // returned.
  //
  // Component extension manifests must contain a "key" property with a unique
  // public key, serialized in base64. You can create a suitable value with the
  // following commands on a unixy system:
  //
  //   ssh-keygen -t rsa -b 1024 -N '' -f /tmp/key.pem
  //   openssl rsa -pubout -outform DER < /tmp/key.pem 2>/dev/null | base64 -w 0
  ExtensionId Add(std::string_view manifest_contents,
                  const base::FilePath& root_directory);

  // Convenience method for registering a component extension by resource id.
  ExtensionId Add(int manifest_resource_id,
                  const base::FilePath& root_directory);

  // Convenience method for registering a component extension by parsed
  // manifest.
  ExtensionId Add(base::Value::Dict manifest,
                  const base::FilePath& root_directory);

  // Loads a component extension from file system. Replaces previously added
  // extension with the same ID.
  ExtensionId AddOrReplace(const base::FilePath& path);

  // Returns true if an extension with the specified id has been added.
  bool Exists(const ExtensionId& id) const;

  // Unloads a component extension and removes it from the list of component
  // extensions to be loaded.
  void Remove(const base::FilePath& root_directory);
  void Remove(const ExtensionId& id);

  // Call this during test setup to load component extensions that have
  // background pages for testing, which could otherwise interfere with tests.
  static void EnableBackgroundExtensionsForTesting();

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Call this during test setup to disabling loading the HelpApp.
  static void DisableHelpAppForTesting();
#endif

  // Adds the default component extensions. If `skip_session_components`
  // the loader will skip loading component extensions that weren't supposed to
  // be loaded unless we are in signed user session (ChromeOS). For all other
  // platforms this `skip_session_components` is expected to be unset.
  void AddDefaultComponentExtensions(bool skip_session_components);

  // Similar to above but adds the default component extensions for kiosk mode.
  void AddDefaultComponentExtensionsForKioskMode(bool skip_session_components);

  // Reloads a registered component extension.
  void Reload(const ExtensionId& extension_id);

  // Return ids of all registered extensions.
  std::vector<ExtensionId> GetRegisteredComponentExtensionsIds() const;

#if BUILDFLAG(IS_CHROMEOS)
  // Whether the given extension is being loaded in the file task runner.
  bool IsPendingAdd(const ExtensionId& extension_id) const;

  // Convenience wrapper of `Exists` and `IsPendingAdd` since most callers do
  // not need to differentiate the two cases.
  bool ExistsOrPendingAdd(const ExtensionId& extension_id) const;

  // Identical to AddComponentFromDir() except allows for the caller to supply
  // the name of the manifest file.
  void AddComponentFromDirWithManifestFilename(
      const base::FilePath& root_directory,
      const ExtensionId& extension_id,
      const base::FilePath::CharType* manifest_file_name,
      const base::FilePath::CharType* guest_manifest_file_name,
      base::OnceClosure done_cb,
      base::OnceClosure error_cb);

  // Add a component extension from a specific directory. Assumes that the
  // extension uses a different manifest file when this is a guest session
  // and that the manifest file lives in `root_directory`. Calls `done_cb`
  // on success, unless the component loader is shut down during loading.
  void AddComponentFromDir(const base::FilePath& root_directory,
                           const ExtensionId& extension_id,
                           base::OnceClosure done_cb);

  void AddChromeOsSpeechSynthesisExtensions();
#endif  // BUILDFLAG(IS_CHROMEOS)

  void set_ignore_allowlist_for_testing(bool value) {
    ignore_allowlist_for_testing_ = value;
  }

  // Allows setting the profile used by the loader for testing purposes.
  void set_profile_for_testing(Profile* profile) { profile_ = profile; }

 private:
  friend class ComponentLoaderFactory;
  friend class TtsApiTest;
  FRIEND_TEST_ALL_PREFIXES(ComponentLoaderTest, ParseManifest);

  // Information about a registered component extension.
  struct ComponentExtensionInfo {
    ComponentExtensionInfo(base::Value::Dict manifest_param,
                           const base::FilePath& root_directory);

    ComponentExtensionInfo(const ComponentExtensionInfo&) = delete;
    ComponentExtensionInfo& operator=(const ComponentExtensionInfo&) = delete;

    ~ComponentExtensionInfo();

    ComponentExtensionInfo(ComponentExtensionInfo&& other);
    ComponentExtensionInfo& operator=(ComponentExtensionInfo&& other);

    // The parsed contents of the extensions's manifest file.
    base::Value::Dict manifest;

    // Directory where the extension is stored.
    base::FilePath root_directory;

    // The component extension's ID.
    ExtensionId extension_id;
  };

  explicit ComponentLoader(Profile* profile);

  // Parses the given JSON manifest. Returns `std::nullopt` if it cannot be
  // parsed or if the result is not a base::Value::Dict.
  std::optional<base::Value::Dict> ParseManifest(
      std::string_view manifest_contents) const;

  ExtensionId Add(std::string_view manifest_contents,
                  const base::FilePath& root_directory,
                  bool skip_allowlist);
  ExtensionId Add(base::Value::Dict parsed_manifest,
                  const base::FilePath& root_directory,
                  bool skip_allowlist);

  // Loads a registered component extension.
  void Load(const ComponentExtensionInfo& info);

  void AddDefaultComponentExtensionsWithBackgroundPages(
      bool skip_session_components);
  void AddDefaultComponentExtensionsWithBackgroundPagesForKioskMode();

#if BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)
  void AddHangoutServicesExtension();
#endif  // BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)

  void AddNetworkSpeechSynthesisExtension();

  void AddWithNameAndDescription(int manifest_resource_id,
                                 const base::FilePath& root_directory,
                                 const std::string& name_string,
                                 const std::string& description_string);
  void AddWebStoreApp();

#if BUILDFLAG(IS_CHROMEOS)
  void AddChromeApp();
  void AddFileManagerExtension();
  void AddGalleryExtension();
  void AddImageLoaderExtension();
  void AddGuestModeTestExtension(const base::FilePath& path);
  void AddKeyboardApp();

  // Used as a reply callback by `AddComponentFromDir`.
  // Called with a `root_directory` and parsed `manifest` and invokes
  // `done_cb` after adding the extension.
  void FinishAddComponentFromDir(
      const base::FilePath& root_directory,
      const ExtensionId& extension_id,
      const std::optional<std::string>& name_string,
      const std::optional<std::string>& description_string,
      base::OnceClosure done_cb,
      base::OnceClosure error_cb,
      std::optional<base::Value::Dict> manifest);

  // Finishes loading an extension tts engine.
  void FinishLoadSpeechSynthesisExtension(const ExtensionId& extension_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

  scoped_refptr<const Extension> CreateExtension(
      const ComponentExtensionInfo& info,
      std::u16string* error);

  // Unloads `component` from the memory.
  void UnloadComponent(ComponentExtensionInfo* component);

  raw_ptr<Profile> profile_;

  raw_ptr<ExtensionSystem, AcrossTasksDanglingUntriaged> extension_system_;

  // List of registered component extensions (see mojom::ManifestLocation).
  using RegisteredComponentExtensions = std::vector<ComponentExtensionInfo>;
  RegisteredComponentExtensions component_extensions_;

  bool ignore_allowlist_for_testing_;

#if BUILDFLAG(IS_CHROMEOS)
  // Ids of extensions that are being loaded on file task runner.
  ExtensionIdSet pending_extension_ids_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::WeakPtrFactory<ComponentLoader> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
