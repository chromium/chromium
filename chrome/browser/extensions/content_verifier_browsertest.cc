// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <memory>
#include <set>
#include <string>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/extensions/content_verifier_test_utils.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/policy_extension_reinstaller.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/content_verify_job.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_test_delegate.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"

namespace extensions {

class ContentVerifierTest : public ExtensionBrowserTest {
 public:
  ContentVerifierTest() {}
  ~ContentVerifierTest() override {}

  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        extensions_features::kNewExtensionUpdaterService);
    // Override content verification mode before ExtensionSystemImpl initializes
    // ChromeContentVerifierDelegate.
    ChromeContentVerifierDelegate::SetDefaultModeForTesting(
        ContentVerifierDelegate::ENFORCE);

    ExtensionBrowserTest::SetUp();
  }

  void TearDown() override {
    ExtensionBrowserTest::TearDown();
    ChromeContentVerifierDelegate::SetDefaultModeForTesting(base::nullopt);
  }

  bool ShouldEnableContentVerification() override { return true; }

  void TestContentScriptExtension(const std::string& crx_relpath,
                                  const std::string& id,
                                  const std::string& script_relpath) {
    VerifierObserver verifier_observer;

    // Install the extension with content scripts. The initial read of the
    // content scripts will fail verification because they are read before the
    // content verification system has completed a one-time processing of the
    // expected hashes. (The extension only contains the root level hashes of
    // the merkle tree, but the content verification system builds the entire
    // tree and caches it in the extension install directory - see
    // ContentHashFetcher for more details).
    const Extension* extension = InstallExtensionFromWebstore(
        test_data_dir_.AppendASCII(crx_relpath), 1);
    ASSERT_TRUE(extension);
    EXPECT_EQ(id, extension->id());

    // Wait for the content verification code to finish processing the hashes.
    if (!base::ContainsKey(verifier_observer.completed_fetches(), id))
      verifier_observer.WaitForFetchComplete(id);

    // Now disable the extension, since content scripts are read at enable time,
    // set up our job observer, and re-enable, expecting a success this time.
    DisableExtension(id);
    using Result = TestContentVerifyJobObserver::Result;
    TestContentVerifyJobObserver job_observer;
    base::FilePath script_relfilepath =
        base::FilePath().AppendASCII(script_relpath);
    job_observer.ExpectJobResult(id, script_relfilepath, Result::SUCCESS);
    EnableExtension(id);
    EXPECT_TRUE(job_observer.WaitForExpectedJobs());

    // Now alter the contents of the content script, reload the extension, and
    // expect to see a job failure due to the content script content hash not
    // being what was signed by the webstore.
    base::FilePath scriptfile = extension->path().AppendASCII(script_relpath);
    std::string extra = "some_extra_function_call();";
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::AppendToFile(scriptfile, extra.data(), extra.size()));
    }
    DisableExtension(id);
    job_observer.ExpectJobResult(id, script_relfilepath, Result::FAILURE);
    EnableExtension(id);
    EXPECT_TRUE(job_observer.WaitForExpectedJobs());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContentVerifierTest, DotSlashPaths) {
  TestContentVerifyJobObserver job_observer;
  std::string id = "hoipipabpcoomfapcecilckodldhmpgl";

  using Result = TestContentVerifyJobObserver::Result;
  job_observer.ExpectJobResult(
      id, base::FilePath(FILE_PATH_LITERAL("background.js")), Result::SUCCESS);
  job_observer.ExpectJobResult(
      id, base::FilePath(FILE_PATH_LITERAL("page.html")), Result::SUCCESS);
  job_observer.ExpectJobResult(id, base::FilePath(FILE_PATH_LITERAL("page.js")),
                               Result::SUCCESS);
  job_observer.ExpectJobResult(
      id, base::FilePath(FILE_PATH_LITERAL("dir/page2.html")), Result::SUCCESS);
  job_observer.ExpectJobResult(
      id, base::FilePath(FILE_PATH_LITERAL("page2.js")), Result::SUCCESS);
  job_observer.ExpectJobResult(id, base::FilePath(FILE_PATH_LITERAL("cs1.js")),
                               Result::SUCCESS);
  job_observer.ExpectJobResult(id, base::FilePath(FILE_PATH_LITERAL("cs2.js")),
                               Result::SUCCESS);

  auto verifier_observer = std::make_unique<VerifierObserver>();

  // Install a test extension we copied from the webstore that has actual
  // signatures, and contains paths with a leading "./" in various places.
  const Extension* extension = InstallExtensionFromWebstore(
      test_data_dir_.AppendASCII("content_verifier/dot_slash_paths.crx"), 1);

  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->id(), id);

  // The content scripts might fail verification the first time since the
  // one-time processing might not be finished yet - if that's the case then
  // we want to wait until that work is done.
  if (!base::ContainsKey(verifier_observer->completed_fetches(), id))
    verifier_observer->WaitForFetchComplete(id);

  // It is important to destroy |verifier_observer| here so that it doesn't see
  // any fetch from EnableExtension call below (the observer pointer in
  // content_verifier.cc isn't thread safe, so it might asynchronously call
  // OnFetchComplete after this test's body executes).
  verifier_observer.reset();

  EXPECT_TRUE(job_observer.WaitForExpectedJobs());

  // Set expectations for extension enablement below.
  job_observer.ExpectJobResult(id, base::FilePath(FILE_PATH_LITERAL("cs1.js")),
                               Result::SUCCESS);
  job_observer.ExpectJobResult(id, base::FilePath(FILE_PATH_LITERAL("cs2.js")),
                               Result::SUCCESS);

  // Now disable/re-enable the extension to cause the content scripts to be
  // read again.
  DisableExtension(id);
  EnableExtension(id);

  EXPECT_TRUE(job_observer.WaitForExpectedJobs());
}

