// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/committed_all_nudged_changes_checker.h"
#include "chrome/browser/sync/test/integration/device_info_helper.h"
#include "chrome/browser/sync/test/integration/fake_sync_gcm_driver_for_instance_id.h"
#include "chrome/browser/sync/test/integration/session_hierarchy_match_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_disabled_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/fcm_network_handler.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/invalidation_helper.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/sync_scheduler_impl.h"
#include "components/sync/invalidations/sync_invalidations_service_impl.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_server_network_resources.h"
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
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/sync/test/integration/sync_arc_package_helper.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sync/test/integration/sync_test_utils_android.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

using syncer::SyncServiceImpl;

namespace {

// Sender ID coming from the Firebase console.
const char kInvalidationGCMSenderId[] = "8181035976";

void SetURLLoaderFactoryForTest(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  ChromeSigninClient* signin_client = static_cast<ChromeSigninClient*>(
      ChromeSigninClientFactory::GetForProfile(profile));
  signin_client->SetURLLoaderFactoryForTest(url_loader_factory);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::AccountManagerFactory* factory =
      g_browser_process->platform_part()->GetAccountManagerFactory();
  account_manager::AccountManager* account_manager =
      factory->GetAccountManager(profile->GetPath().value());
  account_manager->SetUrlLoaderFactoryForTests(url_loader_factory);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  account_manager::AccountManager* account_manager =
      MaybeGetAshAccountManagerForTests();
  if (account_manager)
    account_manager->SetUrlLoaderFactoryForTests(url_loader_factory);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

class FakePerUserTopicSubscriptionManager
    : public invalidation::PerUserTopicSubscriptionManager {
 public:
  explicit FakePerUserTopicSubscriptionManager(PrefService* local_state)
      : invalidation::PerUserTopicSubscriptionManager(
            /*identity_provider=*/nullptr,
            /*pref_service=*/local_state,
            /*url_loader_factory=*/nullptr,
            /*project_id*/ kInvalidationGCMSenderId) {}

  FakePerUserTopicSubscriptionManager(
      const FakePerUserTopicSubscriptionManager&) = delete;
  FakePerUserTopicSubscriptionManager& operator=(
      const FakePerUserTopicSubscriptionManager&) = delete;

  ~FakePerUserTopicSubscriptionManager() override = default;

  void UpdateSubscribedTopics(const invalidation::Topics& topics,
                              const std::string& instance_id_token) override {}
};

std::unique_ptr<invalidation::FCMNetworkHandler> CreateFCMNetworkHandler(
    Profile* profile,
    std::map<const Profile*, invalidation::FCMNetworkHandler*>*
        profile_to_fcm_network_handler_map,
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    const std::string& sender_id,
    const std::string& app_id) {
  auto handler = std::make_unique<invalidation::FCMNetworkHandler>(
      gcm_driver, instance_id_driver, sender_id, app_id);
  (*profile_to_fcm_network_handler_map)[profile] = handler.get();
  return handler;
}

std::unique_ptr<invalidation::PerUserTopicSubscriptionManager>
CreatePerUserTopicSubscriptionManager(PrefService* local_state,
                                      const std::string& project_id) {
  return std::make_unique<FakePerUserTopicSubscriptionManager>(local_state);
}

invalidation::FCMNetworkHandler* GetFCMNetworkHandler(
    Profile* profile,
    std::map<const Profile*, invalidation::FCMNetworkHandler*>*
        profile_to_fcm_network_handler_map) {
  // Delivering FCM notifications does not work if explicitly signed-out.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return nullptr;
  }

  auto it = profile_to_fcm_network_handler_map->find(profile);
  return it != profile_to_fcm_network_handler_map->end() ? it->second : nullptr;
}

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
class SyncTest::ClosedBrowserObserver : public BrowserListObserver {
 public:
  using OnBrowserRemovedCallback =
      base::RepeatingCallback<void(Browser* browser)>;

  explicit ClosedBrowserObserver(OnBrowserRemovedCallback callback)
      : browser_remove_callback_(std::move(callback)) {
    BrowserList::AddObserver(this);
  }

  ~ClosedBrowserObserver() override { BrowserList::RemoveObserver(this); }

  // BrowserListObserver overrides.
  void OnBrowserRemoved(Browser* browser) override {
    browser_remove_callback_.Run(browser);
  }

