// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/invalidation/deprecated_profile_invalidation_provider_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/fake_server_invalidation_service.h"
#include "chrome/browser/sync/test/integration/p2p_invalidation_forwarder.h"
#include "chrome/browser/sync/test/integration/p2p_sync_refresher.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/p2p_invalidation_service.h"
#include "components/invalidation/impl/p2p_invalidator.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine_impl/sync_scheduler_impl.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/base/port_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/sync/test/integration/printers_helper.h"
#include "chrome/browser/sync/test/integration/sync_arc_package_helper.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_util.h"
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_APP_LIST)
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#endif  // BUILDFLAG(ENABLE_APP_LIST)

using browser_sync::ProfileSyncService;
using content::BrowserThread;

namespace switches {
const char kPasswordFileForTest[] = "password-file-for-test";
const char kSyncUserForTest[] = "sync-user-for-test";
const char kSyncPasswordForTest[] = "sync-password-for-test";
}

namespace {

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
    g_browser_process->profile_manager()->RegisterTestingProfile(profile,
                                                                 true,
                                                                 false);

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
  return service->IsEncryptEverythingEnabled() &&
         !service->encryption_pending();
}

// Helper class to wait for encryption to complete.
class EncryptionChecker : public SingleClientStatusChangeChecker {
 public:
  explicit EncryptionChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    return IsEncryptionComplete(service());
  }

  std::string GetDebugMessage() const override { return "Encryption"; }
};

std::unique_ptr<KeyedService> BuildFakeServerProfileInvalidationProvider(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return std::make_unique<invalidation::ProfileInvalidationProvider>(
      std::make_unique<fake_server::FakeServerInvalidationService>(),
      std::make_unique<invalidation::ProfileIdentityProvider>(
          IdentityManagerFactory::GetForProfile(profile)));
}

std::unique_ptr<KeyedService> BuildP2PProfileInvalidationProvider(
    content::BrowserContext* context,
    syncer::P2PNotificationTarget notification_target) {
  Profile* profile = static_cast<Profile*>(context);
  return std::make_unique<invalidation::ProfileInvalidationProvider>(
      std::make_unique<invalidation::P2PInvalidationService>(
          profile->GetRequestContext(), content::GetNetworkConnectionTracker(),
          notification_target),
      std::make_unique<invalidation::ProfileIdentityProvider>(
          IdentityManagerFactory::GetForProfile(profile)));
}

std::unique_ptr<KeyedService> BuildSelfNotifyingP2PProfileInvalidationProvider(
    content::BrowserContext* context) {
  return BuildP2PProfileInvalidationProvider(context, syncer::NOTIFY_ALL);
}

std::unique_ptr<KeyedService> BuildRealisticP2PProfileInvalidationProvider(
    content::BrowserContext* context) {
  return BuildP2PProfileInvalidationProvider(context, syncer::NOTIFY_OTHERS);
}

}  // namespace

