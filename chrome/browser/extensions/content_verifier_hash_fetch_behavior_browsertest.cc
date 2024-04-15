// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/file_util.h"
#include "services/network/public/cpp/features.h"

namespace extensions {

namespace {

// Specifies the content verification mode.
enum ContentVerificationMode {
  // Uses ContentVerifierDelegate::ENFORCE mode.
  kEnforce,
  // Uses ContentVerifierDelegate::ENFORCE_STRICT mode.
  kEnforceStrict
};

}  // namespace

// Tests content verification's hash fetch behavior and its implication on
// verification failure in different verification modes (enforce and
// enforce_strict).
// TODO(lazyboy): Add assertions for checking verified_contents.json file's
// validity after running each test.
class ContentVerifierHashTest
    : public ExtensionBrowserTest,
      public testing::WithParamInterface<ContentVerificationMode> {
 public:
  ContentVerifierHashTest() = default;

  ContentVerifierHashTest(const ContentVerifierHashTest&) = delete;
  ContentVerifierHashTest& operator=(const ContentVerifierHashTest&) = delete;

  ~ContentVerifierHashTest() override {}

  enum TamperResourceType {
    kTamperRequestedResource,
    kTamperNotRequestedResource
  };

  // ExtensionBrowserTest:
  bool ShouldEnableContentVerification() override { return true; }

  void SetUp() override {
    // Override content verification mode before ExtensionSystemImpl initializes
    // ChromeContentVerifierDelegate.
    ChromeContentVerifierDelegate::SetDefaultModeForTesting(
        uses_enforce_strict_mode()
            ? ChromeContentVerifierDelegate::VerifyInfo::Mode::ENFORCE_STRICT
            : ChromeContentVerifierDelegate::VerifyInfo::Mode::ENFORCE);

    ExtensionBrowserTest::SetUp();
  }

  void TearDown() override {
    ExtensionBrowserTest::TearDown();
    ChromeContentVerifierDelegate::SetDefaultModeForTesting(std::nullopt);
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  bool uses_enforce_strict_mode() {
    return GetParam() == ContentVerificationMode::kEnforceStrict;
  }
  bool uses_enforce_mode() {
    return GetParam() == ContentVerificationMode::kEnforce;
  }

  void DisableHashFetching() { hash_fetching_disabled_ = true; }

  testing::AssertionResult InstallDefaultResourceExtension() {
    LOG(INFO) << "InstallDefaultResourceExtension";
    return InstallExtension(kHasDefaultResource);
  }
  testing::AssertionResult InstallNoDefaultResourceExtension() {
    LOG(INFO) << "InstallNoDefaultResourceExtension";
    return InstallExtension(kDoesNotHaveDefaultResource);
  }

  void DisableExtension() { ExtensionBrowserTest::DisableExtension(id()); }

  testing::AssertionResult DeleteVerifiedContents() {
    base::ScopedAllowBlockingForTesting allow_blocking;

    base::FilePath verified_contents_path =
        file_util::GetVerifiedContentsPath(info_->extension_root);
    if (!base::PathExists(verified_contents_path)) {
      return testing::AssertionFailure()
             << "Could not find verified_contents.json.";
    }

    // Delete verified_contents.json:
    if (!base::DeleteFile(verified_contents_path)) {
      return testing::AssertionFailure()
             << "Could not delete verified_contents.json.";
    }
    return testing::AssertionSuccess();
  }

  bool HasComputedHashes() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::PathExists(
        file_util::GetComputedHashesPath(info_->extension_root));
  }

  testing::AssertionResult DeleteComputedHashes() {
    LOG(INFO) << "Deleting computed_hashes.json";
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!HasComputedHashes()) {
      return testing::AssertionFailure()
             << "Could not find computed_hashes.json for deletion. "
             << "Make sure the previous steps created a "
             << "computed_hashes.json, otherwise tests might fail/flake";
    }
    base::FilePath computed_hashes_path =
        file_util::GetComputedHashesPath(info_->extension_root);
    if (!base::DeleteFile(computed_hashes_path)) {
      return testing::AssertionFailure()
             << "Error deleting computed_hashes.json.";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult TamperComputedHashes() {
    LOG(INFO) << "Tampering computed_hashes.json";
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!HasComputedHashes()) {
      return testing::AssertionFailure()
             << "Could not find computed_hashes.json for tampering.";
    }
    base::FilePath computed_hashes_path =
        file_util::GetComputedHashesPath(info_->extension_root);
    std::string extra = R"({hello:"world"})";
    if (!base::AppendToFile(computed_hashes_path, extra)) {
      return testing::AssertionFailure()
             << "Could not tamper computed_hashes.json";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult TamperResource(TamperResourceType type) {
    const std::string resource_to_tamper =
        type == kTamperRequestedResource ? "background.js" : "script.js";
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Modify content of a resource if this test requested that, time the
    // extension loads, hash fetch will discover content verification failure
    // due to hash mismatch.
    std::string extra = "some_extra_function_call();";
    base::FilePath real_path =
        info_->extension_root.AppendASCII(resource_to_tamper);
    if (!base::AppendToFile(real_path, extra)) {
      return testing::AssertionFailure()
             << "Could not tamper " << resource_to_tamper << ".";
    }
    return testing::AssertionSuccess();
  }

  const ExtensionId& id() const { return info_->extension_id; }

  void EnableExtensionAndWaitForCompletion(bool expect_disabled) {
    LOG(INFO) << "EnableExtensionAndWaitForCompletion: expect_disabled = "
              << expect_disabled;
    // Only observe ContentVerifyJob when necessary. This is because
    // ContentVerifyJob's callback and ContentVerifyJob::OnExtensionLoad's
    // callbacks can be race-y.
    std::unique_ptr<TestContentVerifyJobObserver> job_observer;
    const bool needs_to_observe_content_verify_job =
        // If the test wouldn't disable the extension, extensions with
        // default resource(s) will always see at at least one ContentVerifyJob
        // to a default resource (background.js).
        info_->type == kHasDefaultResource && !expect_disabled;

    if (needs_to_observe_content_verify_job) {
      LOG(INFO) << "Observing ContentVerifyJob";
      job_observer = std::make_unique<TestContentVerifyJobObserver>();
      using Result = TestContentVerifyJobObserver::Result;
      job_observer->ExpectJobResult(
          id(), base::FilePath(FILE_PATH_LITERAL("background.js")),
          Result::SUCCESS);
    }

    TestExtensionRegistryObserver registry_observer(
        ExtensionRegistry::Get(profile()), id());
    VerifierObserver verifier_observer;
    {
      EnableExtension(id());
      registry_observer.WaitForExtensionLoaded();
    }
    verifier_observer.EnsureFetchCompleted(id());
    LOG(INFO) << "Verifier observer has seen FetchComplete";

    if (job_observer) {
      LOG(INFO) << "ContentVerifyJobObserver, wait for expected job";
      job_observer->WaitForExpectedJobs();
      LOG(INFO) << "ContentVerifyJobObserver, completed expected job";
    }
  }

  bool ExtensionIsDisabledForCorruption() {
    const Extension* extension =
        ExtensionRegistry::Get(profile())->disabled_extensions().GetByID(id());
    if (!extension)
      return false;

    ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
    // Make sure the extension got disabled due to corruption (and only due to
    // corruption).
    int reasons = prefs->GetDisableReasons(id());
    return reasons == disable_reason::DISABLE_CORRUPTED;
  }

  bool ExtensionIsEnabled() {
    return ExtensionRegistry::Get(profile())->enabled_extensions().Contains(
        id());
  }

  bool HasValidComputedHashes() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ComputedHashes::Status computed_hashes_status;
    return ComputedHashes::CreateFromFile(
               file_util::GetComputedHashesPath(info_->extension_root),
               &computed_hashes_status) != std::nullopt;
  }

  bool HasValidVerifiedContents() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath verified_contents_path =
        file_util::GetVerifiedContentsPath(info_->extension_root);
    if (!base::PathExists(verified_contents_path)) {
      ADD_FAILURE() << "Could not find verified_contents.json.";
      return false;
    }
    std::string contents;
    if (!base::ReadFileToString(verified_contents_path, &contents)) {
      ADD_FAILURE() << "Could not read verified_contents.json.";
      return false;
    }
    return verified_contents_contents_ == contents;
  }

 private:
  enum ExtensionType {
    // An extension (has_default_resource.crx) that by default requests a
    // resource in it during ExtensionLoad.
    kHasDefaultResource,
    // An extension (no_default_resources.crx) that doesn't request any
    // resource during ExtensionLoad.
    kDoesNotHaveDefaultResource
  };

  struct ExtensionInfo {
    ExtensionId extension_id;
    base::FilePath extension_root;
    base::Version version;
    ExtensionType type;

    ExtensionInfo(const ExtensionId& extension_id,
                  const base::FilePath& extension_root,
                  const base::Version& version,
                  ExtensionType type)
        : extension_id(extension_id),
          extension_root(extension_root),
          version(version),
          type(type) {}
    ExtensionInfo(const Extension* extension, ExtensionType type)
        : ExtensionInfo(extension->id(),
                        extension->path(),
                        extension->version(),
                        type) {}
  };

  // Stores verified_contents.json into a temp file.
  bool CopyVerifiedContents(base::FilePath* out_path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!temp_dir_.CreateUniqueTempDir()) {
      ADD_FAILURE() << "Could not create temp dir for test.";
      return false;
    }

    base::FilePath verified_contents_path =
        file_util::GetVerifiedContentsPath(info_->extension_root);
    if (!base::PathExists(verified_contents_path)) {
      ADD_FAILURE() << "Could not find verified_contents.json for copying.";
      return false;
    }
    base::FilePath destination = temp_dir_.GetPath();
    *out_path = destination.Append(FILE_PATH_LITERAL("copy.json"));
    if (!base::CopyFile(verified_contents_path, *out_path)) {
      ADD_FAILURE() << "Could not copy verified_contents.json to a temp dir.";
      return false;
    }
    if (!base::ReadFileToString(verified_contents_path,
                                &verified_contents_contents_)) {
      ADD_FAILURE() << "Could not read verified_contents.json.";
      return false;
    }
    return true;
  }

