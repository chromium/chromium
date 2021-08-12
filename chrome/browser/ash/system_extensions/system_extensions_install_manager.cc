// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/one_shot_event.h"
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
          "id": "01020304",
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
  return it->second.get();
}

void SystemExtensionsInstallManager::InstallFromCommandLineIfNecessary() {
  sandboxed_unpacker_.GetSystemExtensionFromString(
      kEchoSystemExtensionManifest,
      base::BindOnce(
          &SystemExtensionsInstallManager::OnGetSystemExtensionFromDir,
          weak_ptr_factory_.GetWeakPtr()));
}

void SystemExtensionsInstallManager::OnGetSystemExtensionFromDir(
    SystemExtensionsSandboxedUnpacker::Status status,
    std::unique_ptr<SystemExtension> system_extension) {
  if (status != SystemExtensionsSandboxedUnpacker::Status::kOk) {
    LOG(ERROR) << "Failed to install extension from command line: "
               << static_cast<int32_t>(status);
    on_command_line_install_finished_.Signal();
    return;
  }

  // TODO(ortuno): Move resources from the specified directory into the user
  // profile.

  SystemExtensionsWebUIConfigMap::GetInstance().AddForSystemExtension(
      *system_extension.get());
  system_extensions_[{1, 2, 3, 4}] = std::move(system_extension);
  on_command_line_install_finished_.Signal();
}
