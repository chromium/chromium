// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/per_user_topic_registration_manager.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/engine_impl/sync_scheduler_impl.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/port_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/sync/test/integration/printers_helper.h"
#include "chrome/browser/sync/test/integration/sync_arc_package_helper.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_util.h"
#endif  // defined(OS_CHROMEOS)

using syncer::ProfileSyncService;

namespace switches {
const char kPasswordFileForTest[] = "password-file-for-test";
const char kSyncUserForTest[] = "sync-user-for-test";
const char kSyncPasswordForTest[] = "sync-password-for-test";
}  // namespace switches

namespace {
// Sender ID coming from the Firebase console.
const char kInvalidationGCMSenderId[] = "8181035976";

void SetURLLoaderFactoryForTest(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  ChromeSigninClient* signin_client = static_cast<ChromeSigninClient*>(
      ChromeSigninClientFactory::GetForProfile(profile));
  signin_client->SetURLLoaderFactoryForTest(url_loader_factory);

#if defined(OS_CHROMEOS)
  chromeos::AccountManagerFactory* factory =
      g_browser_process->platform_part()->GetAccountManagerFactory();
  chromeos::AccountManager* account_manager =
      factory->GetAccountManager(profile->GetPath().value());
  account_manager->SetUrlLoaderFactoryForTests(url_loader_factory);
#endif  // defined(OS_CHROMEOS)
}
class FakePerUserTopicRegistrationManager
    : public syncer::PerUserTopicRegistrationManager {
 public:
  explicit FakePerUserTopicRegistrationManager(PrefService* local_state)
      : syncer::PerUserTopicRegistrationManager(
            /*identity_provider=*/nullptr,
            /*pref_service=*/local_state,
            /*url_loader_factory=*/nullptr,
            /*project_id*/ kInvalidationGCMSenderId,
            /*migrate_prefs=*/false) {}
  ~FakePerUserTopicRegistrationManager() override = default;

  void UpdateRegisteredTopics(const syncer::Topics& topics,
                              const std::string& instance_id_token) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePerUserTopicRegistrationManager);
};

std::unique_ptr<syncer::FCMNetworkHandler> CreateFCMNetworkHandler(
    Profile* profile,
    std::map<const Profile*, syncer::FCMNetworkHandler*>*
        profile_to_fcm_network_handler_map,
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    const std::string& sender_id,
    const std::string& app_id) {
  auto handler = std::make_unique<syncer::FCMNetworkHandler>(
      gcm_driver, instance_id_driver, sender_id, app_id);
  (*profile_to_fcm_network_handler_map)[profile] = handler.get();
  return handler;
}

std::unique_ptr<syncer::PerUserTopicRegistrationManager>
CreatePerUserTopicRegistrationManager(
    invalidation::IdentityProvider* identity_provider,
    PrefService* local_state,
    network::mojom::URLLoaderFactory* url_loader_factory,
    const std::string& project_id,
    bool migrate_prefs) {
  return std::make_unique<FakePerUserTopicRegistrationManager>(local_state);
}

syncer::FCMNetworkHandler* GetFCMNetworkHandler(
    const Profile* profile,
    std::map<const Profile*, syncer::FCMNetworkHandler*>*
        profile_to_fcm_network_handler_map) {
  auto it = profile_to_fcm_network_handler_map->find(profile);
  return it != profile_to_fcm_network_handler_map->end() ? it->second : nullptr;
}

// Helper class to ensure a profile is registered before the manager is
// notified of creation.
class SyncProfileDelegate : public Profile::Delegate {
 public:
  explicit SyncProfileDelegate(
      const base::Callback<void(Profile*)>& on_profile_created_callback)
      : on_profile_created_callback_(on_profile_created_callback) {}
  ~SyncProfileDelegate() override {}

  void OnProfileCreated(Profile* profile,
                        bool success,
                        bool is_new_profile) override {
    g_browser_process->profile_manager()->RegisterTestingProfile(
        base::WrapUnique(profile), true, false);

    // Perform any custom work needed before the profile is initialized.
    if (!on_profile_created_callback_.is_null())
      on_profile_created_callback_.Run(profile);

    g_browser_process->profile_manager()->OnProfileCreated(profile, success,
                                                           is_new_profile);
  }

 private:
  base::Callback<void(Profile*)> on_profile_created_callback_;

  DISALLOW_COPY_AND_ASSIGN(SyncProfileDelegate);
};

bool IsEncryptionComplete(const ProfileSyncService* service) {
  return service->GetUserSettings()->IsEncryptEverythingEnabled() &&
         !service->IsEncryptionPendingForTest();
}

// Helper class to wait for encryption to complete.
class EncryptionChecker : public SingleClientStatusChangeChecker {
 public:
  explicit EncryptionChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for encryption to complete";
    return IsEncryptionComplete(service());
  }
};

}  // namespace

instance_id::InstanceID* SyncTest::FakeInstanceIDDriver::GetInstanceID(
    const std::string& app_id) {
  return &fake_instance_id_;
}
bool SyncTest::FakeInstanceIDDriver::ExistsInstanceID(
    const std::string& app_id) const {
  return true;
}