SyncTest::SyncTest(TestType test_type)
    : test_type_(test_type),
      server_type_(SERVER_TYPE_UNDECIDED),
      previous_profile_(nullptr),
      num_clients_(-1),
      use_verifier_(true),
      create_gaia_account_at_runtime_(false),
      test_shared_url_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)) {
  sync_datatype_helper::AssociateWithTest(this);
  switch (test_type_) {
    case SINGLE_CLIENT:
    case SINGLE_CLIENT_LEGACY: {
      num_clients_ = 1;
      break;
    }
    case TWO_CLIENT:
    case TWO_CLIENT_LEGACY: {
      num_clients_ = 2;
      break;
    }
    default:
      NOTREACHED() << "Invalid test type specified.";
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

  // Start up a sync test server if one is needed and setup mock gaia responses.
  // Note: This must be done prior to the call to SetupClients() because we want
  // the mock gaia responses to be available before GaiaUrls is initialized.
  SetUpTestServerIfRequired();

  // Yield control back to the InProcessBrowserTest framework.
  InProcessBrowserTest::SetUp();
}

void SyncTest::TearDown() {
  // Clear any mock gaia responses that might have been set.
  ClearMockGaiaResponses();

  // Allow the InProcessBrowserTest framework to perform its tear down.
  InProcessBrowserTest::TearDown();

  // Stop the local python test server. This is a no-op if one wasn't started.
  TearDownLocalPythonTestServer();

  // Stop the local sync test server. This is a no-op if one wasn't started.
  TearDownLocalTestServer();

  // Return OSCrypt to its real behaviour
  OSCryptMocker::TearDown();

  test_shared_url_loader_factory_->Detach();
  fake_server_.reset();
}

void SyncTest::SetUpCommandLine(base::CommandLine* cl) {
  AddTestSwitches(cl);
  AddOptionalTypesToCommandLine(cl);

#if defined(OS_CHROMEOS)
  cl->AppendSwitch(chromeos::switches::kIgnoreUserProfileMappingForTests);
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
}

void SyncTest::AddOptionalTypesToCommandLine(base::CommandLine* cl) {}

bool SyncTest::CreateGaiaAccount(const std::string& username,
                                 const std::string& password) {
  std::string relative_url = base::StringPrintf("/CreateUsers?%s=%s",
                                                username.c_str(),
                                                password.c_str());
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

void SyncTest::BeforeSetupClient(int index) {}

bool SyncTest::CreateProfile(int index) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  tmp_profile_paths_[index] = new base::ScopedTempDir();
  if (UsingExternalServers() && num_clients_ > 1) {
    // For multi profile UI signin, profile paths should be outside user data
    // dir to allow signing-in multiple profiles to same account. Otherwise, we
    // get an error that the profile has already signed in on this device.
    // Note: Various places in Chrome assume that all profiles are within the
    // user data dir. We violate that assumption here, which can lead to weird
    // issues, see https://crbug.com/801569 and the workaround in
    // TearDownOnMainThread.
    if (!tmp_profile_paths_[index]->CreateUniqueTempDir()) {
      ADD_FAILURE();
      return false;
    }
  } else {
    // Create new profiles in user data dir so that other profiles can know
    // about it. This is needed in tests such as supervised user cases which
    // assume browser->profile() as the custodian profile.
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    if (!tmp_profile_paths_[index]->CreateUniqueTempDirUnderPath(
            user_data_dir)) {
      ADD_FAILURE();
      return false;
    }
  }
  base::FilePath profile_path = tmp_profile_paths_[index]->GetPath();
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
    ChromeSigninClient* signin_client = static_cast<ChromeSigninClient*>(
        ChromeSigninClientFactory::GetForProfile(profile));
    signin_client->SetURLLoaderFactoryForTest(test_shared_url_loader_factory_);
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
  ProfileManager::CreateCallback create_callback = base::Bind(
      &CreateProfileCallback, run_loop.QuitClosure());
  profile_manager->CreateProfileAsync(profile_path,
                                      create_callback,
                                      base::string16(),
                                      std::string());
  run_loop.Run();
  return profile_manager->GetProfileByPath(profile_path);
}

Profile* SyncTest::MakeTestProfile(base::FilePath profile_path, int index) {
  const auto& preference_contents_it =
      preexisting_preferences_file_contents_.find(index);
  if (preference_contents_it != preexisting_preferences_file_contents_.end() &&
      !preference_contents_it->second.empty()) {
    // The profile directory might not exist yet (e.g. for the verifier_
    // profile).
    if (!base::PathExists(profile_path) &&
        !base::CreateDirectory(profile_path)) {
      LOG(FATAL) << "Could not create profile directory: " << profile_path;
    }
    base::FilePath pref_path(profile_path.Append(chrome::kPreferencesFilename));
    int write_result =
        base::WriteFile(pref_path, preference_contents_it->second.c_str(),
                        preference_contents_it->second.size());
    if (write_result !=
        static_cast<int>(preference_contents_it->second.size())) {
      LOG(FATAL) << "Preexisting Preferences file could not be written to "
                 << pref_path;
    }
  }

  Profile* profile =
      Profile::CreateProfile(profile_path, profile_delegates_[index].get(),
                             Profile::CREATE_MODE_SYNCHRONOUS);
  return profile;
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
  return ProfileSyncServiceFactory::GetForProfile(GetProfile(index));
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
  tmp_profile_paths_.resize(num_clients_);
  clients_.resize(num_clients_);
  invalidation_forwarders_.resize(num_clients_);
  sync_refreshers_.resize(num_clients_);
  fake_server_invalidation_services_.resize(num_clients_);

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
    BeforeSetupClient(i);
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
      ProfileSyncServiceFactory::GetForProfile(GetProfile(index));

  if (server_type_ == IN_PROCESS_FAKE_SERVER) {
    // TODO(pvalenzuela): Run the fake server via EmbeddedTestServer.
    profile_sync_service->OverrideNetworkResourcesForTest(
        base::WrapUnique<syncer::NetworkResources>(
            new fake_server::FakeServerNetworkResources(
                fake_server_->AsWeakPtr())));
  }

  ProfileSyncServiceHarness::SigninType singin_type = UsingExternalServers()
          ? ProfileSyncServiceHarness::SigninType::UI_SIGNIN
          : ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN;

  DCHECK(!clients_[index]);
  clients_[index] = ProfileSyncServiceHarness::Create(
      GetProfile(index), username_, password_, singin_type);
  EXPECT_NE(nullptr, GetClient(index)) << "Could not create Client " << index;
  InitializeInvalidations(index);
}

void SyncTest::DisableNotificationsForClient(int index) {
  fake_server_->RemoveObserver(fake_server_invalidation_services_[index]);
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
  ChromeSigninClient* signin_client = static_cast<ChromeSigninClient*>(
      ChromeSigninClientFactory::GetForProfile(profile));
  signin_client->SetURLLoaderFactoryForTest(test_shared_url_loader_factory_);
}

void SyncTest::SetUpInvalidations(int index) {
  bool fcm_invalidations_enabled =
      base::FeatureList::IsEnabled(invalidation::switches::kFCMInvalidations);
  switch (server_type_) {
    case EXTERNAL_LIVE_SERVER:
      // DO NOTHING. External live sync servers use GCM to notify profiles of
      // any invalidations in sync'ed data. In this case, to notify other
      // profiles of invalidations, we use sync refresh notifications instead.
      break;

    case IN_PROCESS_FAKE_SERVER: {
      KeyedService* test_factory;
      if (fcm_invalidations_enabled) {
        test_factory =
            invalidation::ProfileInvalidationProviderFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    GetProfile(index),
                    base::BindRepeating(
                        &BuildFakeServerProfileInvalidationProvider));

      } else {
        test_factory =
            invalidation::DeprecatedProfileInvalidationProviderFactory::
                GetInstance()
                    ->SetTestingFactoryAndUse(
                        GetProfile(index),
                        base::BindRepeating(
                            &BuildFakeServerProfileInvalidationProvider));
      }
      invalidation::InvalidationService* invalidation_service =
          static_cast<invalidation::ProfileInvalidationProvider*>(test_factory)
              ->GetInvalidationService();
      auto* fake_invalidation_service =
          static_cast<fake_server::FakeServerInvalidationService*>(
              invalidation_service);

      fake_server_->AddObserver(fake_invalidation_service);
      if (TestUsesSelfNotifications())
        fake_invalidation_service->EnableSelfNotifications();
      else
        fake_invalidation_service->DisableSelfNotifications();
      fake_server_invalidation_services_[index] = fake_invalidation_service;
      break;
    }
    case SERVER_TYPE_UNDECIDED:
    case LOCAL_PYTHON_SERVER:
      BrowserContextKeyedServiceFactory::TestingFactory invalidation_provider =
          base::BindRepeating(
              TestUsesSelfNotifications()
                  ? &BuildSelfNotifyingP2PProfileInvalidationProvider
                  : &BuildRealisticP2PProfileInvalidationProvider);
      if (fcm_invalidations_enabled) {
        invalidation::ProfileInvalidationProviderFactory::GetInstance()
            ->SetTestingFactoryAndUse(GetProfile(index),
                                      std::move(invalidation_provider));
      } else {
        invalidation::DeprecatedProfileInvalidationProviderFactory::
            GetInstance()
                ->SetTestingFactoryAndUse(GetProfile(index),
                                          std::move(invalidation_provider));
      }
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
    case LOCAL_PYTHON_SERVER:
      bool fcm_invalidations_enabled = base::FeatureList::IsEnabled(
          invalidation::switches::kFCMInvalidations);
      invalidation::InvalidationService* invalidation_service;
      if (fcm_invalidations_enabled) {
        invalidation_service =
            invalidation::ProfileInvalidationProviderFactory::GetForProfile(
                GetProfile(index))
                ->GetInvalidationService();
      } else {
        invalidation_service =
            invalidation::DeprecatedProfileInvalidationProviderFactory::
                GetForProfile(GetProfile(index))
                    ->GetInvalidationService();
      }
      invalidation::P2PInvalidationService* p2p_invalidation_service =
          static_cast<invalidation::P2PInvalidationService*>(
              invalidation_service);
      p2p_invalidation_service->UpdateCredentials(username_, password_);
      // Start listening for and emitting notifications of commits.
      DCHECK(!invalidation_forwarders_[index]);
      invalidation_forwarders_[index] =
          std::make_unique<P2PInvalidationForwarder>(clients_[index]->service(),
                                                     p2p_invalidation_service);
  }
}

