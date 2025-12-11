// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_test.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
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
#include "components/commerce/core/commerce_feature_list.h"
#include "components/data_sharing/public/features.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/plus_addresses/core/common/features.h"
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
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/fake_oauth2_token_response.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/port_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/sync/test/integration/sync_arc_package_helper.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/experiences/arc/test/arc_util_test_support.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

using syncer::SyncServiceImpl;

namespace {

// A small ChromeBrowserMainExtraParts that invokes a callback when threads are
// ready.
class ChromeBrowserMainExtraPartsThreadNotifier final
    : public ChromeBrowserMainExtraParts {
 public:
  explicit ChromeBrowserMainExtraPartsThreadNotifier(
      base::OnceClosure threads_ready_closure)
      : threads_ready_closure_(std::move(threads_ready_closure)) {}

  // ChromeBrowserMainExtraParts:
  void PostCreateThreads() final { std::move(threads_ready_closure_).Run(); }

 private:
  base::OnceClosure threads_ready_closure_;
};

int GetNumClients(SyncTest::TestType test_type) {
  switch (test_type) {
    case SyncTest::SINGLE_CLIENT:
      return 1;
    case SyncTest::TWO_CLIENT:
      return 2;
  }
  NOTREACHED();
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
      num_clients_(GetNumClients(test_type_)),
      sync_run_loop_timeout(FROM_HERE, TestTimeouts::action_max_timeout()),
      previous_profile_(nullptr) {
  // Any RunLoop timeout will by default result in test failure.
  sync_run_loop_timeout.SetAddGTestFailureOnTimeout();

  sync_datatype_helper::AssociateWithTest(this);

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

void SyncTest::CreatedBrowserMainParts(content::BrowserMainParts* parts) {
  static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
      std::make_unique<ChromeBrowserMainExtraPartsThreadNotifier>(
          base::BindOnce(&SyncTest::PostCreateThreads,
                         weak_ptr_factory_.GetWeakPtr())));
  PlatformBrowserTest::CreatedBrowserMainParts(parts);
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

  if (server_type_ == EXTERNAL_LIVE_SERVER &&
      !cl->HasSwitch(switches::kDisableSyncInvalidationOptimizations)) {
    // This flag is required because multiple devices in tests become active at
    // the same time, and they may populate a single client optimization flag
    // incorrectly resulting in missed invalidations.
    cl->AppendSwitch(switches::kDisableSyncInvalidationOptimizations);
  }

#if BUILDFLAG(IS_CHROMEOS)
  cl->AppendSwitch(ash::switches::kIgnoreUserProfileMappingForTests);
  cl->AppendSwitch(ash::switches::kDisableArcOptInVerification);
  arc::SetArcAvailableCommandLineForTesting(cl);
#endif
}

void SyncTest::BeforeSetupClient(int index,
                                 const base::FilePath& profile_path) {}

base::FilePath SyncTest::GetProfileBaseName(int index) {
  return base::FilePath::FromASCII("SyncIntegrationTestClient" +
                                   base::NumberToString(index));
}

void SyncTest::PostCreateThreads() {
  CHECK(g_browser_process);
  CHECK(g_browser_process->profile_manager());
  profile_manager_observation_.Observe(g_browser_process->profile_manager());

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
      // Note: This must be done prior to the call to SetUpOnMainThread()
      // because PlatformBrowserTest creates a default profile early, shortly
      // after the threadpool is initialized.
      base::FilePath user_data_dir;
      base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
      fake_server_ = std::make_unique<fake_server::FakeServer>(
          user_data_dir.AppendASCII("FakeServer"));
      fake_server_sync_invalidation_sender_ =
          std::make_unique<fake_server::FakeServerSyncInvalidationSender>(
              fake_server_.get());

      SetupMockGaiaResponses();
      break;
    }
  }
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
  profile_path = user_data_dir.Append(GetProfileBaseName(index));
#endif