SyncTest::SyncTest(TestType test_type)
    : test_type_(test_type),
      server_type_(SERVER_TYPE_UNDECIDED),
      previous_profile_(nullptr),
      num_clients_(-1),
      use_verifier_(true),
      create_gaia_account_at_runtime_(false) {
  sync_datatype_helper::AssociateWithTest(this);
  switch (test_type_) {
    case SINGLE_CLIENT: {
      num_clients_ = 1;
      break;
    }
    case TWO_CLIENT: {
      num_clients_ = 2;
      break;
    }
  }
}

SyncTest::~SyncTest() {}

void SyncTest::SetUp() {
  // Sets |server_type_| if it wasn't specified by the test.
  DecideServerType();

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kPasswordFileForTest)) {
    ReadPasswordFile();
  } else {
    // Decide on username to use or create one.
    if (cl->HasSwitch(switches::kSyncUserForTest)) {
      username_ = cl->GetSwitchValueASCII(switches::kSyncUserForTest);
    } else if (UsingExternalServers()) {
      // We assume the need to automatically create a Gaia account which
      // requires URL navigation and needs to be done outside SetUp() function.
      create_gaia_account_at_runtime_ = true;
      username_ = base::GenerateGUID();
    } else {
      username_ = "user@gmail.com";
    }
    // Decide on password to use.
    password_ = cl->HasSwitch(switches::kSyncPasswordForTest)
                    ? cl->GetSwitchValueASCII(switches::kSyncPasswordForTest)
                    : "password";
  }

  if (username_.empty() || password_.empty())
    LOG(FATAL) << "Cannot run sync tests without GAIA credentials.";

  // Mock the Mac Keychain service.  The real Keychain can block on user input.
  OSCryptMocker::SetUp();

  // Yield control back to the InProcessBrowserTest framework.
  InProcessBrowserTest::SetUp();
}

void SyncTest::TearDown() {
  // Clear any mock gaia responses that might have been set.
  ClearMockGaiaResponses();

  // Allow the InProcessBrowserTest framework to perform its tear down.
  InProcessBrowserTest::TearDown();

  // Return OSCrypt to its real behaviour
  OSCryptMocker::TearDown();

  fake_server_.reset();
}

void SyncTest::SetUpCommandLine(base::CommandLine* cl) {
  AddTestSwitches(cl);

#if defined(OS_CHROMEOS)
  cl->AppendSwitch(chromeos::switches::kIgnoreUserProfileMappingForTests);
  cl->AppendSwitch(chromeos::switches::kDisableArcOptInVerification);
  arc::SetArcAvailableCommandLineForTesting(cl);
#endif
}

void SyncTest::AddTestSwitches(base::CommandLine* cl) {
  // Disable non-essential access of external network resources.
  if (!cl->HasSwitch(switches::kDisableBackgroundNetworking))
    cl->AppendSwitch(switches::kDisableBackgroundNetworking);

  if (!cl->HasSwitch(switches::kSyncShortInitialRetryOverride))
    cl->AppendSwitch(switches::kSyncShortInitialRetryOverride);

  if (!cl->HasSwitch(switches::kSyncShortNudgeDelayForTest))
    cl->AppendSwitch(switches::kSyncShortNudgeDelayForTest);
  // TODO(crbug.com/657130): This a temporary switch because sync integration
  // tests depend on the precommit get updates because invalidations aren't
  // working for them. Therefore, they pass the command line switch to enable
  // this feature. Once sync integrations test support invalidation, this
  // should be removed.
  if (!cl->HasSwitch(switches::kSyncEnableGetUpdatesBeforeCommit))
    cl->AppendSwitch(switches::kSyncEnableGetUpdatesBeforeCommit);
}

