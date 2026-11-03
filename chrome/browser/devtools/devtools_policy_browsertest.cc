// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_availability_checker.h"
#include "chrome/browser/devtools/devtools_policy_dialog.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/policy/developer_tools_policy_checker.h"
#include "chrome/browser/policy/developer_tools_policy_checker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/developer_tools_availability.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

constexpr char kBlockedByPolicyHistogram[] = "DevTools.BlockedByPolicy";
constexpr char kBlockedByPolicyReasonHistogram[] =
    "DevTools.BlockedByPolicy.Reason";
constexpr char kOpenAttemptAllowedByPolicyHistogram[] =
    "DevTools.OpenAttempt.AllowedByPolicy";
constexpr char kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram[] =
    "DevTools.OpenAttempt.AllowedByPolicy.OnProfileWithPolicyCheck";

class TestObserverImpl : public DevToolsPolicyDialog::TestObserver {
 public:
  ~TestObserverImpl() override = default;

  void OnDialogShown(DevToolsPolicyDialog* dialog) override { shown_count_++; }
  void OnDialogDestroyed(DevToolsPolicyDialog* dialog) override {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  int shown_count() const { return shown_count_; }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  int shown_count_ = 0;
  base::OnceClosure quit_closure_;
};

class DevToolsPolicyDialogTest : public PlatformBrowserTest {
 public:
  DevToolsPolicyDialogTest() {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
  }
  ~DevToolsPolicyDialogTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    PlatformBrowserTest::SetUpInProcessBrowserTestFixture();
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    DevToolsPolicyDialog::SetTestObserver(&observer_);
  }
  void TearDownOnMainThread() override {
    DevToolsPolicyDialog::SetTestObserver(nullptr);
    PlatformBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Sets the DeveloperToolsAvailability policy for the test profile.
  void SetDevToolsAvailability(
      policy::DeveloperToolsAvailability availability) {
    policy::PolicyMap policies;
    SetMandatoryPolicy(policies, policy::key::kDeveloperToolsAvailability,
                       base::Value(static_cast<int>(availability)));
    policy_provider_.UpdateChromePolicy(policies);
  }

  // Sets DeveloperToolsAvailability to Disallowed while allowlisting `url`
  // via DeveloperToolsAvailabilityAllowlist, so DevTools are only available
  // on that origin.
  void DisallowDevToolsExceptFor(const GURL& url) {
    policy::PolicyMap policies;
    SetMandatoryPolicy(policies, policy::key::kDeveloperToolsAvailability,
                       base::Value(static_cast<int>(
                           policy::DeveloperToolsAvailability::kDisallowed)));
    SetMandatoryPolicy(policies,
                       policy::key::kDeveloperToolsAvailabilityAllowlist,
                       base::Value(base::ListValue().Append(url.host())));
    SetUrlListPolicyAndWait(std::move(policies));
  }

  // Blocks DevTools on `url` via DeveloperToolsAvailabilityBlocklist, leaving
  // DeveloperToolsAvailability at its default value.
  void BlockDevToolsOn(const GURL& url) {
    policy::PolicyMap policies;
    SetMandatoryPolicy(policies,
                       policy::key::kDeveloperToolsAvailabilityBlocklist,
                       base::Value(base::ListValue().Append(url.host())));
    SetUrlListPolicyAndWait(std::move(policies));
  }

  // Blocks `urls` via the generic URLBlocklist policy without setting any
  // DeveloperTools* policy.
  void SetGenericUrlBlocklist(base::ListValue urls) {
    policy::PolicyMap policies;
    SetMandatoryPolicy(policies, policy::key::kURLBlocklist,
                       base::Value(std::move(urls)));
    policy_provider_.UpdateChromePolicy(policies);
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  TestObserverImpl observer_;

 private:
  // Policies are applied through the mock provider rather than by writing
  // prefs directly, so that the resulting prefs are managed
  // (PrefService::IsManagedPreference() is true), exactly as with real
  // enterprise policy. HasDevToolsPolicyCheck() relies on that.
  static void SetMandatoryPolicy(policy::PolicyMap& policies,
                                 const std::string& policy_name,
                                 base::Value value) {
    policies.Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 std::move(value), nullptr);
  }

  // Applies `policies` and waits until DeveloperToolsPolicyChecker has
  // rebuilt its URL lists. URLBlocklistManager rebuilds asynchronously on pref
  // changes, so without this the DeveloperToolsAvailability pref would be
  // enforced before the allowlist/blocklist is, and the test would depend on
  // whether the checker was created lazily (synchronous initial build) or
  // ahead of time.
  void SetUrlListPolicyAndWait(policy::PolicyMap policies) {
    policy::DeveloperToolsPolicyChecker* checker =
        policy::DeveloperToolsPolicyCheckerFactory::GetForBrowserContext(
            chrome_test_utils::GetProfile(this));
    ASSERT_TRUE(checker);
    base::test::TestFuture<void> lists_rebuilt;
    base::CallbackListSubscription subscription =
        checker->AddObserver(lists_rebuilt.GetRepeatingCallback());
    policy_provider_.UpdateChromePolicy(policies);
    ASSERT_TRUE(lists_rebuilt.Wait());
  }
};

// Runs once per DeveloperToolsAvailability policy value.
class DevToolsPolicyDialogHistogramTest
    : public DevToolsPolicyDialogTest,
      public testing::WithParamInterface<policy::DeveloperToolsAvailability> {};

std::string DeveloperToolsAvailabilityToString(
    const testing::TestParamInfo<policy::DeveloperToolsAvailability>& info) {
  switch (info.param) {
    case policy::DeveloperToolsAvailability::
        kDisallowedForForceInstalledExtensions:
      return "DisallowedForForceInstalledExtensions";
    case policy::DeveloperToolsAvailability::kAllowed:
      return "Allowed";
    case policy::DeveloperToolsAvailability::kDisallowed:
      return "Disallowed";
  }
  NOTREACHED();
}

}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest, ShowDialogOnce) {
  base::HistogramTester histogram_tester;
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1, observer_.shown_count());
  // Trying to show it again on the same WebContents should not create a new
  // dialog.
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1, observer_.shown_count());
  // Only the dialog that was actually shown is counted, bucketed by the
  // effective DeveloperToolsAvailability policy value (default when unset).
  histogram_tester.ExpectUniqueSample(
      kBlockedByPolicyHistogram,
      policy::DeveloperToolsAvailability::
          kDisallowedForForceInstalledExtensions,
      1);
}

IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest, ShowDialogOnTwoWebContents) {
  base::HistogramTester histogram_tester;
  auto* web_contents1 = chrome_test_utils::GetActiveWebContents(this);
  DevToolsPolicyDialog::Show(web_contents1);
  EXPECT_EQ(1, observer_.shown_count());

  // Open a new tab.
  auto* tab_list = TabListInterface::From(GetBrowserWindowInterface());
  ASSERT_TRUE(tab_list);
  auto* tab2 = tab_list->OpenTab(GURL("about:blank"), 1);
  ASSERT_TRUE(tab2);
  auto* web_contents2 = tab2->GetContents();
  ASSERT_NE(web_contents1, web_contents2);
  DevToolsPolicyDialog::Show(web_contents2);
  EXPECT_EQ(2, observer_.shown_count());
  // A dialog on a different tab is a separate block.
  histogram_tester.ExpectTotalCount(kBlockedByPolicyHistogram, 2);
}

#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest, ShowDialogTwice) {
  base::HistogramTester histogram_tester;
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1, observer_.shown_count());
  DevToolsPolicyDialog::TestOnlyCloseDialog(web_contents);
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(2, observer_.shown_count());
  // Showing the dialog again after it was closed is a new block.
  histogram_tester.ExpectTotalCount(kBlockedByPolicyHistogram, 2);
}
#endif

IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest,
                       DialogCleanedUpOnWebContentsDestruction) {
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1u, DevToolsPolicyDialog::GetCurrentDialogsSizeForTesting());

  base::RunLoop run_loop;
  observer_.SetQuitClosure(run_loop.QuitClosure());

  auto* tab_list = TabListInterface::From(GetBrowserWindowInterface());
  ASSERT_TRUE(tab_list);
  auto* tab = tab_list->GetTab(0);
  ASSERT_TRUE(tab);
  tab_list->CloseTab(tab->GetHandle());

  run_loop.Run();

  EXPECT_EQ(0u, DevToolsPolicyDialog::GetCurrentDialogsSizeForTesting());
}

