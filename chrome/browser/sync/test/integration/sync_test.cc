// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
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
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/sync_protocol_error.h"
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
#include "components/trusted_vault/command_line_switches.h"
#endif  // BUILDFLAG(IS_ANDROID)

using syncer::SyncServiceImpl;

namespace {

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

  std::vector<base::test::FeatureRefAndParams> enabled_features;
  if (num_clients_ > 1) {
    // Workaround to turn off single client optimization for sync standalone
    // invalidations in tests.
    // TODO(crbug.com/40908214): Remove once resolved.
    enabled_features.push_back(
        {switches::kSyncFilterOutInactiveDevicesForSingleClient,
         {{switches::kSyncActiveDeviceMargin.name, "-2d"}}});
  }
  if (!enabled_features.empty()) {
    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

#if !BUILDFLAG(IS_ANDROID)
  browser_list_observer_ = std::make_unique<ClosedBrowserObserver>(
      base::BindRepeating(&SyncTest::OnBrowserRemoved, base::Unretained(this)));
#endif
}

SyncTest::~SyncTest() = default;

void SyncTest::SetUp() {
#if BUILDFLAG(IS_ANDROID)
  if (server_type_ == IN_PROCESS_FAKE_SERVER) {
    sync_test_utils_android::SetUpFakeAuthForTesting();
  }
#endif

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kPasswordFileForTest)) {
    ReadPasswordFile();
  } else {
    // Decide on username to use or create one.
    if (cl->HasSwitch(switches::kSyncUserForTest)) {
      username_ = cl->GetSwitchValueASCII(switches::kSyncUserForTest);
    } else if (server_type_ != EXTERNAL_LIVE_SERVER) {
      username_ = kDefaultUserEmail;
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
  // TODO(crbug.com/368091420): Consider moving into SyncSigninDelegateAndroid.
  switch (server_type_) {
    case EXTERNAL_LIVE_SERVER:
      sync_test_utils_android::ShutdownLiveAuthForTesting();
      break;
    case IN_PROCESS_FAKE_SERVER:
      sync_test_utils_android::TearDownFakeAuthForTesting();
      break;
  }
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

  if (!cl->HasSwitch(
          switches::kBypassAccountAlreadyUsedByAnotherProfileCheck)) {
    cl->AppendSwitch(switches::kBypassAccountAlreadyUsedByAnotherProfileCheck);
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
  return base::FilePath::FromASCII("SyncIntegrationTestClient" +
                                   base::NumberToString(index));
}

bool SyncTest::CreateProfile(int index) {
  base::FilePath profile_path;

// For Android, we don't create profile because Clank doesn't support
// multiple profiles.
#if !BUILDFLAG(IS_ANDROID)
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

  // Instead of creating a new directory, use a deterministic name such that
  // PRE_ tests (i.e. tests that span browser restarts) can reuse the same
  // directory and carry over state.
  base::FilePath base_name = GetProfileBaseName(index);
  if (use_new_user_data_dir_) {
    base_name = base::FilePath::FromASCII(
        "SyncIntegrationTestClientForClearServerData");
  }
  profile_path = user_data_dir.Append(base_name);
#endif

  BeforeSetupClient(index, profile_path);

#if BUILDFLAG(IS_ANDROID)
  DCHECK_EQ(index, 0);
  Profile* profile = ProfileManager::GetLastUsedProfile();
#else   // BUILDFLAG(IS_ANDROID)
  Profile* profile =
      g_browser_process->profile_manager()->GetProfile(profile_path);

  if (server_type_ != EXTERNAL_LIVE_SERVER) {
    SetupMockGaiaResponsesForProfile(profile);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  InitializeProfile(index, profile);

  // Once profile initialization has kicked off, wait for it to finish.
  WaitForDataModels(GetProfile(index));
  return true;
}

Profile* SyncTest::GetProfile(int index) const {
  DCHECK(!profiles_.empty()) << "SetupClients() has not yet been called.";
  DCHECK(index >= 0 && index < static_cast<int>(profiles_.size()))
      << "GetProfile(): Index is out of bounds: " << index;

  Profile* profile = profiles_[index];
  DCHECK(profile) << "No profile found at index: " << index;

  return profile;
}

std::vector<raw_ptr<Profile, VectorExperimental>> SyncTest::GetAllProfiles() {
  std::vector<raw_ptr<Profile, VectorExperimental>> profiles;
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
  return base::ToVector(clients_,
                        &std::unique_ptr<SyncServiceImplHarness>::get);
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

std::vector<raw_ptr<SyncServiceImpl, VectorExperimental>>
SyncTest::GetSyncServices() {
  std::vector<raw_ptr<SyncServiceImpl, VectorExperimental>> services;
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

  auto* cl = base::CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(syncer::kSyncDeferredStartupTimeoutSeconds)) {
    cl->AppendSwitchASCII(syncer::kSyncDeferredStartupTimeoutSeconds, "0");
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Sets Arc flags, need to be called before create test profiles.
  ArcAppListPrefsFactory::SetFactoryForSyncTest();

  // Uses a fake app list model updater to avoid interacting with Ash.
  model_updater_factory_scope_ =
      app_list::AppListSyncableService::SetScopedModelUpdaterFactoryForTest(
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
  CHECK(profile);
  CHECK(!profiles_[index]) << " for index " << index;

  profiles_[index] = profile;
  profile->AddObserver(this);

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
    DCHECK(profile_to_fake_gcm_driver_.contains(profile));
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

void SyncTest::SetupMockGaiaResponsesForProfile(Profile* profile) {
  SetURLLoaderFactoryForTest(profile,
                             test_url_loader_factory_.GetSafeWeakWrapper());
}

void SyncTest::SetupSyncInternal(SetupSyncMode setup_mode) {
  // Create sync profiles and clients if they haven't already been created.
  if (profiles_.empty()) {
    if (!SetupClients()) {
      LOG(FATAL) << "SetupClients() failed.";
    }
  }

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
      // TODO(crbug.com/40173160): remove this workaround once SetupSync doesn't
      // rely on self-notifications.
      DCHECK(GetSyncService(client_index)->IsEngineInitialized());
      GetSyncService(client_index)->SetInvalidationsForSessionsEnabled(true);
    }

    // It's important to wait for each client before setting up the next one,
    // otherwise multi-client tests get flaky. This may happen in some tests
    // which have local data before sync is enabled. In such tests it's
    // important (and this is closer to real behavior) that the initial merge is
    // happening sequentially in two clients, otherwise both clients can upload
    // their data simultaneously, e.g. resulting in duplicates (most prominent
    // for bookmarks).
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

    LOG(INFO) << "SetupSync for client " << client_index << " finished, "
              << "cache guid: " << GetCacheGuid(client_index);
  }
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
  // local session-related data is rewritten), we need to ensure all
  // startup-based changes have propagated between the clients.
  //
  // Tests that don't use self-notifications can't await quiescence.  They'll
  // have to find their own way of waiting for an initial state if they really
  // need such guarantees.
  if (setup_mode != NO_WAITING && TestUsesSelfNotifications()) {
    if (!AwaitQuiescence()) {
      LOG(FATAL) << "AwaitQuiescence() failed.";
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
    fake_server_sync_invalidation_sender_.reset();
    fake_server_.reset();
  }

  for (size_t index = 0; index < profiles_.size(); ++index) {
    // Profile could be removed earlier.
    if (profiles_[index]) {
      profiles_[index]->RemoveObserver(this);

#if BUILDFLAG(IS_ANDROID)
      if (server_type_ == EXTERNAL_LIVE_SERVER) {
        // A profile could have backend tasks from the associate sync engine.
        // In browser tests, on non-Android platforms, these tasks are cancelled
        // during the browser process shutdown.
        // On Android, however, browser process is not shutdown after test run.
        // As a result, these backend tasks could keep running and cause timeout
        // error during test shutdown.
        // To fix this issue, we explicitly mimic a dashboard reset to cancel
        // any ongoing sync engine's backend tasks.
        GetSyncService(index)->OnActionableProtocolError(
            {.error_type = syncer::NOT_MY_BIRTHDAY,
             .action = syncer::DISABLE_SYNC_ON_CLIENT});
      }
#endif  // BUILDFLAG(IS_ANDROID)

    }
  }

  // Note: Closing all the browsers (see above) may destroy the Profiles, if
  // kDestroyProfileOnBrowserClose is enabled. So clear them out here, to make
  // sure they're not used anymore.
  profiles_.clear();
  clients_.clear();
  profile_to_fake_gcm_driver_.clear();
  // TODO(crbug.com/40798524): There are various other Profile-related members
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
    if (profile_to_fake_gcm_driver_.contains(profile)) {
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
  gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&SyncTest::CreateGCMProfileService,
                                   base::Unretained(this)));
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
  if (server_type_ != EXTERNAL_LIVE_SERVER) {
    // No-op for anything other than when external servers are used.
    return;
  }

  // FakeGCMDriver isn't used in combination with external servers.
  CHECK(profile_to_fake_gcm_driver_.empty());

  base::ScopedAllowBlockingForTesting allow_blocking;
  // For external server testing, we need to have a clean account. The following
  // code will sign in one chrome browser, get the client id and access token,
  // then clean the server data.
  base::AutoReset<bool> scoped_user_new_user_data_dir(&use_new_user_data_dir_,
                                                      true);
  base::AutoReset<int> scoped_num_clients(&num_clients_, 1);
  // Do not wait for sync complete. Some tests set passphrase and sync will
  // fail. NO_WAITING mode gives access token and birthday so
  // SyncServiceImplHarness::ResetSyncForPrimaryAccount() can succeed. The
  // passphrase will be reset together with the rest of the sync data clearing.
  ASSERT_TRUE(SetupSync(NO_WAITING));
  GetClient(0)->ResetSyncForPrimaryAccount();
  // After reset account, the client should get a NOT_MY_BIRTHDAY error and
  // disable sync. Adding a wait to make sure this is propagated.
  ASSERT_TRUE(SyncDisabledChecker(GetSyncService(0)).Wait());

#if !BUILDFLAG(IS_ANDROID)
  if (browsers_[0]) {
    CloseBrowserSynchronously(browsers_[0]);
  }
#endif

  // After reset, this client will disable sync. It may log some messages that
  // do not contribute to test failures. It includes:
  //   PostClientToServerMessage with SERVER_RETURN_NOT_MY_BIRTHDAY
  //   PostClientToServerMessage with NETWORK_CONNECTION_UNAVAILABLE
  //   mcs_client fails with 401.
  LOG(WARNING) << "Finished reset account. Warning logs before "
               << "this log may be safe to ignore.";

  CHECK_EQ(1u, profiles_.size());
  CHECK(profiles_[0]);
  profiles_[0]->RemoveObserver(this);
  profiles_.clear();

  scoped_temp_dirs_.clear();
#if !BUILDFLAG(IS_ANDROID)
  browsers_.clear();
#endif
  clients_.clear();
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
      fake_server_ = std::make_unique<fake_server::FakeServer>(
          user_data_dir.AppendASCII("FakeServer"));
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

void SyncTest::TriggerMigrationDoneError(syncer::DataTypeSet data_types) {
  ASSERT_TRUE(server_type_ == IN_PROCESS_FAKE_SERVER);
  fake_server_->TriggerMigrationDoneError(data_types);
}

fake_server::FakeServer* SyncTest::GetFakeServer() const {
  return fake_server_.get();
}

void SyncTest::TriggerSyncForDataTypes(int index,
                                       syncer::DataTypeSet data_types) {
  GetSyncService(index)->TriggerRefresh(data_types);
}

arc::SyncArcPackageHelper* SyncTest::sync_arc_helper() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return arc::SyncArcPackageHelper::GetInstance();
#else
  return nullptr;
#endif
}

std::string SyncTest::GetCacheGuid(size_t profile_index) const {
  syncer::SyncTransportDataPrefs prefs(
      GetProfile(profile_index)->GetPrefs(),
      GetClient(profile_index)->GetGaiaIdHashForPrimaryAccount());
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
  syncer::DataTypeSet types_to_check = service->GetRegisteredDataTypesForTest();
  types_to_check.RemoveAll(excluded_types_from_check_for_data_type_failures_);

  ASSERT_FALSE(service->HasAnyModelErrorForTest(types_to_check))
      << " for client " << client_index << " and types "
      << syncer::DataTypeSetToDebugString(types_to_check);
}

void SyncTest::ExcludeDataTypesFromCheckForDataTypeFailures(
    syncer::DataTypeSet types) {
  excluded_types_from_check_for_data_type_failures_ = types;
}

// The set of types that *can* run in transport mode. Doesn't mean they are all
// enabled by default, e.g. HISTORY requires a dedicated opt-in via
// SyncUserSettings::SetSelectedTypes().
syncer::DataTypeSet AllowedTypesInStandaloneTransportMode() {
  static_assert(53 == syncer::GetNumDataTypes(),
                "Add new types below if they can run in transport mode");
  // Only some types will run by default in transport mode (i.e. without their
  // own separate opt-in).
  syncer::DataTypeSet allowed_types = {syncer::AUTOFILL_WALLET_CREDENTIAL,
                                       syncer::AUTOFILL_WALLET_DATA,
                                       syncer::AUTOFILL_WALLET_USAGE,
                                       syncer::DEVICE_INFO,
                                       syncer::SECURITY_EVENTS,
                                       syncer::SEND_TAB_TO_SELF,
                                       syncer::SHARING_MESSAGE,
                                       syncer::USER_CONSENTS};
  allowed_types.PutAll(syncer::ControlTypes());

  if (base::FeatureList::IsEnabled(
          syncer::kSyncEnableContactInfoDataTypeInTransportMode)) {
    allowed_types.Put(syncer::CONTACT_INFO);
  }

  allowed_types.Put(syncer::PLUS_ADDRESS);
  if (base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting)) {
    allowed_types.Put(syncer::PLUS_ADDRESS_SETTING);
  }

  if (base::FeatureList::IsEnabled(
          syncer::kSyncEnableWalletMetadataInTransportMode)) {
    allowed_types.Put(syncer::AUTOFILL_WALLET_METADATA);
  }
  if (base::FeatureList::IsEnabled(
          syncer::kSyncEnableWalletOfferInTransportMode)) {
    allowed_types.Put(syncer::AUTOFILL_WALLET_OFFER);
  }

  bool allow_passwords = base::FeatureList::IsEnabled(
      syncer::kEnablePasswordsAccountStorageForNonSyncingUsers);
#if !BUILDFLAG(IS_ANDROID)
  // This is an approximation because passwords are only enabled if the signin
  // is explicit (they are not enabled for users who signed in through Dice).
  allow_passwords &= switches::IsExplicitBrowserSigninUIOnDesktopEnabled();
#endif

  if (allow_passwords) {
    allowed_types.Put(syncer::PASSWORDS);
    allowed_types.Put(syncer::WEBAUTHN_CREDENTIAL);
    allowed_types.Put(syncer::INCOMING_PASSWORD_SHARING_INVITATION);
    allowed_types.Put(syncer::OUTGOING_PASSWORD_SHARING_INVITATION);
  }
  if (base::FeatureList::IsEnabled(syncer::kEnablePreferencesAccountStorage) &&
      base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    allowed_types.Put(syncer::PREFERENCES);
    allowed_types.Put(syncer::PRIORITY_PREFERENCES);
  }
  if (base::FeatureList::IsEnabled(
          syncer::kSyncEnableBookmarksInTransportMode)) {
    allowed_types.Put(syncer::BOOKMARKS);
  }
  if (base::FeatureList::IsEnabled(
          syncer::kReadingListEnableSyncTransportModeUponSignIn)) {
    allowed_types.Put(syncer::READING_LIST);
  }
  if (base::FeatureList::IsEnabled(
          syncer::kSyncSharedTabGroupDataInTransportMode)) {
    allowed_types.Put(syncer::COLLABORATION_GROUP);
    allowed_types.Put(syncer::SHARED_TAB_GROUP_DATA);
  }
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    allowed_types.Put(syncer::HISTORY);
    allowed_types.Put(syncer::SESSIONS);
    allowed_types.Put(syncer::PRODUCT_COMPARISON);
    allowed_types.Put(syncer::SAVED_TAB_GROUP);
  }
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    allowed_types.Put(syncer::WEB_APKS);
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On Lacros, Apps-related types may run in transport mode.
  allowed_types.PutAll({syncer::APPS, syncer::APP_SETTINGS, syncer::WEB_APPS});
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // OS sync types run in transport mode.
  allowed_types.PutAll({syncer::APP_LIST, syncer::ARC_PACKAGE,
                        syncer::OS_PREFERENCES, syncer::OS_PRIORITY_PREFERENCES,
                        syncer::PRINTERS,
                        syncer::PRINTERS_AUTHORIZATION_SERVERS,
                        syncer::WIFI_CONFIGURATIONS, syncer::WORKSPACE_DESK});
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return allowed_types;
}