bool SyncTest::CreateGaiaAccount(const std::string& username,
                                 const std::string& password) {
  std::string relative_url = base::StringPrintf(
      "/CreateUsers?%s=%s", username.c_str(), password.c_str());
  GURL create_user_url =
      GaiaUrls::GetInstance()->gaia_url().Resolve(relative_url);
  // NavigateToURL blocks until the navigation finishes.
  ui_test_utils::NavigateToURL(browser(), create_user_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  EXPECT_TRUE(entry)
      << "Could not get a hold on NavigationEntry post URL navigate.";
  DVLOG(1) << "Create Gaia account request return code = "
           << entry->GetHttpStatusCode();
  return entry->GetHttpStatusCode() == 200;
}

void SyncTest::BeforeSetupClient(int index,
                                 const base::FilePath& profile_path) {}

bool SyncTest::CreateProfile(int index) {
  base::FilePath profile_path;

  base::ScopedAllowBlockingForTesting allow_blocking;
  if (UsingExternalServers() && (num_clients_ > 1 || use_new_user_data_dir_)) {
    scoped_temp_dirs_.push_back(std::make_unique<base::ScopedTempDir>());
    // For multi profile UI signin, profile paths should be outside user data
    // dir to allow signing-in multiple profiles to same account. Otherwise, we
    // get an error that the profile has already signed in on this device.
    // Note: Various places in Chrome assume that all profiles are within the
    // user data dir. We violate that assumption here, which can lead to weird
    // issues, see https://crbug.com/801569 and the workaround in
    // TearDownOnMainThread.
    if (!scoped_temp_dirs_.back()->CreateUniqueTempDir()) {
      ADD_FAILURE();
      return false;
    }

    profile_path = scoped_temp_dirs_.back()->GetPath();
  } else {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    // Create new profiles in user data dir so that other profiles can know
    // about it. This is needed in tests such as supervised user cases which
    // assume browser->profile() as the custodian profile. Instead of creating
    // a new directory, we use a deterministic name such that PRE_ tests (i.e.
    // test that span browser restarts) can reuse the same directory and carry
    // over state.
    profile_path = user_data_dir.AppendASCII(
        base::StringPrintf("SyncIntegrationTestClient%d", index));
  }

  BeforeSetupClient(index, profile_path);

  if (UsingExternalServers()) {
    // If running against an EXTERNAL_LIVE_SERVER, we signin profiles using real
    // GAIA server. This requires creating profiles with no test hooks.
    InitializeProfile(index, MakeProfileForUISignin(profile_path));
  } else {
    // Without need of real GAIA authentication, we create new test profiles.
    // For test profiles, a custom delegate needs to be used to do the
    // initialization work before the profile is registered.
    profile_delegates_[index] =
        std::make_unique<SyncProfileDelegate>(base::Bind(
            &SyncTest::InitializeProfile, base::Unretained(this), index));
    Profile* profile = MakeTestProfile(profile_path, index);
    SetupMockGaiaResponsesForProfile(profile);
  }

  // Once profile initialization has kicked off, wait for it to finish.
  WaitForDataModels(GetProfile(index));
  return true;
}

// Called when the ProfileManager has created a profile.
// static
void SyncTest::CreateProfileCallback(const base::Closure& quit_closure,
                                     Profile* profile,
                                     Profile::CreateStatus status) {
  EXPECT_TRUE(profile);
  EXPECT_NE(Profile::CREATE_STATUS_LOCAL_FAIL, status);
  EXPECT_NE(Profile::CREATE_STATUS_REMOTE_FAIL, status);
  // This will be called multiple times. Wait until the profile is initialized
  // fully to quit the loop.
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    quit_closure.Run();
}

// TODO(shadi): Ideally creating a new profile should not depend on signin
// process. We should try to consolidate MakeProfileForUISignin() and
// MakeProfile(). Major differences are profile paths and creation methods. For
// UI signin we need profiles in unique user data dir's and we need to use
// ProfileManager::CreateProfileAsync() for proper profile creation.
// static
Profile* SyncTest::MakeProfileForUISignin(base::FilePath profile_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::RunLoop run_loop;
  ProfileManager::CreateCallback create_callback =
      base::Bind(&CreateProfileCallback, run_loop.QuitClosure());
  profile_manager->CreateProfileAsync(profile_path, create_callback,
                                      base::string16(), std::string());
  run_loop.Run();
  return profile_manager->GetProfileByPath(profile_path);
}

Profile* SyncTest::MakeTestProfile(base::FilePath profile_path, int index) {
  std::unique_ptr<Profile> profile =
      Profile::CreateProfile(profile_path, profile_delegates_[index].get(),
                             Profile::CREATE_MODE_SYNCHRONOUS);
  return profile.release();
}

Profile* SyncTest::GetProfile(int index) {
  EXPECT_FALSE(profiles_.empty()) << "SetupClients() has not yet been called.";
  EXPECT_FALSE(index < 0 || index >= static_cast<int>(profiles_.size()))
      << "GetProfile(): Index is out of bounds.";

  Profile* profile = profiles_[index];
  EXPECT_NE(nullptr, profile) << "No profile found at index: " << index;

  return profile;
}

std::vector<Profile*> SyncTest::GetAllProfiles() {
  std::vector<Profile*> profiles;
  if (use_verifier()) {
    profiles.push_back(verifier());
  }
  for (int i = 0; i < num_clients(); ++i) {
    profiles.push_back(GetProfile(i));
  }
  return profiles;
}

Browser* SyncTest::GetBrowser(int index) {
  EXPECT_FALSE(browsers_.empty()) << "SetupClients() has not yet been called.";
  EXPECT_FALSE(index < 0 || index >= static_cast<int>(browsers_.size()))
      << "GetBrowser(): Index is out of bounds.";

  Browser* browser = browsers_[index];
  EXPECT_NE(browser, nullptr);

  return browsers_[index];
}

Browser* SyncTest::AddBrowser(int profile_index) {
  Profile* profile = GetProfile(profile_index);
  browsers_.push_back(new Browser(Browser::CreateParams(profile, true)));
  profiles_.push_back(profile);

  return browsers_[browsers_.size() - 1];
}

ProfileSyncServiceHarness* SyncTest::GetClient(int index) {
  if (clients_.empty())
    LOG(FATAL) << "SetupClients() has not yet been called.";
  if (index < 0 || index >= static_cast<int>(clients_.size()))
    LOG(FATAL) << "GetClient(): Index is out of bounds.";
  return clients_[index].get();
}

std::vector<ProfileSyncServiceHarness*> SyncTest::GetSyncClients() {
  std::vector<ProfileSyncServiceHarness*> clients(clients_.size());
  for (size_t i = 0; i < clients_.size(); ++i)
    clients[i] = clients_[i].get();
  return clients;
}

ProfileSyncService* SyncTest::GetSyncService(int index) {
  return ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(
      GetProfile(index));
}

syncer::UserSelectableTypeSet SyncTest::GetRegisteredSelectableTypes(
    int index) {
  return GetSyncService(index)
      ->GetUserSettings()
      ->GetRegisteredSelectableTypes();
}

std::vector<ProfileSyncService*> SyncTest::GetSyncServices() {
  std::vector<ProfileSyncService*> services;
  for (int i = 0; i < num_clients(); ++i) {
    services.push_back(GetSyncService(i));
  }
  return services;
}

Profile* SyncTest::verifier() {
  if (!use_verifier_)
    LOG(FATAL) << "Verifier account is disabled.";
  if (verifier_ == nullptr)
    LOG(FATAL) << "SetupClients() has not yet been called.";
  return verifier_;
}

void SyncTest::DisableVerifier() {
  use_verifier_ = false;
}

bool SyncTest::SetupClients() {
  previous_profile_ =
      g_browser_process->profile_manager()->GetLastUsedProfile();

  base::ScopedAllowBlockingForTesting allow_blocking;
  if (num_clients_ <= 0)
    LOG(FATAL) << "num_clients_ incorrectly initialized.";
  if (!profiles_.empty() || !browsers_.empty() || !clients_.empty())
    LOG(FATAL) << "SetupClients() has already been called.";

  // Create the required number of sync profiles, browsers and clients.
  profiles_.resize(num_clients_);
  profile_delegates_.resize(num_clients_ + 1);  // + 1 for the verifier.
  clients_.resize(num_clients_);
  fake_server_invalidation_observers_.resize(num_clients_);

  if (create_gaia_account_at_runtime_) {
    if (!UsingExternalServers()) {
      ADD_FAILURE() << "Cannot create Gaia accounts without external "
                       "authentication servers.";
      return false;
    }
    if (!CreateGaiaAccount(username_, password_))
      LOG(FATAL) << "Could not create Gaia account.";
  }

  auto* cl = base::CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(switches::kSyncDeferredStartupTimeoutSeconds)) {
    cl->AppendSwitchASCII(switches::kSyncDeferredStartupTimeoutSeconds, "1");
  }

#if defined(OS_CHROMEOS)
  // ARC_PACKAGE do not support supervised users, switches::kSupervisedUserId
  // need to be set in SetUpCommandLine() when a test will use supervise users.
  if (!cl->HasSwitch(switches::kSupervisedUserId)) {
    // Sets Arc flags, need to be called before create test profiles.
    ArcAppListPrefsFactory::SetFactoryForSyncTest();
  }

  // Uses a fake app list model updater to avoid interacting with Ash.
  model_updater_factory_ = std::make_unique<
      app_list::AppListSyncableService::ScopedModelUpdaterFactoryForTest>(
      base::Bind([]() -> std::unique_ptr<AppListModelUpdater> {
        return std::make_unique<FakeAppListModelUpdater>();
      }));
#endif

  for (int i = 0; i < num_clients_; ++i) {
    if (!CreateProfile(i)) {
      return false;
    }
  }

  // Verifier account is not useful when running against external servers.
  if (UsingExternalServers())
    DisableVerifier();

  // Create the verifier profile.
  if (use_verifier_) {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    profile_delegates_[num_clients_] =
        std::make_unique<SyncProfileDelegate>(base::Callback<void(Profile*)>());
    verifier_ = MakeTestProfile(
        user_data_dir.Append(FILE_PATH_LITERAL("Verifier")), num_clients_);
    WaitForDataModels(verifier());
  }

#if defined(OS_CHROMEOS)
  if (ArcAppListPrefsFactory::IsFactorySetForSyncTest()) {
    // Init SyncArcPackageHelper to ensure that the arc services are initialized
    // for each Profile, only can be called after test profiles are created.
    if (!sync_arc_helper())
      return false;
  }
#endif

  return true;
}

