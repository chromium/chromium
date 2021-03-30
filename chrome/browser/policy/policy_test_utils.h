// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_

#include <string>

#include "ash/public/cpp/keyboard/keyboard_types.h"
#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace policy {

extern const base::FilePath::CharType kTestExtensionsDir[];

class PolicyMap;

void GetTestDataDirectory(base::FilePath* test_data_directory);

class PolicyTest : public InProcessBrowserTest {
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

  // Verifies that access to the given url |spec| is blocked.
  void CheckURLIsBlockedInWebContents(content::WebContents* web_contents,
                                      const GURL& url);

  // Verifies that access to the given url |spec| is blocked.
  void CheckURLIsBlocked(Browser* browser, const std::string& spec);

  void SetScreenshotPolicy(bool enabled);

  void SetRequireCTForTesting(bool required);

  scoped_refptr<const extensions::Extension> LoadUnpackedExtension(
      const base::FilePath::StringType& name);

  void UpdateProviderPolicy(const PolicyMap& policy);

  // Sends a mouse click at the given coordinates to the current renderer.
  void PerformClick(int x, int y);

  static void SetPolicy(PolicyMap* policies,
                        const char* key,
                        base::Optional<base::Value> value);

  void ApplySafeSearchPolicy(base::Optional<base::Value> legacy_safe_search,
                             base::Optional<base::Value> google_safe_search,
                             base::Optional<base::Value> legacy_youtube,
                             base::Optional<base::Value> youtube_restrict);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void TestScreenshotFile(bool enabled);

  void SetEnableFlag(const keyboard::KeyboardEnableFlag& flag);

  void ClearEnableFlag(const keyboard::KeyboardEnableFlag& flag);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  static GURL GetExpectedSearchURL(bool expect_safe_search);

  static void CheckSafeSearch(Browser* browser,
                              bool expect_safe_search,
                              const std::string& url = "http://google.com/");

  static void CheckYouTubeRestricted(int youtube_restrict_mode,
                                     const net::HttpRequestHeaders& headers);

  static void CheckAllowedDomainsHeader(const std::string& allowed_domain,
                                        const net::HttpRequestHeaders& headers);

  static bool FetchSubresource(content::WebContents* web_contents,
                               const GURL& url);

  bool IsShowingInterstitial(content::WebContents* tab);

  void WaitForInterstitial(content::WebContents* tab);

  int IsEnhancedProtectionMessageVisibleOnInterstitial();

  void SendInterstitialCommand(
      content::WebContents* tab,
      security_interstitials::SecurityInterstitialCommand command);

  void FlushBlacklistPolicy();

  MockConfigurationPolicyProvider provider_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_TEST_UTILS_H_
