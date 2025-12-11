// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"

#include <cstddef>
#include <sstream>
#include <utility>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/invalidations/invalidations_status_checker.h"
#include "chrome/browser/sync/test/integration/quiesce_status_change_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_signin_delegate.h"
#include "chrome/common/channel_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/engine/net/url_translator.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/engine/traffic_logger.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_internals_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/zlib/google/compression_utils.h"

namespace {

using syncer::SyncCycleSnapshot;
using syncer::SyncServiceImpl;

constexpr char kSyncUrlClearServerDataKey[] = "sync-url-clear-server-data";

bool HasAuthError(SyncServiceImpl* service) {
  return service->GetAuthError().state() ==
             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::SERVICE_ERROR ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::REQUEST_CANCELED;
}

class EngineInitializeChecker : public SingleClientStatusChangeChecker {
 public:
  explicit EngineInitializeChecker(SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync engine initialization to complete; actual "
           "transport state: "
        << syncer::sync_ui_util::TransportStateStringToDebugString(
               service()->GetTransportState())
        << ", disable reasons: "
        << syncer::sync_ui_util::GetDisableReasonsDebugString(
               service()->GetDisableReasons());
    if (service()->IsEngineInitialized()) {
      return true;
    }
    // Engine initialization is blocked by an auth error.
    if (HasAuthError(service())) {
      LOG(WARNING) << "Sync engine initialization blocked by auth error";
      return true;
    }
    // Engine initialization is blocked by a failure to fetch Oauth2 tokens.
    if (service()->IsRetryingAccessTokenFetchForTest()) {
      LOG(WARNING) << "Sync engine initialization blocked by failure to fetch "
                      "access tokens";
      return true;
    }
    // Still waiting on engine initialization.
    return false;
  }
};

class SyncTransportStateChecker : public SingleClientStatusChangeChecker {
 public:
  SyncTransportStateChecker(SyncServiceImpl* service,
                            syncer::SyncService::TransportState state)
      : SingleClientStatusChangeChecker(service), state_(state) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync transport state to change";
    return service()->GetTransportState() == state_;
  }

 private:
  const syncer::SyncService::TransportState state_;
};

// Same as reset on chrome.google.com/data.
// This function will wait until the reset is done. If error occurs,
// it will log error messages.
bool ResetAccount(network::SharedURLLoaderFactory* url_loader_factory,
                  const std::string& access_token,
                  const GURL& url,
                  const std::string& username,
                  const std::string& birthday) {
  // Generate https POST payload.
  sync_pb::ClientToServerMessage message;
  message.set_share(username);
  message.set_message_contents(
      sync_pb::ClientToServerMessage::CLEAR_SERVER_DATA);
  message.set_store_birthday(birthday);
  message.set_api_key(google_apis::GetAPIKey());
  syncer::LogClientToServerMessage(message);
  std::string payload;
  message.SerializeToString(&payload);
  std::string request_to_send;
  compression::GzipCompress(payload, &request_to_send);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Authorization",
                                      "Bearer " + access_token);
  resource_request->headers.SetHeader("Content-Encoding", "gzip");
  resource_request->headers.SetHeader("Accept-Language", "en-US,en");
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->AttachStringForUpload(request_to_send,
                                       "application/octet-stream");
  simple_loader->SetTimeoutDuration(base::Seconds(10));
  content::SimpleURLLoaderTestHelper url_loader_helper;
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory, url_loader_helper.GetCallback());
  url_loader_helper.WaitForCallback();
  if (simple_loader->NetError() != 0) {
    LOG(ERROR) << "Reset account failed with error "
               << net::ErrorToString(simple_loader->NetError())
               << ". The account will remain dirty and may cause test fail.";
    return false;
  }
  return true;
}

std::unique_ptr<SyncSigninDelegate> CreateSyncSigninDelegateForType(
    SyncServiceImplHarness::SigninType signin_type,
    Profile* profile) {
  CHECK(profile);
  switch (signin_type) {
    case SyncServiceImplHarness::SigninType::UI_SIGNIN:
      return CreateSyncSigninDelegateWithLiveSignin(profile);
    case SyncServiceImplHarness::SigninType::FAKE_SIGNIN:
      return CreateSyncSigninDelegateWithFakeSignin(profile);
  }
  NOTREACHED();
}

}  // namespace