void SyncTest::InitializeProfile(int index, Profile* profile) {
  DCHECK(profile);
  profiles_[index] = profile;

  SetUpInvalidations(index);
  AddBrowser(index);

  // Make sure the ProfileSyncService has been created before creating the
  // ProfileSyncServiceHarness - some tests expect the ProfileSyncService to
  // already exist.
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(
          GetProfile(index));

  if (server_type_ == IN_PROCESS_FAKE_SERVER) {
    profile_sync_service->OverrideNetworkForTest(
        fake_server::CreateFakeServerHttpPostProviderFactory(
            GetFakeServer()->AsWeakPtr()));
  }

  ProfileSyncServiceHarness::SigninType singin_type =
      UsingExternalServers()
          ? ProfileSyncServiceHarness::SigninType::UI_SIGNIN
          : ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN;

  DCHECK(!clients_[index]);
  clients_[index] = ProfileSyncServiceHarness::Create(
      GetProfile(index), username_, password_, singin_type);
  EXPECT_NE(nullptr, GetClient(index)) << "Could not create Client " << index;
  InitializeInvalidations(index);
}

void SyncTest::DisableNotificationsForClient(int index) {
  fake_server_->RemoveObserver(
      fake_server_invalidation_observers_[index].get());
}

void SyncTest::SetEncryptionPassphraseForClient(int index,
                                                const std::string& passphrase) {
  // Must be called before client initialization.
  DCHECK(clients_.empty());
  client_encryption_passphrases_[index] = passphrase;
}

