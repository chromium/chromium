// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <stddef.h>

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_multi_source_observation.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_closed_waiter.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/exit_type_service.h"
#endif

namespace {

// A helper class which creates a SimpleURLLoader with an expected final status
// and the ability to wait until a request completes. It's not considered a
// failure for the load to never complete.
class SimpleURLLoaderHelper {
 public:
  // Creating the SimpleURLLoaderHelper automatically creates and starts a
  // SimpleURLLoader.
  SimpleURLLoaderHelper(network::mojom::URLLoaderFactory* factory,
                        const GURL& url,
                        int expected_error_code,
                        int load_flags = net::LOAD_NORMAL)
      : expected_error_code_(expected_error_code), is_complete_(false) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = url;
    request->load_flags = load_flags;

    // Populate Network Isolation Key so that the request is cacheable.
    url::Origin origin = url::Origin::Create(url);
    request->trusted_params = network::ResourceRequest::TrustedParams();
    request->trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(origin);
    request->site_for_cookies =
        request->trusted_params->isolation_info.site_for_cookies();

    loader_ = network::SimpleURLLoader::Create(std::move(request),
                                               TRAFFIC_ANNOTATION_FOR_TESTS);

    loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        factory, base::BindOnce(&SimpleURLLoaderHelper::OnSimpleLoaderComplete,
                                base::Unretained(this)));
  }

  SimpleURLLoaderHelper(const SimpleURLLoaderHelper&) = delete;
  SimpleURLLoaderHelper& operator=(const SimpleURLLoaderHelper&) = delete;

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body) {
    EXPECT_EQ(expected_error_code_, loader_->NetError());
    is_complete_ = true;
    run_loop_.Quit();
  }

  void WaitForCompletion() { run_loop_.Run(); }

  bool is_complete() const { return is_complete_; }

 private:
  const int expected_error_code_;
  base::RunLoop run_loop_;

  bool is_complete_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
};

class MockProfileDelegate : public Profile::Delegate {
 public:
  MOCK_METHOD2(OnProfileCreationStarted, void(Profile*, Profile::CreateMode));
  MOCK_METHOD4(OnProfileCreationFinished,
               void(Profile*, Profile::CreateMode, bool, bool));
};

// Creates a prefs file in the given directory.
void CreatePrefsFileInDirectory(const base::FilePath& directory_path) {
  base::FilePath pref_path(directory_path.Append(chrome::kPreferencesFilename));
  std::string data("{}");
  ASSERT_TRUE(base::WriteFile(pref_path, data));
}

void CheckChromeVersion(Profile* profile, bool is_new) {
  std::string created_by_version;
  if (is_new) {
    created_by_version = version_info::GetVersionNumber();
  } else {
    created_by_version = "1.0.0.0";
  }
  std::string pref_version =
      ChromeVersionService::GetVersion(profile->GetPrefs());
  // Assert that created_by_version pref gets set to current version.
  EXPECT_EQ(created_by_version, pref_version);
}

void FlushTaskRunner(base::SequencedTaskRunner* runner) {
  ASSERT_TRUE(runner);
  base::WaitableEvent unblock(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);

  runner->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                             base::Unretained(&unblock)));

  unblock.Wait();
}

void SpinThreads() {
  // Give threads a chance to do their stuff before shutting down (i.e.
  // deleting scoped temp dir etc).
  // Should not be necessary anymore once Profile deletion is fixed
  // (see crbug.com/88586).
  content::RunAllPendingInMessageLoop();

  // This prevents HistoryBackend from accessing its databases after the
  // directory that contains them has been deleted.
  base::ThreadPoolInstance::Get()->FlushForTesting();
}

}  // namespace

class ProfileBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_CHROMEOS)
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  std::unique_ptr<Profile> CreateProfile(const base::FilePath& path,
                                         Profile::Delegate* delegate,
                                         Profile::CreateMode create_mode) {
    std::unique_ptr<Profile> profile =
        Profile::CreateProfile(path, delegate, create_mode);
    EXPECT_TRUE(profile.get());

    // Store the Profile's IO task runner so we can wind it down.
    profile_io_task_runner_ = profile->GetIOTaskRunner();

    return profile;
  }

  void FlushIoTaskRunnerAndSpinThreads() {
    FlushTaskRunner(profile_io_task_runner_.get());
    SpinThreads();
  }

  // Starts a test where a SimpleURLLoader is active during profile
  // shutdown. The test completes during teardown of the test fixture. The
  // request should be canceled by |context_getter| during profile shutdown,
  // before the URLRequestContext is destroyed. If that doesn't happen, the
  // Context's will still have outstanding requests during its destruction, and
  // will trigger a CHECK failure.
  void StartActiveLoaderDuringProfileShutdownTest(
      network::mojom::URLLoaderFactory* factory) {
    // This method should only be called once per test.
    DCHECK(!simple_loader_helper_);

    // Start a hanging request.  This request may or may not completed before
    // the end of the request.
    simple_loader_helper_ = std::make_unique<SimpleURLLoaderHelper>(
        factory, embedded_test_server()->GetURL("/hung"), net::ERR_FAILED);

    // Start a second mock request that just fails, and wait for it to complete.
    // This ensures the first request has reached the network stack.
    SimpleURLLoaderHelper simple_loader_helper2(
        factory, embedded_test_server()->GetURL("/echo?status=400"),
        net::ERR_HTTP_RESPONSE_CODE_FAILURE);
    simple_loader_helper2.WaitForCompletion();

    // The first request should still be hung.
    EXPECT_FALSE(simple_loader_helper_->is_complete());
  }

  // Runs a test where an incognito profile's SimpleURLLoader is active during
  // teardown of the profile, and makes sure the request fails as expected.
  // Also tries issuing a request after the incognito profile has been
  // destroyed.
  static void RunURLLoaderActiveDuringIncognitoTeardownTest(
      net::EmbeddedTestServer* embedded_test_server,
      Browser* incognito_browser,
      network::mojom::URLLoaderFactory* factory) {
    // Start a hanging request.
    SimpleURLLoaderHelper simple_loader_helper1(
        factory, embedded_test_server->GetURL("/hung"), net::ERR_FAILED);

    // Start a second mock request that just fails, and wait for it to complete.
    // This ensures the first request has reached the network stack.
    SimpleURLLoaderHelper simple_loader_helper2(
        factory, embedded_test_server->GetURL("/echo?status=400"),
        net::ERR_HTTP_RESPONSE_CODE_FAILURE);
    simple_loader_helper2.WaitForCompletion();

    // The first request should still be hung.
    EXPECT_FALSE(simple_loader_helper1.is_complete());

    // Close all incognito tabs, starting profile shutdown.
    incognito_browser->tab_strip_model()->CloseAllTabs();

    // The request should have been canceled when the Profile shut down.
    simple_loader_helper1.WaitForCompletion();

    // Requests issued after Profile shutdown should fail in a similar manner.
    SimpleURLLoaderHelper simple_loader_helper3(
        factory, embedded_test_server->GetURL("/hung"), net::ERR_FAILED);
    simple_loader_helper3.WaitForCompletion();
  }

  scoped_refptr<base::SequencedTaskRunner> profile_io_task_runner_;

  // SimpleURLLoader that outlives the Profile, to test shutdown.
  std::unique_ptr<SimpleURLLoaderHelper> simple_loader_helper_;
};

// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile synchronously.
// TODO(crbug.com/40771709): Flaky on ChromeOS-Ash.
// TODO(crbug.com/40826385): Failing on Mac.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_CreateNewProfileSynchronous DISABLED_CreateNewProfileSynchronous
#else
#define MAYBE_CreateNewProfileSynchronous CreateNewProfileSynchronous
#endif
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, MAYBE_CreateNewProfileSynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreationFinished(
                            testing::NotNull(),
                            Profile::CreateMode::kSynchronous, true, true));

  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CreateMode::kSynchronous));
    CheckChromeVersion(profile.get(), true);

    // Creating a profile causes an implicit connection attempt to a Mojo
    // service, which occurs as part of a new task. Before deleting |profile|,
    // ensure this task runs to prevent a crash.
    FlushIoTaskRunnerAndSpinThreads();
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile synchronously with an existing prefs file.
// TODO(crbug.com/40826385): Failing on Mac.
// TODO(b/328177667): Flaky on linux-chromeos-rel.
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_CHROMEOS) && defined(NDEBUG))
#define MAYBE_CreateOldProfileSynchronous DISABLED_CreateOldProfileSynchronous
#else
#define MAYBE_CreateOldProfileSynchronous CreateOldProfileSynchronous
#endif
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, MAYBE_CreateOldProfileSynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.GetPath());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreationFinished(
                            testing::NotNull(),
                            Profile::CreateMode::kSynchronous, true, false));

  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CreateMode::kSynchronous));
    CheckChromeVersion(profile.get(), false);

    // Creating a profile causes an implicit connection attempt to a Mojo
    // service, which occurs as part of a new task. Before deleting |profile|,
    // ensure this task runs to prevent a crash.
    FlushIoTaskRunnerAndSpinThreads();
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile asynchronously.
// TODO(crbug.com/40811337): Flaky on ChromeOS-Ash.
// TODO(crbug.com/40826385): Failing on Mac.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_CreateNewProfileAsynchronous DISABLED_CreateNewProfileAsynchronous
#else
#define MAYBE_CreateNewProfileAsynchronous CreateNewProfileAsynchronous
#endif
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, MAYBE_CreateNewProfileAsynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnProfileCreationFinished(
                            testing::NotNull(),
                            Profile::CreateMode::kAsynchronous, true, true))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  std::unique_ptr<Profile> profile(CreateProfile(
      temp_dir.GetPath(), &delegate, Profile::CreateMode::kAsynchronous));

  // Wait for the profile to be created.
  run_loop.Run();
  CheckChromeVersion(profile.get(), true);

  // Let all posted tasks complete before the profile is destroyed.
  FlushIoTaskRunnerAndSpinThreads();
}

// TODO(crbug.com/40812649): Flaky on ChromeOS-Ash.
// TODO(crbug.com/40771709): Flaky on Mac.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_CreateOldProfileAsynchronous DISABLED_CreateOldProfileAsynchronous
#else
#define MAYBE_CreateOldProfileAsynchronous CreateOldProfileAsynchronous
#endif
// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile asynchronously with an existing prefs file.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, MAYBE_CreateOldProfileAsynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.GetPath());

  MockProfileDelegate delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnProfileCreationFinished(
                            testing::NotNull(),
                            Profile::CreateMode::kAsynchronous, true, false))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  std::unique_ptr<Profile> profile(CreateProfile(
      temp_dir.GetPath(), &delegate, Profile::CreateMode::kAsynchronous));

  // Wait for the profile to be created.
  run_loop.Run();
  CheckChromeVersion(profile.get(), false);

  // Let all posted tasks complete before the profile is destroyed.
  FlushIoTaskRunnerAndSpinThreads();
}

// Test that a README file is created for profiles that didn't have it.
// TODO(crbug.com/40817682): Flaky on ChromeOS-Ash.
// TODO(crbug.com/40826385): Failing on Mac.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_ProfileReadmeCreated DISABLED_ProfileReadmeCreated
#else
#define MAYBE_ProfileReadmeCreated ProfileReadmeCreated
#endif
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, MAYBE_ProfileReadmeCreated) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnProfileCreationFinished(
                            testing::NotNull(),
                            Profile::CreateMode::kAsynchronous, true, true))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  std::unique_ptr<Profile> profile(CreateProfile(
      temp_dir.GetPath(), &delegate, Profile::CreateMode::kAsynchronous));

  // Wait for the profile to be created.
  run_loop.Run();

  // Wait until README is created on a background thread.
  FlushIoTaskRunnerAndSpinThreads();

  // Verify that README exists.
  EXPECT_TRUE(
      base::PathExists(temp_dir.GetPath().Append(chrome::kReadmeFilename)));
}

// The EndSession IO synchronization is only critical on Windows, but also
// happens under Ozone. See BrowserProcessImpl::EndSession.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE)