// static
std::unique_ptr<SyncServiceImplHarness> SyncServiceImplHarness::Create(
    Profile* profile,
    SigninType signin_type) {
  CHECK(profile);
  return base::WrapUnique(new SyncServiceImplHarness(
      profile, CreateSyncSigninDelegateForType(signin_type, profile)));
}

SyncServiceImplHarness::SyncServiceImplHarness(
    Profile* profile,
    std::unique_ptr<SyncSigninDelegate> signin_delegate)
    : profile_(CHECK_DEREF(profile).GetWeakPtr()),
      service_(SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          profile)),
      profile_debug_name_(profile->GetDebugName()),
      signin_delegate_(std::move(signin_delegate)) {
  CHECK(profile_);
  CHECK(service_);
  CHECK(signin_delegate_);
}

SyncServiceImplHarness::~SyncServiceImplHarness() = default;

signin::GaiaIdHash SyncServiceImplHarness::GetGaiaIdHashForPrimaryAccount()
    const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  return signin::GaiaIdHash::FromGaiaId(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia);
}

GaiaId SyncServiceImplHarness::GetGaiaIdForAccount(
    SyncTestAccount account) const {
  return signin_delegate_->GetGaiaIdForAccount(account);
}

std::string SyncServiceImplHarness::GetEmailForAccount(
    SyncTestAccount account) const {
  return signin_delegate_->GetEmailForAccount(account);
}

bool SyncServiceImplHarness::SignInPrimaryAccount(SyncTestAccount account) {
  if (!signin_delegate_->SignIn(account, signin::ConsentLevel::kSignin)) {
    return false;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  CHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  CHECK(identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));
  CHECK(!service()->GetAccountInfo().IsEmpty());

  return true;
}

bool SyncServiceImplHarness::ResetSyncForPrimaryAccount() {
  syncer::SyncTransportDataPrefs transport_data_prefs(
      profile_->GetPrefs(), GetGaiaIdHashForPrimaryAccount());
  // Generate the https url.
  // CLEAR_SERVER_DATA isn't enabled on the prod Sync server,
  // so --sync-url-clear-server-data can be used to specify an
  // alternative endpoint.
  // Note: Any OTA(Owned Test Account) tries to clear data need to be
  // whitelisted.
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  DCHECK(cmd_line->HasSwitch(kSyncUrlClearServerDataKey))
      << "Missing switch " << kSyncUrlClearServerDataKey;
  GURL base_url(cmd_line->GetSwitchValueASCII(kSyncUrlClearServerDataKey) +
                "/command/?");
  GURL url = syncer::AppendSyncQueryString(base_url,
                                           transport_data_prefs.GetCacheGuid());

  // Call sync server to clear sync data.
  const std::string access_token = service()->GetAccessTokenForTest();
  if (access_token.empty()) {
    LOG(ERROR) << "Access token is not available.";
    return false;
  }
  return ResetAccount(profile_->GetURLLoaderFactory().get(), access_token, url,
                      service()->GetAccountInfo().email,
                      transport_data_prefs.GetBirthday());
}

#if !BUILDFLAG(IS_CHROMEOS)
void SyncServiceImplHarness::SignOutPrimaryAccount() {
  signin_delegate_->SignOut();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
bool SyncServiceImplHarness::EnterSyncPausedStateForPrimaryAccount() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  CHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CHECK(service_->IsSyncFeatureEnabled());
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);
  return AwaitSyncTransportPaused();
}

bool SyncServiceImplHarness::ExitSyncPausedStateForPrimaryAccount() {
  signin::SetRefreshTokenForPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_.get()));
  // The engine was off in the sync-paused state, so wait for it to start.
  return AwaitSyncTransportActive();
}

bool SyncServiceImplHarness::EnterSignInPendingStateForPrimaryAccount() {
  CHECK(!service_->IsSyncFeatureEnabled());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  CHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);
  return AwaitSyncTransportPaused();
}