void SyncTest::SetDecryptionPassphraseForClient(int index,
                                                const std::string& passphrase) {
  // Must be called before client initialization.
  DCHECK(clients_.empty());
  client_decryption_passphrases_[index] = passphrase;
}

void SyncTest::SetupMockGaiaResponsesForProfile(Profile* profile) {
  SetURLLoaderFactoryForTest(profile,
                             test_url_loader_factory_.GetSafeWeakWrapper());
}

void SyncTest::SetUpInvalidations(int index) {
  switch (server_type_) {
    case EXTERNAL_LIVE_SERVER:
      // DO NOTHING. External live sync servers use GCM to notify profiles of
      // any invalidations in sync'ed data. In this case, to notify other
      // profiles of invalidations, we use sync refresh notifications instead.
      break;

    case IN_PROCESS_FAKE_SERVER: {
      const std::string client_id = "Client " + base::NumberToString(index);
      // Listen for fake server changes.
      fake_server_invalidation_observers_[index] =
          std::make_unique<fake_server::FakeServerInvalidationSender>(
              client_id, TestUsesSelfNotifications(),
              base::BindRepeating(&GetFCMNetworkHandler, GetProfile(index),
                                  &profile_to_fcm_network_handler_map_));
      fake_server_->AddObserver(
          fake_server_invalidation_observers_[index].get());

      // Store in prefs the mapping between public and private topics names. In
      // real clients, those are stored upon registration with the
      // per-user-topic server. The pref name is defined in
      // per_user_topic_registration_manager.cc.
      DictionaryPrefUpdate update(
          GetProfile(index)->GetPrefs(),
          "invalidation.per_sender_registered_for_invalidation");
      update->SetDictionary(kInvalidationGCMSenderId,
                            std::make_unique<base::DictionaryValue>());
      for (syncer::ModelType model_type :
           GetSyncService(index)->GetPreferredDataTypes()) {
        std::string notification_type;
        if (!RealModelTypeToNotificationType(model_type, &notification_type)) {
          continue;
        }
        update->FindDictKey(kInvalidationGCMSenderId)
            ->SetKey(notification_type,
                     base::Value("/private/" + notification_type +
                                 "-topic_server_user_id"));
      }
      DictionaryPrefUpdate update_client_id(
          GetProfile(index)->GetPrefs(),
          invalidation::prefs::kInvalidationClientIDCache);

      update_client_id->SetString(kInvalidationGCMSenderId, client_id);
      break;
    }
    case SERVER_TYPE_UNDECIDED:
      NOTREACHED();
  }
}

void SyncTest::InitializeInvalidations(int index) {
  // Lazily create |configuration_refresher_| the first time we get here (or the
  // first time after a previous call to StopConfigurationRefresher).
  if (!configuration_refresher_) {
    configuration_refresher_ = std::make_unique<ConfigurationRefresher>();
  }

  switch (server_type_) {
    case EXTERNAL_LIVE_SERVER:
      // DO NOTHING. External live sync servers use GCM to notify profiles of
      // any invalidations in sync'ed data. In this case, to notify other
      // profiles of invalidations, we use sync refresh notifications instead.
      break;
    case IN_PROCESS_FAKE_SERVER: {
      configuration_refresher_->Observe(
          ProfileSyncServiceFactory::GetForProfile(GetProfile(index)));
      break;
    }
    case SERVER_TYPE_UNDECIDED:
      NOTREACHED();
  }
}

void SyncTest::SetupSyncNoWaitingForCompletion() {
  SetupSyncInternal(/*setup_mode=*/NO_WAITING);
}

void SyncTest::SetupSyncOneClientAfterAnother() {
  SetupSyncInternal(/*setup_mode=*/WAIT_FOR_COMMITS_TO_COMPLETE);
}

