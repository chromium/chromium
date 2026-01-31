// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_MOCK_EXTENSION_INSTALL_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_CLOUD_MOCK_EXTENSION_INSTALL_POLICY_SERVICE_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/policy/cloud/extension_install_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class MockExtensionInstallPolicyService : public ExtensionInstallPolicyService {
 public:
  MockExtensionInstallPolicyService();
  ~MockExtensionInstallPolicyService() override;

  // ExtensionInstallPolicyService:
  MOCK_METHOD(void,
              CanInstallExtension,
              (const ExtensionIdAndVersion&, base::OnceCallback<void(bool)>),
              (const, override));
  MOCK_METHOD(std::optional<bool>,
              IsExtensionAllowed,
              (const ExtensionIdAndVersion&),
              (const, override));
  MOCK_METHOD(void,
              AddObserver,
              (ExtensionInstallPolicyService::Observer*),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (ExtensionInstallPolicyService::Observer*),
              (override));

  // ManagementPolicy::Provider:
  MOCK_METHOD(std::string, GetDebugPolicyProviderName, (), (const, override));
  MOCK_METHOD(
      void,
      UserMayInstall,
      (scoped_refptr<const extensions::Extension>,
       base::OnceCallback<void(extensions::ManagementPolicy::Decision)>),
      (const, override));
  MOCK_METHOD(bool,
              UserMayLoad,
              (const extensions::Extension*, std::u16string*),
              (const, override));
  MOCK_METHOD(bool,
              MustRemainDisabled,
              (const extensions::Extension*,
               extensions::disable_reason::DisableReason*),
              (const, override));
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_MOCK_EXTENSION_INSTALL_POLICY_SERVICE_H_
