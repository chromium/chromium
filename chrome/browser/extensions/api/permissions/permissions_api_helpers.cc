// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/permissions/permissions_api_helpers.h"

#include <stddef.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/common/extensions/api/permissions.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "extensions/common/url_pattern_set.h"

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

}  // namespace

std::unique_ptr<Permissions> PackPermissionSet(const PermissionSet& set) {
  std::unique_ptr<Permissions> permissions(new Permissions());

  permissions->permissions.reset(new std::vector<std::string>());
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

  permissions->origins.reset(new std::vector<std::string>());
  for (const URLPattern& pattern : set.explicit_hosts())
    permissions->origins->push_back(pattern.GetAsString());

  return permissions;
}

std::unique_ptr<const PermissionSet> UnpackPermissionSet(
    const Permissions& permissions,
    bool allow_file_access,
    std::string* error) {
  DCHECK(error);
  APIPermissionSet apis;
  std::vector<std::string>* permissions_list = permissions.permissions.get();
  if (permissions_list) {
    PermissionsInfo* info = PermissionsInfo::GetInstance();
    for (auto it = permissions_list->begin(); it != permissions_list->end();
         ++it) {
      // This is a compromise: we currently can't switch to a blend of
      // objects/strings all the way through the API. Until then, put this
      // processing here.
      // http://code.google.com/p/chromium/issues/detail?id=162042
      if (it->find(kDelimiter) != std::string::npos) {
        size_t delimiter = it->find(kDelimiter);
        std::string permission_name = it->substr(0, delimiter);
        std::string permission_arg = it->substr(delimiter + 1);

        std::unique_ptr<base::Value> permission_json =
            base::JSONReader::Read(permission_arg);
        if (!permission_json.get()) {
          *error = ErrorUtils::FormatErrorMessage(kInvalidParameter, *it);
          return NULL;
        }

        APIPermission* permission = NULL;

        // Explicitly check the permissions that accept arguments until the bug
        // referenced above is fixed.
        const APIPermissionInfo* usb_device_permission_info =
            info->GetByID(APIPermission::kUsbDevice);
        if (permission_name == usb_device_permission_info->name()) {
          permission = new UsbDevicePermission(usb_device_permission_info);
        } else {
          *error = kUnsupportedPermissionId;
          return NULL;
        }

        CHECK(permission);
        if (!permission->FromValue(permission_json.get(), NULL, NULL)) {
          *error = ErrorUtils::FormatErrorMessage(kInvalidParameter, *it);
          return NULL;
        }
        apis.insert(permission);
      } else {
        const APIPermissionInfo* permission_info = info->GetByName(*it);
        if (!permission_info) {
          *error = ErrorUtils::FormatErrorMessage(
              kUnknownPermissionError, *it);
          return NULL;
        }
        apis.insert(permission_info->id());
      }
    }
  }

  // TODO(rpaquay): We currently don't expose manifest permissions
  // to apps/extensions via the permissions API.
  ManifestPermissionSet manifest_permissions;

  URLPatternSet origins;
  if (permissions.origins.get()) {
    for (auto it = permissions.origins->begin();
         it != permissions.origins->end(); ++it) {
      int allowed_schemes = Extension::kValidHostPermissionSchemes;
      if (!allow_file_access)
        allowed_schemes &= ~URLPattern::SCHEME_FILE;
      URLPattern origin(allowed_schemes);
      URLPattern::ParseResult parse_result = origin.Parse(*it);
      if (URLPattern::ParseResult::kSuccess != parse_result) {
        *error = ErrorUtils::FormatErrorMessage(
            kInvalidOrigin,
            *it,
            URLPattern::GetParseResultString(parse_result));
        return NULL;
      }
      origins.AddPattern(origin);
    }
  }

  return std::make_unique<PermissionSet>(apis, manifest_permissions, origins,
                                         URLPatternSet());
}

}  // namespace permissions_api_helpers
}  // namespace extensions
