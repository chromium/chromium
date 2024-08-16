// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PERMISSIONS_PERMISSIONS_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_PERMISSIONS_PERMISSIONS_HELPERS_H_

#include <memory>
#include <string>

#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/url_pattern_set.h"

namespace extensions {

class PermissionSet;

namespace api {
namespace permissions {
struct Permissions;
}
}

namespace permissions_api_helpers {

// Converts the permission |set| to a permissions object.
std::unique_ptr<api::permissions::Permissions> PackPermissionSet(
    const PermissionSet& set);

// The result of unpacking the API permissions object.
struct UnpackPermissionSetResult {
  UnpackPermissionSetResult();
  ~UnpackPermissionSetResult();

  // API permissions that are in the extension's "required" permission set.
  APIPermissionSet required_apis;
  // Explicit hosts that are in the extension's "required" permission set.
  URLPatternSet required_explicit_hosts;
  // Scriptable hosts that are in the extension's "required" permission set.
  URLPatternSet required_scriptable_hosts;

  // API permissions that are in the extension's "optional" permission set.
  APIPermissionSet optional_apis;
  // Explicit hosts that are in the extension's "optional" permission set.
  URLPatternSet optional_explicit_hosts;

  // API permissions that were not listed in the extension's permissions.
  APIPermissionSet unlisted_apis;
  // Host permissions that were not listed in the extension's permissions.
  URLPatternSet unlisted_hosts;

  // Special case: restricted file:-scheme patterns. These are populated with
  // the patterns that are explicitly related to file:-schemes if the extension
  // does *not* have file access.
  // Consider unpacking ["<all_urls>", "file:///*"]:
  // - If the extension does *not* have file access:
  //   * <all_urls> will be unpacked normally, but will not include
  //     URLPattern::SCHEME_FILE as a valid scheme.
  //   * file:///* will be included in restricted_file_scheme_patterns, because
  //     it is restricted and cannot be granted without explicit access from the
  //     chrome://extensions page.
  // - If the extension *has* file access:
  //   * <all_urls> will be unpacked normally, and will include
  //     URLPattern::SCHEME_FILE as a valid scheme.
  //   * file:///* will be unpacked normally (|restricted_file_scheme_patterns|
  //     will be empty).
  URLPatternSet restricted_file_scheme_patterns;
};

// Parses the |permissions_input| object, and partitions permissions into the
// result. |required_permissions| and |optional_permissions| are the required
// and optional permissions specified in the extension's manifest, used for
// separating permissions. |has_file_access| is used to determine whether the
// file:-scheme is valid for host permissions. If file access is allowed,
// <all_urls> will match the file:-scheme (otherwise, it will not). Patterns
// that specifically specify "file:" will be parsed regardless (and placed into
// restricted_file_scheme_patterns if file access is disallowed). If an error is
// detected (e.g., an unknown API permission, invalid URL pattern, or API that
// doesn't support being optional), |error| is populated and null is returned.
std::unique_ptr<UnpackPermissionSetResult> UnpackPermissionSet(
    const api::permissions::Permissions& permissions_input,
    const PermissionSet& required_permissions,
    const PermissionSet& optional_permissions,
    bool has_file_access,
    std::string* error);

}  // namespace permissions_api_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PERMISSIONS_PERMISSIONS_HELPERS_H_