  // Installs test extension that is copied from the webstore with actual
  // signatures.
  testing::AssertionResult InstallExtension(ExtensionType type) {
    // This observer will make sure content hash read and computed_hashes.json
    // writing is complete before we proceed.
    VerifierObserver verifier_observer;

    const std::string crx_relative_path =
        type == kHasDefaultResource
            ? "content_verifier/has_default_resource.crx"
            : "content_verifier/no_default_resources.crx";
    // These test extension is copied from the webstore that has actual
    // signatures.
    const Extension* extension = InstallExtensionFromWebstore(
        test_data_dir_.AppendASCII(crx_relative_path), 1);
    if (!extension) {
      return testing::AssertionFailure()
             << "Could not install extension: " << crx_relative_path;
    }

    const ExtensionId& extension_id = extension->id();
    verifier_observer.EnsureFetchCompleted(extension_id);

    info_ = std::make_unique<ExtensionInfo>(extension, type);

    // Set up the interceptor functor and data needed by it.
    if (!hash_fetching_disabled_ && !InstallInterceptor())
      return testing::AssertionFailure() << "Failed to install interceptor.";

    return testing::AssertionSuccess();
  }

  bool InstallInterceptor() {
    if (url_loader_interceptor_) {
      testing::AssertionFailure() << "Already created interceptor.";
      return false;
    }

    SetUpInterceptorData();

    auto interceptor_function =
        [](GURL* fetch_url, base::FilePath* file_path,
           content::URLLoaderInterceptor::RequestParams* params) {
          GURL url = params->url_request.url;
          if (url == *fetch_url) {
            base::ScopedAllowBlockingForTesting allow_io;
            std::string contents;
            CHECK(base::ReadFileToString(*file_path, &contents));

            content::URLLoaderInterceptor::WriteResponse(
                std::string(), contents, params->client.get());
            return true;
          }
          return false;
        };
    url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindRepeating(interceptor_function, &fetch_url_,
                            &copied_verified_contents_path_));
    return true;
  }

  bool SetUpInterceptorData() {
    // Use stored copy of verified_contents.json as hash fetch response.
    if (!CopyVerifiedContents(&copied_verified_contents_path_)) {
      ADD_FAILURE() << "Could not copy verified_contents.json.";
      return false;
    }

    ExtensionSystem* system = ExtensionSystem::Get(profile());
    fetch_url_ = system->content_verifier()->GetSignatureFetchUrlForTest(
        id(), info_->version);

    return true;
  }

  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  base::ScopedTempDir temp_dir_;

  base::FilePath copied_verified_contents_path_;
  GURL fetch_url_;

  // Information about the loaded extension.
  std::unique_ptr<ExtensionInfo> info_;

  // Contents of verified_contents.json (if available).
  std::string verified_contents_contents_;

  bool hash_fetching_disabled_ = false;
};

