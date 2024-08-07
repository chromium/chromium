// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/permissions/permissions_helpers.h"

#include <stddef.h>

#include <memory>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/common/extensions/api/permissions.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"

namespace extensions {

using api::permissions::Permissions;

namespace permissions_api_helpers {

namespace {

const char kDelimiter[] = "|";
const char kInvalidParameter[] =
    "Invalid argument for permission '*'.";
const char kInvalidOrigin[] =
    "Invalid value for origin pattern *: *";
const char kUnknownPermissionError[] =
    "'*' is not a recognized permission.";
const char kUnsupportedPermissionId[] =
    "Only the usbDevices permission supports arguments.";

// Extracts an API permission that supports arguments. In practice, this is
// restricted to the UsbDevicePermission.
std::unique_ptr<APIPermission> UnpackPermissionWithArguments(
    std::string_view permission_name,
    std::string_view permission_arg,
    const std::string& permission_str,
    std::string* error) {
  std::optional<base::Value> permission_json =
      base::JSONReader::Read(permission_arg);
  if (!permission_json) {
    *error = ErrorUtils::FormatErrorMessage(kInvalidParameter, permission_str);
    return nullptr;
  }

  std::unique_ptr<APIPermission> permission;

  // Explicitly check the permissions that accept arguments until
  // https://crbug.com/162042 is fixed.
  const APIPermissionInfo* usb_device_permission_info =
      PermissionsInfo::GetInstance()->GetByID(
          mojom::APIPermissionID::kUsbDevice);
  if (permission_name == usb_device_permission_info->name()) {
    permission =
        std::make_unique<UsbDevicePermission>(usb_device_permission_info);
  } else {
    *error = kUnsupportedPermissionId;
    return nullptr;
  }

  CHECK(permission);
  if (!permission->FromValue(&permission_json.value(), nullptr, nullptr)) {
    *error = ErrorUtils::FormatErrorMessage(kInvalidParameter, permission_str);
    return nullptr;
  }

  return permission;
}

// A helper method to unpack API permissions from the list in
// |permissions_input|, and populate the appropriate fields of |result|.
// Returns true on success; on failure, returns false and populates |error|.
bool UnpackAPIPermissions(const std::vector<std::string>& permissions_input,
                          const PermissionSet& required_permissions,
                          const PermissionSet& optional_permissions,
                          UnpackPermissionSetResult* result,
                          std::string* error) {
  PermissionsInfo* info = PermissionsInfo::GetInstance();
  APIPermissionSet apis;
  // Iterate over the inputs and find the corresponding API permissions.
  for (const auto& permission_str : permissions_input) {
    // This is a compromise: we currently can't switch to a blend of
    // objects/strings all the way through the API. Until then, put this
    // processing here.
    // http://code.google.com/p/chromium/issues/detail?id=162042
    size_t delimiter = permission_str.find(kDelimiter);
    if (delimiter != std::string::npos) {
      std::string_view permission_piece(permission_str);
      std::unique_ptr<APIPermission> permission = UnpackPermissionWithArguments(
          permission_piece.substr(0, delimiter),
          permission_piece.substr(delimiter + 1), permission_str, error);
      if (!permission)
        return false;

      apis.insert(std::move(permission));
    } else {
      const APIPermissionInfo* permission_info =
          info->GetByName(permission_str);
      if (!permission_info) {
        *error = ErrorUtils::FormatErrorMessage(kUnknownPermissionError,
                                                permission_str);
        return false;
      }
      apis.insert(permission_info->id());
    }
  }

  // Validate and partition the parsed APIs.
  for (const auto* api_permission : apis) {
    if (required_permissions.apis().count(api_permission->id())) {
      result->required_apis.insert(api_permission->Clone());
      continue;
    }

    if (!optional_permissions.apis().count(api_permission->id())) {
      result->unlisted_apis.insert(api_permission->Clone());
      continue;
    }

    // Permissions that don't support being optional are filtered out during
    // manifest parsing, so between that and filtering out APIs that aren't in
    // the optional set, all of these should support being optional.
    DCHECK(api_permission->info()->supports_optional());

    result->optional_apis.insert(api_permission->Clone());
  }

  return true;
}

// A helper method to unpack host permissions from the list in
// |permissions_input|, and populate the appropriate fields of |result|.
// Returns true on success; on failure, returns false and populates |error|.
bool UnpackOriginPermissions(const std::vector<std::string>& origins_input,
                             const PermissionSet& required_permissions,
                             const PermissionSet& optional_permissions,
                             bool allow_file_access,
                             UnpackPermissionSetResult* result,
                             std::string* error) {
  int user_script_schemes = UserScript::ValidUserScriptSchemes();
  int explicit_schemes = Extension::kValidHostPermissionSchemes;

  auto filter_schemes = [allow_file_access](URLPattern* pattern) {
    // NOTE: We use pattern->valid_schemes() here (instead of
    // |user_script_schemes| or |explicit_schemes|) because
    // URLPattern::Parse() can mutate the valid schemes for a pattern, and we
    // don't want to override those changes.
    int valid_schemes = pattern->valid_schemes();

    // We disallow the chrome:-scheme unless the pattern is explicitly
    // "chrome://..." - that is, <all_urls> should not match the chrome:-scheme.
    // Patterns which explicitly specify the chrome:-scheme are safe, since
    // manifest parsing won't allow them unless the kExtensionsOnChromeURLs
    // switch is enabled.
    // Note that we don't check PermissionsData::AllUrlsIncludesChromeUrls()
    // here, since that's only needed for Chromevox (which doesn't use optional
    // permissions).
    if (pattern->scheme() != content::kChromeUIScheme)
      valid_schemes &= ~URLPattern::SCHEME_CHROMEUI;

    // Similarly, <all_urls> should only match file:-scheme URLs if file access
    // is granted.
    if (!allow_file_access && pattern->scheme() != url::kFileScheme)
      valid_schemes &= ~URLPattern::SCHEME_FILE;

    if (valid_schemes != pattern->valid_schemes())
      pattern->SetValidSchemes(valid_schemes);
  };

  for (const auto& origin_str : origins_input) {
    URLPattern explicit_origin(explicit_schemes);
    URLPattern::ParseResult parse_result = explicit_origin.Parse(origin_str);
    if (URLPattern::ParseResult::kSuccess != parse_result) {
      *error = ErrorUtils::FormatErrorMessage(
          kInvalidOrigin, origin_str,
          URLPattern::GetParseResultString(parse_result));
      return false;
    }

    filter_schemes(&explicit_origin);

    if ((explicit_origin.valid_schemes() & URLPattern::SCHEME_FILE) &&
        !allow_file_access) {
      // This should only happen with patterns that specify file schemes;
      // otherwise they should have been filtered out in filter_schemes().
      DCHECK_EQ(url::kFileScheme, explicit_origin.scheme());
      result->restricted_file_scheme_patterns.AddPattern(explicit_origin);
      // Don't add the pattern to any other set to indicate that it can't be
      // requested/granted/contained.
      continue;
    }

    bool used_origin = false;
    if (required_permissions.explicit_hosts().ContainsPattern(
            explicit_origin)) {
      used_origin = true;
      result->required_explicit_hosts.AddPattern(explicit_origin);
    } else if (optional_permissions.explicit_hosts().ContainsPattern(
                   explicit_origin)) {
      used_origin = true;
      result->optional_explicit_hosts.AddPattern(explicit_origin);
    }

    URLPattern scriptable_origin(user_script_schemes);
    if (scriptable_origin.Parse(origin_str) ==
        URLPattern::ParseResult::kSuccess) {
      filter_schemes(&scriptable_origin);
      if (required_permissions.scriptable_hosts().ContainsPattern(
              scriptable_origin)) {
        used_origin = true;
        result->required_scriptable_hosts.AddPattern(scriptable_origin);
      }
    }

    if (!used_origin)
      result->unlisted_hosts.AddPattern(explicit_origin);
  }

  return true;
}

}  // namespace

UnpackPermissionSetResult::UnpackPermissionSetResult() = default;
UnpackPermissionSetResult::~UnpackPermissionSetResult() = default;

std::unique_ptr<Permissions> PackPermissionSet(const PermissionSet& set) {
  std::unique_ptr<Permissions> permissions(new Permissions());

  permissions->permissions.emplace();
  for (const APIPermission* api : set.apis()) {
    std::unique_ptr<base::Value> value(api->ToValue());
    if (!value) {
      permissions->permissions->push_back(api->name());
    } else {
      std::string name(api->name());
      std::string json;
      base::JSONWriter::Write(*value, &json);
      permissions->permissions->push_back(name + kDelimiter + json);
    }
  }

  // TODO(rpaquay): We currently don't expose manifest permissions
  // to apps/extensions via the permissions API.

  permissions->origins.emplace();
  for (const URLPattern& pattern : set.effective_hosts())
    permissions->origins->push_back(pattern.GetAsString());

  return permissions;
}

std::unique_ptr<UnpackPermissionSetResult> UnpackPermissionSet(
    const Permissions& permissions_input,
    const PermissionSet& required_permissions,
    const PermissionSet& optional_permissions,
    bool allow_file_access,
    std::string* error) {
  DCHECK(error);

  // TODO(rpaquay): We currently don't expose manifest permissions
  // to apps/extensions via the permissions API.

  auto result = std::make_unique<UnpackPermissionSetResult>();

  if (permissions_input.permissions &&
      !UnpackAPIPermissions(*permissions_input.permissions,
                            required_permissions, optional_permissions,
                            result.get(), error)) {
    return nullptr;
  }

  if (permissions_input.origins &&
      !UnpackOriginPermissions(*permissions_input.origins, required_permissions,
                               optional_permissions, allow_file_access,
                               result.get(), error)) {
    return nullptr;
  }

  return result;
}

}  // namespace permissions_api_helpers
}  // namespace extensions
