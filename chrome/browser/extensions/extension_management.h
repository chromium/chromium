// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

class GURL;
class PrefService;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

namespace internal {

struct IndividualSettings;
struct GlobalSettings;

}  // namespace internal

class APIPermissionSet;
class Extension;
class PermissionSet;

// Tracks the management policies that affect extensions and provides interfaces
// for observing and obtaining the global settings for all extensions, as well
// as per-extension settings.
class ExtensionManagement : public KeyedService {
 public:
  // Observer class for extension management settings changes.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the extension management settings change.
    virtual void OnExtensionManagementSettingsChanged() = 0;
  };

  // Installation mode for extensions, default is INSTALLATION_ALLOWED.
  // * INSTALLATION_ALLOWED: Extension can be installed.
  // * INSTALLATION_BLOCKED: Extension cannot be installed.
  // * INSTALLATION_FORCED: Extension will be installed automatically
  //                        and cannot be disabled.
  // * INSTALLATION_RECOMMENDED: Extension will be installed automatically but
  //                             can be disabled.
  // * INSTALLATION_REMOVED:  Extension cannot be installed and will be
  //                          automatically removed.
  enum InstallationMode {
    INSTALLATION_ALLOWED = 0,
    INSTALLATION_BLOCKED,
    INSTALLATION_FORCED,
    INSTALLATION_RECOMMENDED,
    INSTALLATION_REMOVED,
  };

  explicit ExtensionManagement(Profile* profile);
  ~ExtensionManagement() override;

  // KeyedService implementations:
  void Shutdown() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Get the list of ManagementPolicy::Provider controlled by extension
  // management policy settings.
  const std::vector<std::unique_ptr<ManagementPolicy::Provider>>& GetProviders()
      const;

  // Checks if extensions are blacklisted by default, by policy. When true,
  // this means that even extensions without an ID should be blacklisted (e.g.
  // from the command line, or when loaded as an unpacked extension).
  bool BlacklistedByDefault() const;

  // Returns installation mode for an extension.
  InstallationMode GetInstallationMode(const Extension* extension) const;

  // Returns installation mode for an extension with id |id| and updated with
  // |update_url|.
  InstallationMode GetInstallationMode(const ExtensionId& extension_id,
                                       const std::string& update_url) const;

  // Returns the force install list, in format specified by
  // ExternalPolicyLoader::AddExtension().
  std::unique_ptr<base::DictionaryValue> GetForceInstallList() const;

  // Like GetForceInstallList(), but returns recommended install list instead.
  std::unique_ptr<base::DictionaryValue> GetRecommendedInstallList() const;

  // Returns |true| if there is at least one extension with
  // |INSTALLATION_ALLOWED| as installation mode. This excludes force installed
  // extensions.
  bool HasWhitelistedExtension() const;

  // Returns if an extension with id |id| is explicitly allowed by enterprise
  // policy or not.
  bool IsInstallationExplicitlyAllowed(const ExtensionId& id) const;

  // Returns if an extension with id |id| is explicitly blocked by enterprise
  // policy or not.
  bool IsInstallationExplicitlyBlocked(const ExtensionId& id) const;

  // Returns true if an extension download should be allowed to proceed.
  bool IsOffstoreInstallAllowed(const GURL& url,
                                const GURL& referrer_url) const;

  // Returns true if an extension with manifest type |manifest_type| and
  // id |extension_id| is allowed to be installed.
  bool IsAllowedManifestType(Manifest::Type manifest_type,
                             const std::string& extension_id) const;

  // Returns the list of blocked API permissions for |extension|.
  APIPermissionSet GetBlockedAPIPermissions(const Extension* extension) const;

  // Returns the list of hosts blocked by policy for |extension|.
  const URLPatternSet& GetPolicyBlockedHosts(const Extension* extension) const;

  // Returns the hosts exempted by policy from the PolicyBlockedHosts for
  // |extension|.
  const URLPatternSet& GetPolicyAllowedHosts(const Extension* extension) const;

  // Returns the list of hosts blocked by policy for Default scope. This can be
  // overridden by an individual scope which is queried via
  // GetPolicyBlockedHosts.
  const URLPatternSet& GetDefaultPolicyBlockedHosts() const;

  // Returns the hosts exempted by policy from PolicyBlockedHosts for
  // the default scope. This can be overridden by an individual scope which is
  // queries via GetPolicyAllowedHosts. This should only be used to
  // initialize a new renderer.
  const URLPatternSet& GetDefaultPolicyAllowedHosts() const;

  // Checks if an |extension| has its own runtime_blocked_hosts or
  // runtime_allowed_hosts defined in the individual scope of the
  // ExtensionSettings policy.
  // Returns false if an individual scoped setting isn't defined.
  bool UsesDefaultPolicyHostRestrictions(const Extension* extension) const;

  // Checks if a URL is on the blocked host permissions list for a specific
  // extension.
  bool IsPolicyBlockedHost(const Extension* extension, const GURL& url) const;

  // Returns blocked permission set for |extension|.
  std::unique_ptr<const PermissionSet> GetBlockedPermissions(
      const Extension* extension) const;

  // If the extension is blocked from install and a custom error message
  // was defined returns it. Otherwise returns an empty string. The maximum
  // string length is 1000 characters.
  const std::string BlockedInstallMessage(const ExtensionId& id) const;

  // Returns true if every permission in |perms| is allowed for |extension|.
  bool IsPermissionSetAllowed(const Extension* extension,
                              const PermissionSet& perms) const;

  // Returns true if |extension| meets the minimum required version set for it.
  // If there is no such requirement set for it, returns true as well.
  // If false is returned and |required_version| is not null, the minimum
  // required version is returned.
  bool CheckMinimumVersion(const Extension* extension,
                           std::string* required_version) const;

 private:
  using SettingsIdMap =
      std::unordered_map<ExtensionId,
                         std::unique_ptr<internal::IndividualSettings>>;
  using SettingsUpdateUrlMap =
      std::unordered_map<std::string,
                         std::unique_ptr<internal::IndividualSettings>>;
  friend class ExtensionManagementServiceTest;

  // Load all extension management preferences from |pref_service|, and
  // refresh the settings.
  void Refresh();

  // Load preference with name |pref_name| and expected type |expected_type|.
  // If |force_managed| is true, only loading from the managed preference store
  // is allowed. Returns NULL if the preference is not present, not allowed to
  // be loaded from or has the wrong type.
  const base::Value* LoadPreference(const char* pref_name,
                                    bool force_managed,
                                    base::Value::Type expected_type) const;

  void OnExtensionPrefChanged();
  void NotifyExtensionManagementPrefChanged();

  // Helper to return an extension install list, in format specified by
  // ExternalPolicyLoader::AddExtension().
  std::unique_ptr<base::DictionaryValue> GetInstallListByMode(
      InstallationMode installation_mode) const;

  // Helper to update |extension_dict| for forced installs.
  void UpdateForcedExtensions(const base::DictionaryValue* extension_dict);

  // Helper to update |settings_by_id_| for forced cloud reporting extension.
  void UpdateForcedCloudReportingExtension();

  // Returns true if cloud reporting policy is enabled.
  bool IsCloudReportingPolicyEnabled() const;

  // Helper function to access |settings_by_id_| with |id| as key.
  // Adds a new IndividualSettings entry to |settings_by_id_| if none exists for
  // |id| yet.
  internal::IndividualSettings* AccessById(const ExtensionId& id);

  // Similar to AccessById(), but access |settings_by_update_url_| instead.
  internal::IndividualSettings* AccessByUpdateUrl(
      const std::string& update_url);

  // A map containing all IndividualSettings applied to an individual extension
  // identified by extension ID. The extension ID is used as index key of the
  // map.
  SettingsIdMap settings_by_id_;

  // Similar to |settings_by_id_|, but contains the settings for a group of
  // extensions with same update URL. The update url itself is used as index
  // key for the map.
  SettingsUpdateUrlMap settings_by_update_url_;

  // The default IndividualSettings.
  // For extension settings applied to an individual extension (identified by
  // extension ID) or a group of extension (with specified extension update
  // URL), all unspecified part will take value from |default_settings_|.
  // For all other extensions, all settings from |default_settings_| will be
  // enforced.
  std::unique_ptr<internal::IndividualSettings> default_settings_;

  // Extension settings applicable to all extensions.
  std::unique_ptr<internal::GlobalSettings> global_settings_;

  Profile* const profile_ = nullptr;
  PrefService* pref_service_ = nullptr;
  bool is_signin_profile_ = false;

  base::ObserverList<Observer, true>::Unchecked observer_list_;
  PrefChangeRegistrar pref_change_registrar_;
  std::vector<std::unique_ptr<ManagementPolicy::Provider>> providers_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionManagement);
};

class ExtensionManagementFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionManagement* GetForBrowserContext(
      content::BrowserContext* context);
  static ExtensionManagementFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ExtensionManagementFactory>;

  ExtensionManagementFactory();
  ~ExtensionManagementFactory() override;

  // BrowserContextKeyedServiceExtensionManagementFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionManagementFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_H_
