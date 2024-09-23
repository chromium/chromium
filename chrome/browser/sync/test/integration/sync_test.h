// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/sync/test/integration/invalidations/fake_server_sync_invalidation_sender.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/fake_server.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
#include "components/gcm_driver/instance_id/scoped_use_fake_instance_id_android.h"
#else
#include "chrome/browser/extensions/install_verifier.h"
#endif

// The E2E tests are designed to run against real backend servers. To identify
// those tests we use *E2ETest* test name filter and run disabled tests.
//
// The following macros define how a test is run:
// - E2E_ONLY: Marks a test to run only as an E2E test against backend servers.
//             These tests DO NOT run on regular Chromium waterfalls.
// - E2E_ENABLED: Marks a test to run as an E2E test in addition to Chromium
//                waterfalls.
//
// To disable a test from running on Chromium waterfalls, you would still use
// the default DISABLED_test_name macro. To disable it from running as an E2E
// test outside Chromium waterfalls you would need to remove the E2E* macro.
#define MACRO_CONCAT(prefix, test_name) prefix##_##test_name
#define E2E_ONLY(test_name) MACRO_CONCAT(DISABLED_E2ETest, test_name)
#define E2E_ENABLED(test_name) MACRO_CONCAT(test_name, E2ETest)

class FakeSyncGCMDriver;
class KeyedService;
class SyncServiceImplHarness;

namespace arc {
class SyncArcPackageHelper;
}  // namespace arc

namespace base {
class CommandLine;
class ScopedTempDir;
}  // namespace base

namespace fake_server {
class FakeServer;
}  // namespace fake_server

namespace syncer {
class SyncServiceImpl;
}  // namespace syncer

namespace switches {

inline constexpr char kPasswordFileForTest[] = "password-file-for-test";
inline constexpr char kSyncUserForTest[] = "sync-user-for-test";
inline constexpr char kSyncPasswordForTest[] = "sync-password-for-test";

}  // namespace switches

// This is the base class for integration tests for all sync data types. Derived
// classes must be defined for each sync data type. Individual tests are defined
// using the IN_PROC_BROWSER_TEST_F macro.
//
// The list below shows some command line switches that can customize test
// behavior. It may become non-exaustive over time.
// syncer::kSyncServiceURL - By default, tests use a fake_server::FakeServer
//    to emulate the sync server. This switch causes them to run against an
//    external server instead, pointed by the provided URL. This translates into
//    the ServerType of the test being EXTERNAL_LIVE_SERVER.
// switches::kSyncUserForTest - Overrides the username of the syncing account.
//    Mostly useful for EXTERNAL_LIVE_SERVER tests to use an allowlisted value.
// switches::kSyncPasswordForTest - Same as above, but for the password.
// switches::kPasswordFileForTests - Causes the username and password of the
//    syncing account to be read from the passed file. The username must be on
//    the first line and the password on the second. The individual switches for
//    username and password are ignored if this is set.
// Other switches may modify the behavior of helper classes frequently used in
// sync integration tests, see StatusChangeChecker for example.
class SyncTest : public PlatformBrowserTest, public ProfileObserver {
 public:
  // The different types of live sync tests that can be implemented.
  enum TestType {
    // Tests where only one client profile is synced with the server. Typically
    // sanity level tests.
    SINGLE_CLIENT,

    // Tests where two client profiles are synced with the server. Typically
    // functionality level tests.
    TWO_CLIENT,
  };

  // The type of server we're running against.
  enum ServerType {
    EXTERNAL_LIVE_SERVER,  // A remote server that the test code has no control
                           // over whatsoever; cross your fingers that the
                           // account state is initially clean.
    IN_PROCESS_FAKE_SERVER,  // The fake Sync server (FakeServer) running
                             // in-process (bypassing HTTP calls).
  };

  // Modes when setting up sync.
  enum SetupSyncMode {
    // Do not wait for clients to be ready to sync.
    NO_WAITING,

    // Wait for sync engine initialization only. This may be used when waiting
    // for commits is impossible (e.g. due to commit errors or a custom
    // passphrase).
    WAIT_FOR_SYNC_SETUP_TO_COMPLETE,

