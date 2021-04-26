// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/configuration_refresher.h"
#include "chrome/browser/sync/test/integration/fake_server_invalidation_sender.h"
#include "chrome/browser/sync/test/integration/fake_server_sync_invalidation_sender.h"
#include "chrome/common/buildflags.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/test/base/in_process_browser_test.h"
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

class ProfileSyncServiceHarness;

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
class ProfileSyncService;
}  // namespace syncer

namespace switches {

extern const char kPasswordFileForTest[];
extern const char kSyncUserForTest[];
extern const char kSyncPasswordForTest[];

}  // namespace switches

// This is the base class for integration tests for all sync data types. Derived
// classes must be defined for each sync data type. Individual tests are defined
// using the IN_PROC_BROWSER_TEST_F macro.
//
// The list below shows some command line switches that can customize test
// behavior. It may become non-exaustive over time.
// switches::kSyncServiceURL - By default, tests use a fake_server::FakeServer
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
class SyncTest : public PlatformBrowserTest {
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
    SERVER_TYPE_UNDECIDED,
    EXTERNAL_LIVE_SERVER,  // A remote server that the test code has no control
                           // over whatsoever; cross your fingers that the
                           // account state is initially clean.
    IN_PROCESS_FAKE_SERVER,  // The fake Sync server (FakeServer) running
                             // in-process (bypassing HTTP calls).
  };

  class FakeInstanceID : public instance_id::InstanceID {
   public:
    explicit FakeInstanceID(const std::string& app_id,
                            gcm::GCMDriver* gcm_driver);
    ~FakeInstanceID() override = default;

    void GetID(GetIDCallback callback) override {}

    void GetCreationTime(GetCreationTimeCallback callback) override {}

    void GetToken(const std::string& authorized_entity,
                  const std::string& scope,
                  base::TimeDelta time_to_live,
                  std::set<Flags> flags,
                  GetTokenCallback callback) override;

    void ValidateToken(const std::string& authorized_entity,
                       const std::string& scope,
                       const std::string& token,
                       ValidateTokenCallback callback) override {}

    void DeleteToken(const std::string& authorized_entity,
                     const std::string& scope,
                     DeleteTokenCallback callback) override {}

   protected:
    void DeleteTokenImpl(const std::string& authorized_entity,
                         const std::string& scope,
                         DeleteTokenCallback callback) override {}

    void DeleteIDImpl(DeleteIDCallback callback) override;

   private:
    static std::string GenerateNextToken();

    std::string token_;
    DISALLOW_COPY_AND_ASSIGN(FakeInstanceID);
  };

  class FakeInstanceIDDriver : public instance_id::InstanceIDDriver {
   public:
    explicit FakeInstanceIDDriver(gcm::GCMDriver* gcm_driver);
    ~FakeInstanceIDDriver() override;
    instance_id::InstanceID* GetInstanceID(const std::string& app_id) override;
    void RemoveInstanceID(const std::string& app_id) override {}
    bool ExistsInstanceID(const std::string& app_id) const override;

   private:
    gcm::GCMDriver* gcm_driver_;
    std::map<std::string, std::unique_ptr<FakeInstanceID>> fake_instance_ids_;
    DISALLOW_COPY_AND_ASSIGN(FakeInstanceIDDriver);
  };

  // A SyncTest must be associated with a particular test type.
  explicit SyncTest(TestType test_type);

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
  Profile* GetProfile(int index);

  // Returns a list of all profiles including the verifier if available. Callee
  // owns the objects and manages its lifetime.
  std::vector<Profile*> GetAllProfiles();

