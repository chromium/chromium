// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/security_interstitials/core/controller_client.h"
#include "url/gurl.h"

class Browser;

namespace content {
class WebContents;
}  // namespace content

namespace extensions {
class Extension;
}  // namespace extensions

namespace policy {

extern const base::FilePath::CharType kTestExtensionsDir[];

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

  void SetUp() override;

  void SetUpInProcessBrowserTestFixture() override;

  void SetUpOnMainThread() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void SetScreenshotPolicy(bool enabled);

  scoped_refptr<const extensions::Extension> LoadUnpackedExtension(
      const base::FilePath::StringType& name);

  void UpdateProviderPolicy(const PolicyMap& policy);

  static void SetPolicy(PolicyMap* policies,
                        const char* key,
                        absl::optional<base::Value> value);

  void ApplySafeSearchPolicy(absl::optional<base::Value> legacy_safe_search,
                             absl::optional<base::Value> google_safe_search,
                             absl::optional<base::Value> legacy_youtube,
                             absl::optional<base::Value> youtube_restrict);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void TestScreenshotFile(bool enabled);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  static GURL GetExpectedSearchURL(bool expect_safe_search);

  static void CheckSafeSearch(Browser* browser,
                              bool expect_safe_search,
                              const std::string& url = "http://google.com/");

  static bool FetchSubresource(content::WebContents* web_contents,
                               const GURL& url);

  bool IsShowingInterstitial(content::WebContents* tab);

  void WaitForInterstitial(content::WebContents* tab);

  int IsEnhancedProtectionMessageVisibleOnInterstitial();

  void FlushBlocklistPolicy();

  testing::NiceMock<MockConfigurationPolicyProvider> provider_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_