    // Wait for all the changes to be committed including asynchronous changes
    // (e.g. DeviceInfo fields).
    WAIT_FOR_COMMITS_TO_COMPLETE,
  };

  // Used unless specified otherwise by command line switches.
  static constexpr char kDefaultUserEmail[] = "user@gmail.com";

  // A SyncTest must be associated with a particular test type.
  explicit SyncTest(TestType test_type);

  SyncTest(const SyncTest&) = delete;
  SyncTest& operator=(const SyncTest&) = delete;

  ~SyncTest() override;

  void SetUp() override;

  void TearDown() override;

  void PostRunTestOnMainThread() override;

  // Sets up command line flags required for sync tests.
  void SetUpCommandLine(base::CommandLine* cl) override;

  // Used to get the number of sync clients used by a test.
  int num_clients() { return num_clients_; }

  // Returns a pointer to a particular sync profile. Callee owns the object
  // and manages its lifetime.
  Profile* GetProfile(int index) const;

  // Returns a list of all profiles including the verifier if available. Callee
  // owns the objects and manages its lifetime.
  std::vector<raw_ptr<Profile, VectorExperimental>> GetAllProfiles();

#if !BUILDFLAG(IS_ANDROID)
  // Returns a pointer to a particular browser. Callee owns the object
  // and manages its lifetime. The called browser must not be closed before.
  Browser* GetBrowser(int index);

  // Adds a new browser belonging to the profile at |profile_index|, and appends
  // it to the list of browsers. Creates a copy of the Profile pointer in
  // position |profile_index| and appends it to the list of profiles. This is
  // done so that the profile associated with the new browser can be found at
  // the same index as it. Tests typically use browser indexes and profile
  // indexes interchangeably; this allows them to do so freely.
  Browser* AddBrowser(int profile_index);
#endif

  // Returns a pointer to a particular sync client. Callee owns the object
  // and manages its lifetime.
  SyncServiceImplHarness* GetClient(int index);
  const SyncServiceImplHarness* GetClient(int index) const;

  // Returns a list of the collection of sync clients.
  std::vector<SyncServiceImplHarness*> GetSyncClients();

  // Returns a SyncServiceImpl at the given index.
  syncer::SyncServiceImpl* GetSyncService(int index) const;

  // Returns the set of SyncServiceImpls.
  std::vector<raw_ptr<syncer::SyncServiceImpl, VectorExperimental>>
  GetSyncServices();

  // Returns the set of registered UserSelectableTypes.  This is retrieved from
  // the SyncServiceImpl at the given |index|.
  syncer::UserSelectableTypeSet GetRegisteredSelectableTypes(int index);

  // Returns a pointer to the sync profile that is used to verify changes to
  // individual sync profiles. Callee owns the object and manages its lifetime.
  Profile* verifier();

  // Used to determine whether the verifier profile should be updated or not.
  // Default is to return false. Test should override this if they require
  // different behavior.
  // Warning: do not use verifier in new tests.
  // TODO(crbug.com/40152770): remove verifier profile logic completely, once
  // all tests are rewritten in a way to not use verifier.
  virtual bool UseVerifier();

  // Initializes sync clients and profiles but does not sync any of them.
  [[nodiscard]] virtual bool SetupClients();

  // Initializes sync clients and waits for different stages to complete
  // depending on |setup_mode|.
  [[nodiscard]] bool SetupSync(
      SetupSyncMode setup_mode = WAIT_FOR_COMMITS_TO_COMPLETE);

  // This is similar to click the reset button on chrome.google.com/sync.
  // Only takes effect when running with external servers.
  // Please call this before setting anything. This method will clear all
  // local profiles, browsers, etc.
  void ResetSyncForPrimaryAccount();

  // Sets whether or not the sync clients in this test should respond to
  // notifications of their own commits.  Real sync clients do not do this, but
  // many test assertions require this behavior.
  //
  // Default is to return true.  Test should override this if they require
  // different behavior.
  virtual bool TestUsesSelfNotifications();

  // Blocks until all sync clients have completed their mutual sync cycles.
  // Returns true if a quiescent state was successfully reached.
  [[nodiscard]] bool AwaitQuiescence();

