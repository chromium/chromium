// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/startup_task_runner_service.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/constants/chromeos_switches.h"
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
    request->trusted_params->network_isolation_key =
        net::NetworkIsolationKey(origin, origin);

    loader_ = network::SimpleURLLoader::Create(std::move(request),
                                               TRAFFIC_ANNOTATION_FOR_TESTS);

    loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        factory, base::BindOnce(&SimpleURLLoaderHelper::OnSimpleLoaderComplete,
                                base::Unretained(this)));
  }

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

  DISALLOW_COPY_AND_ASSIGN(SimpleURLLoaderHelper);
};

class MockProfileDelegate : public Profile::Delegate {
 public:
  MOCK_METHOD1(OnPrefsLoaded, void(Profile*));
  MOCK_METHOD3(OnProfileCreated, void(Profile*, bool, bool));
};

// Creates a prefs file in the given directory.
void CreatePrefsFileInDirectory(const base::FilePath& directory_path) {
  base::FilePath pref_path(directory_path.Append(chrome::kPreferencesFilename));
  std::string data("{}");
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(pref_path, data.c_str(), data.size()));
}

void CheckChromeVersion(Profile *profile, bool is_new) {
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
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
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
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateNewProfileSynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
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
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateOldProfileSynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.GetPath());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, false));

  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
    CheckChromeVersion(profile.get(), false);

    // Creating a profile causes an implicit connection attempt to a Mojo
    // service, which occurs as part of a new task. Before deleting |profile|,
    // ensure this task runs to prevent a crash.
    FlushIoTaskRunnerAndSpinThreads();
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Flaky: http://crbug.com/393177
// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile asynchronously.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_CreateNewProfileAsynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());

    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    observer.Wait();
    CheckChromeVersion(profile.get(), true);
  }

  FlushIoTaskRunnerAndSpinThreads();
}


// Flaky: http://crbug.com/393177
// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile asynchronously with an existing prefs file.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_CreateOldProfileAsynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.GetPath());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, false));

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());

    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    observer.Wait();
    CheckChromeVersion(profile.get(), false);
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Flaky: http://crbug.com/393177
// Test that a README file is created for profiles that didn't have it.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, DISABLED_ProfileReadmeCreated) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());

    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    observer.Wait();

    // Verify that README exists.
    EXPECT_TRUE(
        base::PathExists(temp_dir.GetPath().Append(chrome::kReadmeFilename)));
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Test that repeated setting of exit type is handled correctly.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, ExitType) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));
  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));

    PrefService* prefs = profile->GetPrefs();
    // The initial state is crashed; store for later reference.
    std::string crash_value(prefs->GetString(prefs::kSessionExitType));

    // The first call to a type other than crashed should change the value.
    profile->SetExitType(Profile::EXIT_SESSION_ENDED);
    std::string first_call_value(prefs->GetString(prefs::kSessionExitType));
    EXPECT_NE(crash_value, first_call_value);

    // Subsequent calls to a non-crash value should be ignored.
    profile->SetExitType(Profile::EXIT_NORMAL);
    std::string second_call_value(prefs->GetString(prefs::kSessionExitType));
    EXPECT_EQ(first_call_value, second_call_value);

    // Setting back to a crashed value should work.
    profile->SetExitType(Profile::EXIT_CRASHED);
    std::string final_value(prefs->GetString(prefs::kSessionExitType));
    EXPECT_EQ(crash_value, final_value);

    // Creating a profile causes an implicit connection attempt to a Mojo
    // service, which occurs as part of a new task. Before deleting |profile|,
    // ensure this task runs to prevent a crash.
    FlushIoTaskRunnerAndSpinThreads();
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// The EndSession IO synchronization is only critical on Windows, but also
// happens under the USE_X11 define. See BrowserProcessImpl::EndSession.
#if defined(USE_X11) || defined(OS_WIN) || defined(USE_OZONE)