bool SyncServiceImplHarness::ExitSignInPendingStateForPrimaryAccount() {
  CHECK_EQ(service_->GetTransportState(),
           syncer::SyncService::TransportState::PAUSED);
  signin::SetRefreshTokenForPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_.get()));
  return AwaitSyncTransportActive();
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool SyncServiceImplHarness::SetupSync(SyncTestAccount account) {
  bool result =
      SetupSyncNoWaitForCompletion(account) && AwaitSyncTransportActive();
  if (!result) {
    LOG(ERROR) << profile_debug_name_ << ": SetupSync failed. Syncer status:\n"
               << GetServiceStatus();
  } else {
    DVLOG(1) << profile_debug_name_ << ": SetupSync successful.";
  }
  return result;
}

bool SyncServiceImplHarness::SetupSyncWithCustomSettings(
    SetUserSettingsCallback user_settings_callback,
    SyncTestAccount account) {
  bool result = SetupSyncWithCustomSettingsNoWaitForCompletion(
                    std::move(user_settings_callback), account) &&
                AwaitSyncTransportActive();
  if (!result) {
    LOG(ERROR) << profile_debug_name_ << ": SetupSync failed. Syncer status:\n"
               << GetServiceStatus();
  } else {
    DVLOG(1) << profile_debug_name_ << ": SetupSync successful.";
  }
  return result;
}

