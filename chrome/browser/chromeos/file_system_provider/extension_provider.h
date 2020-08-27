// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_EXTENSION_PROVIDER_H_

#include <memory>
#include <string>

#include "chrome/browser/chromeos/file_system_provider/icon_set.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/provider_interface.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {
class ExtensionRegistry;
}  // namespace extensions

namespace chromeos {
namespace file_system_provider {

// Holds information for a providing extension.
struct ProvidingExtensionInfo {
  ProvidingExtensionInfo();
  ~ProvidingExtensionInfo();

  extensions::ExtensionId extension_id;
  std::string name;
  extensions::FileSystemProviderCapabilities capabilities;
};

class ExtensionProvider : public ProviderInterface,
                          public apps::AppRegistryCache::Observer {
 public:
  ExtensionProvider(Profile* profile,
                    const extensions::ExtensionId& extension_id,
                    const ProvidingExtensionInfo& info);
  ~ExtensionProvider() override;

  // Returns a provider instance for the specified extension. If the extension
  // cannot be a providing extension, returns nullptr.
  static std::unique_ptr<ProviderInterface> Create(
      extensions::ExtensionRegistry* registry,
      const extensions::ExtensionId& extension_id);

  // ProviderInterface overrides.
  std::unique_ptr<ProvidedFileSystemInterface> CreateProvidedFileSystem(
      Profile* profile,
      const ProvidedFileSystemInfo& file_system_info) override;
  const Capabilities& GetCapabilities() const override;
  const ProviderId& GetId() const override;
  const std::string& GetName() const override;
  const IconSet& GetIconSet() const override;
  bool RequestMount(Profile* profile) override;

 private:
  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  ProviderId provider_id_;
  Capabilities capabilities_;
  std::string name_;
  IconSet icon_set_;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_EXTENSION_PROVIDER_H_