void SyncTest::SetupSyncInternal(SetupSyncMode setup_mode) {
  // Create sync profiles and clients if they haven't already been created.
  if (profiles_.empty()) {
    if (!SetupClients()) {
      LOG(FATAL) << "SetupClients() failed.";
    }
  }

  // TODO(crbug.com/801482): If we ever start running tests against external
  // servers again, we might have to find a way to clear any pre-existing data
  // from the test account.
  if (UsingExternalServers()) {
    LOG(ERROR) << "WARNING: Running against external servers with an existing "
                  "account. If there is any pre-existing data in the account, "
                  "things will likely break.";
  }

  // Sync each of the profiles.
  for (int client_index = 0; client_index < num_clients_; client_index++) {
    ProfileSyncServiceHarness* client = GetClient(client_index);
    DVLOG(1) << "Setting up " << client_index << " client";

    auto decryption_passphrase_it =
        client_decryption_passphrases_.find(client_index);
    auto encryption_passphrase_it =
        client_encryption_passphrases_.find(client_index);
    bool decryption_passphrase_provided =
        (decryption_passphrase_it != client_decryption_passphrases_.end());
    bool encryption_passphrase_provided =
        (encryption_passphrase_it != client_encryption_passphrases_.end());
    if (decryption_passphrase_provided && encryption_passphrase_provided) {
      LOG(FATAL) << "Both an encryption and decryption passphrase were "
                    "provided for the client. This is disallowed.";
    }

    if (encryption_passphrase_provided) {
      CHECK(client->SetupSyncWithEncryptionPassphraseNoWaitForCompletion(
          GetRegisteredSelectableTypes(client_index),
          encryption_passphrase_it->second))
          << "SetupSync() failed.";
    } else if (decryption_passphrase_provided) {
      CHECK(client->SetupSyncWithDecryptionPassphraseNoWaitForCompletion(
          GetRegisteredSelectableTypes(client_index),
          decryption_passphrase_it->second))
          << "SetupSync() failed.";
    } else {
      CHECK(client->SetupSyncNoWaitForCompletion(
          GetRegisteredSelectableTypes(client_index)))
          << "SetupSync() failed.";
    }

    // It's important to wait for each client before setting up the next one,
    // otherwise multi-client tests get flaky.
    // TODO(crbug.com/956043): It would be nice to figure out why.
    switch (setup_mode) {
      case NO_WAITING:
        break;
      case WAIT_FOR_SYNC_SETUP_TO_COMPLETE:
        client->AwaitSyncSetupCompletion();
        break;
      case WAIT_FOR_COMMITS_TO_COMPLETE:
        DCHECK(TestUsesSelfNotifications())
            << "We need that for the UpdatedProgressMarkerChecker";
        UpdatedProgressMarkerChecker checker(GetSyncService(client_index));
        checker.Wait();
        break;
    }
  }
}

void SyncTest::ClearProfiles() {
  profiles_.clear();
  profile_delegates_.clear();
  scoped_temp_dirs_.clear();
  browsers_.clear();
  clients_.clear();
}

bool SyncTest::SetupSync() {
  base::ScopedAllowBlockingForTesting allow_blocking;

  SetupSyncInternal(/*setup_mode=*/WAIT_FOR_SYNC_SETUP_TO_COMPLETE);

  // Because clients may modify sync data as part of startup (for example
  // local session-releated data is rewritten), we need to ensure all
  // startup-based changes have propagated between the clients.
  //
  // Tests that don't use self-notifications can't await quiescense.  They'll
  // have to find their own way of waiting for an initial state if they really
  // need such guarantees.
  if (TestUsesSelfNotifications()) {
    if (!AwaitQuiescence()) {
      LOG(FATAL) << "AwaitQuiescence() failed.";
      return false;
    }
  }

  if (UsingExternalServers()) {
    // OneClickSigninSyncStarter observer is created with a real user sign in.
    // It is deleted on certain conditions which are not satisfied by our tests,
    // and this causes the SigninTracker observer to stay hanging at shutdown.
    // Calling LoginUIService::SyncConfirmationUIClosed forces the observer to
    // be removed. http://crbug.com/484388
    for (int i = 0; i < num_clients_; ++i) {
      LoginUIServiceFactory::GetForProfile(GetProfile(i))
          ->SyncConfirmationUIClosed(
              LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
    }
  }

  return true;
}

void SyncTest::TearDownOnMainThread() {
  // Workaround for https://crbug.com/801569: |prefs::kProfileLastUsed| stores
  // the profile path relative to the user dir, but our testing profiles are
  // outside the user dir (see CreateProfile). So code trying to access the last
  // used profile by path will fail. To work around that, set the last used
  // profile back to the originally created default profile (which does live in
  // the user data dir, and which we don't use otherwise).
  if (previous_profile_) {
    profiles::SetLastUsedProfile(
        previous_profile_->GetPath().BaseName().MaybeAsASCII());
  }

  // Closing all browsers created by this test. The calls here block until
  // they are closed. Other browsers created outside SyncTest setup should be
  // closed by the creator of that browser.
  size_t init_browser_count = chrome::GetTotalBrowserCount();
  for (size_t i = 0; i < browsers_.size(); ++i) {
    CloseBrowserSynchronously(browsers_[i]);
  }
  ASSERT_EQ(chrome::GetTotalBrowserCount(),
            init_browser_count - browsers_.size());

  if (fake_server_.get()) {
    for (const std::unique_ptr<fake_server::FakeServerInvalidationSender>&
             observer : fake_server_invalidation_observers_) {
      fake_server_->RemoveObserver(observer.get());
    }
  }

  // Delete things that unsubscribe in destructor before their targets are gone.
  configuration_refresher_.reset();
}

void SyncTest::SetUpInProcessBrowserTestFixture() {
  will_create_browser_context_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
              base::BindRepeating(&SyncTest::OnWillCreateBrowserContextServices,
                                  base::Unretained(this)));
}