bool SyncTest::SetupSync() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Create sync profiles and clients if they haven't already been created.
  if (profiles_.empty()) {
    if (!SetupClients()) {
      LOG(FATAL) << "SetupClients() failed.";
      return false;
    }
  }

  int clientIndex = 0;
  // If we're using external servers, clear server data so the account starts
  // with a clean slate.
  if (UsingExternalServers()) {
    if (!SetupAndClearClient(clientIndex++)) {
      LOG(FATAL) << "Setting up and clearing data for client "
                 << clientIndex - 1 << " failed";
      return false;
    }
  }

  // Sync each of the profiles.
  for (; clientIndex < num_clients_; clientIndex++) {
    ProfileSyncServiceHarness* client = GetClient(clientIndex);
    DVLOG(1) << "Setting up " << clientIndex << " client";

    auto decryption_passphrase_it =
        client_decryption_passphrases_.find(clientIndex);
    auto encryption_passphrase_it =
        client_encryption_passphrases_.find(clientIndex);
    bool decryption_passphrase_provided =
        (decryption_passphrase_it != client_decryption_passphrases_.end());
    bool encryption_passphrase_provided =
        (encryption_passphrase_it != client_encryption_passphrases_.end());
    if (decryption_passphrase_provided && encryption_passphrase_provided) {
      LOG(FATAL) << "Both an encryption and decryption passphrase were "
                    "provided for the client. This is disallowed.";
      return false;
    }

    bool setup_succeeded;
    if (encryption_passphrase_provided) {
      setup_succeeded = client->SetupSyncWithEncryptionPassphrase(
          syncer::UserSelectableTypes(), encryption_passphrase_it->second);
    } else if (decryption_passphrase_provided) {
      setup_succeeded = client->SetupSyncWithDecryptionPassphrase(
          syncer::UserSelectableTypes(), decryption_passphrase_it->second);
    } else {
      setup_succeeded = client->SetupSync(syncer::UserSelectableTypes());
    }

    if (!setup_succeeded) {
      LOG(FATAL) << "SetupSync() failed.";
      return false;
    }
  }

  // Because clients may modify sync data as part of startup (for example local
  // session-releated data is rewritten), we need to ensure all startup-based
  // changes have propagated between the clients.
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

  // SyncRefresher is used instead of invalidations to notify other profiles to
  // do a sync refresh on committed data sets. This is only needed when running
  // tests against external live server, otherwise invalidation service is used.
  // With external live servers, the profiles commit data on first sync cycle
  // automatically after signing in. To avoid misleading sync commit
  // notifications at start up, we start the SyncRefresher observers post
  // client set up.
  if (UsingExternalServers()) {
    for (int i = 0; i < num_clients_; ++i) {
      DCHECK(!sync_refreshers_[i]);
      sync_refreshers_[i] = std::make_unique<P2PSyncRefresher>(
          GetProfile(i), clients_[i]->service());
    }

    // OneClickSigninSyncStarter observer is created with a real user sign in.
    // It is deleted on certain conditions which are not satisfied by our tests,
    // and this causes the SigninTracker observer to stay hanging at shutdown.
    // Calling LoginUIService::SyncConfirmationUIClosed forces the observer to
    // be removed. http://crbug.com/484388
    for (int i = 0; i < num_clients_; ++i) {
      LoginUIServiceFactory::GetForProfile(GetProfile(i))->
          SyncConfirmationUIClosed(LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
    }
  }

  return true;
}

