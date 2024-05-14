// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/permissions_data.h"

namespace ash::file_system_provider {

// static
std::unique_ptr<ProviderInterface> FakeExtensionProvider::Create(
    const extensions::ExtensionId& extension_id) {
  Capabilities default_capabilities(false, false, false,
                                    extensions::SOURCE_NETWORK);
  return std::unique_ptr<ProviderInterface>(
      new FakeExtensionProvider(extension_id, default_capabilities));
}

// Factory callback, to be used in Service::SetFileSystemFactory(). The
// |event_router| argument can be NULL.
std::unique_ptr<ProvidedFileSystemInterface>
FakeExtensionProvider::CreateProvidedFileSystem(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info,
    CacheManager* cache_manager) {
  DCHECK(profile);
  return std::make_unique<FakeProvidedFileSystem>(file_system_info);
}

const Capabilities& FakeExtensionProvider::GetCapabilities() const {
  return capabilities_;
}

const ProviderId& FakeExtensionProvider::GetId() const {
  return provider_id_;
}

const std::string& FakeExtensionProvider::GetName() const {
  return name_;
}

const IconSet& FakeExtensionProvider::GetIconSet() const {
  return icon_set_;
}

RequestManager* FakeExtensionProvider::GetRequestManager() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool FakeExtensionProvider::RequestMount(Profile* profile,
                                         RequestMountCallback callback) {
  std::move(callback).Run(base::File::Error::FILE_OK);
  return true;
}

FakeExtensionProvider::FakeExtensionProvider(
    const extensions::ExtensionId& extension_id,
    const Capabilities& capabilities)
    : provider_id_(ProviderId::CreateFromExtensionId(extension_id)),
      capabilities_(capabilities),
      name_("Fake Extension Provider") {}

}  // namespace ash::file_system_provider
