// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_sandboxed_unpacker.h"

#include "ash/constants/ash_features.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "content/public/common/url_constants.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

namespace ash {

namespace {

const constexpr char kManifestName[] = "manifest.json";
const constexpr char kIdKey[] = "id";
const constexpr char kTypeKey[] = "type";
const constexpr char kNameKey[] = "name";
const constexpr char kShortNameKey[] = "short_name";
const constexpr char kServiceWorkerUrlKey[] = "service_worker_url";

GURL GetBaseURL(const std::string& id, SystemExtensionType type) {
  // The host is made of up a System Extension prefix based on the type and
  // the System Extension Id.
  base::StringPiece host_prefix;
  switch (type) {
    case SystemExtensionType::kWindowManagement:
      static constexpr char kSystemExtensionWindowManagementPrefix[] =
          "system-extension-window-management-";
      host_prefix = kSystemExtensionWindowManagementPrefix;
      break;
    case SystemExtensionType::kPeripheralPrototype:
      static constexpr char kSystemExtensionPeripheralPrototypePrefix[] =
          "system-extension-peripheral-prototype-";
      host_prefix = kSystemExtensionPeripheralPrototypePrefix;
      break;
    case SystemExtensionType::kManagedDeviceHealthServices:
      static constexpr char
          kSystemExtensionManagedDeviceHealthServicesPrefix[] =
              "system-extension-managed-device-health-services-";
      host_prefix = kSystemExtensionManagedDeviceHealthServicesPrefix;
      break;
  }
  const std::string host = base::StrCat({host_prefix, id});
  return GURL(base::StrCat({content::kChromeUIUntrustedScheme,
                            url::kStandardSchemeSeparator, host}));
}

SystemExtensionType* GetTypeFromString(base::StringPiece type_str) {
  static base::NoDestructor<
      base::flat_map<base::StringPiece, SystemExtensionType>>
      kStrToType([]() {
        std::vector<std::pair<base::StringPiece, SystemExtensionType>>
            str_to_type_list;
        str_to_type_list.emplace_back("window-management",
                                      SystemExtensionType::kWindowManagement);
        str_to_type_list.emplace_back(
            "peripheral-prototype", SystemExtensionType::kPeripheralPrototype);

        if (base::FeatureList::IsEnabled(
                features::kSystemExtensionsManagedDeviceHealthServices)) {
          str_to_type_list.emplace_back(
              "managed-device-health-services",
              SystemExtensionType::kManagedDeviceHealthServices);
        }
        return str_to_type_list;
      }());

  auto it = kStrToType->find(type_str);
  if (it == kStrToType->end())
    return nullptr;

  return &it->second;
}

}  // namespace

SystemExtensionsSandboxedUnpacker::SystemExtensionsSandboxedUnpacker() =
    default;

SystemExtensionsSandboxedUnpacker::~SystemExtensionsSandboxedUnpacker() =
    default;

void SystemExtensionsSandboxedUnpacker::GetSystemExtensionFromDir(
    base::FilePath system_extension_dir,
    GetSystemExtensionFromCallback callback) {
  io_helper_.AsyncCall(&IOHelper::ReadManifestInDirectory)
      .WithArgs(system_extension_dir)
      .Then(base::BindOnce(
          &SystemExtensionsSandboxedUnpacker::OnSystemExtensionManifestRead,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemExtensionsSandboxedUnpacker::OnSystemExtensionManifestRead(
    GetSystemExtensionFromCallback callback,
    SystemExtensionsStatusOr<SystemExtensionsInstallStatus, std::string>
        result) {
  if (!result.ok()) {
    std::move(callback).Run(result.status());
    return;
  }
  GetSystemExtensionFromString(result.value(), std::move(callback));
}

void SystemExtensionsSandboxedUnpacker::GetSystemExtensionFromString(
    base::StringPiece system_extension_manifest_string,
    GetSystemExtensionFromCallback callback) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      std::string(system_extension_manifest_string),
      base::BindOnce(
          &SystemExtensionsSandboxedUnpacker::OnSystemExtensionManifestParsed,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemExtensionsSandboxedUnpacker::OnSystemExtensionManifestParsed(
    GetSystemExtensionFromCallback callback,
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  if (!value_or_error.has_value()) {
    std::move(callback).Run(
        SystemExtensionsInstallStatus::kFailedJsonErrorParsingManifest);
    return;
  }

  if (!value_or_error->is_dict()) {
    std::move(callback).Run(
        SystemExtensionsInstallStatus::kFailedJsonErrorParsingManifest);
    return;
  }

  std::move(callback).Run(
      GetSystemExtensionFromValue(value_or_error->GetDict()));
}

InstallStatusOrSystemExtension
SystemExtensionsSandboxedUnpacker::GetSystemExtensionFromValue(
    const base::Value::Dict& parsed_manifest) {
  SystemExtension system_extension;
  system_extension.manifest = parsed_manifest.Clone();

  // Parse mandatory fields.

  // Parse id.
  const std::string* id_str = parsed_manifest.FindString(kIdKey);
  if (!id_str) {
    return SystemExtensionsInstallStatus::kFailedIdMissing;
  }
  absl::optional<SystemExtensionId> id = SystemExtension::StringToId(*id_str);
  if (!id.has_value()) {
    return SystemExtensionsInstallStatus::kFailedIdInvalid;
  }
  system_extension.id = id.value();

  // Parse type.
  const std::string* type_str = parsed_manifest.FindString(kTypeKey);
  if (!type_str) {
    return SystemExtensionsInstallStatus::kFailedTypeMissing;
  }
  SystemExtensionType* type = GetTypeFromString(*type_str);
  if (type == nullptr) {
    return SystemExtensionsInstallStatus::kFailedTypeInvalid;
  }
  system_extension.type = *type;

  // Parse base_url.
  const GURL base_url = GetBaseURL(*id_str, system_extension.type);
  // If both the type and id are valid, there is no possible way for the
  // base_url to be invalid.
  CHECK(base_url.is_valid());
  CHECK(SystemExtension::IsSystemExtensionOrigin(url::Origin::Create(base_url)))
      << base_url.spec();
  system_extension.base_url = base_url;

  // Parse service_worker_url.
  const std::string* service_worker_path =
      parsed_manifest.FindString(kServiceWorkerUrlKey);
  if (!service_worker_path) {
    return SystemExtensionsInstallStatus::kFailedServiceWorkerUrlMissing;
  }
  const GURL service_worker_url = base_url.Resolve(*service_worker_path);
  if (!service_worker_url.is_valid() || service_worker_url == base_url) {
    return SystemExtensionsInstallStatus::kFailedServiceWorkerUrlInvalid;
  }
  if (!url::IsSameOriginWith(base_url, service_worker_url)) {
    return SystemExtensionsInstallStatus::
        kFailedServiceWorkerUrlDifferentOrigin;
  }
  system_extension.service_worker_url = service_worker_url;

  // Parse name.
  // TODO(ortuno): Decide a set of invalid characters and remove them/fail
  // installation.
  const std::string* name = parsed_manifest.FindString(kNameKey);
  if (!name) {
    return SystemExtensionsInstallStatus::kFailedNameMissing;
  }

  if (name->empty()) {
    return SystemExtensionsInstallStatus::kFailedNameEmpty;
  }
  system_extension.name = *name;

  // Parse optional fields.

  // Parse short_name.
  const std::string* short_name_str = parsed_manifest.FindString(kShortNameKey);
  if (short_name_str && !short_name_str->empty()) {
    system_extension.short_name = *short_name_str;
  }

  return system_extension;
}

SystemExtensionsSandboxedUnpacker::IOHelper::~IOHelper() = default;

SystemExtensionsStatusOr<SystemExtensionsInstallStatus, std::string>
SystemExtensionsSandboxedUnpacker::IOHelper::ReadManifestInDirectory(
    const base::FilePath& system_extension_dir) {
  // Validate input |system_extension_dir|.
  if (system_extension_dir.value().empty() ||
      !base::DirectoryExists(system_extension_dir)) {
    return SystemExtensionsInstallStatus::kFailedDirectoryMissing;
  }

  base::FilePath manifest_path = system_extension_dir.Append(kManifestName);
  std::string manifest_contents;
  if (!base::ReadFileToString(manifest_path, &manifest_contents)) {
    return SystemExtensionsInstallStatus::kFailedManifestReadError;
  }

  return manifest_contents;
}

}  // namespace ash
