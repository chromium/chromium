// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_mutable_registry.h"

#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

SystemExtensionsMutableRegistry::SystemExtensionsMutableRegistry() = default;

SystemExtensionsMutableRegistry::~SystemExtensionsMutableRegistry() = default;

void SystemExtensionsMutableRegistry::AddSystemExtension(
    SystemExtension system_extension) {
  SystemExtensionId id = system_extension.id;
  system_extensions_[id] = std::move(system_extension);
}

void SystemExtensionsMutableRegistry::RemoveSystemExtension(
    const SystemExtensionId& system_extension_id) {
  system_extensions_.erase(system_extension_id);
}

std::vector<SystemExtensionId> SystemExtensionsMutableRegistry::GetIds() {
  std::vector<SystemExtensionId> extension_ids;
  for (const auto& [id, system_extension] : system_extensions_) {
    extension_ids.push_back(id);
  }
  return extension_ids;
}

const SystemExtension* SystemExtensionsMutableRegistry::GetById(
    const SystemExtensionId& system_extension_id) {
  auto it = system_extensions_.find(system_extension_id);
  if (it == system_extensions_.end()) {
    return nullptr;
  }
  return &it->second;
}

const SystemExtension* SystemExtensionsMutableRegistry::GetByUrl(
    const GURL& url) {
  for (auto& [id, system_extension] : system_extensions_) {
    if (url::IsSameOriginWith(system_extension.base_url, url)) {
      return &system_extension;
    }
  }

  return nullptr;
}

}  // namespace ash