bool SyncTest::SetupAndClearClient(size_t index) {
  // Setup the first client so the sync engine is initialized, which is
  // required to clear server data.
  DVLOG(1) << "Setting up first client for clear.";
  if (!GetClient(index)->SetupSyncForClearingServerData()) {
    LOG(FATAL) << "SetupSync() failed.";
    return false;
  }

  DVLOG(1) << "Done setting up first client for clear.";
  if (!ClearServerData(GetClient(index++))) {
    LOG(FATAL) << "ClearServerData failed.";
    return false;
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

  for (size_t i = 0; i < clients_.size(); ++i) {
    clients_[i]->service()->RequestStop(ProfileSyncService::CLEAR_DATA);
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
    std::vector<fake_server::FakeServerInvalidationService*>::const_iterator it;
    for (it = fake_server_invalidation_services_.begin();
         it != fake_server_invalidation_services_.end(); ++it) {
      fake_server_->RemoveObserver(*it);
    }
  }

  // Delete things that unsubscribe in destructor before their targets are gone.
  invalidation_forwarders_.clear();
  sync_refreshers_.clear();
  configuration_refresher_.reset();
}

void SyncTest::SetUpOnMainThread() {
  if (!UsingExternalServers())
    SetupMockGaiaResponsesForProfile(ProfileManager::GetActiveUserProfile());

  // Allows google.com as well as country-specific TLDs.
  host_resolver()->AllowDirectLookup("*.google.com");
  host_resolver()->AllowDirectLookup("accounts.google.*");

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
  ASSERT_NE(file_contents, "") << "Password file \""
      << password_file_.value() << "\" does not exist.";
  std::vector<std::string> tokens = base::SplitString(
      file_contents, "\r\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(2U, tokens.size()) << "Password file \""
      << password_file_.value()
      << "\" must contain exactly two lines of text.";
  username_ = tokens[0];
  password_ = tokens[1];
}

void SyncTest::SetupMockGaiaResponses() {
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->get_user_info_url().spec(),
      "email=user@gmail.com\ndisplayEmail=user@gmail.com");
  test_url_loader_factory_.AddResponse(GoogleURLTracker::kSearchDomainCheckURL,
                                       ".google.com");
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
  network::ResourceResponseHead resource_response;
  resource_response.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(response);
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_token_url(), resource_response,
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
      // and the only one that makes sense for most developers. FakeServer is
      // the current solution but some scenarios are only supported by the
      // legacy python server.
      switch (test_type_) {
        case SINGLE_CLIENT:
        case TWO_CLIENT:
          server_type_ = IN_PROCESS_FAKE_SERVER;
          break;
        case SINGLE_CLIENT_LEGACY:
        case TWO_CLIENT_LEGACY:
          server_type_ = LOCAL_PYTHON_SERVER;
      }
      DCHECK_NE(server_type_, SERVER_TYPE_UNDECIDED);
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
  } else if (server_type_ == LOCAL_PYTHON_SERVER) {
    if (!SetUpLocalPythonTestServer())
      LOG(FATAL) << "Failed to set up local python sync and XMPP servers";
    SetupMockGaiaResponses();
  } else if (server_type_ == IN_PROCESS_FAKE_SERVER) {
    fake_server_ = std::make_unique<fake_server::FakeServer>();
    SetupMockGaiaResponses();
  } else {
    LOG(FATAL) << "Don't know which server environment to run test in.";
  }
}

