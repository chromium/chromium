// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"

class Profile;

namespace extensions {

class Extension;
class ExtensionServiceInterface;

// For registering, loading, and unloading component extensions.
class ComponentLoader {
 public:
  ComponentLoader(ExtensionServiceInterface* extension_service,
                  Profile* browser_context);
  virtual ~ComponentLoader();

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
  std::string Add(const base::StringPiece& manifest_contents,
                  const base::FilePath& root_directory);

  // Convenience method for registering a component extension by resource id.
  std::string Add(int manifest_resource_id,
                  const base::FilePath& root_directory);

  // Loads a component extension from file system. Replaces previously added
  // extension with the same ID.
  std::string AddOrReplace(const base::FilePath& path);

  // Returns true if an extension with the specified id has been added.
  bool Exists(const std::string& id) const;

  // Unloads a component extension and removes it from the list of component
  // extensions to be loaded.
  void Remove(const base::FilePath& root_directory);
  void Remove(const std::string& id);

  // Call this during test setup to load component extensions that have
  // background pages for testing, which could otherwise interfere with tests.
  static void EnableBackgroundExtensionsForTesting();

  // Adds the default component extensions. If |skip_session_components|
  // the loader will skip loading component extensions that weren't supposed to
  // be loaded unless we are in signed user session (ChromeOS). For all other
  // platforms this |skip_session_components| is expected to be unset.
  void AddDefaultComponentExtensions(bool skip_session_components);

  // Similar to above but adds the default component extensions for kiosk mode.
  void AddDefaultComponentExtensionsForKioskMode(bool skip_session_components);

  // Reloads a registered component extension.
  void Reload(const std::string& extension_id);

#if defined(OS_CHROMEOS)
  // Add a component extension from a specific directory. Assumes that the
  // extension uses a different manifest file when this is a guest session
  // and that the manifest file lives in |root_directory|. Calls |done_cb|
  // on success, unless the component loader is shut down during loading.
  void AddComponentFromDir(
      const base::FilePath& root_directory,
      const char* extension_id,
      const base::Closure& done_cb);

  // Add a component extension from a specific directory. Assumes that the
  // extension's manifest file lives in |root_directory| and its name is
  // 'manifest.json'. |name_string| and |description_string| are used to
  // localize component extension's name and description text exclusively.
  void AddWithNameAndDescriptionFromDir(const base::FilePath& root_directory,
                                        const char* extension_id,
                                        const std::string& name_string,
                                        const std::string& description_string);

  void AddChromeOsSpeechSynthesisExtensions();
#endif

  void set_ignore_whitelist_for_testing(bool value) {
    ignore_whitelist_for_testing_ = value;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ComponentLoaderTest, ParseManifest);

  // Information about a registered component extension.
  struct ComponentExtensionInfo {
    ComponentExtensionInfo(
        std::unique_ptr<base::DictionaryValue> manifest_param,
        const base::FilePath& root_directory);
    ~ComponentExtensionInfo();

    ComponentExtensionInfo(ComponentExtensionInfo&& other);
    ComponentExtensionInfo& operator=(ComponentExtensionInfo&& other);

    // The parsed contents of the extensions's manifest file.
    std::unique_ptr<base::DictionaryValue> manifest;

    // Directory where the extension is stored.
    base::FilePath root_directory;

    // The component extension's ID.
    std::string extension_id;

   private:
    DISALLOW_COPY_AND_ASSIGN(ComponentExtensionInfo);
  };

  // Parses the given JSON manifest. Returns nullptr if it cannot be parsed or
  // if the result is not a DictionaryValue.
  std::unique_ptr<base::DictionaryValue> ParseManifest(
      base::StringPiece manifest_contents) const;

  std::string Add(const base::StringPiece& manifest_contents,
                  const base::FilePath& root_directory,
                  bool skip_whitelist);
  std::string Add(std::unique_ptr<base::DictionaryValue> parsed_manifest,
                  const base::FilePath& root_directory,
                  bool skip_whitelist);

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

#if defined(OS_CHROMEOS)
  void AddChromeApp();
  void AddFileManagerExtension();
  void AddVideoPlayerExtension();
  void AddAudioPlayerExtension();
  void AddGalleryExtension();
  void AddImageLoaderExtension();
  void AddKeyboardApp();
  void AddChromeCameraApp();
  void AddZipArchiverExtension();
#endif  // defined(OS_CHROMEOS)

  scoped_refptr<const Extension> CreateExtension(
      const ComponentExtensionInfo& info, std::string* utf8_error);

  // Unloads |component| from the memory.
  void UnloadComponent(ComponentExtensionInfo* component);

#if defined(OS_CHROMEOS)
  // Used as a reply callback by |AddComponentFromDir|.
  // Called with a |root_directory| and parsed |manifest| and invokes
  // |done_cb| after adding the extension.
  void FinishAddComponentFromDir(
      const base::FilePath& root_directory,
      const char* extension_id,
      const base::Optional<std::string>& name_string,
      const base::Optional<std::string>& description_string,
      const base::Closure& done_cb,
      std::unique_ptr<base::DictionaryValue> manifest);

  // Finishes loading an extension tts engine.
  void FinishLoadSpeechSynthesisExtension(const char* extension_id);
#endif

  Profile* profile_;

  ExtensionServiceInterface* extension_service_;

  // List of registered component extensions (see Manifest::Location).
  typedef std::vector<ComponentExtensionInfo> RegisteredComponentExtensions;
  RegisteredComponentExtensions component_extensions_;

  bool ignore_whitelist_for_testing_;

  base::WeakPtrFactory<ComponentLoader> weak_factory_{this};

  friend class TtsApiTest;

  DISALLOW_COPY_AND_ASSIGN(ComponentLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_H_