IN_PROC_BROWSER_TEST_F(ContentVerifierTest, ContentScripts) {
  TestContentScriptExtension("content_verifier/content_script.crx",
                             "jmllhlobpjcnnomjlipadejplhmheiif", "script.js");
}

// crbug.com/897059 tracks test flakiness.
#if defined(OS_WIN)
#define MAYBE_ContentScriptsInLocales DISABLED_ContentScriptsInLocales
#else
#define MAYBE_ContentScriptsInLocales ContentScriptsInLocales
#endif
IN_PROC_BROWSER_TEST_F(ContentVerifierTest, MAYBE_ContentScriptsInLocales) {
  TestContentScriptExtension("content_verifier/content_script_locales.crx",
                             "jaghonccckpcikmliipifpoodmeofoon",
                             "_locales/en/content_script.js");
}

// Tests the case of a corrupt extension that is force-installed by policy and
// should not be allowed to be manually uninstalled/disabled by the user.
IN_PROC_BROWSER_TEST_F(ContentVerifierTest, PolicyCorrupted) {
  ExtensionSystem* system = ExtensionSystem::Get(profile());
  ExtensionService* service = system->extension_service();

  // The id of our test extension.
  ExtensionId kExtensionId("npnbmohejbjohgpjnmjagbafnjhkmgko");

  // Setup fake policy and update check objects.
  content_verifier_test::ForceInstallProvider policy(kExtensionId);
  content_verifier_test::DownloaderTestDelegate downloader;
  system->management_policy()->RegisterProvider(&policy);
  ExtensionDownloader::set_test_delegate(&downloader);
  auto external_provider = std::make_unique<MockExternalProvider>(
      service, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  external_provider->UpdateOrAddExtension(
      std::make_unique<ExternalInstallInfoUpdateUrl>(
          kExtensionId, std::string() /* install_parameter */,
          extension_urls::GetWebstoreUpdateUrl(),
          Manifest::EXTERNAL_POLICY_DOWNLOAD, 0 /* creation_flags */,
          true /* mark_acknowldged */));
  service->AddProviderForTesting(std::move(external_provider));

  base::FilePath crx_path =
      test_data_dir_.AppendASCII("content_verifier/v1.crx");
  const Extension* extension =
      InstallExtension(crx_path, 1, Manifest::EXTERNAL_POLICY_DOWNLOAD);
  ASSERT_TRUE(extension);

  downloader.AddResponse(kExtensionId, extension->VersionString(), crx_path);
  EXPECT_EQ(kExtensionId, extension->id());

  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), kExtensionId);
  ContentVerifier* verifier = system->content_verifier();
  verifier->VerifyFailed(kExtensionId, ContentVerifyJob::HASH_MISMATCH);

  // Make sure the extension first got disabled due to corruption.
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  int reasons = prefs->GetDisableReasons(kExtensionId);
  EXPECT_TRUE(reasons & disable_reason::DISABLE_CORRUPTED);

  // Make sure the extension then got re-installed, and that after reinstall it
  // is no longer disabled due to corruption.
  EXPECT_TRUE(registry_observer.WaitForExtensionInstalled());

  reasons = prefs->GetDisableReasons(kExtensionId);
  EXPECT_FALSE(reasons & disable_reason::DISABLE_CORRUPTED);

  // Make sure that the update check request properly included a parameter
  // indicating that this was a corrupt policy reinstall.
  bool found = false;
  for (const auto& request : downloader.requests()) {
    if (request->Includes(kExtensionId)) {
      std::string query = request->full_url().query();
      for (const auto& part : base::SplitString(
               query, "&", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
        if (base::StartsWith(part, "x=", base::CompareCase::SENSITIVE) &&
            part.find(std::string("id%3D") + kExtensionId) !=
                std::string::npos) {
          found = true;
          EXPECT_NE(std::string::npos, part.find("installsource%3Dreinstall"));
        }
      }
    }
  }
  EXPECT_TRUE(found);
}