  BeforeSetupClient(index, profile_path);

#if BUILDFLAG(IS_ANDROID)
  DCHECK_EQ(index, 0);
  Profile* profile = ProfileManager::GetLastUsedProfile();
#else   // BUILDFLAG(IS_ANDROID)
  Profile* profile = nullptr;
#if BUILDFLAG(IS_CHROMEOS)
  if (use_primary_user_profile_) {
    CHECK_EQ(index, 0);
    profile = Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(
            user_manager::UserManager::Get()->GetPrimaryUser()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  if (!profile) {
    profile = g_browser_process->profile_manager()->GetProfile(profile_path);
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

#if BUILDFLAG(IS_CHROMEOS)
void SyncTest::SetUsePrimaryUserProfile(bool value) {
  // Must be called early enough.
  CHECK(profiles_.empty());
  use_primary_user_profile_ = true;
}
#endif

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

SyncTest::SetupSyncMode SyncTest::GetSetupSyncMode() const {
  return SetupSyncMode::kSyncTheFeature;
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

#if BUILDFLAG(IS_CHROMEOS)
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

#if BUILDFLAG(IS_CHROMEOS)
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

  if (server_type_ == IN_PROCESS_FAKE_SERVER) {
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
  clients_[index] =
      SyncServiceImplHarness::Create(GetProfile(index), signin_type);
  EXPECT_NE(nullptr, GetClient(index)) << "Could not create Client " << index;
}

bool SyncTest::SetupSyncInternal(SetupSyncMode setup_mode,
                                 SyncWaitCondition wait_condition,
                                 SyncTestAccount account) {
  // Create sync profiles and clients if they haven't already been created.
  if (profiles_.empty()) {
    if (!SetupClients()) {
      ADD_FAILURE() << "SetupClients() failed.";
      return false;
    }
  }

  if (server_type_ == EXTERNAL_LIVE_SERVER) {
    LOG(ERROR) << "WARNING: Running against external servers with an existing "
                  "account. If there is any pre-existing data in the account, "
                  "things will likely break.";
  }

  // Sync each of the profiles.
  for (int client_index = 0; client_index < num_clients_; client_index++) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    auto resetter =
        enterprise_util::DisableAutomaticManagementDisclaimerUntilReset(
            GetProfile(client_index));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    SyncServiceImplHarness* client = GetClient(client_index);
    DVLOG(1) << "Setting up " << client_index << " client";

    if (setup_mode == SetupSyncMode::kSyncTransportOnly) {
      if (!client->SignInPrimaryAccount(account) ||
          !client->AwaitEngineInitialization() ||
          !client->EnableHistorySyncNoWaitForCompletion()) {
        ADD_FAILURE() << "SetupSync() failed.";
        return false;
      }
    } else {
      if (!client->SetupSyncNoWaitForCompletion(account)) {
        ADD_FAILURE() << "SetupSync() failed.";
        return false;
      }
    }

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
    switch (wait_condition) {
      case NO_WAITING:
        break;
      case WAIT_FOR_SYNC_SETUP_TO_COMPLETE:
        if (!client->AwaitSyncTransportActive()) {
          ADD_FAILURE() << "AwaitSyncTransportActive() failed";
          return false;
        }
        if (!client->AwaitInvalidationsStatus(/*expected_status=*/true)) {
          ADD_FAILURE() << "AwaitInvalidationsStatus() failed";
          return false;
        }
        break;
      case WAIT_FOR_COMMITS_TO_COMPLETE:
        if (!client->AwaitSyncTransportActive()) {
          ADD_FAILURE() << "AwaitSyncTransportActive() failed";
          return false;
        }
        if (!client->AwaitInvalidationsStatus(/*expected_status=*/true)) {
          ADD_FAILURE() << "AwaitInvalidationsStatus() failed";
          return false;
        }
        if (!WaitForAsyncChangesToBeCommitted(client_index)) {
          ADD_FAILURE() << "WaitForAsyncChangesToBeCommitted() failed";
          return false;
        }
        break;
    }

    LOG(INFO) << "SetupSync for client " << client_index << " finished, "
              << "cache guid: " << GetCacheGuid(client_index);
  }

  return true;
}

bool SyncTest::SetupSync(SyncWaitCondition wait_condition) {
  return SetupSync(SyncTestAccount::kDefaultAccount, wait_condition);
}

bool SyncTest::SetupSync(SyncTestAccount account,
                         SyncWaitCondition wait_condition) {
  return SetupSyncWithMode(GetSetupSyncMode(), wait_condition, account);
}

bool SyncTest::SetupSyncWithMode(SetupSyncMode setup_mode,
                                 SyncWaitCondition wait_condition,
                                 SyncTestAccount account) {
#if BUILDFLAG(IS_ANDROID)
  // For Android, currently the framework only supports one client.
  // The client uses the default profile.
  CHECK(num_clients_ == 1)
      << "For Android, currently it only supports one client.";
#endif

  base::ScopedAllowBlockingForTesting allow_blocking;

  if (!SetupSyncInternal(setup_mode, wait_condition, account)) {
    return false;
  }

  // Because clients may modify sync data as part of startup (for example
  // local session-related data is rewritten), we need to ensure all
  // startup-based changes have propagated between the clients.
  //
  // Tests that don't use self-notifications can't await quiescence.  They'll
  // have to find their own way of waiting for an initial state if they really
  // need such guarantees.
  if (wait_condition != NO_WAITING && TestUsesSelfNotifications()) {
    if (!AwaitQuiescence()) {
      ADD_FAILURE() << "AwaitQuiescence() failed.";
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

  clients_.clear();
  // Note: Closing all the browsers (see above) may destroy the Profiles, if
  // kDestroyProfileOnBrowserClose is enabled. So clear them out here, to make
  // sure they're not used anymore.
  profiles_.clear();
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

void SyncTest::OnProfileAdded(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  // This cannot run in OnProfileCreationStarted() because it would be too
  // early, and ProfileImpl's constructor would override it once again when
  // invoking ash::InitializeAccountManager().
  if (server_type_ == IN_PROCESS_FAKE_SERVER) {
    ash::AccountManagerFactory* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    account_manager::AccountManager* account_manager =
        factory->GetAccountManager(profile->GetPath().value());
    account_manager->SetUrlLoaderFactoryForTests(
        test_url_loader_factory_.GetSafeWeakWrapper());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void SyncTest::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void SyncTest::OnProfileCreationStarted(Profile* profile) {
  if (server_type_ == EXTERNAL_LIVE_SERVER) {
    // DO NOTHING. External live sync servers use real factories without quirks
    // or overrides.
    return;
  }

  CHECK(GetFakeServer());

  gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&SyncTest::CreateGCMProfileService,
                                   base::Unretained(this)));
  SyncServiceFactory::GetInstance()->SetTestingFactory(
      profile, SyncServiceFactory::GetDefaultFactory(
                   fake_server::CreateFakeServerHttpPostProviderFactory(
                       GetFakeServer()->AsWeakPtr())));
  ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                   &test_url_loader_factory_));
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

bool SyncTest::ResetSyncForPrimaryAccount() {
  if (server_type_ != EXTERNAL_LIVE_SERVER) {
    // No-op for anything other than when external servers are used.
    return true;
  }

  // For external server testing, we need to have a clean account. The following
  // code will sign in one chrome browser, get the client id and access token,
  // then clean the server data.
#if BUILDFLAG(IS_ANDROID)
  Profile& profile = CHECK_DEREF(ProfileManager::GetLastUsedProfile());
#else   // BUILDFLAG(IS_ANDROID)
  base::ScopedAllowBlockingForTesting allow_blocking;
  Profile& profile = profiles::testing::CreateProfileSync(
      g_browser_process->profile_manager(),
      g_browser_process->profile_manager()->GenerateNextProfileDirectoryPath());
#endif  // BUILDFLAG(IS_ANDROID)

  std::unique_ptr<SyncServiceImplHarness> client =
      SyncServiceImplHarness::Create(
          &profile, SyncServiceImplHarness::SigninType::UI_SIGNIN);
  CHECK(client);
  if (!client->SignInPrimaryAccount()) {
    LOG(ERROR) << "Failed to sign in primary account";
    return false;
  }
  if (!client->AwaitSyncTransportActive()) {
    return false;
  }
  if (!client->ResetSyncForPrimaryAccount()) {
    LOG(ERROR) << "Failed to reset sync for primary account";
    return false;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  client->SignOutPrimaryAccount();
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // After reset, this client will disable sync. It may log some messages that
  // do not contribute to test failures. It includes:
  //   PostClientToServerMessage with SERVER_RETURN_NOT_MY_BIRTHDAY
  //   PostClientToServerMessage with NETWORK_CONNECTION_UNAVAILABLE
  //   mcs_client fails with 401.
  LOG(WARNING) << "Finished reset account. Warning logs before "
               << "this log may be safe to ignore.";
  return true;
}

void SyncTest::WaitForDataModels(Profile* profile) {
  // Ideally the waiting for bookmarks should be done exclusively for
  // bookmark-related tests, but there are several tests that use bookmarks as
  // a way to generally check if sync is working, although the test is not
  // really about bookmarks.
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(profile));
}

void SyncTest::SetupMockGaiaResponses() {
  gaia::FakeOAuth2TokenResponse::Success("at1").AddToTestURLLoaderFactory(
      test_url_loader_factory_);
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth_user_info_url().spec(),
      "{ \"id\": \"12345\" }");
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()->oauth2_revoke_url().spec(), "");
}

void SyncTest::SetOAuth2TokenResponse(
    const gaia::FakeOAuth2TokenResponse& response) {
  response.AddToTestURLLoaderFactory(test_url_loader_factory_);
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
  GetSyncService(index)->TriggerRefresh(
      syncer::SyncService::TriggerRefreshSource::kUnknown, data_types);
}

arc::SyncArcPackageHelper* SyncTest::sync_arc_helper() {
#if BUILDFLAG(IS_CHROMEOS)
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
    if (GetSyncService(profile_index)
            ->GetUserSettings()
            ->GetSelectedTypes()
            .Has(syncer::UserSelectableType::kTabs) &&
        !SessionHierarchyMatchChecker(
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

std::ostream& operator<<(std::ostream& stream,
                         SyncTest::SetupSyncMode sync_test_mode) {
  stream << SetupSyncModeAsString(sync_test_mode);
  return stream;
}

std::string SetupSyncModeAsString(SyncTest::SetupSyncMode sync_test_mode) {
  switch (sync_test_mode) {
    case SyncTest::SetupSyncMode::kSyncTransportOnly:
      return "kSyncTransportOnly";
    case SyncTest::SetupSyncMode::kSyncTheFeature:
      return "kSyncTheFeature";
  }
  NOTREACHED();
}

// The set of types that *can* run in transport mode. Doesn't mean they are all
// enabled by default, e.g. HISTORY requires a dedicated opt-in via
// SyncUserSettings::SetSelectedTypes().
syncer::DataTypeSet AllowedTypesInStandaloneTransportMode() {
  static_assert(59 == syncer::GetNumDataTypes(),
                "Add new types below if they can run in transport mode");

#if BUILDFLAG(IS_ANDROID)
  // On Android, `kReplaceSyncPromosWithSignInPromos` has been enabled by
  // default for a long time, so it is not expected to be exercised in tests.
  CHECK(
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos));
#endif  // BUILDFLAG(IS_ANDROID)

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
  allowed_types.Put(syncer::CONTACT_INFO);
  allowed_types.Put(syncer::PASSWORDS);

#if BUILDFLAG(IS_CHROMEOS)
  // OS sync types run in transport mode.
  allowed_types.PutAll({syncer::APP_LIST, syncer::ARC_PACKAGE, syncer::WEB_APPS,
                        syncer::OS_PREFERENCES,
                        syncer::OS_PRIORITY_PREFERENCES});
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (base::FeatureList::IsEnabled(
          switches::kEnablePreferencesAccountStorage)) {
    allowed_types.Put(syncer::PRIORITY_PREFERENCES);
#if BUILDFLAG(IS_ANDROID)
    allowed_types.Put(syncer::PREFERENCES);
#else
    // On desktop, support for transport mode for preferences is implemented
    // alongside that of search engines.
    if (base::FeatureList::IsEnabled(
            syncer::kSeparateLocalAndAccountSearchEngines)) {
      allowed_types.Put(syncer::PREFERENCES);
    }
#endif  // BUILDFLAG(IS_ANDROID)
  }
  if (base::FeatureList::IsEnabled(
          switches::kSyncEnableBookmarksInTransportMode)) {
    allowed_types.Put(syncer::BOOKMARKS);
  }
  if (syncer::IsReadingListAccountStorageEnabled()) {
    allowed_types.Put(syncer::READING_LIST);
  }
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    allowed_types.Put(syncer::AUTOFILL_WALLET_METADATA);
    allowed_types.Put(syncer::AUTOFILL_WALLET_OFFER);
    allowed_types.Put(syncer::HISTORY);
    allowed_types.Put(syncer::HISTORY_DELETE_DIRECTIVES);
    allowed_types.Put(syncer::SAVED_TAB_GROUP);
    allowed_types.Put(syncer::SESSIONS);
    allowed_types.Put(syncer::USER_EVENTS);

#if BUILDFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_CHROMEOS)
    allowed_types.Put(syncer::WEB_APPS);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_CHROMEOS)

    if (data_sharing::features::IsDataSharingFunctionalityEnabled()) {
      allowed_types.Put(syncer::SHARED_TAB_GROUP_DATA);
      allowed_types.Put(syncer::COLLABORATION_GROUP);

      if (base::FeatureList::IsEnabled(
              syncer::kSyncSharedTabGroupAccountData)) {
        allowed_types.Put(syncer::SHARED_TAB_GROUP_ACCOUNT_DATA);
      }

      if (base::FeatureList::IsEnabled(syncer::kSyncSharedComment)) {
        allowed_types.Put(syncer::SHARED_COMMENT);
      }
    }

#if BUILDFLAG(ENABLE_EXTENSIONS)
    if (switches::IsExtensionsExplicitBrowserSigninEnabled()) {
      allowed_types.Put(syncer::EXTENSIONS);
      allowed_types.Put(syncer::EXTENSION_SETTINGS);
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  }
  if (base::FeatureList::IsEnabled(syncer::kSyncAutofillLoyaltyCard)) {
    allowed_types.Put(syncer::AUTOFILL_VALUABLE);
  }

  if (base::FeatureList::IsEnabled(syncer::kSyncAutofillValuableMetadata)) {
    allowed_types.Put(syncer::AUTOFILL_VALUABLE_METADATA);
  }

  if (base::FeatureList::IsEnabled(syncer::kSyncAccountSettings)) {
    allowed_types.Put(syncer::ACCOUNT_SETTING);
  }

  if (base::FeatureList::IsEnabled(syncer::kSyncAIThread)) {
    allowed_types.Put(syncer::AI_THREAD);
  }

  if (base::FeatureList::IsEnabled(syncer::kSyncContextualTask)) {
    allowed_types.Put(syncer::CONTEXTUAL_TASK);
  }

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    allowed_types.Put(syncer::WEB_APKS);
  }
#else   // BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(syncer::kSeparateLocalAndAccountThemes)) {
    allowed_types.Put(syncer::THEMES);
  }

  if (base::FeatureList::IsEnabled(
          syncer::kSeparateLocalAndAccountSearchEngines) &&
      // Support for transport mode for search engines is implemented alongside
      // that of preferences.
      base::FeatureList::IsEnabled(
          switches::kEnablePreferencesAccountStorage)) {
    allowed_types.Put(syncer::SEARCH_ENGINES);
  }

  // These types are excluded on Android as they run outside Chrome.
  allowed_types.Put(syncer::INCOMING_PASSWORD_SHARING_INVITATION);
  allowed_types.Put(syncer::OUTGOING_PASSWORD_SHARING_INVITATION);
  allowed_types.Put(syncer::WEBAUTHN_CREDENTIAL);
#endif  // BUILDFLAG(IS_ANDROID)

  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressesEnabled) &&
      !plus_addresses::features::kEnterprisePlusAddressServerUrl.Get()
           .empty()) {
    allowed_types.Put(syncer::PLUS_ADDRESS);
    allowed_types.Put(syncer::PLUS_ADDRESS_SETTING);
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(
          syncer::kSpellcheckSeparateLocalAndAccountDictionaries)) {
    allowed_types.Put(syncer::DICTIONARY);
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)

  return allowed_types;
}
