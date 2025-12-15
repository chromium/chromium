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

class Profile;

namespace policy {
struct ExtensionIdAndVersion;

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
  void CanInstallExtension(
      const ExtensionIdAndVersion& extension_id_and_version,
      base::OnceCallback<void(bool)>);

  std::optional<bool> IsExtensionAllowed(
      const ExtensionIdAndVersion& extension_id_and_version);

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_SERVICE_H_
