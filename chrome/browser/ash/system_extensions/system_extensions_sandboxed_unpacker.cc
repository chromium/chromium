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
#include "content/public/common/url_constants.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

namespace {

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

void SystemExtensionsSandboxedUnpacker::GetSystemExtensionFromString(
    base::StringPiece system_extension_manifest_string,
    GetSystemExtensionFromStringCallback callback) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      std::string(system_extension_manifest_string),
      base::BindOnce(
          &SystemExtensionsSandboxedUnpacker::OnSystemExtensionManifestParsed,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemExtensionsSandboxedUnpacker::OnSystemExtensionManifestParsed(
    GetSystemExtensionFromStringCallback callback,
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  if (value_or_error.error.has_value()) {
    std::move(callback).Run(Status::kFailedJsonErrorParsingManifest, nullptr);
    return;
  }

  base::Value& parsed_manifest = value_or_error.value.value();

  auto system_extension = std::make_unique<SystemExtension>();

  // Parse mandatory fields.

  // Parse id.
  std::string* id_str = parsed_manifest.FindStringKey(kIdKey);
  if (!id_str) {
    std::move(callback).Run(Status::kFailedIdMissing, nullptr);
    return;
  }
  absl::optional<SystemExtensionId> id = SystemExtension::StringToId(*id_str);
  if (!id.has_value()) {
    std::move(callback).Run(Status::kFailedIdInvalid, nullptr);
    return;
  }
  system_extension->id = id.value();

  // Parse type.
  std::string* type_str = parsed_manifest.FindStringKey(kTypeKey);
  if (!type_str) {
    std::move(callback).Run(Status::kFailedTypeMissing, nullptr);
    return;
  }
  if (base::CompareCaseInsensitiveASCII("echo", *type_str) != 0) {
    std::move(callback).Run(Status::kFailedTypeInvalid, nullptr);
    return;
  }
  system_extension->type = SystemExtensionType::kEcho;

  // Parse base_url.
  const GURL base_url = GetBaseURL(*id_str, system_extension->type);
  // If both the type and id are valid, there is no possible way for the
  // base_url to be invalid.
  CHECK(base_url.is_valid());
  system_extension->base_url = base_url;

  // Parse service_worker_url.
  std::string* service_worker_path =
      parsed_manifest.FindStringKey(kServiceWorkerUrlKey);
  if (!service_worker_path) {
    std::move(callback).Run(Status::kFailedServiceWorkerUrlMissing, nullptr);
    return;
  }
  const GURL service_worker_url = base_url.Resolve(*service_worker_path);
  if (!service_worker_url.is_valid() || service_worker_url == base_url) {
    std::move(callback).Run(Status::kFailedServiceWorkerUrlInvalid, nullptr);
    return;
  }
  if (!url::IsSameOriginWith(base_url, service_worker_url)) {
    std::move(callback).Run(Status::kFailedServiceWorkerUrlDifferentOrigin,
                            nullptr);
    return;
  }
  system_extension->service_worker_url = service_worker_url;

  // Parse name.
  // TODO(ortuno): Decide a set of invalid characters and remove them/fail
  // installation.
  std::string* name = parsed_manifest.FindStringKey(kNameKey);
  if (!name) {
    std::move(callback).Run(Status::kFailedNameMissing, nullptr);
    return;
  }

  if (name->empty()) {
    std::move(callback).Run(Status::kFailedNameEmpty, nullptr);
    return;
  }
  system_extension->name = *name;

  // Parse optional fields.

  // Parse short_name.
  std::string* short_name_str = parsed_manifest.FindStringKey(kShortNameKey);
  if (short_name_str && !short_name_str->empty()) {
    system_extension->short_name = *short_name_str;
  }

  // Parse companion_web_app_url.
  if (std::string* companion_web_app_url_str =
          parsed_manifest.FindStringKey(kCompanionWebAppUrlKey)) {
    GURL companion_web_app_url(*companion_web_app_url_str);
    if (companion_web_app_url.is_valid() &&
        companion_web_app_url.SchemeIs(url::kHttpsScheme)) {
      system_extension->companion_web_app_url = companion_web_app_url;
    } else {
      LOG(WARNING) << "Companion Web App URL is invalid: "
                   << companion_web_app_url;
    }
  }

  std::move(callback).Run(Status::kOk, std::move(system_extension));
}