// Tests that corruption of a requested extension resource always disables the
// extension.
// Flaky test. See crbug.com/1276043.
IN_PROC_BROWSER_TEST_P(ContentVerifierHashTest,
                       DISABLED_TamperRequestedResourceKeepComputedHashes) {
  ASSERT_TRUE(InstallDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // computed_hashes.json should remain valid.
  EXPECT_TRUE(HasComputedHashes());

  // Tamper an extension resource that will be requested on next load.
  EXPECT_TRUE(TamperResource(kTamperRequestedResource));

  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), id());
  // Since we tampered with the resource, content verification should
  // disable the extension, both in "enforce_strict" and "enforce" mode.
  EnableExtensionAndWaitForCompletion(true /* expect_disabled */);
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());

  EXPECT_TRUE(ExtensionIsDisabledForCorruption());

  // Expect a valid verified_contents.json file at the end.
  EXPECT_TRUE(HasValidVerifiedContents());
}

// Tests that tampering a resource that will be requested by the extension and
// deleting computed_hashes.json will always disable the extension.
IN_PROC_BROWSER_TEST_P(ContentVerifierHashTest,
                       TamperRequestedResourceDeleteComputedHashes) {
  ASSERT_TRUE(InstallDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // Delete computed_hashes.json.
  EXPECT_TRUE(DeleteComputedHashes());

  // Tamper an extension resource that will be requested on next load.
  EXPECT_TRUE(TamperResource(kTamperRequestedResource));

  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), id());
  // Since we tampered with the resource, content verification should
  // disable the extension, both in "enforce_strict" and "enforce" mode.
  EnableExtensionAndWaitForCompletion(true /* expect_disabled */);
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());

  EXPECT_TRUE(ExtensionIsDisabledForCorruption());

  // Expect a valid computed_hashes.json file at the end, the verification
  // failure must have used this file to detect corruption.
  EXPECT_TRUE(HasValidComputedHashes());

  // Expect a valid verified_contents.json file at the end.
  EXPECT_TRUE(HasValidVerifiedContents());
}

