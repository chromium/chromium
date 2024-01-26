// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_INTERNAL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_INTERNAL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/extensions/extension_management.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/api_permission_set.h"

namespace base {
class Version;
}  // namespace base

namespace extensions {

class URLPatternSet;

namespace internal {

// Class to hold extension management settings for one or a group of
// extensions. Settings can be applied to an individual extension identified
// by an ID, a group of extensions with specific |update_url| or all
// extensions at once.
// The settings applied to all extensions are the default settings and can be
// overridden by per-extension or per-update-url settings.
// There are multiple fields in this class. Unspecified fields in per-extension
// and per-update-url settings will take the default fallback value, and do not
// inherit from default settings.
// Since update URL is not directly associated to extension ID, per-extension
// and per-update-url settings might be enforced at the same time, see per-field
// comments below for details.
// Some features do not support per-update-url setttings.
struct IndividualSettings {
  enum ParsingScope {
    // Parses the default settings.
    SCOPE_DEFAULT = 0,
    // Parses the settings for extensions with specified update URL in its
    // manifest.
    SCOPE_UPDATE_URL,
    // Parses the settings for an extension with specified extension ID.
    SCOPE_INDIVIDUAL,
  };

  IndividualSettings();
  explicit IndividualSettings(const IndividualSettings* default_settings);

  IndividualSettings(const IndividualSettings&) = delete;
  IndividualSettings& operator=(const IndividualSettings&) = delete;

  ~IndividualSettings();

  void Reset();

  // Parses the individual settings. |dict| is a sub-dictionary in extension
  // management preference and |scope| represents the applicable range of the
  // settings, a single extension, a group of extensions or default settings.
  // Note that in case of parsing errors, |this| will NOT be left untouched.
  // This method is required to be called for SCOPE_DEFAULT first, then
  // for SCOPE_INDIVIDUAL and SCOPE_UPDATE_URL.
  bool Parse(const base::Value::Dict& dict, ParsingScope scope);

  // Extension installation mode. Setting this to INSTALLATION_FORCED or
  // INSTALLATION_RECOMMENDED will enable extension auto-loading (only
  // applicable to single extension), and in this case the |update_url| must
  // be specified, containing the update URL for this extension.
  // Note that |update_url| will be ignored for INSTALLATION_ALLOWED and
  // INSTALLATION_BLOCKED installation mode.
  // This setting will NOT merge from the default settings. Any settings from
  // the default settings that should be applied to an individual extension
  // should be re-declared.
  // In case this setting is specified in both per-extensions and
  // per-update-url settings, per-extension settings will override
  // per-update-url settings.
  ExtensionManagement::InstallationMode installation_mode;
  std::string update_url;

  // Boolean to indicate whether the update URL of the extension/app is
  // overridden by the policy or not. It can be true only for extensions/apps
  // which are marked as |force_installed|.
  bool override_update_url{false};

  // Permissions block list for extensions. This setting won't grant permissions
  // to extensions automatically. Instead, this setting will provide a list of
  // blocked permissions for each extension. That is, if an extension requires a
  // permission which has been blocklisted, this extension will not be allowed
  // to load. And if it contains a blocked permission as optional requirement,
  // it will be allowed to load (of course, with permission granted from user if
  // necessary), but conflicting permissions will be dropped.
  // This setting will NOT merge from the default settings. Any settings from
  // the default settings that should be applied to an individual extension
  // should be re-declared.
  // In case this setting is specified in both per-extensions and per-update-url
  // settings, both settings will be enforced.
  APIPermissionSet blocked_permissions;

  // This setting will provide a list of hosts that are blocked for each
  // extension at runtime. That is, if an extension attempts to use an API
  // call which requires a host permission specified in policy_blocked_hosts
  // it will fail no matter which host permissions are declared in the
  // extension manifest. This setting will NOT merge from the default settings.
  // Either the default settings will be applied, or an extension specific
  // setting.
  // If a URL is specified in the policy_allowed_hosts, and in the
  // policy_blocked_hosts, the policy_allowed_hosts wins and the call will be
  // allowed.
  // This setting is only supported per-extensions or default
  // (per-update-url not supported)
  URLPatternSet policy_blocked_hosts;

  // This setting will provide a list of hosts that are exempted from the
  // policy_blocked_hosts setting and may be used at runtime. That is,
  // if an extension attempts to use an API call which requires a host
  // permission that was blocked using policy_blocked_hosts it will
  // fail unless also declared here.
  // A generic pattern may be declared in policy_blocked_hosts and a
  // more specific pattern declared here. For example, if we block
  // "*://*.example.com/*" with policy_blocked_hosts we can then
  // allow "http://good.example.com/*" in policy_allowed_hosts.
  // This setting will NOT merge from the default settings. Either the
  // default settings will be applied, or an extension specific setting.
  // If a URL is specified in policy_blocked_hosts, and in
  // policy_allowed_hosts, the allowed list wins.
  // This setting is only supported per-extensions or default
  // (per-update-url not supported)
  URLPatternSet policy_allowed_hosts;

  // Minimum version required for an extensions, applies to per-extension
  // settings only. Extension (with specified extension ID) with version older
  // than the specified minimum version will be disabled.
  std::unique_ptr<base::Version> minimum_version_required;

  // Allows the admin to provide text that will be displayed to the user in the
  // chrome webstore if installation is blocked. This is plain text and will not
  // support any HTML, links, or anything special. This can be used to direct
  // users to company information about acceptable extensions, ways to request
  // exceptions etc. This string is limited to 1000 characters.
  std::string blocked_install_message;

  // Allows admins to control whether the extension icon should be pinned to
  // the toolbar next to the omnibar. If it is pinned, the icon is visible at
  // all times.
  ExtensionManagement::ToolbarPinMode toolbar_pin =
      ExtensionManagement::ToolbarPinMode::kDefaultUnpinned;

  // Boolean to indicate whether the extension can navigate to file URLs.
  bool file_url_navigation_allowed{false};
};

// Global extension management settings, applicable to all extensions.
struct GlobalSettings {
  enum class ManifestV2Setting {
    kDefault = 0,
    kDisabled,
    kEnabled,
    kEnabledForForceInstalled,
  };

  enum class UnpublishedAvailability {
    kAllowUnpublished = 0,
    kDisableUnpublished = 1,
  };

  GlobalSettings();

  GlobalSettings(const GlobalSettings&) = delete;
  GlobalSettings& operator=(const GlobalSettings&) = delete;

  ~GlobalSettings();

  void Reset();

  // Settings specifying which URLs are allowed to install extensions, will be
  // enforced only if |has_restricted_install_sources| is set to true.
  std::optional<URLPatternSet> install_sources;

  // Settings specifying all allowed app/extension types, will be enforced
  // only of |has_restricted_allowed_types| is set to true.
  std::optional<std::vector<Manifest::Type>> allowed_types;

  // An enum setting indicates if manifest v2 is allowed.
  ManifestV2Setting manifest_v2_setting = ManifestV2Setting::kDefault;

  UnpublishedAvailability unpublished_availability_setting =
      UnpublishedAvailability::kAllowUnpublished;
};

}  // namespace internal

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_INTERNAL_H_