namespace {

std::string GetExitTypePreferenceFromDisk(Profile* profile) {
  base::FilePath prefs_path =
      profile->GetPath().Append(chrome::kPreferencesFilename);
  std::string prefs;
  if (!base::ReadFileToString(prefs_path, &prefs))
    return std::string();

  std::optional<base::Value> value = base::JSONReader::Read(prefs);
  if (!value)
    return std::string();

  base::Value::Dict* dict = value->GetIfDict();
  if (!dict)
    return std::string();

  const std::string* exit_type =
      dict->FindStringByDottedPath("profile.exit_type");
  if (!exit_type)
    return std::string();

  return *exit_type;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       WritesProfilesSynchronouslyOnEndSession) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);
  std::vector<Profile*> loaded_profiles = profile_manager->GetLoadedProfiles();

  ASSERT_NE(loaded_profiles.size(), 0UL);
  Profile* profile = loaded_profiles[0];

#if BUILDFLAG(IS_CHROMEOS)
  for (auto* loaded_profile : loaded_profiles) {
    if (!ash::ProfileHelper::IsSigninProfile(loaded_profile)) {
      profile = loaded_profile;
      break;
    }
  }
#endif

  // It is important that the MessageLoop not pump extra messages during
  // EndSession() as some of those may be tasks queued to attempt to revive
  // services and processes that were just intentionally killed. This is a
  // regression blocker for https://crbug.com/318527.
  // Need to use this WeakPtr workaround as the browser test harness runs all
  // tasks until idle when tearing down.
  struct FailsIfCalledWhileOnStack {
    void Fail() { ADD_FAILURE(); }
    base::WeakPtrFactory<FailsIfCalledWhileOnStack> weak_ptr_factory{this};
  } fails_if_called_while_on_stack;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FailsIfCalledWhileOnStack::Fail,
          fails_if_called_while_on_stack.weak_ptr_factory.GetWeakPtr()));

  // This retry loop reduces flakiness due to the fact that this ultimately
  // tests whether or not a code path hits a timed wait.
  bool succeeded = false;
  for (size_t retries = 0; !succeeded && retries < 3; ++retries) {
    // Flush the profile data to disk for all loaded profiles.
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
    ExitTypeService::GetInstanceForProfile(profile)->SetCurrentSessionExitType(
        ExitType::kCrashed);
#endif
    profile->GetPrefs()->CommitPendingWrite();
    FlushTaskRunner(profile->GetIOTaskRunner().get());

    // Make sure that the prefs file was written with the expected key/value.
    ASSERT_EQ(GetExitTypePreferenceFromDisk(profile), "Crashed");

    // The blocking wait in EndSession has a timeout.
    base::Time start = base::Time::Now();

    // This must not return until the profile data has been written to disk.
    // If this test flakes, then logoff on Windows has broken again.
    g_browser_process->EndSession();

    base::Time end = base::Time::Now();

    // The EndSession timeout is 10 seconds. If we take more than half that,
    // go around again, as we may have timed out on the wait.
    // This helps against flakes, and also ensures that if the IO thread starts
    // blocking systemically for that length of time (e.g. deadlocking or such),
    // we'll get a consistent test failure.
    if (end - start > base::Seconds(5))
      continue;

    // Make sure that the prefs file was written with the expected key/value.
    ASSERT_EQ(GetExitTypePreferenceFromDisk(profile), "SessionEnded");

    // Mark the success.
    succeeded = true;
  }

  ASSERT_TRUE(succeeded) << "profile->EndSession() timed out too often.";
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_OZONE)

// The following tests make sure that it's safe to shut down while one of the
// Profile's URLLoaderFactories is in use by a SimpleURLLoader.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       SimpleURLLoaderUsingMainContextDuringShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  StartActiveLoaderDuringProfileShutdownTest(
      browser()
          ->profile()
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
}