#if !defined(OS_ANDROID)
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
  ProfileSyncServiceHarness* GetClient(int index);

  // Returns a list of the collection of sync clients.
  std::vector<ProfileSyncServiceHarness*> GetSyncClients();

  // Returns a ProfileSyncService at the given index.
  syncer::ProfileSyncService* GetSyncService(int index);

  // Returns the set of ProfileSyncServices.
  std::vector<syncer::ProfileSyncService*> GetSyncServices();

  // Returns the set of registered UserSelectableTypes.  This is retrieved from
  // the ProfileSyncService at the given |index|.
  syncer::UserSelectableTypeSet GetRegisteredSelectableTypes(int index);

  // Returns a pointer to the sync profile that is used to verify changes to
  // individual sync profiles. Callee owns the object and manages its lifetime.
  Profile* verifier();

  // Used to determine whether the verifier profile should be updated or not.
  // Default is to return false. Test should override this if they require
  // different behavior.
  // Warning: do not use verifier in new tests.
  // TODO(crbug.com/1137705): remove verifier profile logic completely, once all
  // tests are rewritten in a way to not use verifier.
  virtual bool UseVerifier();

  // Initializes sync clients and profiles but does not sync any of them.
  virtual bool SetupClients() WARN_UNUSED_RESULT;

  // Initializes sync clients and profiles if required and syncs each of them.
  virtual bool SetupSync() WARN_UNUSED_RESULT;

  // This is similar to click the reset button on chrome.google.com/sync.
  // Only takes effect when running with external servers.
  // Please call this before setting anything. This method will clear all
  // local profiles, browsers, etc.
  void ResetSyncForPrimaryAccount();

  // Like SetupSync() but does not wait for the clients to be ready to sync.
  void SetupSyncNoWaitingForCompletion();

  // Like SetupSync() but does wait for commits to complete before proceeding to
  // another client.
  // TODO(crbug.com/956043): Investigate deeper why such sequential setup is
  // needed by some tests and why using SetupSync() instead is causing
  // flakiness. Ideally get rid of this function.
  void SetupSyncOneClientAfterAnother();

  // Sets whether or not the sync clients in this test should respond to
  // notifications of their own commits.  Real sync clients do not do this, but
  // many test assertions require this behavior.
  //
  // Default is to return true.  Test should override this if they require
  // different behavior.
  virtual bool TestUsesSelfNotifications();

  // Blocks until all sync clients have completed their mutual sync cycles.
  // Returns true if a quiescent state was successfully reached.
  bool AwaitQuiescence();

  // Returns true if we are running tests against external servers.
  bool UsingExternalServers();

  // Sets the mock gaia response for when an OAuth2 token is requested.
  // Each call to this method will overwrite responses that were previously set.
  void SetOAuth2TokenResponse(const std::string& response_data,
                              net::HttpStatusCode response_code,
                              net::Error net_error);

  // Triggers a migration for one or more datatypes, and waits
  // for the server to complete it.  This operation is available
  // only if ServerSupportsErrorTriggering() returned true.
  void TriggerMigrationDoneError(syncer::ModelTypeSet model_types);

  // Returns the FakeServer being used for the test or null if FakeServer is
  // not being used.
  fake_server::FakeServer* GetFakeServer() const;

  // Triggers a sync for the given |model_types| for the Profile at |index|.
  void TriggerSyncForModelTypes(int index, syncer::ModelTypeSet model_types);

  // The configuration refresher is triggering refreshes after the configuration
  // phase is done (during start-up). Call this function before SetupSync() to
  // avoid its effects.
  void StopConfigurationRefresher();

  arc::SyncArcPackageHelper* sync_arc_helper();

 protected:
  // Add custom switches needed for running the test.
  void AddTestSwitches(base::CommandLine* cl);

  // BrowserTestBase implementation:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;

  void OnWillCreateBrowserContextServices(content::BrowserContext* context);

  // Invoked immediately before creating profile |index| under |profile_path|.
  virtual void BeforeSetupClient(int index, const base::FilePath& profile_path);

  // Implementations of the EnableNotifications() and DisableNotifications()
  // functions defined above.
  void DisableNotificationsImpl();
  void EnableNotificationsImpl();

  // Helper to ProfileManager::CreateProfileAsync that creates a new profile
  // used for UI Signin. Blocks until profile is created.
  static Profile* MakeProfileForUISignin(base::FilePath profile_path);

  // Stops notificatinos being sent to a client.
  void DisableNotificationsForClient(int index);

  // Sets a decryption passphrase to be used for a client. The passphrase will
  // be provided to the client during initialization, before Sync starts. It is
  // an error to provide both a decryption and encryption passphrases for one
  // client.
  void SetDecryptionPassphraseForClient(int index,
                                        const std::string& passphrase);

  // Sets an explicit encryption passphrase to be used for a client. The
  // passphrase will be set for the client during initialization, before Sync
  // starts. An encryption passphrase can be also enabled after initialization,
  // but using this method ensures that Sync is never enabled when there is no
  // passphrase, which allows tests to check for unencrypted data leaks. It is
  // an error to provide both a decryption and encryption passphrases for one
  // client.
  void SetEncryptionPassphraseForClient(int index,
                                        const std::string& passphrase);

  // Sets up fake responses for kClientLoginUrl, kIssueAuthTokenUrl,
  // kGetUserInfoUrl and kSearchDomainCheckUrl in order to mock out calls to
  // GAIA servers.
  void SetupMockGaiaResponsesForProfile(Profile* profile);

  // The FakeServer used in tests with server type IN_PROCESS_FAKE_SERVER.
  std::unique_ptr<fake_server::FakeServer> fake_server_;

  // The factory used to mock out GAIA signin.
  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  enum SetupSyncMode {
    NO_WAITING,
    WAIT_FOR_SYNC_SETUP_TO_COMPLETE,
    WAIT_FOR_COMMITS_TO_COMPLETE
  };
  // Handles Profile creation for given index. Profile's path and type is
  // determined at runtime based on server type.
  bool CreateProfile(int index);

  // Callback for MakeProfileForUISignin() method. It runs the quit_closure once
  // profile is created successfully.
  static void CreateProfileCallback(const base::RepeatingClosure& quit_closure,
                                    Profile* profile,
                                    Profile::CreateStatus status);

  static std::unique_ptr<KeyedService> CreateProfileInvalidationProvider(
      std::map<const Profile*, invalidation::FCMNetworkHandler*>*
          profile_to_fcm_network_handler_map,
      std::map<const Profile*, std::unique_ptr<instance_id::InstanceIDDriver>>*
          profile_to_instance_id_driver_map,
      content::BrowserContext* context);

  static std::unique_ptr<KeyedService> CreateSyncInvalidationsService(
      std::map<const Profile*, std::unique_ptr<instance_id::InstanceIDDriver>>*
          profile_to_instance_id_driver_map,
      std::vector<syncer::FCMHandler*>* sync_invalidations_fcm_handlers,
      content::BrowserContext* context);