 private:
  OnBrowserRemovedCallback browser_remove_callback_;
};
#endif

SyncTest::SyncTest(TestType test_type)
    : test_type_(test_type),
      server_type_(base::CommandLine::ForCurrentProcess()->HasSwitch(
                       syncer::kSyncServiceURL)
                       ? EXTERNAL_LIVE_SERVER
                       : IN_PROCESS_FAKE_SERVER),
      test_construction_time_(base::Time::Now()),
      sync_run_loop_timeout(FROM_HERE, TestTimeouts::action_max_timeout()),
      previous_profile_(nullptr),
      num_clients_(-1) {
  // Any RunLoop timeout will by default result in test failure.
  sync_run_loop_timeout.SetAddGTestFailureOnTimeout();

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

  if (num_clients_ > 1) {
    // Workaround to turn off single client optimization for sync standalone
    // invalidations in tests.
    // TODO(crbug.com/1438806): remove once resolved.
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{switches::
                                   kSyncFilterOutInactiveDevicesForSingleClient,
                               {{"SyncActiveDeviceMargin", "-2d"}}}},
        /*disabled_features=*/{});
  }

#if !BUILDFLAG(IS_ANDROID)
  browser_list_observer_ = std::make_unique<ClosedBrowserObserver>(
      base::BindRepeating(&SyncTest::OnBrowserRemoved, base::Unretained(this)));
#endif
}

SyncTest::~SyncTest() = default;

void SyncTest::SetUp() {
#if BUILDFLAG(IS_ANDROID)
  sync_test_utils_android::SetUpAuthForTesting();
#endif

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kPasswordFileForTest)) {
    ReadPasswordFile();
  } else {
    // Decide on username to use or create one.
    if (cl->HasSwitch(switches::kSyncUserForTest)) {
      username_ = cl->GetSwitchValueASCII(switches::kSyncUserForTest);
    } else if (server_type_ != EXTERNAL_LIVE_SERVER) {
      username_ = "user@gmail.com";
    }
    // Decide on password to use.
    password_ = cl->HasSwitch(switches::kSyncPasswordForTest)
                    ? cl->GetSwitchValueASCII(switches::kSyncPasswordForTest)
                    : "password";
  }

  if (username_.empty() || password_.empty()) {
    LOG(FATAL) << "Cannot run sync tests without GAIA credentials.";
  }

  // Mock the Mac Keychain service.  The real Keychain can block on user input.
  OSCryptMocker::SetUp();

  // Yield control back to the PlatformBrowserTest framework.
  PlatformBrowserTest::SetUp();

  LOG(INFO) << "SyncTest::SetUp() completed; elapsed time since construction: "
            << (base::Time::Now() - test_construction_time_);
}

void SyncTest::TearDown() {
  // Clear any mock gaia responses that might have been set.
  ClearMockGaiaResponses();

  // Allow the PlatformBrowserTest framework to perform its tear down.
  PlatformBrowserTest::TearDown();

  // Return OSCrypt to its real behaviour
  OSCryptMocker::TearDown();
}

void SyncTest::PostRunTestOnMainThread() {
  PlatformBrowserTest::PostRunTestOnMainThread();

#if BUILDFLAG(IS_ANDROID)
  sync_test_utils_android::TearDownAuthForTesting();
#endif
}