// The recorded bucket must be the effective DeveloperToolsAvailability value,
// so that blocks under the default or Allowed values (contexts admins did not
// explicitly disallow) can be told apart from admin-intended blocks under
// Disallowed.
IN_PROC_BROWSER_TEST_P(DevToolsPolicyDialogHistogramTest,
                       RecordsEffectivePolicyValue) {
  base::HistogramTester histogram_tester;
  SetDevToolsAvailability(GetParam());

  DevToolsPolicyDialog::Show(chrome_test_utils::GetActiveWebContents(this));

  EXPECT_EQ(1, observer_.shown_count());
  histogram_tester.ExpectUniqueSample(kBlockedByPolicyHistogram, GetParam(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DevToolsPolicyDialogHistogramTest,
    testing::Values(policy::DeveloperToolsAvailability::
                        kDisallowedForForceInstalledExtensions,
                    policy::DeveloperToolsAvailability::kAllowed,
                    policy::DeveloperToolsAvailability::kDisallowed),
    &DeveloperToolsAvailabilityToString);

// End-to-end: an actual attempt to open DevTools that the policy blocks must
// go through the dialog and be counted.
IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest,
                       RecordedWhenOpeningDevToolsIsBlocked) {
  base::HistogramTester histogram_tester;
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  SetDevToolsAvailability(policy::DeveloperToolsAvailability::kDisallowed);

  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);

  EXPECT_FALSE(
      DevToolsWindow::GetInstanceForInspectedWebContents(web_contents));
  EXPECT_EQ(1, observer_.shown_count());
  histogram_tester.ExpectUniqueSample(
      kBlockedByPolicyHistogram,
      policy::DeveloperToolsAvailability::kDisallowed, 1);
  histogram_tester.ExpectUniqueSample(kBlockedByPolicyReasonHistogram,
                                      DevToolsBlockReason::kPolicyDisallowed,
                                      1);
  histogram_tester.ExpectUniqueSample(kOpenAttemptAllowedByPolicyHistogram,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, false, 1);

  // Repeated attempts while the policy dialog is already open on the same tab
  // must not record duplicate samples.
  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);
  histogram_tester.ExpectTotalCount(kOpenAttemptAllowedByPolicyHistogram, 1);
  histogram_tester.ExpectTotalCount(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, 1);
}

// A block while the DeveloperToolsAvailability policy has its default value
// is attributed to the check that actually blocked, here the URL blocklist.
IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest,
                       RecordedWithUrlReasonWhenPageIsOnBlocklist) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  const GURL url =
      embedded_test_server()->GetURL("blocked.example", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  ASSERT_NO_FATAL_FAILURE(BlockDevToolsOn(url));

  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);

  EXPECT_FALSE(
      DevToolsWindow::GetInstanceForInspectedWebContents(web_contents));
  EXPECT_EQ(1, observer_.shown_count());
  histogram_tester.ExpectUniqueSample(
      kBlockedByPolicyHistogram,
      policy::DeveloperToolsAvailability::
          kDisallowedForForceInstalledExtensions,
      1);
  histogram_tester.ExpectUniqueSample(
      kBlockedByPolicyReasonHistogram,
      DevToolsBlockReason::kUrlAllowlistOrBlocklist, 1);
  histogram_tester.ExpectUniqueSample(kOpenAttemptAllowedByPolicyHistogram,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, false, 1);
}

// Negative control: with no DevTools policy checks active on the profile,
// DevTools are allowed on regular pages, so opening them must not show the
// dialog, must record an allowed open in the overall histogram, and must not
// record the profile-with-policy-check slice.
IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest,
                       AllowedOpenWithoutPolicyRecordsOnlyOverallHistogram) {
  base::HistogramTester histogram_tester;
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);

  DevToolsWindow* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      web_contents, /*is_docked=*/true);
  ASSERT_TRUE(window);

  EXPECT_EQ(0, observer_.shown_count());
  histogram_tester.ExpectTotalCount(kBlockedByPolicyHistogram, 0);
  histogram_tester.ExpectTotalCount(kBlockedByPolicyReasonHistogram, 0);
  histogram_tester.ExpectUniqueSample(kOpenAttemptAllowedByPolicyHistogram,
                                      true, 1);
  histogram_tester.ExpectTotalCount(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, 0);

  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}