namespace {

std::string GetExitTypePreferenceFromDisk(Profile* profile) {
  base::FilePath prefs_path =
      profile->GetPath().Append(chrome::kPreferencesFilename);
  std::string prefs;
  if (!base::ReadFileToString(prefs_path, &prefs))
    return std::string();

  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(prefs);
  if (!value)
    return std::string();

  base::DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict) || !dict)
    return std::string();

  std::string exit_type;
  if (!dict->GetString("profile.exit_type", &exit_type))
    return std::string();

  return exit_type;
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

#if defined(OS_CHROMEOS)
  for (auto* loaded_profile : loaded_profiles) {
    if (!chromeos::ProfileHelper::IsSigninProfile(loaded_profile)) {
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
  struct FailsIfCalledWhileOnStack
      : public base::SupportsWeakPtr<FailsIfCalledWhileOnStack> {
    void Fail() { ADD_FAILURE(); }
  } fails_if_called_while_on_stack;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FailsIfCalledWhileOnStack::Fail,
                                fails_if_called_while_on_stack.AsWeakPtr()));

  // This retry loop reduces flakiness due to the fact that this ultimately
  // tests whether or not a code path hits a timed wait.
  bool succeeded = false;
  for (size_t retries = 0; !succeeded && retries < 3; ++retries) {
    // Flush the profile data to disk for all loaded profiles.
    profile->SetExitType(Profile::EXIT_CRASHED);
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
    if (end - start > base::TimeDelta::FromSeconds(5))
      continue;

    // Make sure that the prefs file was written with the expected key/value.
    ASSERT_EQ(GetExitTypePreferenceFromDisk(profile), "SessionEnded");

    // Mark the success.
    succeeded = true;
  }

  ASSERT_TRUE(succeeded) << "profile->EndSession() timed out too often.";
}

#endif  // defined(USE_X11) || defined(OS_WIN) || defined(USE_OZONE)

// The following tests make sure that it's safe to shut down while one of the
// Profile's URLLoaderFactories is in use by a SimpleURLLoader.

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       SimpleURLLoaderUsingMainContextDuringShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  StartActiveLoaderDuringProfileShutdownTest(
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
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
      content::BrowserContext::GetDefaultStoragePartition(
          incognito_browser->profile())
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
}

// Verifies the cache directory supports multiple profiles when it's overriden
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

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, Notifications) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create the profile and check that a notification is received for it.
  std::unique_ptr<Profile> profile;
  {
    content::WindowedNotificationObserver profile_created_observer(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());

    profile = CreateProfile(temp_dir.GetPath(), nullptr,
                            Profile::CREATE_MODE_SYNCHRONOUS);
    profile_created_observer.Wait();

    EXPECT_EQ(profile_created_observer.source(),
              content::Source<Profile>(profile.get()));
  }

  // Now retrieve the off-the-record profile, which will be created because it
  // doesn't exist yet.
  Profile* otr_profile = nullptr;
  {
    content::WindowedNotificationObserver profile_created_observer(
        chrome::NOTIFICATION_PROFILE_CREATED,
        content::NotificationService::AllSources());

    otr_profile = profile->GetOffTheRecordProfile();
    profile_created_observer.Wait();

    EXPECT_EQ(profile_created_observer.source(),
              content::Source<Profile>(otr_profile));
    EXPECT_TRUE(profile->HasOffTheRecordProfile());
    EXPECT_TRUE(otr_profile->IsOffTheRecord());
    EXPECT_FALSE(otr_profile->IsIndependentOffTheRecordProfile());
  }

  // Destroy the off-the-record profile.
  {
    content::WindowedNotificationObserver profile_destroyed_observer(
        chrome::NOTIFICATION_PROFILE_DESTROYED,
        content::Source<Profile>(otr_profile));

    profile->DestroyOffTheRecordProfile();
    profile_destroyed_observer.Wait();

    EXPECT_FALSE(profile->HasOffTheRecordProfile());
  }

  // Destroy the regular profile.
  {
    content::WindowedNotificationObserver profile_destroyed_observer(
        chrome::NOTIFICATION_PROFILE_DESTROYED,
        content::Source<Profile>(profile.get()));

    profile.reset();
    profile_destroyed_observer.Wait();
  }
}