void SyncTest::SetUpCommandLine(base::CommandLine* cl) {
  // Disable non-essential access of external network resources.
  if (!cl->HasSwitch(switches::kDisableBackgroundNetworking)) {
    cl->AppendSwitch(switches::kDisableBackgroundNetworking);
  }

  if (!cl->HasSwitch(syncer::kSyncShortInitialRetryOverride)) {
    cl->AppendSwitch(syncer::kSyncShortInitialRetryOverride);
  }

  if (!cl->HasSwitch(syncer::kSyncShortNudgeDelayForTest)) {
    cl->AppendSwitch(syncer::kSyncShortNudgeDelayForTest);
  }

  // TODO(crbug.com/1060366): This is a temporary switch to allow having two
  // profiles syncing the same account. Having a profile outside of the user
  // directory isn't supported in Chrome.
  if (!cl->HasSwitch(switches::kAllowProfilesOutsideUserDir)) {
    cl->AppendSwitch(switches::kAllowProfilesOutsideUserDir);
  }

  if (cl->HasSwitch(syncer::kSyncServiceURL)) {
    // TODO(crbug.com/1243653): setup real SecurityDomainService if
    // server_type_ == EXTERNAL_LIVE_SERVER.
    // Effectively disables interaction with SecurityDomainService for E2E
    // tests.
    cl->AppendSwitchASCII(syncer::kTrustedVaultServiceURL, "broken_url");
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  cl->AppendSwitch(ash::switches::kIgnoreUserProfileMappingForTests);
  cl->AppendSwitch(ash::switches::kDisableArcOptInVerification);
  cl->AppendSwitch(ash::switches::kDisableLacrosKeepAliveForTesting);
  arc::SetArcAvailableCommandLineForTesting(cl);
#endif
}

void SyncTest::BeforeSetupClient(int index,
                                 const base::FilePath& profile_path) {}

base::FilePath SyncTest::GetProfileBaseName(int index) {
  return base::FilePath(base::StringPrintf(
      FILE_PATH_LITERAL("SyncIntegrationTestClient%d"), index));
}

bool SyncTest::CreateProfile(int index) {
  base::FilePath profile_path;

// For Android, we don't create profile because Clank doesn't support
// multiple profiles.
#if !BUILDFLAG(IS_ANDROID)
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (server_type_ == EXTERNAL_LIVE_SERVER &&
      (num_clients_ > 1 || use_new_user_data_dir_)) {
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
    profile_path = user_data_dir.Append(GetProfileBaseName(index));
  }
#endif

  BeforeSetupClient(index, profile_path);

  if (server_type_ == EXTERNAL_LIVE_SERVER) {
    // If running against an EXTERNAL_LIVE_SERVER, we signin profiles using real
    // GAIA server. This requires creating profiles with no test hooks.
    InitializeProfile(index, MakeProfileForUISignin(profile_path));
  } else {
// Use default profile for Android.
#if BUILDFLAG(IS_ANDROID)
    DCHECK(index == 0);
    Profile* profile = ProfileManager::GetLastUsedProfile();
#else
    // Without need of real GAIA authentication, we create new test profiles.
    Profile* profile =
        g_browser_process->profile_manager()->GetProfile(profile_path);
#endif

    SetupMockGaiaResponsesForProfile(profile);
    InitializeProfile(index, profile);
  }

  // Once profile initialization has kicked off, wait for it to finish.
  WaitForDataModels(GetProfile(index));
  return true;
}

// TODO(shadi): Ideally creating a new profile should not depend on signin
// process. We should try to consolidate MakeProfileForUISignin() and
// MakeProfile(). Major differences are profile paths and creation methods. For
// UI signin we need profiles in unique user data dir's and we need to use
// ProfileManager::CreateProfileAsync() for proper profile creation.
// static
Profile* SyncTest::MakeProfileForUISignin(base::FilePath profile_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return &profiles::testing::CreateProfileSync(profile_manager, profile_path);
}

Profile* SyncTest::GetProfile(int index) const {
  DCHECK(!profiles_.empty()) << "SetupClients() has not yet been called.";
  DCHECK(index >= 0 && index < static_cast<int>(profiles_.size()))
      << "GetProfile(): Index is out of bounds: " << index;

  Profile* profile = profiles_[index];
  DCHECK(profile) << "No profile found at index: " << index;

  return profile;
}

std::vector<Profile*> SyncTest::GetAllProfiles() {
  std::vector<Profile*> profiles;
  if (UseVerifier()) {
    profiles.push_back(verifier());
  }
  for (int i = 0; i < num_clients(); ++i) {
    profiles.push_back(GetProfile(i));
  }
  return profiles;
}

#if !BUILDFLAG(IS_ANDROID)
Browser* SyncTest::GetBrowser(int index) {
  DCHECK(!browsers_.empty()) << "SetupClients() has not yet been called.";
  DCHECK(index >= 0 && index < static_cast<int>(browsers_.size()))
      << "GetBrowser(): Index is out of bounds: " << index;

  Browser* browser = browsers_[index];
  DCHECK(browser);

  return browser;
}

Browser* SyncTest::AddBrowser(int profile_index) {
  Profile* profile = GetProfile(profile_index);
  browsers_.push_back(Browser::Create(Browser::CreateParams(profile, true)));
  profiles_.push_back(profile);
  DCHECK_EQ(browsers_.size(), profiles_.size());

  return browsers_[browsers_.size() - 1];
}

void SyncTest::OnBrowserRemoved(Browser* browser) {
  for (size_t i = 0; i < browsers_.size(); ++i) {
    if (browsers_[i] == browser) {
      browsers_[i] = nullptr;
      break;
    }
  }
}
#endif

SyncServiceImplHarness* SyncTest::GetClient(int index) {
  return const_cast<SyncServiceImplHarness*>(
      std::as_const(*this).GetClient(index));
}

const SyncServiceImplHarness* SyncTest::GetClient(int index) const {
  if (clients_.empty()) {
    LOG(FATAL) << "SetupClients() has not yet been called.";
  }
  if (index < 0 || index >= static_cast<int>(clients_.size())) {
    LOG(FATAL) << "GetClient(): Index is out of bounds.";
  }
  return clients_[index].get();
}

std::vector<SyncServiceImplHarness*> SyncTest::GetSyncClients() {
  std::vector<SyncServiceImplHarness*> clients(clients_.size());
  for (size_t i = 0; i < clients_.size(); ++i) {
    clients[i] = clients_[i].get();
  }
  return clients;
}

SyncServiceImpl* SyncTest::GetSyncService(int index) const {
  return SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
      GetProfile(index));
}