void SyncTest::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  if (UsingExternalServers()) {
    // DO NOTHING. External live sync servers use GCM to notify profiles of
    // any invalidations in sync'ed data. No need to provide a testing
    // factory the ProfileInvalidationProvider.
    return;
  }
  invalidation::ProfileInvalidationProviderFactory::GetInstance()
      ->SetTestingFactory(
          context,
          base::BindRepeating(&SyncTest::CreateProfileInvalidationProvider,
                              &profile_to_fcm_network_handler_map_,
                              &fake_instance_id_driver_));
}

// static
std::unique_ptr<KeyedService> SyncTest::CreateProfileInvalidationProvider(
    std::map<const Profile*, syncer::FCMNetworkHandler*>*
        profile_to_fcm_network_handler_map,
    instance_id::InstanceIDDriver* instance_id_driver,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  gcm::GCMProfileService* gcm_profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile);

  auto profile_identity_provider =
      std::make_unique<invalidation::ProfileIdentityProvider>(
          IdentityManagerFactory::GetForProfile(profile));

  auto fcm_invalidation_service =
      std::make_unique<invalidation::FCMInvalidationService>(
          profile_identity_provider.get(),
          base::BindRepeating(&CreateFCMNetworkHandler, profile,
                              profile_to_fcm_network_handler_map,
                              gcm_profile_service->driver(),
                              instance_id_driver),
          base::BindRepeating(
              &CreatePerUserTopicRegistrationManager,
              profile_identity_provider.get(), profile->GetPrefs(),
              base::RetainedRef(
                  content::BrowserContext::GetDefaultStoragePartition(profile)
                      ->GetURLLoaderFactoryForBrowserProcess()
                      .get())),
          instance_id_driver, profile->GetPrefs(), kInvalidationGCMSenderId);
  fcm_invalidation_service->Init();

  return std::make_unique<invalidation::ProfileInvalidationProvider>(
      std::move(fcm_invalidation_service), std::move(profile_identity_provider),
      /*custom_sender_invalidation_service_factory=*/
      base::BindRepeating(
          [](const std::string&)
              -> std::unique_ptr<invalidation::InvalidationService> {
            return std::make_unique<invalidation::FakeInvalidationService>();
          }));
}

void SyncTest::ResetSyncForPrimaryAccount() {
  if (UsingExternalServers()) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // For external server testing, we need to have a clean account.
    // The following code will sign in one chrome browser, get
    // the client id and access token, then clean the server data.
    int old_num_clients = num_clients_;
    int old_use_new_user_data_dir = use_new_user_data_dir_;
    use_new_user_data_dir_ = true;
    num_clients_ = 1;
    // Do not wait for sync complete. Some tests set passphrase and sync
    // will fail. NO_WAITING mode gives access token and birthday so
    // ProfileSyncServiceHarness::ResetSyncForPrimaryAccount() can succeed.
    // The passphrase will be reset together with the rest of the sync data
    // clearing.
    SetupSyncNoWaitingForCompletion();
    GetClient(0)->ResetSyncForPrimaryAccount();
    GetClient(0)->StopSyncServiceAndClearData();
    ClearProfiles();
    use_new_user_data_dir_ = old_use_new_user_data_dir;
    num_clients_ = old_num_clients;
  }
}

void SyncTest::SetUpOnMainThread() {
  // Start up a sync test server if one is needed and setup mock gaia responses.
  // Note: This must be done prior to the call to SetupClients() because we want
  // the mock gaia responses to be available before GaiaUrls is initialized.
  SetUpTestServerIfRequired();

  if (!UsingExternalServers())
    SetupMockGaiaResponsesForProfile(ProfileManager::GetActiveUserProfile());

  // Allows google.com as well as country-specific TLDs.
  host_resolver()->AllowDirectLookup("*.google.com");
  host_resolver()->AllowDirectLookup("accounts.google.*");
  host_resolver()->AllowDirectLookup("*.googleusercontent.com");
  // Allow connection to googleapis.com for oauth token requests in E2E tests.
  host_resolver()->AllowDirectLookup("*.googleapis.com");

  // On Linux, we use Chromium's NSS implementation which uses the following
  // hosts for certificate verification. Without these overrides, running the
  // integration tests on Linux causes error as we make external DNS lookups.
  host_resolver()->AllowDirectLookup("*.thawte.com");
  host_resolver()->AllowDirectLookup("*.geotrust.com");
  host_resolver()->AllowDirectLookup("*.gstatic.com");
}

void SyncTest::WaitForDataModels(Profile* profile) {
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(profile));
  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS));
  search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(profile));
#if defined(OS_CHROMEOS)
  printers_helper::WaitForPrinterStoreToLoad(profile);
#endif
}

void SyncTest::ReadPasswordFile() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  password_file_ = cl->GetSwitchValuePath(switches::kPasswordFileForTest);
  if (password_file_.empty())
    LOG(FATAL) << "Can't run live server test without specifying --"
               << switches::kPasswordFileForTest << "=<filename>";
  std::string file_contents;
  base::ReadFileToString(password_file_, &file_contents);
  ASSERT_NE(file_contents, "")
      << "Password file \"" << password_file_.value() << "\" does not exist.";
  std::vector<std::string> tokens = base::SplitString(
      file_contents, "\r\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(2U, tokens.size()) << "Password file \"" << password_file_.value()
                               << "\" must contain exactly two lines of text.";
  username_ = tokens[0];
  password_ = tokens[1];
}