// The following tests make sure that it's safe to destroy an incognito profile
// while one of the its URLLoaderFactory is in use by a SimpleURLLoader.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       SimpleURLLoaderUsingMainContextDuringIncognitoTeardown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  RunURLLoaderActiveDuringIncognitoTeardownTest(
      embedded_test_server(), incognito_browser,
      incognito_browser->profile()
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Regression test for https://crbug.com/1136214 - verification that
// ExtensionURLLoaderFactory won't hit a use-after-free bug when used after
// a Profile has been torn down already.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       ExtensionURLLoaderFactoryAfterIncognitoTeardown) {
  // Create a mojo::Remote to ExtensionURLLoaderFactory for the incognito
  // profile.
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  Profile* incognito_profile = incognito_browser->profile();
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
  url_loader_factory.Bind(extensions::CreateExtensionNavigationURLLoaderFactory(
      incognito_profile, false /* is_web_view_request */));

  // Verify that the factory works fine while the profile is still alive.
  // We don't need to test with a real extension URL - it is sufficient to
  // verify that the factory responds with ERR_BLOCKED_BY_CLIENT that indicates
  // a missing extension.
  GURL missing_extension_url("chrome-extension://no-such-extension/blah");
  {
    SimpleURLLoaderHelper simple_loader_helper(url_loader_factory.get(),
                                               missing_extension_url,
                                               net::ERR_BLOCKED_BY_CLIENT);
    simple_loader_helper.WaitForCompletion();
  }

  {
    // Start monitoring |incognito_profile| for shutdown.
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    EXPECT_TRUE(profile_manager->IsValidProfile(incognito_profile));
    ProfileDestructionWaiter waiter(incognito_profile);

    // Close all incognito tabs, starting profile shutdown.
    incognito_browser->tab_strip_model()->CloseAllTabs();

    // ProfileDestructionWaiter waits for
    // BrowserContext::NotifyWillBeDestroyed, but after the RunLoop unwinds, the
    // profile should already be gone - let's assert this below (since this
    // ensures that |simple_loader_helper2| really tests what needs to be
    // tested).
    waiter.Wait();
    EXPECT_FALSE(profile_manager->IsValidProfile(incognito_profile));
  }

  // Verify that the factory doesn't crash (https://crbug.com/1136214), but
  // instead SimpleURLLoaderImpl::OnMojoDisconnect reports net::ERR_FAILED.
  {
    SimpleURLLoaderHelper simple_loader_helper2(
        url_loader_factory.get(), missing_extension_url, net::ERR_FAILED);
    simple_loader_helper2.WaitForCompletion();
  }
}
#endif

// Verifies the cache directory supports multiple profiles when it's overridden
// by group policy or command line switches.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, DiskCacheDirOverride) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const base::FilePath::StringPieceType profile_name =
      FILE_PATH_LITERAL("Profile 1");
  base::ScopedTempDir mock_user_data_dir;
  ASSERT_TRUE(mock_user_data_dir.CreateUniqueTempDir());
  base::FilePath profile_path =
      mock_user_data_dir.GetPath().Append(profile_name);

  {
    base::ScopedTempDir temp_disk_cache_dir;
    ASSERT_TRUE(temp_disk_cache_dir.CreateUniqueTempDir());
    g_browser_process->local_state()->SetFilePath(
        prefs::kDiskCacheDir, temp_disk_cache_dir.GetPath());
  }
}

// Verifies the last selected directory has a default value.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, LastSelectedDirectory) {
  ProfileImpl* profile_impl = static_cast<ProfileImpl*>(browser()->profile());
  base::FilePath home;
  base::PathService::Get(base::DIR_HOME, &home);
  ASSERT_EQ(profile_impl->last_selected_directory(), home);
}

// Verifies creating an OTR with non-primary id results in a different profile
// from incognito profile.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateNonPrimaryOTR) {
  auto otr_profile_id = Profile::OTRProfileID::CreateUniqueForTesting();

  Profile* regular_profile = browser()->profile();
  EXPECT_FALSE(regular_profile->HasAnyOffTheRecordProfile());

  EXPECT_FALSE(regular_profile->GetOffTheRecordProfile(
      otr_profile_id, /*create_if_needed=*/false));
  Profile* otr_profile = regular_profile->GetOffTheRecordProfile(
      otr_profile_id, /*create_if_needed=*/true);
  EXPECT_TRUE(regular_profile->HasAnyOffTheRecordProfile());
  EXPECT_TRUE(otr_profile->IsOffTheRecord());
  EXPECT_EQ(otr_profile_id, otr_profile->GetOTRProfileID());
  EXPECT_TRUE(regular_profile->HasOffTheRecordProfile(otr_profile_id));
  EXPECT_NE(otr_profile,
            regular_profile->GetOffTheRecordProfile(
                Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true));

  regular_profile->DestroyOffTheRecordProfile(otr_profile);
  EXPECT_FALSE(regular_profile->HasOffTheRecordProfile(otr_profile_id));
  EXPECT_TRUE(regular_profile->HasOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID()));
  EXPECT_TRUE(regular_profile->HasAnyOffTheRecordProfile());
}