syncer::UserSelectableTypeSet SyncTest::GetRegisteredSelectableTypes(
    int index) {
  return GetSyncService(index)
      ->GetUserSettings()
      ->GetRegisteredSelectableTypes();
}

std::vector<SyncServiceImpl*> SyncTest::GetSyncServices() {
  std::vector<SyncServiceImpl*> services;
  for (int i = 0; i < num_clients(); ++i) {
    services.push_back(GetSyncService(i));
  }
  return services;
}

Profile* SyncTest::verifier() {
  if (!UseVerifier()) {
    LOG(FATAL) << "Verifier account is disabled.";
  }
  if (verifier_ == nullptr) {
    LOG(FATAL) << "SetupClients() has not yet been called.";
  }
  return verifier_;
}

bool SyncTest::UseVerifier() {
  return false;
}

bool SyncTest::UseArcPackage() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ARC_PACKAGE do not support supervised users, switches::kSupervisedUserId
  // need to be set in SetUpCommandLine() when a test will use supervise users.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSupervisedUserId);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool SyncTest::SetupClients() {
  previous_profile_ =
      g_browser_process->profile_manager()->GetLastUsedProfile();

  base::ScopedAllowBlockingForTesting allow_blocking;
  if (num_clients_ <= 0) {
    LOG(FATAL) << "num_clients_ incorrectly initialized.";
  }
  bool has_any_browser = false;
#if !BUILDFLAG(IS_ANDROID)
  has_any_browser = !browsers_.empty();
#endif
  if (!profiles_.empty() || has_any_browser || !clients_.empty()) {
    LOG(FATAL) << "SetupClients() has already been called.";
  }

  // Create the required number of sync profiles, browsers and clients.
  profiles_.resize(num_clients_);
  clients_.resize(num_clients_);
  fake_server_invalidation_observers_.resize(num_clients_);

  auto* cl = base::CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(syncer::kSyncDeferredStartupTimeoutSeconds)) {
    cl->AppendSwitchASCII(syncer::kSyncDeferredStartupTimeoutSeconds, "0");
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (UseArcPackage()) {
    // Sets Arc flags, need to be called before create test profiles.
    ArcAppListPrefsFactory::SetFactoryForSyncTest();
  }

  // Uses a fake app list model updater to avoid interacting with Ash.
  model_updater_factory_ = std::make_unique<
      app_list::AppListSyncableService::ScopedModelUpdaterFactoryForTest>(
      base::BindRepeating(
          [](app_list::reorder::AppListReorderDelegate* reorder_delegate)
              -> std::unique_ptr<AppListModelUpdater> {
            return std::make_unique<FakeAppListModelUpdater>(
                /*profile=*/nullptr, reorder_delegate);
          }));
#endif

  for (int i = 0; i < num_clients_; ++i) {
    if (!CreateProfile(i)) {
      return false;
    }

    LOG(INFO) << "SyncTest::SetupClients() created profile " << i
              << "; elapsed time since construction: "
              << (base::Time::Now() - test_construction_time_);
  }

  // Verifier account is not useful when running against external servers.
  DCHECK(server_type_ != EXTERNAL_LIVE_SERVER || !UseVerifier());

// Verifier needs to create a test profile. But Clank doesn't support multiple
// profiles.
#if BUILDFLAG(IS_ANDROID)
  DCHECK(!UseVerifier());
#endif

  // Create the verifier profile.
  if (UseVerifier()) {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    verifier_ = g_browser_process->profile_manager()->GetProfile(
        user_data_dir.Append(FILE_PATH_LITERAL("Verifier")));
    WaitForDataModels(verifier());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ArcAppListPrefsFactory::IsFactorySetForSyncTest()) {
    // Init SyncArcPackageHelper to ensure that the arc services are initialized
    // for each Profile, only can be called after test profiles are created.
    if (!sync_arc_helper()) {
      return false;
    }
  }
#endif

  LOG(INFO)
      << "SyncTest::SetupClients() completed; elapsed time since construction: "
      << (base::Time::Now() - test_construction_time_);

  return true;
}

