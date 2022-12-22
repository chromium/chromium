// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_EXTENSION_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {
class ExtensionRegistry;
}  // namespace extensions

namespace ash {
namespace file_system_provider {

class RequestDispatcher;

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
  ExtensionProvider(Profile* profile,
                    ProviderId id,
                    Capabilities capabilities,
                    std::string name);

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
  RequestManager* GetRequestManager() override;
  bool RequestMount(Profile* profile, RequestMountCallback callback) override;

 private:
  // This method is only partially functional since non-app extensions are not
  // registered with the app service.
  void ObserveAppServiceForIcons(Profile* profile);

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  void OnLacrosOperationForwarded(int request_id, base::File::Error error);

  ProviderId provider_id_;
  Capabilities capabilities_;
  std::string name_;
  IconSet icon_set_;
  std::unique_ptr<RequestDispatcher> request_dispatcher_;
  std::unique_ptr<RequestManager> request_manager_;

  base::WeakPtrFactory<ExtensionProvider> weak_ptr_factory_{this};
};

}  // namespace file_system_provider
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_EXTENSION_PROVIDER_H_