bool SyncTest::SetUpLocalPythonTestServer() {
  EXPECT_TRUE(sync_server_.Start())
      << "Could not launch local python test server.";

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (server_type_ == LOCAL_PYTHON_SERVER) {
    std::string sync_service_url = sync_server_.GetURL("chromiumsync").spec();
    cl->AppendSwitchASCII(switches::kSyncServiceURL, sync_service_url);
    DVLOG(1) << "Started local python sync server at " << sync_service_url;
  }

  int xmpp_port = 0;
  if (!sync_server_.server_data().GetInteger("xmpp_port", &xmpp_port)) {
    LOG(ERROR) << "Could not find valid xmpp_port value";
    return false;
  }
  if ((xmpp_port <= 0) || (xmpp_port > std::numeric_limits<uint16_t>::max())) {
    LOG(ERROR) << "Invalid xmpp port: " << xmpp_port;
    return false;
  }

  net::HostPortPair xmpp_host_port_pair(sync_server_.host_port_pair());
  xmpp_host_port_pair.set_port(xmpp_port);
  xmpp_port_ = std::make_unique<net::ScopedPortException>(xmpp_port);

  if (!cl->HasSwitch(invalidation::switches::kSyncNotificationHostPort)) {
    cl->AppendSwitchASCII(invalidation::switches::kSyncNotificationHostPort,
                          xmpp_host_port_pair.ToString());
    // The local XMPP server only supports insecure connections.
    cl->AppendSwitch(invalidation::switches::kSyncAllowInsecureXmppConnection);
  }
  DVLOG(1) << "Started local python XMPP server at "
           << xmpp_host_port_pair.ToString();

  return true;
}

