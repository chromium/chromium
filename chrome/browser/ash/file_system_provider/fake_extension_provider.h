// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FAKE_EXTENSION_PROVIDER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FAKE_EXTENSION_PROVIDER_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/file_system_provider/extension_provider.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace ash::file_system_provider {

class FakeExtensionProvider : public ProviderInterface {
 public:
  ~FakeExtensionProvider() override = default;

  // Returns a fake provider instance for the specified extension. The extension
  // doesn't have to exist.
  static std::unique_ptr<ProviderInterface> Create(
      const extensions::ExtensionId& extension_id);

  // ProviderInterface overrides.
  std::unique_ptr<ProvidedFileSystemInterface> CreateProvidedFileSystem(
      Profile* profile,
      const ProvidedFileSystemInfo& file_system_info,
      CacheManager* cache_manager) override;
  const Capabilities& GetCapabilities() const override;
  const ProviderId& GetId() const override;
  const std::string& GetName() const override;
  const IconSet& GetIconSet() const override;
  RequestManager* GetRequestManager() override;
  bool RequestMount(Profile* profile, RequestMountCallback callback) override;

 protected:
  FakeExtensionProvider(const extensions::ExtensionId& extension_id,
                        const Capabilities& capabilities);

  ProviderId provider_id_;
  Capabilities capabilities_;
  std::string name_;
  IconSet icon_set_;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FAKE_EXTENSION_PROVIDER_H_