void SyncTest::InitializeProfile(int index, Profile* profile) {
  DCHECK(profile);
  profiles_[index] = profile;
  profile->AddObserver(this);

  SetUpInvalidations(index);
#if !BUILDFLAG(IS_ANDROID)
  browsers_.push_back(Browser::Create(Browser::CreateParams(profile, true)));
  DCHECK_EQ(static_cast<size_t>(index), browsers_.size() - 1);
#endif

  // Make sure the SyncServiceImpl has been created before creating the
  // SyncServiceImplHarness - some tests expect the SyncServiceImpl to
  // already exist.
  SyncServiceImpl* sync_service_impl =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          GetProfile(index));

  if (server_type_ == IN_PROCESS_FAKE_SERVER) {
    sync_service_impl->OverrideNetworkForTest(
        fake_server::CreateFakeServerHttpPostProviderFactory(
            GetFakeServer()->AsWeakPtr()));

    // Make sure that an instance of GCMProfileService has been created. This is
    // required for some tests which only call SetupClients().
    gcm::GCMProfileServiceFactory::GetForProfile(profile);
    DCHECK(base::Contains(profile_to_fake_gcm_driver_, profile));
    fake_server_sync_invalidation_sender_->AddFakeGCMDriver(
        profile_to_fake_gcm_driver_[profile]);
  }

  SyncServiceImplHarness::SigninType signin_type =
      server_type_ == EXTERNAL_LIVE_SERVER
          ? SyncServiceImplHarness::SigninType::UI_SIGNIN
          : SyncServiceImplHarness::SigninType::FAKE_SIGNIN;

  DCHECK(!clients_[index]);
  clients_[index] = SyncServiceImplHarness::Create(GetProfile(index), username_,
                                                   password_, signin_type);
  EXPECT_NE(nullptr, GetClient(index)) << "Could not create Client " << index;
}

void SyncTest::DisableNotificationsForClient(int index) {
  fake_server_->RemoveObserver(
      fake_server_invalidation_observers_[index].get());
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
      // real clients, those are stored upon subscription with the
      // per-user-topic server. The pref name is defined in
      // per_user_topic_subscription_manager.cc.
      ScopedDictPrefUpdate update(
          GetProfile(index)->GetPrefs(),
          "invalidation.per_sender_registered_for_invalidation");
      update->Set(kInvalidationGCMSenderId, base::Value::Dict());
      for (syncer::ModelType model_type :
           GetSyncService(index)->GetPreferredDataTypes()) {
        std::string notification_type;
        if (!RealModelTypeToNotificationType(model_type, &notification_type)) {
          continue;
        }
        update->FindDict(kInvalidationGCMSenderId)
            ->Set(notification_type,
                  "/private/" + notification_type + "-topic_server_user_id");
      }
      ScopedDictPrefUpdate update_client_id(
          GetProfile(index)->GetPrefs(),
          invalidation::prefs::kInvalidationClientIDCache);

      update_client_id->Set(kInvalidationGCMSenderId, client_id);
      break;
    }
  }
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
  if (server_type_ == EXTERNAL_LIVE_SERVER) {
    LOG(ERROR) << "WARNING: Running against external servers with an existing "
                  "account. If there is any pre-existing data in the account, "
                  "things will likely break.";
  }

  // Sync each of the profiles.
  for (int client_index = 0; client_index < num_clients_; client_index++) {
    SyncServiceImplHarness* client = GetClient(client_index);
    DVLOG(1) << "Setting up " << client_index << " client";
    ASSERT_TRUE(client->SetupSyncNoWaitForCompletion())
        << "SetupSync() failed.";

    if (TestUsesSelfNotifications()) {
      // On Android, invalidations for Session data type are disabled by
      // default. This may result in test flakiness when using when using
      // AwaitQuiescence() because Android commits Session for "about:blank"
      // page, hence AwaitQuiescence() would wait for downloading updates
      // forever.
      // TODO(crbug.com/1188034): remove this workaround once SetupSync doesn't
      // rely on self-notifications.
      DCHECK(GetSyncService(client_index)->IsEngineInitialized());
      GetSyncService(client_index)->SetInvalidationsForSessionsEnabled(true);
    }

    // It's important to wait for each client before setting up the next one,
    // otherwise multi-client tests get flaky.
    // TODO(crbug.com/956043): It would be nice to figure out why.
    switch (setup_mode) {
      case NO_WAITING:
        break;
      case WAIT_FOR_SYNC_SETUP_TO_COMPLETE:
        ASSERT_TRUE(client->AwaitSyncSetupCompletion());
        ASSERT_TRUE(client->AwaitInvalidationsStatus(/*expected_status=*/true));
        break;
      case WAIT_FOR_COMMITS_TO_COMPLETE:
        ASSERT_TRUE(client->AwaitSyncSetupCompletion());
        ASSERT_TRUE(client->AwaitInvalidationsStatus(/*expected_status=*/true));
        ASSERT_TRUE(WaitForAsyncChangesToBeCommitted(client_index));
        break;
    }
  }
}

