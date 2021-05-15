// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/ash/system_extensions/system_extensions_web_ui_config_map.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/origin.h"

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

constexpr char kSystemExtensionEchoPrefix[] = "system-extension-echo-";

GURL GetBaseURL(const std::string& id, SystemExtensionType type) {
  // The host is made of up a System Extension prefix based on the type and
  // the System Extension Id.
  base::StringPiece host_prefix;
  switch (type) {
    case SystemExtensionType::kEcho:
      host_prefix = kSystemExtensionEchoPrefix;
      break;
  }
  const std::string host = base::StrCat({host_prefix, id});
  return GURL(base::StrCat({content::kChromeUIUntrustedScheme,
                            url::kStandardSchemeSeparator, host}));
}

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
  base::FilePath user_data_directory;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_directory);

  // For now just use a hardcoded System Extension manifest. Future CLs will
  // change this to take a command line argument to a CRX.
  absl::optional<base::Value> value =
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

  system_extension.base_url = GetBaseURL(id, system_extension.type);
  system_extension.service_worker_url = system_extension.base_url.Resolve(
      *value->FindStringKey("service_worker_url"));

  SystemExtensionsWebUIConfigMap::GetInstance().AddForSystemExtension(
      system_extension);
  system_extensions_[{1, 2, 3, 4}] = std::move(system_extension);
}