// Verifies creating two OTRs with different ids results in different profiles.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateTwoNonPrimaryOTRs) {
  auto otr_profile_id1 = Profile::OTRProfileID::CreateUniqueForTesting();
  auto otr_profile_id2 = Profile::OTRProfileID::CreateUniqueForTesting();

  Profile* regular_profile = browser()->profile();

  Profile* otr_profile1 = regular_profile->GetOffTheRecordProfile(
      otr_profile_id1, /*create_if_needed=*/true);

  EXPECT_FALSE(regular_profile->GetOffTheRecordProfile(
      otr_profile_id2, /*create_if_needed=*/false));

  Profile* otr_profile2 = regular_profile->GetOffTheRecordProfile(
      otr_profile_id2, /*create_if_needed=*/true);

  EXPECT_NE(otr_profile1, otr_profile2);
  EXPECT_TRUE(regular_profile->HasOffTheRecordProfile(otr_profile_id1));
  EXPECT_TRUE(regular_profile->HasOffTheRecordProfile(otr_profile_id2));

  regular_profile->DestroyOffTheRecordProfile(otr_profile1);
  EXPECT_FALSE(regular_profile->HasOffTheRecordProfile(otr_profile_id1));
  EXPECT_TRUE(regular_profile->HasOffTheRecordProfile(otr_profile_id2));
}

class ProfileBrowserTestWithoutDestroyProfile : public ProfileBrowserTest {
 public:
  ProfileBrowserTestWithoutDestroyProfile() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kDestroyProfileOnBrowserClose);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies destroying regular profile will result in destruction of OTR
// profiles.
// TODO(crbug.com/40924925): Re-enable this test on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DestroyRegularProfileBeforeOTRs \
  DISABLED_DestroyRegularProfileBeforeOTRs
#else
#define MAYBE_DestroyRegularProfileBeforeOTRs DestroyRegularProfileBeforeOTRs
#endif
IN_PROC_BROWSER_TEST_F(ProfileBrowserTestWithoutDestroyProfile,
                       MAYBE_DestroyRegularProfileBeforeOTRs) {
  auto otr_profile_id1 = Profile::OTRProfileID::CreateUniqueForTesting();
  auto otr_profile_id2 = Profile::OTRProfileID::CreateUniqueForTesting();

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  std::unique_ptr<Profile> regular_profile(CreateProfile(
      temp_dir.GetPath(), &delegate, Profile::CreateMode::kSynchronous));

  // Creating a profile causes an implicit connection attempt to a Mojo
  // service, which occurs as part of a new task. Before deleting |profile|,
  // ensure this task runs to prevent a crash.
  FlushIoTaskRunnerAndSpinThreads();

  Profile* otr_profile1 = regular_profile->GetOffTheRecordProfile(
      otr_profile_id1, /*create_if_needed=*/true);
  Profile* otr_profile2 = regular_profile->GetOffTheRecordProfile(
      otr_profile_id2, /*create_if_needed=*/true);

  ProfileDestructionWaiter waiter1(otr_profile1);
  ProfileDestructionWaiter waiter2(otr_profile2);

  ProfileDestroyer::DestroyOriginalProfileWhenAppropriate(
      std::move(regular_profile));

  waiter1.Wait();
  EXPECT_TRUE(waiter1.destroyed());

  waiter2.Wait();
  EXPECT_TRUE(waiter2.destroyed());
}