// Tests that tampering a resource that will be requested by the extension and
// tampering computed_hashes.json will always disable the extension.
// TODO(crbug.com/40810537): Flaky.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TamperRequestedResourceTamperComputedHashes \
  DISABLED_TamperRequestedResourceTamperComputedHashes
#else
#define MAYBE_TamperRequestedResourceTamperComputedHashes \
  TamperRequestedResourceTamperComputedHashes
#endif
IN_PROC_BROWSER_TEST_P(ContentVerifierHashTest,
                       MAYBE_TamperRequestedResourceTamperComputedHashes) {
  ASSERT_TRUE(InstallDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // Tamper computed_hashes.json.
  EXPECT_TRUE(TamperComputedHashes());

  // Tamper an extension resource that will be requested on next load.
  EXPECT_TRUE(TamperResource(kTamperRequestedResource));

  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), id());
  // Since we tampered with the resource, content verification should
  // disable the extension, both in "enforce_strict" and "enforce" mode.
  EnableExtensionAndWaitForCompletion(true /* expect_disabled */);
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());

  EXPECT_TRUE(ExtensionIsDisabledForCorruption());

  // Expect a valid computed_hashes.json file at the end, the verification
  // failure must have used this file to detect corruption.
  EXPECT_TRUE(HasValidComputedHashes());

  // Expect a valid verified_contents.json file at the end.
  EXPECT_TRUE(HasValidVerifiedContents());
}