void SyncTest::ClearProfiles() {
  // This method is called for only a live server, so it shouldn't use
  // FakeGCMDriver.
  DCHECK(profile_to_fake_gcm_driver_.empty());
  profiles_.clear();
  scoped_temp_dirs_.clear();
#if !BUILDFLAG(IS_ANDROID)
  browsers_.clear();
#endif
  clients_.clear();
}

bool SyncTest::SetupSync(SetupSyncMode setup_mode) {
#if BUILDFLAG(IS_ANDROID)
  // For Android, currently the framework only supports one client.
  // The client uses the default profile.
  DCHECK(num_clients_ == 1) << "For Android, currently it only supports "
                            << "one client.";
#endif

  base::ScopedAllowBlockingForTesting allow_blocking;

  SetupSyncInternal(setup_mode);

  // Because clients may modify sync data as part of startup (for example
  // local session-releated data is rewritten), we need to ensure all
  // startup-based changes have propagated between the clients.
  //
  // Tests that don't use self-notifications can't await quiescense.  They'll
  // have to find their own way of waiting for an initial state if they really
  // need such guarantees.
  if (setup_mode != NO_WAITING && TestUsesSelfNotifications()) {
    if (!AwaitQuiescence()) {
      LOG(FATAL) << "AwaitQuiescence() failed.";
      return false;
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  if (server_type_ == EXTERNAL_LIVE_SERVER) {
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
#endif

  DLOG(INFO) << "SyncTest::SetupSync() completed.";
  return true;
}

void SyncTest::TearDownOnMainThread() {
  // Verify that there are no data type failures after the test.
  for (size_t client_index = 0; client_index < clients_.size();
       ++client_index) {
    if (!GetClient(client_index)) {
      // This may happen if the last tab and hence a browser has been closed.
      continue;
    }
    CheckForDataTypeFailures(client_index);
  }

  // Workaround for https://crbug.com/801569: |prefs::kProfileLastUsed| stores
  // the profile path relative to the user dir, but our testing profiles are
  // outside the user dir (see CreateProfile). So code trying to access the last
  // used profile by path will fail. To work around that, set the last used
  // profile back to the originally created default profile (which does live in
  // the user data dir, and which we don't use otherwise).
  if (previous_profile_) {
    profiles::SetLastUsedProfile(previous_profile_->GetBaseName());
    previous_profile_ = nullptr;
  }

  if (fake_server_.get()) {
    for (const std::unique_ptr<fake_server::FakeServerInvalidationSender>&
             observer : fake_server_invalidation_observers_) {
      fake_server_->RemoveObserver(observer.get());
    }
    fake_server_sync_invalidation_sender_.reset();
    fake_server_.reset();
  }

  for (Profile* profile : profiles_) {
    // Profile could be removed earlier.
    if (profile) {
      profile->RemoveObserver(this);
    }
  }

  // Note: Closing all the browsers (see above) may destroy the Profiles, if
  // kDestroyProfileOnBrowserClose is enabled. So clear them out here, to make
  // sure they're not used anymore.
  profiles_.clear();
  clients_.clear();
  profile_to_fake_gcm_driver_.clear();
  // TODO(crbug.com/1260897): There are various other Profile-related members
  // around like profile_to_*_map_ - those should probably be cleaned up too.

#if !BUILDFLAG(IS_ANDROID)
  // Closing all browsers created by this test. The calls here block until
  // they are closed. Other browsers created outside SyncTest setup should be
  // closed by the creator of that browser.
  for (Browser* browser : browsers_) {
    if (browser) {
      CloseBrowserSynchronously(browser);
    }
  }
  browsers_.clear();
#endif

  PlatformBrowserTest::TearDownOnMainThread();
}

void SyncTest::SetUpInProcessBrowserTestFixture() {
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&SyncTest::OnWillCreateBrowserContextServices,
                                  base::Unretained(this)));
}

void SyncTest::OnProfileWillBeDestroyed(Profile* profile) {
  profile->RemoveObserver(this);

  for (size_t index = 0; index < profiles_.size(); ++index) {
    if (profiles_[index] != profile) {
      continue;
    }

    CheckForDataTypeFailures(/*client_index=*/index);

    // |profile_to_fake_gcm_driver_| may be empty when using an external server.
    if (base::Contains(profile_to_fake_gcm_driver_, profile)) {
      fake_server_sync_invalidation_sender_->RemoveFakeGCMDriver(
          profile_to_fake_gcm_driver_[profile]);
      profile_to_fake_gcm_driver_.erase(profile);
    }
    profiles_[index] = nullptr;
    clients_[index].reset();
#if !BUILDFLAG(IS_ANDROID)
    DCHECK(!browsers_[index]);
#endif  // !BUILDFLAG(IS_ANDROID)
  }
}

void SyncTest::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  if (server_type_ == EXTERNAL_LIVE_SERVER) {
    // DO NOTHING. External live sync servers use GCM to notify profiles of
    // any invalidations in sync'ed data. No need to provide a testing
    // factory for ProfileInvalidationProvider and SyncInvalidationsService.
    return;
  }
  invalidation::ProfileInvalidationProviderFactory::GetInstance()
      ->SetTestingFactory(
          context,
          base::BindRepeating(&SyncTest::CreateProfileInvalidationProvider,
                              &profile_to_fcm_network_handler_map_));
  gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&SyncTest::CreateGCMProfileService,
                                   base::Unretained(this)));
}