// When DevTools policies are enabled and allow opening DevTools (e.g.
// DeveloperToolsAvailability set to Allowed), successful openings are recorded
// in both boolean histograms.
IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest,
                       RecordedWhenOpeningDevToolsIsAllowedWithPolicySet) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  SetDevToolsAvailability(policy::DeveloperToolsAvailability::kAllowed);

  DevToolsWindow* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      web_contents, /*is_docked=*/true);
  ASSERT_TRUE(window);

  // Focusing or toggling an already-open DevTools window must not count as a
  // second opening.
  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);

  EXPECT_EQ(0, observer_.shown_count());
  histogram_tester.ExpectTotalCount(kBlockedByPolicyHistogram, 0);
  histogram_tester.ExpectUniqueSample(kOpenAttemptAllowedByPolicyHistogram,
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, true, 1);

  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}

// A page allowlisted by DeveloperToolsAvailabilityAllowlist can open DevTools
// even though DeveloperToolsAvailability is Disallowed; this counts as a
// successful opening with policies enabled.
IN_PROC_BROWSER_TEST_F(
    DevToolsPolicyDialogTest,
    RecordedWhenOpeningDevToolsIsAllowedViaUrlAllowlistPolicy) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  const GURL url =
      embedded_test_server()->GetURL("allowed.example", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  ASSERT_NO_FATAL_FAILURE(DisallowDevToolsExceptFor(url));

  DevToolsWindow* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      web_contents, /*is_docked=*/true);
  ASSERT_TRUE(window);

  EXPECT_EQ(0, observer_.shown_count());
  histogram_tester.ExpectTotalCount(kBlockedByPolicyHistogram, 0);
  histogram_tester.ExpectUniqueSample(kOpenAttemptAllowedByPolicyHistogram,
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, true, 1);

  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}

// On a profile with DeveloperToolsAvailabilityBlocklist configured, opening
// DevTools on a URL not matched by the blocklist still records Allowed in
// `.OnProfileWithPolicyCheck` so the slice measures the open allowance rate
// across all attempts on that profile population.
IN_PROC_BROWSER_TEST_F(
    DevToolsPolicyDialogTest,
    AllowedOpenOnUnmatchedUrlWhenBlocklistSetRecordsBothHistograms) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  const GURL blocked_url =
      embedded_test_server()->GetURL("blocked.example", "/title1.html");
  const GURL other_url =
      embedded_test_server()->GetURL("other.example", "/title1.html");
  ASSERT_NO_FATAL_FAILURE(BlockDevToolsOn(blocked_url));
  ASSERT_TRUE(content::NavigateToURL(web_contents, other_url));

  DevToolsWindow* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      web_contents, /*is_docked=*/true);
  ASSERT_TRUE(window);

  EXPECT_EQ(0, observer_.shown_count());
  histogram_tester.ExpectTotalCount(kBlockedByPolicyHistogram, 0);
  histogram_tester.ExpectUniqueSample(kOpenAttemptAllowedByPolicyHistogram,
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, true, 1);

  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}

// Kiosk mode (`--kiosk`) disables the DevTools UI for non-policy reasons. Even
// when a DevTools policy is configured on the profile, a kiosk block must not
// be recorded as `AllowedByPolicy = false`.
IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest,
                       KioskModeBlockDoesNotRecordAllowedByPolicyHistograms) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kKioskMode);

  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  const GURL blocked_url =
      embedded_test_server()->GetURL("blocked.example", "/title1.html");
  const GURL other_url =
      embedded_test_server()->GetURL("other.example", "/title1.html");
  ASSERT_NO_FATAL_FAILURE(BlockDevToolsOn(blocked_url));
  ASSERT_TRUE(content::NavigateToURL(web_contents, other_url));

  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);

  EXPECT_FALSE(
      DevToolsWindow::GetInstanceForInspectedWebContents(web_contents));
  EXPECT_EQ(1, observer_.shown_count());
  histogram_tester.ExpectUniqueSample(kBlockedByPolicyReasonHistogram,
                                      DevToolsBlockReason::kKioskMode, 1);
  histogram_tester.ExpectTotalCount(kOpenAttemptAllowedByPolicyHistogram, 0);
  histogram_tester.ExpectTotalCount(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, 0);
}

