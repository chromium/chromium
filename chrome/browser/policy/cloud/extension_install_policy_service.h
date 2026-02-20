// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_SERVICE_H_

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_client_types.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/management_policy.h"

namespace extensions {
class Extension;
}  // namespace extensions

class Profile;

namespace policy {

struct ExtensionIdAndVersion;
class CloudPolicyManager;

// A keyed service that provides access to the extension install policy.
class ExtensionInstallPolicyService
    : public KeyedService,
      public extensions::ManagementPolicy::Provider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnExtensionInstallPolicyUpdated() = 0;
  };

  ~ExtensionInstallPolicyService() override = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // To call before installing an extension
  // `extension_id_and_version` is an extension ID and version pair formatted
  // as "id@version".
  virtual void CanInstallExtension(
      const ExtensionIdAndVersion& extension_id_and_version,
      base::OnceCallback<void(bool)>) const = 0;

  virtual std::optional<bool> IsExtensionAllowed(
      const ExtensionIdAndVersion& extension_id_and_version) const = 0;
};

// A keyed service that provides access to the extension install policy.
class ExtensionInstallPolicyServiceImpl
    : public ExtensionInstallPolicyService,
      public PolicyService::Observer,
      public PolicyTypeToFetch::ExtensionsProvider {
 public:
  explicit ExtensionInstallPolicyServiceImpl(Profile* profile);
  ~ExtensionInstallPolicyServiceImpl() override;

  ExtensionInstallPolicyServiceImpl(const ExtensionInstallPolicyServiceImpl&) =
      delete;
  ExtensionInstallPolicyServiceImpl& operator=(
      const ExtensionInstallPolicyServiceImpl&) = delete;

  // ExtensionInstallPolicyService impl:
  void CanInstallExtension(
      const ExtensionIdAndVersion& extension_id_and_version,
      base::OnceCallback<void(bool)>) const override;
  std::optional<bool> IsExtensionAllowed(
      const ExtensionIdAndVersion& extension_id_and_version) const override;

  // extensions::ManagementPolicy::Provider implementation:
  std::string GetDebugPolicyProviderName() const override;
  void UserMayInstall(
      scoped_refptr<const extensions::Extension> extension,
      base::OnceCallback<void(extensions::ManagementPolicy::Decision)> callback)
      const override;
  bool UserMayLoad(const extensions::Extension* extension,
                   std::u16string* error) const override;
  bool MustRemainDisabled(
      const extensions::Extension* extension,
      extensions::disable_reason::DisableReason* reason) const override;

  // PolicyTypeToFetch::ExtensionsProvider:
  std::set<ExtensionIdAndVersion> GetExtensions() override;

  void AddObserver(ExtensionInstallPolicyService::Observer* observer) override;
  void RemoveObserver(
      ExtensionInstallPolicyService::Observer* observer) override;

  // PolicyService::Observer impl:
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;

  // KeyedService impl:
  void Shutdown() override;

 private:
  class ClientInitializationWaiter;

  struct PolicyManagerInfo {
    raw_ref<CloudPolicyManager> manager;
    std::string policy_type;
  };

  std::vector<PolicyManagerInfo> GetPolicyManagerInfos() const;
  std::vector<PolicyManagerInfo> GetConnectedPolicyManagerInfos() const;

  // Adds or removes from CloudPolicyClient::types_to_fetch_ based on
  // the current value of the pref
  // `kExtensionInstallCloudPolicyChecksEnabled`.
  void OnPolicyChecksEnabledChanged();

  void NotifyExtensionInstallPolicyUpdated();

  void OnCloudPolicyManagerReady(CloudPolicyManager* manager);

  base::ObserverList<ExtensionInstallPolicyService::Observer> observers_;
  raw_ref<Profile> profile_;

  base::flat_map<CloudPolicyManager*,
                 std::unique_ptr<ClientInitializationWaiter>>
      initialization_waiters_;

  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar local_state_change_registrar_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_SERVICE_H_