// static
std::unique_ptr<KeyedService> SyncTest::CreateProfileInvalidationProvider(
    std::map<const Profile*, invalidation::FCMNetworkHandler*>*
        profile_to_fcm_network_handler_map,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  gcm::GCMProfileService* gcm_profile_service =
      gcm::GCMProfileServiceFactory::GetForProfile(profile);
  instance_id::InstanceIDDriver* instance_id_driver =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
          ->driver();

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
          base::BindRepeating(&CreatePerUserTopicSubscriptionManager,
                              profile->GetPrefs()),
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

std::unique_ptr<KeyedService> SyncTest::CreateGCMProfileService(
    content::BrowserContext* context) {
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

  Profile* profile = Profile::FromBrowserContext(context);

  auto fake_gcm_driver =
      std::make_unique<FakeSyncGCMDriver>(profile, blocking_task_runner);
  profile_to_fake_gcm_driver_[profile] = fake_gcm_driver.get();
  fake_gcm_driver->WaitForAppIdBeforeConnection(
      fake_server::FakeServerSyncInvalidationSender::kSyncInvalidationsAppId);
  return std::make_unique<gcm::FakeGCMProfileService>(
      std::move(fake_gcm_driver));
}

void SyncTest::ResetSyncForPrimaryAccount() {
  if (server_type_ == EXTERNAL_LIVE_SERVER) {
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
    // SyncServiceImplHarness::ResetSyncForPrimaryAccount() can succeed.
    // The passphrase will be reset together with the rest of the sync data
    // clearing.
    ASSERT_TRUE(SetupSync(NO_WAITING));
    GetClient(0)->ResetSyncForPrimaryAccount();
    // After reset account, the client should get a NOT_MY_BIRTHDAY error
    // and disable sync. Adding a wait to make sure this is propagated.
    ASSERT_TRUE(SyncDisabledChecker(GetSyncService(0)).Wait());

#if !BUILDFLAG(IS_ANDROID)
    if (browsers_[0]) {
      CloseBrowserSynchronously(browsers_[0]);
    }
#endif

    // After reset, this client will disable sync. It may log some messages
    // that do not contribute to test failures. It includes:
    //   PostClientToServerMessage with SERVER_RETURN_NOT_MY_BIRTHDAY
    //   PostClientToServerMessage with NETWORK_CONNECTION_UNAVAILABLE
    //   mcs_client fails with 401.
    LOG(WARNING) << "Finished reset account. Warning logs before "
                 << "this log may be safe to ignore.";
    ClearProfiles();
    use_new_user_data_dir_ = old_use_new_user_data_dir;
    num_clients_ = old_num_clients;
  }
}

void SyncTest::SetUpOnMainThread() {
  switch (server_type_) {
    case EXTERNAL_LIVE_SERVER: {
      // Allows google.com as well as country-specific TLDs.
      host_resolver()->AllowDirectLookup("*.google.com");
      host_resolver()->AllowDirectLookup("accounts.google.*");
      host_resolver()->AllowDirectLookup("*.googleusercontent.com");
      // Allow connection to googleapis.com for oauth token requests in E2E
      // tests.
      host_resolver()->AllowDirectLookup("*.googleapis.com");

      // On Linux, we use Chromium's NSS implementation which uses the following
      // hosts for certificate verification. Without these overrides, running
      // the integration tests on Linux causes error as we make external DNS
      // lookups.
      host_resolver()->AllowDirectLookup("*.thawte.com");
      host_resolver()->AllowDirectLookup("*.geotrust.com");
      host_resolver()->AllowDirectLookup("*.gstatic.com");
      break;
    }
    case IN_PROCESS_FAKE_SERVER: {
      // Start up a sync test server and setup mock gaia responses.
      // Note: This must be done prior to the call to SetupClients() because we
      // want the mock gaia responses to be available before GaiaUrls is
      // initialized.
      base::FilePath user_data_dir;
      base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
      fake_server_ = std::make_unique<fake_server::FakeServer>(user_data_dir);
      fake_server_sync_invalidation_sender_ =
          std::make_unique<fake_server::FakeServerSyncInvalidationSender>(
              fake_server_.get());

      SetupMockGaiaResponses();
      SetupMockGaiaResponsesForProfile(
          ProfileManager::GetLastUsedProfileIfLoaded());
    }
  }
}

void SyncTest::WaitForDataModels(Profile* profile) {
  // Ideally the waiting for bookmarks should be done exclusively for
  // bookmark-related tests, but there are several tests that use bookmarks as
  // a way to generally check if sync is working, although the test is not
  // really about bookmarks.
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(profile));
}