  // Sets the mock gaia response for when an OAuth2 token is requested.
  // Each call to this method will overwrite responses that were previously set.
  void SetOAuth2TokenResponse(const std::string& response_data,
                              net::HttpStatusCode response_code,
                              net::Error net_error);

  // Triggers a migration for one or more datatypes, and waits
  // for the server to complete it.  This operation is available
  // only if ServerSupportsErrorTriggering() returned true.
  void TriggerMigrationDoneError(syncer::DataTypeSet data_types);

  // Returns the FakeServer being used for the test or null if FakeServer is
  // not being used.
  fake_server::FakeServer* GetFakeServer() const;

  // Triggers a sync for the given |data_types| for the Profile at |index|.
  void TriggerSyncForDataTypes(int index, syncer::DataTypeSet data_types);

  arc::SyncArcPackageHelper* sync_arc_helper();

  std::string GetCacheGuid(size_t profile_index) const;

 protected:
  // BrowserTestBase implementation:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;

  // ProfileObserver implementation.
  void OnProfileWillBeDestroyed(Profile* profile) override;

  void OnWillCreateBrowserContextServices(content::BrowserContext* context);

  // Invoked immediately before creating profile |index| under |profile_path|.
  virtual void BeforeSetupClient(int index, const base::FilePath& profile_path);

  // The name for a directory under chrome::DIR_USER_DATA.
  virtual base::FilePath GetProfileBaseName(int index);

  // Implementations of the EnableNotifications() and DisableNotifications()
  // functions defined above.
  void DisableNotificationsImpl();
  void EnableNotificationsImpl();

  // Sets up fake responses for kClientLoginUrl, kIssueAuthTokenUrl,
  // kGetUserInfoUrl and kSearchDomainCheckUrl in order to mock out calls to
  // GAIA servers.
  void SetupMockGaiaResponsesForProfile(Profile* profile);

  // Exclude data types from end of test checks in CheckForDataTypeFailures().
  // Note that this replaces the list of excluded types (if set earlier).
  void ExcludeDataTypesFromCheckForDataTypeFailures(syncer::DataTypeSet types);

  // The FakeServer used in tests with server type IN_PROCESS_FAKE_SERVER.
  std::unique_ptr<fake_server::FakeServer> fake_server_;

  // The factory used to mock out GAIA signin.
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  // Handles Profile creation for given index. Profile's path and type is
  // determined at runtime based on server type.
  bool CreateProfile(int index);

  // Creates a fake GCMProfileService to simulate sync invalidations.
  std::unique_ptr<KeyedService> CreateGCMProfileService(
      content::BrowserContext* context);

#if !BUILDFLAG(IS_ANDROID)
  // Called when the |browser| was removed externally. This just marks the
  // |browser| in the |browsers_| list as nullptr to keep indexes in |browsers_|
  // and |profiles_| in sync. It is used when the |browser| is removed within a
  // test (e.g. when the last tab is closed for the |browser|).
  void OnBrowserRemoved(Browser* browser);
#endif

  // Helper to block the current thread while the data models sync depends on
  // finish loading.
  void WaitForDataModels(Profile* profile);

  // Helper method used to read GAIA credentials from a local password file
  // specified via the "--password-file-for-test" command line switch.
  // Note: The password file must be a plain text file with exactly two lines --
  // the username on the first line and the password on the second line.
  void ReadPasswordFile();

  // Helper method used to set up fake responses for kClientLoginUrl,
  // kIssueAuthTokenUrl, kGetUserInfoUrl and kSearchDomainCheckUrl in order to
  // mock out calls to GAIA servers.
  void SetupMockGaiaResponses();

  // Helper method used to clear any fake responses that might have been set for
  // various gaia URLs, cancel any outstanding URL requests, and return to using
  // the default URLFetcher creation mechanism.
  void ClearMockGaiaResponses();

  // Initializes any custom services needed for the |profile| at |index|.
  void InitializeProfile(int index, Profile* profile);

  // Internal routine for setting up sync.
  void SetupSyncInternal(SetupSyncMode setup_mode);

  // Used to determine whether ARC_PACKAGE data type needs to be enabled. This
  // is applicable on ChromeOS-Ash platform only.
  bool UseArcPackage();

