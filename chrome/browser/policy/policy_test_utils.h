// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_

#include "ash/public/cpp/keyboard/keyboard_types.h"
#include "base/files/file_path.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/security_interstitials/core/controller_client.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace policy {

extern const base::FilePath::CharType kTestExtensionsDir[];

class PolicyMap;

void GetTestDataDirectory(base::FilePath* test_data_directory);

class PolicyTest : public InProcessBrowserTest {
 protected:
  PolicyTest();
  ~PolicyTest() override;

  void SetUp() override;

  void SetUpInProcessBrowserTestFixture() override;

  void SetUpOnMainThread() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void SetScreenshotPolicy(bool enabled);

  void SetShouldRequireCTForTesting(bool* required);

  scoped_refptr<const extensions::Extension> LoadUnpackedExtension(
      const base::FilePath::StringType& name);

  void UpdateProviderPolicy(const PolicyMap& policy);

  // Sends a mouse click at the given coordinates to the current renderer.
  void PerformClick(int x, int y);

  void SetPolicy(PolicyMap* policies,
                 const char* key,
                 std::unique_ptr<base::Value> value);

  void ApplySafeSearchPolicy(std::unique_ptr<base::Value> legacy_safe_search,
                             std::unique_ptr<base::Value> google_safe_search,
                             std::unique_ptr<base::Value> legacy_youtube,
                             std::unique_ptr<base::Value> youtube_restrict);

#if defined(OS_CHROMEOS)
  void TestScreenshotFile(bool enabled);

  void SetEnableFlag(const keyboard::KeyboardEnableFlag& flag);

  void ClearEnableFlag(const keyboard::KeyboardEnableFlag& flag);
#endif  // defined(OS_CHROMEOS)

  static GURL GetExpectedSearchURL(bool expect_safe_search);

  static void CheckSafeSearch(Browser* browser,
                              bool expect_safe_search,
                              const std::string& url = "http://google.com/");

  static void CheckYouTubeRestricted(
      int youtube_restrict_mode,
      const std::map<GURL, net::HttpRequestHeaders>& urls_requested,
      const GURL& url);

  static void CheckAllowedDomainsHeader(
      const std::string& allowed_domain,
      const std::map<GURL, net::HttpRequestHeaders>& urls_requested,
      const GURL& url);

  static bool FetchSubresource(content::WebContents* web_contents,
                               const GURL& url);

  bool IsShowingInterstitial(content::WebContents* tab);

  void WaitForInterstitial(content::WebContents* tab);

  int IsExtendedReportingCheckboxVisibleOnInterstitial();

  void SendInterstitialCommand(
      content::WebContents* tab,
      security_interstitials::SecurityInterstitialCommand command);

  MockConfigurationPolicyProvider provider_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_