bool SyncTest::TearDownLocalPythonTestServer() {
  if (!sync_server_.Stop()) {
    LOG(ERROR) << "Could not stop local python test server.";
    return false;
  }
  xmpp_port_.reset();
  return true;
}

bool SyncTest::TearDownLocalTestServer() {
  if (test_server_.IsValid()) {
    EXPECT_TRUE(test_server_.Terminate(0, false))
        << "Could not stop local test server.";
    test_server_.Close();
  }
  return true;
}

bool SyncTest::WaitForTestServerToStart(base::TimeDelta wait, int intervals) {
  for (int i = 0; i < intervals; ++i) {
    if (IsTestServerRunning())
      return true;
    base::PlatformThread::Sleep(wait / intervals);
  }
  return false;
}

bool SyncTest::IsTestServerRunning() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  std::string sync_url = cl->GetSwitchValueASCII(switches::kSyncServiceURL);
  GURL sync_url_status(sync_url.append("/healthz"));
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = sync_url_status;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE |
                                 net::LOAD_DO_NOT_SEND_COOKIES |
                                 net::LOAD_DO_NOT_SAVE_COOKIES;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  bool server_running = false;
  base::RunLoop run_loop;
  simple_url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory(),
      base::BindLambdaForTesting(
          [&server_running,
           &run_loop](std::unique_ptr<std::string> response_body) {
            server_running =
                response_body && base::StartsWith(*response_body, "ok",
                                                  base::CompareCase::SENSITIVE);
            run_loop.Quit();
          }));
  return server_running;
}

bool SyncTest::TestUsesSelfNotifications() {
  // Default is True unless we are running against external servers.
  return !UsingExternalServers();
}

