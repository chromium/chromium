// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_sandboxed_unpacker.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
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
const constexpr char kCompanionWebAppUrlKey[] = "companion_web_app_url";
const constexpr char kServiceWorkerUrlKey[] = "service_worker_url";

GURL GetBaseURL(const std::string& id, SystemExtensionType type) {
  // The host is made of up a System Extension prefix based on the type and
  // the System Extension Id.

  base::StringPiece host_prefix;
  switch (type) {
    case SystemExtensionType::kEcho:
      static constexpr char kSystemExtensionEchoPrefix[] =
          "system-extension-echo-";
      host_prefix = kSystemExtensionEchoPrefix;
      break;
  }
  const std::string host = base::StrCat({host_prefix, id});
  return GURL(base::StrCat({content::kChromeUIUntrustedScheme,
                            url::kStandardSchemeSeparator, host}));
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
  if (value_or_error.error.has_value()) {
    std::move(callback).Run(
        SystemExtensionsInstallStatus::kFailedJsonErrorParsingManifest);
    return;
  }

  base::Value& parsed_manifest = value_or_error.value.value();

  SystemExtension system_extension;

  // Parse mandatory fields.

  // Parse id.
  std::string* id_str = parsed_manifest.FindStringKey(kIdKey);
  if (!id_str) {
    std::move(callback).Run(SystemExtensionsInstallStatus::kFailedIdMissing);
    return;
  }
  absl::optional<SystemExtensionId> id = SystemExtension::StringToId(*id_str);
  if (!id.has_value()) {
    std::move(callback).Run(SystemExtensionsInstallStatus::kFailedIdInvalid);
    return;
  }
  system_extension.id = id.value();

  // Parse type.
  std::string* type_str = parsed_manifest.FindStringKey(kTypeKey);
  if (!type_str) {
    std::move(callback).Run(SystemExtensionsInstallStatus::kFailedTypeMissing);
    return;
  }
  if (base::CompareCaseInsensitiveASCII("echo", *type_str) != 0) {
    std::move(callback).Run(SystemExtensionsInstallStatus::kFailedTypeInvalid);
    return;
  }
  system_extension.type = SystemExtensionType::kEcho;

  // Parse base_url.
  const GURL base_url = GetBaseURL(*id_str, system_extension.type);
  // If both the type and id are valid, there is no possible way for the
  // base_url to be invalid.
  CHECK(base_url.is_valid());
  CHECK(SystemExtension::IsSystemExtensionOrigin(url::Origin::Create(base_url)))
      << base_url.spec();
  system_extension.base_url = base_url;

  // Parse service_worker_url.
  std::string* service_worker_path =
      parsed_manifest.FindStringKey(kServiceWorkerUrlKey);
  if (!service_worker_path) {
    std::move(callback).Run(
        SystemExtensionsInstallStatus::kFailedServiceWorkerUrlMissing);
    return;
  }
  const GURL service_worker_url = base_url.Resolve(*service_worker_path);
  if (!service_worker_url.is_valid() || service_worker_url == base_url) {
    std::move(callback).Run(
        SystemExtensionsInstallStatus::kFailedServiceWorkerUrlInvalid);
    return;
  }
  if (!url::IsSameOriginWith(base_url, service_worker_url)) {
    std::move(callback).Run(
        SystemExtensionsInstallStatus::kFailedServiceWorkerUrlDifferentOrigin);
    return;
  }
  system_extension.service_worker_url = service_worker_url;

  // Parse name.
  // TODO(ortuno): Decide a set of invalid characters and remove them/fail
  // installation.
  std::string* name = parsed_manifest.FindStringKey(kNameKey);
  if (!name) {
    std::move(callback).Run(SystemExtensionsInstallStatus::kFailedNameMissing);
    return;
  }

  if (name->empty()) {
    std::move(callback).Run(SystemExtensionsInstallStatus::kFailedNameEmpty);
    return;
  }
  system_extension.name = *name;

  // Parse optional fields.

  // Parse short_name.
  std::string* short_name_str = parsed_manifest.FindStringKey(kShortNameKey);
  if (short_name_str && !short_name_str->empty()) {
    system_extension.short_name = *short_name_str;
  }

  // Parse companion_web_app_url.
  if (std::string* companion_web_app_url_str =
          parsed_manifest.FindStringKey(kCompanionWebAppUrlKey)) {
    GURL companion_web_app_url(*companion_web_app_url_str);
    if (companion_web_app_url.is_valid() &&
        companion_web_app_url.SchemeIs(url::kHttpsScheme)) {
      system_extension.companion_web_app_url = companion_web_app_url;
    } else {
      LOG(WARNING) << "Companion Web App URL is invalid: "
                   << companion_web_app_url;
    }
  }

  std::move(callback).Run(std::move(system_extension));
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