// Tests hash fetch failure scenario with an extension that requests resource(s)
// by default.
IN_PROC_BROWSER_TEST_P(ContentVerifierHashTest,
                       FetchFailureWithDefaultResourceExtension) {
  // Do *not* install any hash fetch interceptor, so that hash fetch after
  // reload fails.
  DisableHashFetching();

  ASSERT_TRUE(InstallDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // computed_hashes.json and extension resource(s) aren't touched since they do
  // not matter as hash fetch will fail.

  // In "enforce_strict" mode, hash fetch failures will cause the extension to
  // be disabled. Implementation-wise, this happens because the requested
  // resource's ContentVerifyJob will result in failure, not because the hash
  // fetch failed. See https://crbug.com/819818 for details.
  //
  // In "enforce" mode, the extension won't be disabled. However, since we
  // request a resource (background.js), its corresponding ContentVerifyJob will
  // attempt to fetch hash and that job will also fail. In order to achieve
  // determinism in this case, also observe a ContentVerifyJob that will fail.
  const bool expect_disabled = uses_enforce_strict_mode();

  // Similar to EnableExtensionAndWaitForCompletion, but also forces a
  // ContentVerifyJob observer in "enforce" mode.
  // Instead of generalizing this oddball expectation into
  // EnableExtensionAndWaitForCompletion, provide the implementation right here
  // in the test body.
  {
    LOG(INFO) << "EnableExtensionAndWaitForCompletion: expect_disabled = "
              << expect_disabled;
    ExtensionId extension_id = id();

    // Only observe ContentVerifyJob when necessary. This is because
    // ContentVerifyJob's callback and ContentVerifyJob::OnExtensionLoad's
    // callbacks can be race-y.
    std::unique_ptr<TestContentVerifyJobObserver> job_observer;
    if (uses_enforce_mode()) {
      // In "enforce" mode, we expect to see a job completion (and its failure).
      job_observer = std::make_unique<TestContentVerifyJobObserver>();
      using Result = TestContentVerifyJobObserver::Result;
      job_observer->ExpectJobResult(
          extension_id,
          // This extension has default resource (background.js), so it must
          // request it.
          base::FilePath(FILE_PATH_LITERAL("background.js")), Result::FAILURE);
    }

    TestExtensionRegistryObserver registry_observer(
        ExtensionRegistry::Get(profile()), extension_id);
    VerifierObserver verifier_observer;
    {
      EnableExtension(extension_id);
      registry_observer.WaitForExtensionLoaded();
    }
    verifier_observer.EnsureFetchCompleted(extension_id);
    LOG(INFO) << "Verifier observer has seen FetchComplete";

    if (job_observer) {
      LOG(INFO) << "ContentVerifyJobObserver, wait for expected job";
      job_observer->WaitForExpectedJobs();
      LOG(INFO) << "ContentVerifyJobObserver, completed expected job";
    }

    if (expect_disabled)
      EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());
  }

  EXPECT_TRUE(expect_disabled ? ExtensionIsDisabledForCorruption()
                              : ExtensionIsEnabled());
}