// Tests that verification failure during navigating to an extension resource
// correctly disables the extension.
IN_PROC_BROWSER_TEST_F(ContentVerifierTest, VerificationFailureOnNavigate) {
  const Extension* extension = InstallExtensionFromWebstore(
      test_data_dir_.AppendASCII("content_verifier/dot_slash_paths.crx"), 1);
  ASSERT_TRUE(extension);
  const ExtensionId kExtensionId = extension->id();
  const base::FilePath::CharType kResource[] = FILE_PATH_LITERAL("page.html");
  {
    // Modify content so that content verification fails.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath real_path = extension->path().Append(kResource);
    std::string extra = "some_extra_function_call();";
    ASSERT_TRUE(base::AppendToFile(real_path, extra.data(), extra.size()));
  }

  GURL page_url = extension->GetResourceURL("page.html");
  TestExtensionRegistryObserver unload_observer(
      ExtensionRegistry::Get(profile()), kExtensionId);
  // Wait for 0 navigations to complete because with PlzNavigate it's racy
  // when the didstop IPC arrives relative to the tab being closed. The
  // wait call below is what the tests care about.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), page_url, 0, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  EXPECT_TRUE(unload_observer.WaitForExtensionUnloaded());
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  int reasons = prefs->GetDisableReasons(kExtensionId);
  EXPECT_TRUE(reasons & disable_reason::DISABLE_CORRUPTED);
}

class ContentVerifierPolicyTest : public ContentVerifierTest {
 public:
  // We need to do this work here because the force-install policy values are
  // checked pretty early on in the startup of the ExtensionService, which
  // happens between SetUpInProcessBrowserTestFixture and SetUpOnMainThread.
  void SetUpInProcessBrowserTestFixture() override {
    ContentVerifierTest::SetUpInProcessBrowserTestFixture();

    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    ExtensionManagementPolicyUpdater management_policy(&policy_provider_);
    management_policy.SetIndividualExtensionAutoInstalled(
        id_, extension_urls::kChromeWebstoreUpdateURL, true /* forced */);

    ExtensionDownloader::set_test_delegate(&downloader_);
    base::FilePath crx_path =
        test_data_dir_.AppendASCII("content_verifier/v1.crx");
    std::string version = "2";
    downloader_.AddResponse(id_, version, crx_path);
  }

  void SetUpOnMainThread() override {
    extensions::browsertest_util::CreateAndInitializeLocalCache();
  }

 protected:
  // The id of the extension we want to have force-installed.
  std::string id_ = "npnbmohejbjohgpjnmjagbafnjhkmgko";

 private:
  policy::MockConfigurationPolicyProvider policy_provider_;
  content_verifier_test::DownloaderTestDelegate downloader_;
};

// We want to test what happens at startup with a corroption-disabled policy
// force installed extension. So we set that up in the PRE test here.
IN_PROC_BROWSER_TEST_F(ContentVerifierPolicyTest,
                       PRE_PolicyCorruptedOnStartup) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  TestExtensionRegistryObserver registry_observer(registry, id_);

  // Wait for the extension to be installed by policy we set up in
  // SetUpInProcessBrowserTestFixture.
  if (!registry->GetInstalledExtension(id_))
    EXPECT_TRUE(registry_observer.WaitForExtensionInstalled());

  // Simulate corruption of the extension so that we can test what happens
  // at startup in the non-PRE test.
  ExtensionSystem* system = ExtensionSystem::Get(profile());
  ContentVerifier* verifier = system->content_verifier();
  verifier->VerifyFailed(id_, ContentVerifyJob::HASH_MISMATCH);
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  int reasons = prefs->GetDisableReasons(id_);
  EXPECT_TRUE(reasons & disable_reason::DISABLE_CORRUPTED);
}