void SyncTest::SetupMockGaiaResponses() {
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->get_user_info_url().spec(),
      "email=user@gmail.com\ndisplayEmail=user@gmail.com");
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_token_url().spec(),
      R"({
            "refresh_token": "rt1",
            "access_token": "at1",
            "expires_in": 3600,
            "token_type": "Bearer"
         })");
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth_user_info_url().spec(),
      "{ \"id\": \"12345\" }");
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth1_login_url().spec(),
      "SID=sid\nLSID=lsid\nAuth=auth_token");
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_revoke_url().spec(), "");
}

void SyncTest::SetOAuth2TokenResponse(const std::string& response_data,
                                      net::HttpStatusCode status_code,
                                      net::URLRequestStatus::Status status) {
  network::URLLoaderCompletionStatus completion_status(status);
  completion_status.decoded_body_length = response_data.size();

  std::string response = base::StringPrintf("HTTP/1.1 %d %s\r\n", status_code,
                                            GetHttpReasonPhrase(status_code));
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(response);
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_token_url(), std::move(response_head),
      response_data, completion_status);
  base::RunLoop().RunUntilIdle();
}

void SyncTest::ClearMockGaiaResponses() {
  // Clear any mock gaia responses that might have been set.
  test_url_loader_factory_.ClearResponses();
}

void SyncTest::DecideServerType() {
  // Only set |server_type_| if it hasn't already been set. This allows for
  // tests to explicitly set this value in each test class if needed.
  if (server_type_ == SERVER_TYPE_UNDECIDED) {
    base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
    if (!cl->HasSwitch(switches::kSyncServiceURL)) {
      // If no sync server URL is provided, start up a local sync test server
      // and point Chrome to its URL. This is the most common configuration,
      server_type_ = IN_PROCESS_FAKE_SERVER;
    } else {
      // If a sync server URL is provided, it is assumed that the server is
      // already running. Chrome will automatically connect to it at the URL
      // provided. There is nothing to do here.
      server_type_ = EXTERNAL_LIVE_SERVER;
    }
  }
}

// Start up a local sync server based on the value of server_type_, which
// was determined from the command line parameters.
void SyncTest::SetUpTestServerIfRequired() {
  if (UsingExternalServers()) {
    // Nothing to do; we'll just talk to the URL we were given.
  } else if (server_type_ == IN_PROCESS_FAKE_SERVER) {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    fake_server_ = std::make_unique<fake_server::FakeServer>(user_data_dir);
    SetupMockGaiaResponses();
  } else {
    LOG(FATAL) << "Don't know which server environment to run test in.";
  }
}

bool SyncTest::TestUsesSelfNotifications() {
  // Default is True unless we are running against external servers.
  return !UsingExternalServers();
}

bool SyncTest::EnableEncryption(int index) {
  ProfileSyncService* service = GetClient(index)->service();

  if (::IsEncryptionComplete(service))
    return true;

  service->GetUserSettings()->EnableEncryptEverything();

  // In order to kick off the encryption we have to reconfigure. Just grab the
  // currently synced types and use them.
  syncer::UserSelectableTypeSet selected_types =
      service->GetUserSettings()->GetSelectedTypes();
  bool sync_everything =
      (selected_types == syncer::UserSelectableTypeSet::All());
  service->GetUserSettings()->SetSelectedTypes(sync_everything, selected_types);

  return AwaitEncryptionComplete(index);
}

bool SyncTest::IsEncryptionComplete(int index) {
  return ::IsEncryptionComplete(GetClient(index)->service());
}

bool SyncTest::AwaitEncryptionComplete(int index) {
  ProfileSyncService* service = GetClient(index)->service();
  return EncryptionChecker(service).Wait();
}

bool SyncTest::AwaitQuiescence() {
  return ProfileSyncServiceHarness::AwaitQuiescence(GetSyncClients());
}

bool SyncTest::UsingExternalServers() {
  return server_type_ == EXTERNAL_LIVE_SERVER;
}

void SyncTest::TriggerMigrationDoneError(syncer::ModelTypeSet model_types) {
  ASSERT_TRUE(server_type_ == IN_PROCESS_FAKE_SERVER);
  fake_server_->TriggerMigrationDoneError(model_types);
}

fake_server::FakeServer* SyncTest::GetFakeServer() const {
  return fake_server_.get();
}

void SyncTest::TriggerSyncForModelTypes(int index,
                                        syncer::ModelTypeSet model_types) {
  GetSyncService(index)->TriggerRefresh(model_types);
}

void SyncTest::StopConfigurationRefresher() {
  configuration_refresher_.reset();
}

arc::SyncArcPackageHelper* SyncTest::sync_arc_helper() {
#if defined(OS_CHROMEOS)
  return arc::SyncArcPackageHelper::GetInstance();
#else
  return nullptr;
#endif
}