// Regression test for: https://crbug.com/1357476
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, DestroyOnOTRProfileAmongMany) {
  // Create 3 OTR profiles. The first is the "primary" OTR profile. It is used
  // to create a RenderProcessHost depending on it, holding it alive.
  Profile* otr_profile[3] = {
      browser()->profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID::PrimaryID(), true),
      browser()->profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(), true),
      browser()->profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(), true),
  };
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL(url::kAboutBlankURL));

  ProfileDestructionWaiter waiter[3] = {
      ProfileDestructionWaiter(otr_profile[0]),
      ProfileDestructionWaiter(otr_profile[1]),
      ProfileDestructionWaiter(otr_profile[2]),
  };

  scoped_refptr<base::SequencedTaskRunner> profile_task_runner =
      incognito_browser->profile()->GetIOTaskRunner();

  // Request the destruction of one OTR profile:
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile[1]);
  EXPECT_FALSE(waiter[0].destroyed());
  EXPECT_TRUE(waiter[1].destroyed());
  EXPECT_FALSE(waiter[2].destroyed());
  // The `waiter` are not observing the real destruction of the Profile. Make
  // sure no crash are happening during the real destruction of the Profile.
  // This is needed to reproduce: https://crbug.com/1357476
  base::RunLoop loop;
  profile_task_runner->PostDelayedTask(FROM_HERE, loop.QuitClosure(),
                                       base::Milliseconds(2100));
  loop.Run();

  // Request the destruction of the primary OTR profile. This happens
  // synchronously, because it requires releasing the RenderProcessHost used.
  incognito_browser->tab_strip_model()->CloseAllTabs();
  EXPECT_FALSE(waiter[0].destroyed());
  EXPECT_TRUE(waiter[1].destroyed());
  EXPECT_FALSE(waiter[2].destroyed());

  waiter[0].Wait();
  EXPECT_TRUE(waiter[0].destroyed());
  EXPECT_TRUE(waiter[1].destroyed());
  EXPECT_FALSE(waiter[2].destroyed());

  // Cleanup
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile[2]);
  waiter[2].Wait();
}

#if !BUILDFLAG(IS_CHROMEOS)
class ProfileBrowserTestWithDestroyProfile : public ProfileBrowserTest {
 public:
  ProfileBrowserTestWithDestroyProfile() {
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);

    scoped_feature_list_.InitAndEnableFeature(
        features::kDestroyProfileOnBrowserClose);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;
};

// Verifies the regular Profile doesn't get destroyed as long as there's an OTR
// Profile around.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTestWithDestroyProfile,
                       OTRProfileKeepsRegularProfileAlive) {
  Profile* regular_profile = browser()->profile();
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  EXPECT_FALSE(profile_manager->HasKeepAliveForTesting(
      regular_profile, ProfileKeepAliveOrigin::kOffTheRecordProfile));

  auto otr_profile_id = Profile::OTRProfileID::CreateUniqueForTesting();
  Profile* otr_profile = regular_profile->GetOffTheRecordProfile(
      otr_profile_id, /*create_if_needed=*/true);

  ProfileDestructionWaiter regular_waiter(regular_profile);
  ProfileDestructionWaiter otr_waiter(otr_profile);

  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      regular_profile, ProfileKeepAliveOrigin::kBrowserWindow));
  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      regular_profile, ProfileKeepAliveOrigin::kOffTheRecordProfile));

  // Close the browser. Because there's an OTR profile open, the regular Profile
  // shouldn't get deleted.
  browser()->tab_strip_model()->CloseAllTabs();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(profile_manager->HasKeepAliveForTesting(
      regular_profile, ProfileKeepAliveOrigin::kBrowserWindow));
  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      regular_profile, ProfileKeepAliveOrigin::kOffTheRecordProfile));

  // Destroy the OTR profile. *Now* the regular Profile should get deleted.
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr_profile);
  otr_waiter.Wait();
  regular_waiter.Wait();

  EXPECT_TRUE(regular_waiter.destroyed());
  EXPECT_TRUE(otr_waiter.destroyed());
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