// Tests that hash fetch failure for loading an extension that doesn't request
// any resource by default will not be disabled.
IN_PROC_BROWSER_TEST_P(ContentVerifierHashTest,
                       FetchFailureWithNoDefaultResourceDoesNotDisable) {
  // Do *not* install any hash fetch interceptor, so that hash fetch after
  // reload fails.
  DisableHashFetching();

  ASSERT_TRUE(InstallNoDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // computed_hashes.json and extension resource(s) aren't touched since they do
  // not matter as hash fetch will fail.

  // If the extension didn't explicitly request any resources, then there will
  // not be any content verification failures.
  const bool expect_disabled = false;
  // TODO(lazyboy): https://crbug.com/819818: "enforce_strict" mode should
  // disable the extension.
  // const bool expect_disabled = uses_enforce_strict_mode();
  EnableExtensionAndWaitForCompletion(expect_disabled);

  EXPECT_TRUE(expect_disabled ? ExtensionIsDisabledForCorruption()
                              : ExtensionIsEnabled());
}

// Tests the behavior of tampering an extension resource that is not requested
// by default and without modifying computed_hashes.json.
IN_PROC_BROWSER_TEST_P(ContentVerifierHashTest,
                       TamperNotRequestedResourceKeepComputedHashes) {
  ASSERT_TRUE(InstallDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // computed_hashes.json should remain valid.
  EXPECT_TRUE(HasComputedHashes());

  // Tamper an extension resource that will *not* be requested on next load.
  EXPECT_TRUE(TamperResource(kTamperNotRequestedResource));

  // We tampered a resource that is not requested by the extension. Keeping
  // computed_hashes.json will not compute new compute computed_hashes.json, and
  // we will not discover the tampered hash. So the extension won't be disabled.
  //
  // TODO(lazyboy): http://crbug.com/819832: We fetched a new
  // verified_contents.json in this case. However, if we had recomputed
  // computed_hashes.json we would have discovered the tampered resource's hash
  // mismatch. Fix.
  const bool expect_disabled = false;
  EnableExtensionAndWaitForCompletion(expect_disabled);

  EXPECT_TRUE(ExtensionIsEnabled());

  // Expect a valid verified_contents.json file at the end.
  EXPECT_TRUE(HasValidVerifiedContents());
}

// Tests the behavior of loading an extension without any default resource
// request and deleting its computed_hashes.json before fetching hashes.
IN_PROC_BROWSER_TEST_P(ContentVerifierHashTest,
                       TamperNoResourceExtensionDeleteComputedHashes) {
  ASSERT_TRUE(InstallNoDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // Delete computed_hashes.json.
  EXPECT_TRUE(DeleteComputedHashes());

  // Tamper an extension resource that will *not* be requested on next load.
  EXPECT_TRUE(TamperResource(kTamperNotRequestedResource));

  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), id());
  // The deletion of computed_hashes.json forces its recomputation and disables
  // the extension.
  EnableExtensionAndWaitForCompletion(true /* expect_disabled */);
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());

  EXPECT_TRUE(ExtensionIsDisabledForCorruption());

  // Expect a valid computed_hashes.json file at the end, the verification
  // failure must have used this file to detect corruption.
  EXPECT_TRUE(HasValidComputedHashes());

  // Expect a valid verified_contents.json file at the end.
  EXPECT_TRUE(HasValidVerifiedContents());
}