// Now actually test what happens on the next startup after the PRE test above.
IN_PROC_BROWSER_TEST_F(ContentVerifierPolicyTest, PolicyCorruptedOnStartup) {
  // Depdending on timing, the extension may have already been reinstalled
  // between SetUpInProcessBrowserTestFixture and now (usually not during local
  // testing on a developer machine, but sometimes on a heavily loaded system
  // such as the build waterfall / trybots). If the reinstall didn't already
  // happen, wait for it.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  int disable_reasons = prefs->GetDisableReasons(id_);
  if (disable_reasons & disable_reason::DISABLE_CORRUPTED) {
    TestExtensionRegistryObserver registry_observer(registry, id_);
    EXPECT_TRUE(registry_observer.WaitForExtensionInstalled());
    disable_reasons = prefs->GetDisableReasons(id_);
  }
  EXPECT_FALSE(disable_reasons & disable_reason::DISABLE_CORRUPTED);
  EXPECT_TRUE(registry->enabled_extensions().Contains(id_));
}

IN_PROC_BROWSER_TEST_F(ContentVerifierPolicyTest, Backoff) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ExtensionSystem* system = ExtensionSystem::Get(profile());
  ContentVerifier* verifier = system->content_verifier();

  // Wait for the extension to be installed by the policy we set up in
  // SetUpInProcessBrowserTestFixture.
  if (!registry->GetInstalledExtension(id_)) {
    TestExtensionRegistryObserver registry_observer(registry, id_);
    EXPECT_TRUE(registry_observer.WaitForExtensionInstalled());
  }

  // Setup to intercept reinstall action, so we can see what the delay would
  // have been for the real action.
  content_verifier_test::DelayTracker delay_tracker;

  // Do 4 iterations of disabling followed by reinstall.
  const size_t iterations = 4;
  for (size_t i = 0; i < iterations; i++) {
    TestExtensionRegistryObserver registry_observer(registry, id_);
    verifier->VerifyFailed(id_, ContentVerifyJob::HASH_MISMATCH);
    EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());
    // Resolve the request to |delay_tracker|, so the reinstallation can
    // proceed.
    delay_tracker.Proceed();
    EXPECT_TRUE(registry_observer.WaitForExtensionInstalled());
  }
  const std::vector<base::TimeDelta>& calls = delay_tracker.calls();

  // After |delay_tracker| resolves the 4 (|iterations|) reinstallation
  // requests, it will get an additional request (right away) for retrying
  // reinstallation.
  // Note: the additional request in non-test environment will arrive with
  // a (backoff) delay. But during test, |delay_tracker| issues the request
  // immediately.
  ASSERT_EQ(iterations, calls.size() - 1);
  // Assert that the first reinstall action happened with a delay of 0, and
  // then kept growing each additional time.
  EXPECT_EQ(base::TimeDelta(), delay_tracker.calls()[0]);
  for (size_t i = 1; i < delay_tracker.calls().size(); i++) {
    EXPECT_LT(calls[i - 1], calls[i]);
  }
}

// Tests that if CheckForExternalUpdates() fails, then we retry reinstalling
// corrupted policy extensions. For example: if network is unavailable,
// CheckForExternalUpdates() will fail.
IN_PROC_BROWSER_TEST_F(ContentVerifierPolicyTest, FailedUpdateRetries) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ExtensionSystem* system = ExtensionSystem::Get(profile());
  ExtensionService* service = system->extension_service();
  ContentVerifier* verifier = system->content_verifier();

  // Wait for the extension to be installed by the policy we set up in
  // SetUpInProcessBrowserTestFixture.
  if (!registry->GetInstalledExtension(id_)) {
    TestExtensionRegistryObserver registry_observer(registry, id_);
    EXPECT_TRUE(registry_observer.WaitForExtensionInstalled());
  }

  content_verifier_test::DelayTracker delay_tracker;
  service->set_external_updates_disabled_for_test(true);
  TestExtensionRegistryObserver registry_observer(registry, id_);
  verifier->VerifyFailed(id_, ContentVerifyJob::HASH_MISMATCH);
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());

  const std::vector<base::TimeDelta>& calls = delay_tracker.calls();
  ASSERT_EQ(1u, calls.size());
  EXPECT_EQ(base::TimeDelta(), delay_tracker.calls()[0]);

  delay_tracker.Proceed();

  // Remove the override and set ExtensionService to update again. The extension
  // should be now installed.
  PolicyExtensionReinstaller::set_policy_reinstall_action_for_test(nullptr);
  service->set_external_updates_disabled_for_test(false);
  delay_tracker.Proceed();

  EXPECT_TRUE(registry_observer.WaitForExtensionInstalled());
}

}  // namespace extensions