  // Waits for all the changes which might be done asynchronously after setting
  // up sync engine. This is used to prevent starting another sync cycle after
  // SetupSync() call which might be unexpected in several tests.
  bool WaitForAsyncChangesToBeCommitted(size_t profile_index) const;

  // Verifies that there are no data type failures for the given |client_index|.
  // Otherwise, causes test failure. A corresponding client must exist.
  void CheckForDataTypeFailures(size_t client_index) const;

  // Used to differentiate between single-client and two-client tests.
  const TestType test_type_;

  // The kind of server being used, c.f. ServerType.
  const ServerType server_type_;

  // Used to remember when the test fixture was constructed and later understand
  // how long the setup took.
  const base::Time test_construction_time_;

  // Used to catch any timeout within RunLoop and cause test error.
  base::test::ScopedRunLoopTimeout sync_run_loop_timeout;

  // GAIA account used by the test case.
  std::string username_;

  // GAIA password used by the test case.
  std::string password_;

  // Locally available plain text file in which GAIA credentials are stored.
  base::FilePath password_file_;

  // The default profile, created before our actual testing |profiles_|. This is
  // needed in a workaround for https://crbug.com/801569, see comments in the
  // .cc file.
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> previous_profile_ = nullptr;

  // Number of sync clients that will be created by a test.
  int num_clients_;

  // Collection of sync profiles used by a test. A sync profile maintains sync
  // data contained within its own subdirectory under the chrome user data
  // directory. Profiles are owned by the ProfileManager.
  // TODO(crbug.com/40855871): store |profiles_|, |browsers_| and |clients_| in
  // one structure.
  std::vector<raw_ptr<Profile, AcrossTasksDanglingUntriaged>> profiles_;

  // List of temporary directories that need to be deleted when the test is
  // completed, used for two-client tests with external server.
  std::vector<std::unique_ptr<base::ScopedTempDir>> scoped_temp_dirs_;

#if !BUILDFLAG(IS_ANDROID)
  // Collection of pointers to the browser objects used by a test. One browser
  // instance is created for each sync profile. Browser object lifetime is
  // managed by BrowserList, so we don't use a std::vector<std::unique_ptr<>>
  // here.
  std::vector<raw_ptr<Browser, AcrossTasksDanglingUntriaged>> browsers_;

  class ClosedBrowserObserver;
  std::unique_ptr<ClosedBrowserObserver> browser_list_observer_;
#endif

  // Collection of sync clients used by a test. A sync client is associated
  // with a sync profile, and implements methods that sync the contents of the
  // profile with the server.
  std::vector<std::unique_ptr<SyncServiceImplHarness>> clients_;

  // Used to deliver invalidations to different profiles within
  // FakeSyncServerInvalidationSender.
  std::map<raw_ptr<Profile, AcrossTasksDanglingUntriaged>,
           raw_ptr<FakeSyncGCMDriver, AcrossTasksDanglingUntriaged>>
      profile_to_fake_gcm_driver_;

  base::CallbackListSubscription create_services_subscription_;

  // Sync profile against which changes to individual profiles are verified.
  // We don't need a corresponding verifier sync client because the contents
  // of the verifier profile are strictly local, and are not meant to be
  // synced.
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> verifier_ = nullptr;

  // Indicates whether to use a new user data dir.
  // Only used for external server tests with two clients.
  bool use_new_user_data_dir_ = false;

  syncer::DataTypeSet excluded_types_from_check_for_data_type_failures_;

  // The feature list to override features for all sync tests.
  base::test::ScopedFeatureList feature_list_;

#if !BUILDFLAG(IS_ANDROID)
  // Disable extension install verification.
  extensions::ScopedInstallVerifierBypassForTest ignore_install_verification_;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // A factory-like callback to create a model updater for testing, which will
  // take the place of the real updater in AppListSyncableService for testing.
  std::unique_ptr<base::ScopedClosureRunner> model_updater_factory_scope_;
#endif

#if BUILDFLAG(IS_ANDROID)
  instance_id::ScopedUseFakeInstanceIDAndroid
      scoped_use_fake_instance_id_android_;
#endif

  std::unique_ptr<fake_server::FakeServerSyncInvalidationSender>
      fake_server_sync_invalidation_sender_;
};

syncer::DataTypeSet AllowedTypesInStandaloneTransportMode();

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_