bool SyncTest::EnableEncryption(int index) {
  ProfileSyncService* service = GetClient(index)->service();

  if (::IsEncryptionComplete(service))
    return true;

  service->EnableEncryptEverything();

  // In order to kick off the encryption we have to reconfigure. Just grab the
  // currently synced types and use them.
  syncer::ModelTypeSet synced_datatypes = service->GetPreferredDataTypes();
  bool sync_everything = (synced_datatypes == syncer::ModelTypeSet::All());
  synced_datatypes.RetainAll(syncer::UserSelectableTypes());
  service->OnUserChoseDatatypes(sync_everything, synced_datatypes);

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

bool SyncTest::ServerSupportsNotificationControl() const {
  EXPECT_NE(SERVER_TYPE_UNDECIDED, server_type_);

  // Supported only if we're using the python testserver.
  return server_type_ == LOCAL_PYTHON_SERVER;
}

void SyncTest::DisableNotificationsImpl() {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  std::string path = "chromiumsync/disablenotifications";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notifications disabled",
            base::UTF16ToASCII(
                browser()->tab_strip_model()->GetActiveWebContents()->
                    GetTitle()));
}

void SyncTest::DisableNotifications() {
  DisableNotificationsImpl();
}

void SyncTest::EnableNotificationsImpl() {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  std::string path = "chromiumsync/enablenotifications";
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notifications enabled",
            base::UTF16ToASCII(
                browser()->tab_strip_model()->GetActiveWebContents()->
                    GetTitle()));
}

void SyncTest::EnableNotifications() {
  EnableNotificationsImpl();
}

void SyncTest::TriggerNotification(syncer::ModelTypeSet changed_types) {
  ASSERT_TRUE(ServerSupportsNotificationControl());
  const std::string& data =
      syncer::P2PNotificationData(
          "from_server",
          syncer::NOTIFY_ALL,
          syncer::ObjectIdInvalidationMap::InvalidateAll(
              syncer::ModelTypeSetToObjectIdSet(changed_types))).ToString();
  const std::string& path =
      std::string("chromiumsync/sendnotification?channel=") +
      syncer::kSyncP2PNotificationChannel + "&data=" + data;
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Notification sent",
            base::UTF16ToASCII(
                browser()->tab_strip_model()->GetActiveWebContents()->
                    GetTitle()));
}

bool SyncTest::ServerSupportsErrorTriggering() const {
  EXPECT_NE(SERVER_TYPE_UNDECIDED, server_type_);

  // Supported only if we're using the python testserver.
  return server_type_ == LOCAL_PYTHON_SERVER;
}

void SyncTest::TriggerMigrationDoneError(syncer::ModelTypeSet model_types) {
  ASSERT_TRUE(ServerSupportsErrorTriggering());
  std::string path = "chromiumsync/migrate";
  char joiner = '?';
  for (syncer::ModelType type : model_types) {
    path.append(
        base::StringPrintf("%ctype=%d", joiner,
                           syncer::GetSpecificsFieldNumberFromModelType(type)));
    joiner = '&';
  }
  ui_test_utils::NavigateToURL(browser(), sync_server_.GetURL(path));
  ASSERT_EQ("Migration: 200",
            base::UTF16ToASCII(
                browser()->tab_strip_model()->GetActiveWebContents()->
                    GetTitle()));
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

void SyncTest::SetPreexistingPreferencesFileContents(
    int index,
    const std::string& contents) {
  preexisting_preferences_file_contents_[index] = contents;
}

bool SyncTest::ClearServerData(ProfileSyncServiceHarness* harness) {
  // At this point our birthday is good.
  base::RunLoop run_loop;
  harness->service()->ClearServerDataForTest(run_loop.QuitClosure());
  run_loop.Run();

  // Our birthday is invalidated on the server here so restart sync to get
  // the new birthday from the server.
  harness->StopSyncService(syncer::SyncService::CLEAR_DATA);
  return harness->StartSyncService();
}
