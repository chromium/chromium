// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/security_interstitials/core/controller_client.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#endif

namespace content {
class WebContents;
}  // namespace content

namespace policy {

class PolicyMap;

void GetTestDataDirectory(base::FilePath* test_data_directory);

class PolicyTest : public PlatformBrowserTest {
 public:
  // The possibilities for a boolean policy.
  enum class BooleanPolicy {
    kNotConfigured,
    kFalse,
    kTrue,
  };

 protected:
  PolicyTest();
  ~PolicyTest() override;

  void SetUpInProcessBrowserTestFixture() override;

  void SetUpOnMainThread() override;

  void UpdateProviderPolicy(const PolicyMap& policy);

  static void SetPolicy(PolicyMap* policies,
                        const char* key,
                        std::optional<base::Value> value);

  static bool FetchSubresource(content::WebContents* web_contents,
                               const GURL& url);

  void FlushBlocklistPolicy();

  // Navigates to `url` and returns true if navigation completed.
  bool NavigateToUrl(GURL url, PlatformBrowserTest* browser_test);

  testing::NiceMock<MockConfigurationPolicyProvider> provider_;
};

class PolicyTestAppTerminationObserver {
 public:
  PolicyTestAppTerminationObserver();
  ~PolicyTestAppTerminationObserver();

  bool WasAppTerminated() const { return terminated_; }

 private:
  void OnAppTerminating() { terminated_ = true; }

  base::CallbackListSubscription terminating_subscription_;
  bool terminated_ = false;
};

class [[maybe_unused, nodiscard]] ScopedDomainEnterpriseManagement {
 public:
  ScopedDomainEnterpriseManagement() = default;
  ~ScopedDomainEnterpriseManagement() = default;

 private:
// Indicate a machine is domain-joined by enterprise policy for mac and
// windows only.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  policy::ScopedManagementServiceOverrideForTesting browser_management{
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN};
#endif
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_
