// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/security_interstitials/core/controller_client.h"
#include "url/gurl.h"

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

  void SetScreenshotPolicy(bool enabled);

  void UpdateProviderPolicy(const PolicyMap& policy);

  static void SetPolicy(PolicyMap* policies,
                        const char* key,
                        absl::optional<base::Value> value);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void TestScreenshotFile(bool enabled);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_