// Tests the behavior of loading an extension without any default resource
// request and keeping its computed_hashes.json.
IN_PROC_BROWSER_TEST_P(ContentVerifierHashTest,
                       TamperNoResourceExtensionKeepComputedHashes) {
  ASSERT_TRUE(InstallNoDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // computed_hashes.json should remain valid.
  EXPECT_TRUE(HasComputedHashes());

  // Tamper an extension resource that will *not* be requested on next load.
  EXPECT_TRUE(TamperResource(kTamperNotRequestedResource));

  // Not modifying computed_hashes.json will not trigger any hash computation
  // at OnExtensionLoad, so we won't discover any hash mismatches.
  EnableExtensionAndWaitForCompletion(false /* expect_disabled */);

  EXPECT_TRUE(ExtensionIsEnabled());

  // Expect a valid verified_contents.json file at the end.
  EXPECT_TRUE(HasValidVerifiedContents());
}

// Tests the behavior of loading an extension without any default resource
// request and tampering its computed_hashes.json.
IN_PROC_BROWSER_TEST_P(ContentVerifierHashTest,
                       TamperNoResourceExtensionTamperComputedHashes) {
  ASSERT_TRUE(InstallNoDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // Tamper computed_hashes.json.
  EXPECT_TRUE(TamperComputedHashes());

  // Tamper an extension resource that will *not* be requested on next load.
  EXPECT_TRUE(TamperResource(kTamperNotRequestedResource));

  // Tampering computed_hashes.json will not trigger any hash computation
  // at OnExtensionLoad, so we won't discover any hash mismatches.
  // TODO(lazyboy): Consider fixing this, see http://crbug.com/819832 for
  // details.
  EnableExtensionAndWaitForCompletion(false /* expect_disabled */);

  EXPECT_TRUE(ExtensionIsEnabled());

  // Because we didn't do any hash computation, expect computed_hashes.json to
  // still remain invalid.
  EXPECT_FALSE(HasValidComputedHashes());

  // Expect a valid verified_contents.json file at the end.
  EXPECT_TRUE(HasValidVerifiedContents());
}

// Tests the behavior of loading a default resource extension with tampering
// an extension resource that is not requested by default and without modifying
// computed_hashes.json.
IN_PROC_BROWSER_TEST_P(
    ContentVerifierHashTest,
    DefaultRequestExtensionTamperNotRequestedResourceKeepComputedHashes) {
  ASSERT_TRUE(InstallDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // computed_hashes.json should remain valid.
  EXPECT_TRUE(HasComputedHashes());

  // Tamper an extension resource that will *not* be requested on next load.
  EXPECT_TRUE(TamperResource(kTamperNotRequestedResource));

  // TODO(lazyboy): Not modifying the computed_hashes.json file doesn't prompt
  // a hash recomputation and the requested (not-tampered) resource's
  // corresponding ContentVerifyJob succeeds because that resource's hash
  // remains fine. Therefore, the extension remains enabled. Consider disabling
  // the extension in this case: https://crbug.com/819832.
  EnableExtensionAndWaitForCompletion(false /* expect_disabled */);

  EXPECT_TRUE(ExtensionIsEnabled());

  // Expect a valid verified_contents.json file at the end.
  EXPECT_TRUE(HasValidVerifiedContents());
}

// Tests the behavior of loading a default resource extension with tampering
// an extension resource that is not requested by default and tampering
// computed_hashes.json.
// TODO(crbug.com/40810776): Flaky.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DefaultRequestExtensionTamperNotRequestedResourceTamperComputedHashes \
  DISABLED_DefaultRequestExtensionTamperNotRequestedResourceTamperComputedHashes
#else
#define MAYBE_DefaultRequestExtensionTamperNotRequestedResourceTamperComputedHashes \
  DefaultRequestExtensionTamperNotRequestedResourceTamperComputedHashes
#endif
IN_PROC_BROWSER_TEST_P(
    ContentVerifierHashTest,
    MAYBE_DefaultRequestExtensionTamperNotRequestedResourceTamperComputedHashes) {
  ASSERT_TRUE(InstallDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // Tamper computed_hashes.json.
  EXPECT_TRUE(TamperComputedHashes());

  // Tamper an extension resource that will *not* be requested on next load.
  EXPECT_TRUE(TamperResource(kTamperNotRequestedResource));

  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), id());
  // Subtle: tampering computed_hashes.json (by itself) will not trigger any
  // hash computation or failure during OnExtensionLoad. However, the default
  // resource request (that isn't tampered) will prompt a hash read that will
  // fail due to the tampered computed_hashes.json.
  EnableExtensionAndWaitForCompletion(true /* expect_disabled */);
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());

  EXPECT_TRUE(ExtensionIsDisabledForCorruption());

  // Expect a valid computed_hashes.json file at the end, the verification
  // failure must have used this file to detect corruption.
  EXPECT_TRUE(HasValidComputedHashes());

  // Expect a valid verified_contents.json file at the end.
  EXPECT_TRUE(HasValidVerifiedContents());
}

// Tests the behavior of loading a default resource extension with tampering
// an extension resource that is not requested by default and deleting
// computed_hashes.json.
IN_PROC_BROWSER_TEST_P(
    ContentVerifierHashTest,
    DefaultRequestExtensionTamperNotRequestedResourceDeleteComputedHashes) {
  ASSERT_TRUE(InstallDefaultResourceExtension());

  DisableExtension();

  // Delete verified_contents.json to force a hash fetch on next load.
  EXPECT_TRUE(DeleteVerifiedContents());

  // Delete computed_hashes.json.
  EXPECT_TRUE(DeleteComputedHashes());

  // Tamper an extension resource that will *not* be requested on next load.
  EXPECT_TRUE(TamperResource(kTamperNotRequestedResource));

  TestExtensionRegistryObserver registry_observer(
      ExtensionRegistry::Get(profile()), id());
  // The deletion of computed_hashes.json will trigger hash recomputation and
  // the file's regeneration. This will discover the resource tampering and
  // disable the extension.
  EnableExtensionAndWaitForCompletion(true /* expect_disabled */);
  EXPECT_TRUE(registry_observer.WaitForExtensionUnloaded());

  EXPECT_TRUE(ExtensionIsDisabledForCorruption());

  // Expect a valid computed_hashes.json file at the end, the verification
  // failure must have used this file to detect corruption.
  EXPECT_TRUE(HasValidComputedHashes());

  // Expect a valid verified_contents.json file at the end.
  EXPECT_TRUE(HasValidVerifiedContents());
}

INSTANTIATE_TEST_SUITE_P(FetchBehaviorTests,
                         ContentVerifierHashTest,
                         testing::Values(kEnforce, kEnforceStrict));

}  // namespace extensions