#if !defined(OS_ANDROID)
  // Called when the |browser| was removed externally. This just marks the
  // |browser| in the |browsers_| list as nullptr to keep indexes in |browsers_|
  // and |profiles_| in sync. It is used when the |browser| is removed within a
  // test (e.g. when the last tab is closed for the |browser|).
  void OnBrowserRemoved(Browser* browser);
#endif

  // Helper to Profile::CreateProfile that handles path creation. It creates
  // a profile then registers it as a testing profile.
  Profile* MakeTestProfile(base::FilePath profile_path, int index);

  // Helper to block the current thread while the data models sync depends on
  // finish loading.
  void WaitForDataModels(Profile* profile);

  // Helper method used to read GAIA credentials from a local password file
  // specified via the "--password-file-for-test" command line switch.
  // Note: The password file must be a plain text file with exactly two lines --
  // the username on the first line and the password on the second line.
  void ReadPasswordFile();

  // Helper method that starts up a sync test server if required.
  void SetUpTestServerIfRequired();

  // Helper method used to set up fake responses for kClientLoginUrl,
  // kIssueAuthTokenUrl, kGetUserInfoUrl and kSearchDomainCheckUrl in order to
  // mock out calls to GAIA servers.
  void SetupMockGaiaResponses();

  // Helper method used to clear any fake responses that might have been set for
  // various gaia URLs, cancel any outstanding URL requests, and return to using
  // the default URLFetcher creation mechanism.
  void ClearMockGaiaResponses();

  // Decide which sync server implementation to run against based on the type
  // of test being run and command line args passed in.
  void DecideServerType();

  // Initializes any custom services needed for the |profile| at |index|.
  void InitializeProfile(int index, Profile* profile);

  // Sets up the client-side invalidations infrastructure depending on the
  // value of |server_type_|.
  void SetUpInvalidations(int index);

  // Initializes the invalidations that were set up in SetUpInvalidations.
  void InitializeInvalidations(int index);

  // Internal routine for setting up sync.
  void SetupSyncInternal(SetupSyncMode setup_mode);

  void ClearProfiles();

  // Used to differentiate between single-client and two-client tests.
  const TestType test_type_;

  // Used to remember when the test fixture was constructed and later understand
  // how long the setup took.
  const base::Time test_construction_time_;

  // GAIA account used by the test case.
  std::string username_;

  // GAIA password used by the test case.
  std::string password_;

  // Locally available plain text file in which GAIA credentials are stored.
  base::FilePath password_file_;

  // Tells us what kind of server we're using (some tests run only on certain
  // server types).
  ServerType server_type_;

  // The default profile, created before our actual testing |profiles_|. This is
  // needed in a workaround for https://crbug.com/801569, see comments in the
  // .cc file.
  Profile* previous_profile_;

  // Number of sync clients that will be created by a test.
  int num_clients_;

  // Collection of sync profiles used by a test. A sync profile maintains sync
  // data contained within its own subdirectory under the chrome user data
  // directory. Profiles are owned by the ProfileManager.
  std::vector<Profile*> profiles_;

  // Collection of profile delegates. Only used for test profiles, which
  // require a custom profile delegate to ensure initialization happens at the
  // right time.
  std::vector<std::unique_ptr<Profile::Delegate>> profile_delegates_;

  // List of temporary directories that need to be deleted when the test is
  // completed, used for two-client tests with external server.
  std::vector<std::unique_ptr<base::ScopedTempDir>> scoped_temp_dirs_;