bool SyncServiceImplHarness::SetupSyncNoWaitForCompletion(
    SyncTestAccount account) {
  // By default, mimic the user confirming the default settings.
  return SetupSyncWithCustomSettingsNoWaitForCompletion(
      base::BindLambdaForTesting([](syncer::SyncUserSettings* user_settings) {
#if !BUILDFLAG(IS_CHROMEOS)
        user_settings->SetInitialSyncFeatureSetupComplete(
            syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
#endif  // !BUILDFLAG(IS_CHROMEOS)
      }),
      account);
}

bool SyncServiceImplHarness::SetupSyncWithCustomSettingsNoWaitForCompletion(
    SetUserSettingsCallback user_settings_callback,
    SyncTestAccount account) {
  if (service() == nullptr) {
    LOG(ERROR) << "SetupSync(): service() is null.";
    return false;
  }

  // Tell the sync service that setup is in progress so we don't start syncing
  // until we've finished configuration.
  sync_blocker_ = service()->GetSetupInProgressHandle();

  if (!signin_delegate_->SignIn(account, signin::ConsentLevel::kSync)) {
    return false;
  }
  if (account == SyncTestAccount::kEnterpriseAccount1 ||
      account == SyncTestAccount::kGoogleDotComAccount1) {
    enterprise_util::SetUserAcceptedAccountManagement(profile_.get(), true);
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_.get());
  // Note that `ConsentLevel::kSync` is not actually guaranteed at this stage.
  // Namely, live tests require closing the sync confirmation dialog before
  // `ConsentLevel::kSync` is granted. This is achieved later below with
  // `ConfirmSyncUI()`.
  CHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  if (!AwaitEngineInitialization()) {
    return false;
  }

  // Now give the caller a chance to configure settings (in particular, the
  // selected data types) before actually starting to sync. This callback
  // usually (but not necessarily) invokes SetInitialSyncFeatureSetupComplete().
  std::move(user_settings_callback).Run(service()->GetUserSettings());

  // Notify SyncServiceImpl that we are done with configuration.
  sync_blocker_.reset();

  if (!signin_delegate_->ConfirmSync()) {
    return false;
  }

  CHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CHECK(identity_manager->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
  CHECK(!service()->GetAccountInfo().IsEmpty());

  return true;
}

void SyncServiceImplHarness::FinishSyncSetup() {
#if !BUILDFLAG(IS_CHROMEOS)
  service()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
#endif  // !BUILDFLAG(IS_CHROMEOS)
  sync_blocker_.reset();
}

bool SyncServiceImplHarness::AwaitMutualSyncCycleCompletion(
    SyncServiceImplHarness* partner) {
  std::vector<SyncServiceImplHarness*> harnesses;
  harnesses.push_back(this);
  harnesses.push_back(partner);
  return AwaitQuiescence(harnesses);
}

// static
bool SyncServiceImplHarness::AwaitQuiescence(
    const std::vector<SyncServiceImplHarness*>& clients) {
  if (clients.empty()) {
    return true;
  }

  std::vector<raw_ptr<SyncServiceImpl, VectorExperimental>> services;
  for (SyncServiceImplHarness* harness : clients) {
    services.push_back(harness->service());
  }
  return QuiesceStatusChangeChecker(services).Wait();
}

bool SyncServiceImplHarness::AwaitEngineInitialization() {
  if (!EngineInitializeChecker(service()).Wait()) {
    LOG(ERROR) << "EngineInitializeChecker timed out.";
    return false;
  }

  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  if (service()->IsRetryingAccessTokenFetchForTest()) {
    LOG(ERROR) << "Failed to fetch access token. Sync cannot proceed.";
    return false;
  }

  if (!service()->IsEngineInitialized()) {
    LOG(ERROR) << "Service engine not initialized.";
    return false;
  }

  return true;
}

bool SyncServiceImplHarness::AwaitSyncTransportActive() {
  if (!SyncTransportStateChecker(service(),
                                 syncer::SyncService::TransportState::ACTIVE)
           .Wait()) {
    LOG(ERROR) << "SyncTransportStateChecker timed out.";
    return false;
  }
  // Signal an error if the initial sync wasn't successful.
  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  return true;
}

bool SyncServiceImplHarness::AwaitSyncTransportPaused() {
  if (!SyncTransportStateChecker(service(),
                                 syncer::SyncService::TransportState::PAUSED)
           .Wait()) {
    LOG(ERROR) << "SyncTransportStateChecker timed out.";
    return false;
  }
  return true;
}

bool SyncServiceImplHarness::AwaitInvalidationsStatus(bool expected_status) {
  return InvalidationsStatusChecker(service(), expected_status).Wait();
}

bool SyncServiceImplHarness::EnableHistorySyncNoWaitForCompletion() {
  DVLOG(1) << GetClientInfoString("EnableHistorySync");
  if (service() == nullptr) {
    LOG(ERROR) << "EnableHistorySync(): service() is null.";
    return false;
  }
  service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  // Tabs and history are bundled together in the same toggle.
  service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // On desktop platforms, kSavedTabGroups are not merged to kTabs yet, but
  // they're enabled together.
  service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kSavedTabGroups, true);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return true;
}

bool SyncServiceImplHarness::EnableSelectableType(
    syncer::UserSelectableType type) {
  if (service() == nullptr) {
    LOG(ERROR) << "EnableSelectableType(): service() is null.";
    return false;
  }

  syncer::UserSelectableTypeSet selected_types =
      service()->GetUserSettings()->GetSelectedTypes();
  if (selected_types.Has(type)) {
    DVLOG(1) << "EnableSelectableType(): Sync already enabled for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  selected_types.Put(type);
  service()->GetUserSettings()->SetSelectedTypes(false, selected_types);
  if (AwaitSyncTransportActive()) {
    DVLOG(1) << "EnableSelectableType(): Enabled sync for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableSelectableType failed");
  return false;
}

bool SyncServiceImplHarness::DisableSelectableType(
    syncer::UserSelectableType type) {
  DVLOG(1) << GetClientInfoString(
      "DisableSelectableType(" +
      std::string(syncer::GetUserSelectableTypeName(type)) + ")");

  if (service() == nullptr) {
    LOG(ERROR) << "DisableSelectableType(): service() is null.";
    return false;
  }

  syncer::UserSelectableTypeSet selected_types =
      service()->GetUserSettings()->GetSelectedTypes();
  if (!selected_types.Has(type)) {
    DVLOG(1) << "DisableSelectableType(): "
             << syncer::GetUserSelectableTypeName(type)
             << " already disabled on " << profile_debug_name_ << ".";
    return true;
  }

  selected_types.Remove(type);
  service()->GetUserSettings()->SetSelectedTypes(false, selected_types);
  if (AwaitSyncTransportActive()) {
    DVLOG(1) << "DisableSelectableType(): Disabled "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("DisableSelectableType failed");
  return false;
}

bool SyncServiceImplHarness::EnableSyncForRegisteredDatatypes() {
  DVLOG(1) << GetClientInfoString("EnableSyncForRegisteredDatatypes");

  if (!IsSyncEnabledByUser()) {
    bool result = SetupSync();
    // If SetupSync() succeeded, then Sync must now be enabled.
    DCHECK(!result || IsSyncEnabledByUser());
    return result;
  }

  return EnableAllSelectableTypes();
}

bool SyncServiceImplHarness::EnableAllSelectableTypes() {
  if (service() == nullptr) {
    LOG(ERROR) << "EnableAllSelectableTypes(): service() is null.";
    return false;
  }

  service()->GetUserSettings()->SetSelectedTypes(/*sync_everything=*/true, {});

  if (AwaitSyncTransportActive()) {
    DVLOG(1) << "EnableAllSelectableTypes(): Enabled all types on "
             << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableAllSelectableTypes() failed.");
  return false;
}

bool SyncServiceImplHarness::DisableSyncForAllDatatypes() {
  return DisableAllSelectableTypes();
}

bool SyncServiceImplHarness::DisableAllSelectableTypes() {
  DVLOG(1) << GetClientInfoString("DisableAllSelectableTypes");

  if (service() == nullptr) {
    LOG(ERROR) << "DisableAllSelectableTypes(): service() is null.";
    return false;
  }

  service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  DVLOG(1) << "DisableAllSelectableTypes(): Disabled all types on "
           << profile_debug_name_ << ".";
  return true;
}

SyncCycleSnapshot SyncServiceImplHarness::GetLastCycleSnapshot() const {
  DCHECK(service() != nullptr) << "Sync service has not yet been set up.";
  return service()->GetLastCycleSnapshotForDebugging();
}

absl::flat_hash_map<syncer::DataType, size_t>
SyncServiceImplHarness::GetTypesWithUnsyncedDataAndWait(
    syncer::DataTypeSet requested_types) const {
  base::test::TestFuture<absl::flat_hash_map<syncer::DataType, size_t>> future;
  service()->GetTypesWithUnsyncedData(requested_types, future.GetCallback());
  return future.Get();
}

syncer::LocalDataDescription
SyncServiceImplHarness::GetLocalDataDescriptionAndWait(
    syncer::DataType data_type) {
  base::test::TestFuture<
      std::map<syncer::DataType, syncer::LocalDataDescription>>
      descriptions;
  service()->GetLocalDataDescriptions({data_type}, descriptions.GetCallback());

  if (descriptions.Get().size() != 1u) {
    ADD_FAILURE()
        << "The expected size of local data description map is 1. Found "
        << descriptions.Get().size() << '.';
    return syncer::LocalDataDescription();
  }

  if (descriptions.Get().begin()->first != data_type) {
    ADD_FAILURE()
        << DataTypeToDebugString(data_type)
        << " is the only expected key in the local data description map. Found "
        << DataTypeToDebugString(descriptions.Get().begin()->first) << '.';
    return syncer::LocalDataDescription();
  }

  return descriptions.Get().begin()->second;
}

std::string SyncServiceImplHarness::GetServiceStatus() {
  // This method is only used in test code for debugging purposes, so it's fine
  // to include sensitive data in ConstructAboutInformation().
  base::Value::Dict value = syncer::sync_ui_util::ConstructAboutInformation(
      syncer::sync_ui_util::IncludeSensitiveData(true), service(),
      chrome::GetChannelName(chrome::WithExtendedStable(true)));
  std::string service_status;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &service_status);
  return service_status;
}

// TODO(sync): Clean up this method in a separate CL. Remove all snapshot fields
// and log shorter, more meaningful messages.
std::string SyncServiceImplHarness::GetClientInfoString(
    const std::string& message) const {
  std::stringstream os;
  os << profile_debug_name_ << ": " << message << ": ";
  if (service()) {
    const SyncCycleSnapshot& snap = GetLastCycleSnapshot();
    syncer::SyncStatus status;
    service()->QueryDetailedSyncStatusForDebugging(&status);
    // Capture select info from the sync session snapshot and syncer status.
    os << ", has_unsynced_items: " << snap.has_remaining_local_changes()
       << ", did_commit: "
       << (snap.model_neutral_state().num_successful_commits == 0 &&
           snap.model_neutral_state().commit_result.type() ==
               syncer::SyncerError::Type::kSuccess)
       << ", server conflicts: " << snap.num_server_conflicts()
       << ", num_updates_downloaded : "
       << snap.model_neutral_state().num_updates_downloaded_total
       << ", passphrase_required: "
       << service()->GetUserSettings()->IsPassphraseRequired()
       << ", notifications_enabled: " << status.notifications_enabled
       << ", service_is_active: " << service()->IsSyncFeatureActive();
  } else {
    os << "Sync service not available";
  }
  return os.str();
}

bool SyncServiceImplHarness::IsSyncEnabledByUser() const {
  return service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete();
}
