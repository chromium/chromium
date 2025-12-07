// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace policy {

// A keyed service that provides access to the extension install policy.
class ExtensionInstallPolicyService : public KeyedService {
 public:
  explicit ExtensionInstallPolicyService(Profile* profile);
  ~ExtensionInstallPolicyService() override;

  ExtensionInstallPolicyService(const ExtensionInstallPolicyService&) = delete;
  ExtensionInstallPolicyService& operator=(
      const ExtensionInstallPolicyService&) = delete;

  // To call before installing an extension
  // `extension_id_and_version` is an extension ID and version pair formatted
  // as "id@version".
  void CanInstallExtension(const std::string& extension_id_and_version,
                           base::OnceCallback<void(bool)>);

  // To call at startup to get the disallowed extensions
  // `extension_ids_and_versions` is a list of extension ID and version pairs
  // formatted as "id@version".
  void GetDisallowedExtensions(
      const std::vector<std::string>& extension_ids_and_versions,
      base::OnceCallback<void(std::set<std::string> /*disallowed_extension*/)>);

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_SERVICE_H_