// Blocking `devtools://*` via the generic `URLBlocklist` policy (with no
// `DeveloperTools*` policy set) is a policy check affecting DevTools on the
// profile, so blocked open attempts must be recorded in both histograms.
IN_PROC_BROWSER_TEST_F(
    DevToolsPolicyDialogTest,
    UrlBlocklistOnDevToolsUiRecordsBothHistogramsWithoutDevToolsPolicy) {
  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  Profile* profile = chrome_test_utils::GetProfile(this);

  SetGenericUrlBlocklist(base::ListValue().Append("devtools://*"));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !DevToolsWindow::AllowDevToolsFor(profile, web_contents);
  }));

  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);

  EXPECT_FALSE(
      DevToolsWindow::GetInstanceForInspectedWebContents(web_contents));
  EXPECT_EQ(1, observer_.shown_count());
  histogram_tester.ExpectUniqueSample(
      kBlockedByPolicyReasonHistogram,
      DevToolsBlockReason::kDevToolsUrlBlocklisted, 1);
  histogram_tester.ExpectUniqueSample(kOpenAttemptAllowedByPolicyHistogram,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, false, 1);
}

// With no `DeveloperTools*` policy set, installing a policy-forced extension
// activates the default `DisallowedForForceInstalledExtensions` check on the
// profile, so both allowed opens on regular pages and blocked opens on the
// extension's pages are recorded in `.OnProfileWithPolicyCheck`.
IN_PROC_BROWSER_TEST_F(
    DevToolsPolicyDialogTest,
    ForceInstalledExtensionWithoutDevToolsPolicyRecordsBothHistograms) {
  // The test extension is packed locally rather than coming from the Web
  // Store, so in Chrome-branded Windows/Mac builds the install verifier would
  // keep it disabled and `LoadExtension()` would fail.
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass;
  ASSERT_TRUE(embedded_test_server()->Start());
  Profile* profile = chrome_test_utils::GetProfile(this);

  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(R"({
    "name": "Force Installed Test Extension",
    "version": "1.0",
    "manifest_version": 3
  })");
  extension_dir.WriteFile(FILE_PATH_LITERAL("page.html"),
                          "<html><body>extension page</body></html>");

  extensions::ChromeTestExtensionLoader loader(profile);
  loader.set_location(extensions::mojom::ManifestLocation::kExternalPolicy);
  loader.set_pack_extension(true);
  loader.set_ignore_manifest_warnings(true);
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  base::HistogramTester histogram_tester;
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  const GURL regular_url =
      embedded_test_server()->GetURL("regular.example", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, regular_url));

  DevToolsWindow* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      web_contents, /*is_docked=*/true);
  ASSERT_TRUE(window);
  DevToolsWindowTesting::CloseDevToolsWindowSync(window);

  histogram_tester.ExpectBucketCount(kOpenAttemptAllowedByPolicyHistogram, true,
                                     1);
  histogram_tester.ExpectBucketCount(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, true, 1);

  ASSERT_TRUE(content::NavigateToURL(web_contents,
                                     extension->GetResourceURL("page.html")));
  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);

  EXPECT_FALSE(
      DevToolsWindow::GetInstanceForInspectedWebContents(web_contents));
  EXPECT_EQ(1, observer_.shown_count());
  histogram_tester.ExpectUniqueSample(
      kBlockedByPolicyReasonHistogram,
      DevToolsBlockReason::kForceInstalledExtension, 1);
  histogram_tester.ExpectBucketCount(kOpenAttemptAllowedByPolicyHistogram,
                                     false, 1);
  histogram_tester.ExpectBucketCount(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, false, 1);
  histogram_tester.ExpectTotalCount(kOpenAttemptAllowedByPolicyHistogram, 2);
  histogram_tester.ExpectTotalCount(
      kOpenAttemptAllowedOnProfileWithPolicyCheckHistogram, 2);
}
