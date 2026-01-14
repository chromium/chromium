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
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client_types.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace policy {

struct ExtensionIdAndVersion;

class ExtensionInstallPolicyService : public KeyedService {
 public:
  ~ExtensionInstallPolicyService() override = default;

  // To call before installing an extension
  // `extension_id_and_version` is an extension ID and version pair formatted
  // as "id@version".
  virtual void CanInstallExtension(
      const ExtensionIdAndVersion& extension_id_and_version,
      base::OnceCallback<void(bool)>) = 0;

  virtual std::optional<bool> IsExtensionAllowed(
      const ExtensionIdAndVersion& extension_id_and_version) = 0;
};

// A keyed service that provides access to the extension install policy.
class ExtensionInstallPolicyServiceImpl
    : public ExtensionInstallPolicyService,
      public PolicyTypeToFetch::ExtensionsProvider {
 public:
  explicit ExtensionInstallPolicyServiceImpl(Profile* profile);
  ~ExtensionInstallPolicyServiceImpl() override;

  ExtensionInstallPolicyServiceImpl(const ExtensionInstallPolicyServiceImpl&) =
      delete;
  ExtensionInstallPolicyServiceImpl& operator=(
      const ExtensionInstallPolicyServiceImpl&) = delete;

  // ExtensionInstallPolicyService:
  void CanInstallExtension(
      const ExtensionIdAndVersion& extension_id_and_version,
      base::OnceCallback<void(bool)>) override;
  std::optional<bool> IsExtensionAllowed(
      const ExtensionIdAndVersion& extension_id_and_version) override;

  // PolicyTypeToFetch::ExtensionsProvider:
  std::set<ExtensionIdAndVersion> GetExtensions() override;

 private:
  // Adds or removes from CloudPolicyClient::types_to_fetch_ based on
  // the current value of the pref
  // `kExtensionInstallCloudPolicyChecksEnabled`.
  void OnPolicyChecksEnabledChanged();

  raw_ptr<Profile> profile_;

  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_SERVICE_H_
