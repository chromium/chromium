// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "url/gurl.h"

namespace {
constexpr char kEchoSystemExtensionManifest[] =
    R"({
          "name": "Sample System Web Extension",
          "short_name": "Sample SWX",
          "companion_web_app_url": "https://example.com",
          "service_worker_url": "/sw.js",
          "id": "1234",
          "type": "echo"
    })";

}  // namespace

SystemExtensionsInstallManager::SystemExtensionsInstallManager() {
  InstallFromCommandLineIfNecessary();
}

SystemExtensionsInstallManager::~SystemExtensionsInstallManager() = default;

std::vector<SystemExtensionId>
SystemExtensionsInstallManager::GetSystemExtensionIds() {
  std::vector<SystemExtensionId> extension_ids;
  for (const auto& id_and_extension : system_extensions_) {
    extension_ids.emplace_back(id_and_extension.first);
  }
  return extension_ids;
}

const SystemExtension* SystemExtensionsInstallManager::GetSystemExtensionById(
    const SystemExtensionId& id) {
  const auto it = system_extensions_.find(id);
  if (it == system_extensions_.end())
    return nullptr;
  return &(it->second);
}

void SystemExtensionsInstallManager::InstallFromCommandLineIfNecessary() {
  // For now just use a hardcoded System Extension manifest. Future CLs will
  // change this to take a command line argument to a CRX.
  base::Optional<base::Value> value =
      base::JSONReader::Read(kEchoSystemExtensionManifest);
  if (base::CompareCaseInsensitiveASCII("echo",
                                        *value->FindStringKey("type")) != 0) {
    LOG(ERROR) << "System Extension type is not supported.";
    return;
  }

  SystemExtension system_extension;
  std::string id = *value->FindStringKey("id");
  system_extension.id = {1, 2, 3, 4};
  system_extension.type = SystemExtensionType::kEcho;
  system_extension.name = *value->FindStringKey("name");
  system_extension.short_name = *value->FindStringKey("short_name");
  system_extension.companion_web_app_url =
      GURL(*value->FindStringKey("companion_web_app_url"));
  system_extension.service_worker_url =
      GURL(base::StrCat({"chrome-untrusted://system-extension-echo-", id, "/",
                         *value->FindStringKey("service_worker_url")}));

  system_extensions_[{1, 2, 3, 4}] = std::move(system_extension);
}