// Tests Profile::GetAllOffTheRecordProfiles
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, TestGetAllOffTheRecordProfiles) {
  auto otr_profile_id1 = Profile::OTRProfileID::CreateUniqueForTesting();
  auto otr_profile_id2 = Profile::OTRProfileID::CreateUniqueForTesting();

  Profile* regular_profile = browser()->profile();

  Profile* otr_profile1 = regular_profile->GetOffTheRecordProfile(
      otr_profile_id1, /*create_if_needed=*/true);
  Profile* otr_profile2 = regular_profile->GetOffTheRecordProfile(
      otr_profile_id2, /*create_if_needed=*/true);
  Profile* incognito_profile = regular_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);

  std::vector<Profile*> all_otrs =
      regular_profile->GetAllOffTheRecordProfiles();

  EXPECT_EQ(3u, all_otrs.size());
  EXPECT_TRUE(base::Contains(all_otrs, otr_profile1));
  EXPECT_TRUE(base::Contains(all_otrs, otr_profile2));
  EXPECT_TRUE(base::Contains(all_otrs, incognito_profile));
}

// Tests Profile::IsSameOrParent
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, TestIsSameOrParent) {
  auto otr_profile_id = Profile::OTRProfileID::CreateUniqueForTesting();

  Profile* regular_profile = browser()->profile();
  Profile* otr_profile = regular_profile->GetOffTheRecordProfile(
      otr_profile_id, /*create_if_needed=*/true);
  Profile* incognito_profile =
      regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  EXPECT_TRUE(regular_profile->IsSameOrParent(otr_profile));
  EXPECT_TRUE(otr_profile->IsSameOrParent(regular_profile));

  EXPECT_TRUE(regular_profile->IsSameOrParent(incognito_profile));
  EXPECT_TRUE(incognito_profile->IsSameOrParent(regular_profile));

  EXPECT_FALSE(incognito_profile->IsSameOrParent(otr_profile));
  EXPECT_FALSE(otr_profile->IsSameOrParent(incognito_profile));
}

// Tests if browser creation using non primary OTRs is blocked.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       TestCreatingBrowserUsingNonPrimaryOffTheRecordProfile) {
  auto otr_profile_id = Profile::OTRProfileID::CreateUniqueForTesting();
  Profile* otr_profile = browser()->profile()->GetOffTheRecordProfile(
      otr_profile_id, /*create_if_needed=*/true);

  EXPECT_EQ(Browser::CreationStatus::kErrorProfileUnsuitable,
            Browser::GetCreationStatusForProfile(otr_profile));
}

// Tests if profile type returned by |profile_metrics::GetBrowserProfileType| is
// correct.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, TestProfileTypes) {
  Profile* regular_profile = browser()->profile();
  EXPECT_EQ(profile_metrics::BrowserProfileType::kRegular,
            profile_metrics::GetBrowserProfileType(regular_profile));

  Profile* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(profile_metrics::BrowserProfileType::kIncognito,
            profile_metrics::GetBrowserProfileType(incognito_profile));

  Profile* otr_profile = browser()->profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  EXPECT_EQ(profile_metrics::BrowserProfileType::kOtherOffTheRecordProfile,
            profile_metrics::GetBrowserProfileType(otr_profile));

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  base::HistogramTester tester;
  Browser* guest_browser = CreateGuestBrowser();

  EXPECT_EQ(profile_metrics::BrowserProfileType::kGuest,
            profile_metrics::GetBrowserProfileType(guest_browser->profile()));

  // Verify that both a parent and a child profile creation are recorded
  EXPECT_THAT(tester.GetAllSamples("Profile.Guest.TypeCreated"),
              ::testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
#endif
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, UnderOneMinute) {
  base::HistogramTester tester;
  Browser* browser = CreateGuestBrowser();
  TestBrowserClosedWaiter close_waiter(browser);

  BrowserList::CloseAllBrowsersWithProfile(browser->profile());
  ASSERT_TRUE(close_waiter.WaitUntilClosed());
  tester.ExpectUniqueSample("Profile.Guest.OTR.Lifetime", 0, 1);
}

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, OneHour) {
  base::HistogramTester tester;
  Browser* browser = CreateGuestBrowser();
  TestBrowserClosedWaiter close_waiter(browser);

  browser->profile()->SetCreationTimeForTesting(base::Time::Now() -
                                                base::Seconds(60) * 60);
  BrowserList::CloseAllBrowsersWithProfile(browser->profile());
  ASSERT_TRUE(close_waiter.WaitUntilClosed());
  tester.ExpectUniqueSample("Profile.Guest.OTR.Lifetime", 60, 1);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