void SyncTest::ReadPasswordFile() {
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  password_file_ = cl->GetSwitchValuePath(switches::kPasswordFileForTest);
  if (password_file_.empty()) {
    LOG(FATAL) << "Can't run live server test without specifying --"
               << switches::kPasswordFileForTest << "=<filename>";
  }
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
                                      net::Error net_error) {
  network::URLLoaderCompletionStatus completion_status(net_error);
  completion_status.decoded_body_length = response_data.size();

  std::string response = base::StringPrintf("HTTP/1.1 %d %s\r\n", status_code,
                                            GetHttpReasonPhrase(status_code));
  mojo::StructPtr<network::mojom::URLResponseHead> response_head =
      network::mojom::URLResponseHead::New();
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

bool SyncTest::TestUsesSelfNotifications() {
  // Default is True unless we are running against external servers.
  return server_type_ != EXTERNAL_LIVE_SERVER;
}

bool SyncTest::AwaitQuiescence() {
  return SyncServiceImplHarness::AwaitQuiescence(GetSyncClients());
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

arc::SyncArcPackageHelper* SyncTest::sync_arc_helper() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return arc::SyncArcPackageHelper::GetInstance();
#else
  return nullptr;
#endif
}

std::string SyncTest::GetCacheGuid(size_t profile_index) const {
  syncer::SyncTransportDataPrefs prefs(GetProfile(profile_index)->GetPrefs());
  return prefs.GetCacheGuid();
}

bool SyncTest::WaitForAsyncChangesToBeCommitted(size_t profile_index) const {
  // This is a workaround for E2E tests because currently there is no a good way
  // to wait for asynchronous commits to the external servers. Although
  // CommittedAllNudgedChangesChecker will wait for all the local changes to be
  // committed, it doesn't cover all the cases.
  if (server_type_ != EXTERNAL_LIVE_SERVER) {
    // Wait for committing DeviceInfo with sharing_fields, it may happen
    // asynchronously due to FCM token registration.
    if (GetSyncService(profile_index)
            ->GetPreferredDataTypes()
            .Has(syncer::SHARING_MESSAGE)) {
      if (!device_info_helper::WaitForFullDeviceInfoCommitted(
              GetCacheGuid(profile_index))) {
        return false;
      }
    }

#if BUILDFLAG(IS_ANDROID)
    // On Android, default about:blank page is loaded by default. Wait for
    // Session to be committed to prevent unexpected commit requests during
    // test. It shouldn't be called when custom passphrase is enabled because
    // SessionHierarchyMatchChecker doesn't support custom passphrases.
    if (!SessionHierarchyMatchChecker(
             fake_server::SessionsHierarchy({{url::kAboutBlankURL}}),
             GetSyncService(profile_index), GetFakeServer())
             .Wait()) {
      return false;
    }
#endif  // BUILDFLAG(IS_ANDROID)
  }

  // Wait for any other locally nudged changes to be committed.
  if (!CommittedAllNudgedChangesChecker(GetSyncService(profile_index)).Wait()) {
    return false;
  }

  return true;
}

void SyncTest::CheckForDataTypeFailures(size_t client_index) const {
  DCHECK(GetClient(client_index));

  auto* service = GetClient(client_index)->service();
  syncer::ModelTypeSet types_to_check =
      service->GetRegisteredDataTypesForTest();
  types_to_check.RemoveAll(excluded_types_from_check_for_data_type_failures_);

  if (service->HasAnyDatatypeErrorForTest(types_to_check)) {
    ADD_FAILURE() << "Data types failed during tests: "
                  << GetClient(client_index)
                         ->service()
                         ->GetTypeStatusMapForDebugging()
                         .DebugString();
  }
}

void SyncTest::ExcludeDataTypesFromCheckForDataTypeFailures(
    syncer::ModelTypeSet types) {
  excluded_types_from_check_for_data_type_failures_ = types;
}