#if !defined(OS_ANDROID)
  // Collection of pointers to the browser objects used by a test. One browser
  // instance is created for each sync profile. Browser object lifetime is
  // managed by BrowserList, so we don't use a std::vector<std::unique_ptr<>>
  // here.
  std::vector<Browser*> browsers_;

  class ClosedBrowserObserver;
  std::unique_ptr<ClosedBrowserObserver> browser_list_observer_;
#endif

  // Collection of sync clients used by a test. A sync client is associated
  // with a sync profile, and implements methods that sync the contents of the
  // profile with the server.
  std::vector<std::unique_ptr<ProfileSyncServiceHarness>> clients_;

  // Mapping from client indexes to encryption passphrases to use for them.
  std::map<int, std::string> client_encryption_passphrases_;

  // Mapping from client indexes to decryption passphrases to use for them.
  std::map<int, std::string> client_decryption_passphrases_;

  // Owns the FakeServerInvalidationSender for each profile.
  std::vector<std::unique_ptr<fake_server::FakeServerInvalidationSender>>
      fake_server_invalidation_observers_;

  // Maps a profile to the corresponding FCMNetworkHandler. Contains one entry
  // per profile. It is used to simulate an incoming FCM messages to different
  // profiles within the FakeServerInvalidationSender.
  std::map<const Profile*, invalidation::FCMNetworkHandler*>
      profile_to_fcm_network_handler_map_;

  std::map<const Profile*, std::unique_ptr<instance_id::InstanceIDDriver>>
      profile_to_instance_id_driver_map_;

  // Triggers a GetUpdates via refresh after a configuration.
  std::unique_ptr<ConfigurationRefresher> configuration_refresher_;

  base::CallbackListSubscription create_services_subscription_;

  // Sync profile against which changes to individual profiles are verified.
  // We don't need a corresponding verifier sync client because the contents
  // of the verifier profile are strictly local, and are not meant to be
  // synced.
  Profile* verifier_;

  // Indicates whether to use a new user data dir.
  // Only used for external server tests with two clients.
  bool use_new_user_data_dir_ = false;

  // The feature list to override features for all sync tests.
  base::test::ScopedFeatureList feature_list_;

#if !defined(OS_ANDROID)
  // Disable extension install verification.
  extensions::ScopedInstallVerifierBypassForTest ignore_install_verification_;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // A factory-like callback to create a model updater for testing, which will
  // take the place of the real updater in AppListSyncableService for testing.
  std::unique_ptr<
      app_list::AppListSyncableService::ScopedModelUpdaterFactoryForTest>
      model_updater_factory_;
#endif

  std::vector<syncer::FCMHandler*> sync_invalidations_fcm_handlers_;
  std::unique_ptr<fake_server::FakeServerSyncInvalidationSender>
      fake_server_sync_invalidation_sender_;

  DISALLOW_COPY_AND_ASSIGN(SyncTest);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_H_
