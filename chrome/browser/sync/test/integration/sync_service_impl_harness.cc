// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"

#include <cstddef>
#include <sstream>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "components/signin/public/base/signin_client.h"
#endif

using syncer::SyncCycleSnapshot;
using syncer::SyncServiceImpl;

const char* kSyncUrlClearServerDataKey = "sync-url-clear-server-data";

namespace {

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
    *os << "Waiting for sync engine initialization to complete";
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

class SyncSetupChecker : public SingleClientStatusChangeChecker {
 public:
  enum class State { kTransportActive, kFeatureActive };

  SyncSetupChecker(SyncServiceImpl* service, State wait_for_state)
      : SingleClientStatusChangeChecker(service),
        wait_for_state_(wait_for_state) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync setup to complete";

    syncer::SyncService::TransportState transport_state =
        service()->GetTransportState();
    if (transport_state == syncer::SyncService::TransportState::ACTIVE &&
        (wait_for_state_ != State::kFeatureActive ||
         service()->IsSyncFeatureActive())) {
      return true;
    }
    // Sync is blocked by an auth error.
    if (HasAuthError(service())) {
      return true;
    }

    // Still waiting on sync setup.
    return false;
  }

 private:
  const State wait_for_state_;
};

// Same as reset on chrome.google.com/sync.
// This function will wait until the reset is done. If error occurs,
// it will log error messages.
void ResetAccount(network::SharedURLLoaderFactory* url_loader_factory,
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
      url_loader_factory, url_loader_helper.GetCallbackDeprecated());
  url_loader_helper.WaitForCallback();
  if (simple_loader->NetError() != 0) {
    LOG(ERROR) << "Reset account failed with error "
               << net::ErrorToString(simple_loader->NetError())
               << ". The account will remain dirty and may cause test fail.";
  }
}

}  // namespace

// static
std::unique_ptr<SyncServiceImplHarness> SyncServiceImplHarness::Create(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    SigninType signin_type) {
  return base::WrapUnique(
      new SyncServiceImplHarness(profile, username, password, signin_type));
}

SyncServiceImplHarness::SyncServiceImplHarness(Profile* profile,
                                               const std::string& username,
                                               const std::string& password,
                                               SigninType signin_type)
    : profile_(profile),
      service_(SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          profile)),
      username_(username),
      password_(password),
      signin_type_(signin_type),
      profile_debug_name_(profile->GetDebugName()),
      signin_delegate_(CreateSyncSigninDelegate()) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The Main profile already has a primary account that cannot be changed.
  // Allow changing it for test purposes only.
  if (profile_->IsMainProfile()) {
    ChromeSigninClientFactory::GetForProfile(profile_)
        ->set_is_clear_primary_account_allowed_for_testing(
            SigninClient::SignoutDecision::ALLOW);
  }
#endif
}

SyncServiceImplHarness::~SyncServiceImplHarness() = default;

void SyncServiceImplHarness::SetUsernameForFutureSignins(
    const std::string& username) {
  CHECK(!username.empty());
  CHECK(!IdentityManagerFactory::GetForProfile(profile_)->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  username_ = username;
}

signin::GaiaIdHash SyncServiceImplHarness::GetGaiaIdHashForPrimaryAccount()
    const {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  return signin::GaiaIdHash::FromGaiaId(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia);
}

bool SyncServiceImplHarness::SignInPrimaryAccount(
    signin::ConsentLevel consent_level) {
  DCHECK(!username_.empty());

  switch (signin_type_) {
    case SigninType::UI_SIGNIN: {
      if (!signin_delegate_->SigninUI(profile_, username_, password_,
                                      consent_level)) {
        return false;
      }
      break;
    }

    case SigninType::FAKE_SIGNIN: {
      signin_delegate_->SigninFake(profile_, username_, consent_level);

      // TODO(b/1523197): The below checks should also be satisfied for the
      // above case.
      signin::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile_);
      CHECK(identity_manager->HasPrimaryAccount(consent_level));
      CHECK(identity_manager->HasPrimaryAccountWithRefreshToken(consent_level));
      CHECK(!service()->GetAccountInfo().IsEmpty());

      break;
    }
  }

  return true;
}

void SyncServiceImplHarness::ResetSyncForPrimaryAccount() {
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
  std::string access_token = service()->GetAccessTokenForTest();
  DCHECK(access_token.size()) << "Access token is not available.";
  ResetAccount(profile_->GetURLLoaderFactory().get(), access_token, url,
               username_, transport_data_prefs.GetBirthday());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void SyncServiceImplHarness::SignOutPrimaryAccount() {
  DCHECK(!username_.empty());
  signin_delegate_->SignOutPrimaryAccount(profile_);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
void SyncServiceImplHarness::EnterSyncPausedStateForPrimaryAccount() {
  DCHECK(service_->IsSyncFeatureActive());
  signin::SetInvalidRefreshTokenForPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_));
}

bool SyncServiceImplHarness::ExitSyncPausedStateForPrimaryAccount() {
  signin::SetRefreshTokenForPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_));
  // The engine was off in the sync-paused state, so wait for it to start.
  return AwaitSyncSetupCompletion();
}
#endif  // !BUILDFLAG(IS_ANDROID)

bool SyncServiceImplHarness::SetupSync(
    SetUserSettingsCallback user_settings_callback) {
  bool result =
      SetupSyncNoWaitForCompletion(std::move(user_settings_callback)) &&
      AwaitSyncSetupCompletion();
  if (!result) {
    LOG(ERROR) << profile_debug_name_ << ": SetupSync failed. Syncer status:\n"
               << GetServiceStatus();
  } else {
    DVLOG(1) << profile_debug_name_ << ": SetupSync successful.";
  }
  return result;
}

bool SyncServiceImplHarness::SetupSyncNoWaitForCompletion(
    SetUserSettingsCallback user_settings_callback) {
  if (service() == nullptr) {
    LOG(ERROR) << "SetupSync(): service() is null.";
    return false;
  }

  // Tell the sync service that setup is in progress so we don't start syncing
  // until we've finished configuration.
  sync_blocker_ = service()->GetSetupInProgressHandle();

  if (!SignInPrimaryAccount(signin::ConsentLevel::kSync)) {
    return false;
  }

  // Now that auth is completed, request that sync actually start.
  service()->SetSyncFeatureRequested();

  if (!AwaitEngineInitialization()) {
    return false;
  }

  // Now give the caller a chance to configure settings (in particular, the
  // selected data types) before actually starting to sync.
  if (user_settings_callback) {
    std::move(user_settings_callback).Run(service()->GetUserSettings());
  }

  // Notify SyncServiceImpl that we are done with configuration.
  FinishSyncSetup();

  if (signin_type_ == SigninType::UI_SIGNIN) {
    return signin_delegate_->ConfirmSyncUI(profile_);
  }
  return true;
}

void SyncServiceImplHarness::FinishSyncSetup() {
  sync_blocker_.reset();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  service()->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
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

bool SyncServiceImplHarness::AwaitSyncSetupCompletion() {
  CHECK(service()->GetUserSettings()->IsInitialSyncFeatureSetupComplete())
      << "Waiting for setup completion can only succeed after the first setup "
      << "got marked complete. Did you call SetupSync on this client?";
  if (!SyncSetupChecker(service(), SyncSetupChecker::State::kFeatureActive)
           .Wait()) {
    LOG(ERROR) << "SyncSetupChecker timed out.";
    return false;
  }
  // Signal an error if the initial sync wasn't successful.
  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  return true;
}

bool SyncServiceImplHarness::AwaitSyncTransportActive() {
  if (!SyncSetupChecker(service(), SyncSetupChecker::State::kTransportActive)
           .Wait()) {
    LOG(ERROR) << "SyncSetupChecker timed out.";
    return false;
  }
  // Signal an error if the initial sync wasn't successful.
  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  return true;
}

bool SyncServiceImplHarness::AwaitInvalidationsStatus(bool expected_status) {
  return InvalidationsStatusChecker(service(), expected_status).Wait();
}

bool SyncServiceImplHarness::EnableSyncForType(
    syncer::UserSelectableType type) {
  DVLOG(1) << GetClientInfoString(
      "EnableSyncForType(" +
      std::string(syncer::GetUserSelectableTypeName(type)) + ")");

  if (!IsSyncEnabledByUser()) {
    bool result = SetupSync(base::BindLambdaForTesting(
        [type](syncer::SyncUserSettings* user_settings) {
          user_settings->SetSelectedTypes(false, {type});
        }));
    // If SetupSync() succeeded, then Sync must now be enabled.
    DCHECK(!result || IsSyncEnabledByUser());
    return result;
  }

  if (service() == nullptr) {
    LOG(ERROR) << "EnableSyncForType(): service() is null.";
    return false;
  }

  syncer::UserSelectableTypeSet selected_types =
      service()->GetUserSettings()->GetSelectedTypes();
  if (selected_types.Has(type)) {
    DVLOG(1) << "EnableSyncForType(): Sync already enabled for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  selected_types.Put(type);
  service()->GetUserSettings()->SetSelectedTypes(false, selected_types);
  if (AwaitSyncSetupCompletion()) {
    DVLOG(1) << "EnableSyncForType(): Enabled sync for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableSyncForType failed");
  return false;
}

bool SyncServiceImplHarness::DisableSyncForType(
    syncer::UserSelectableType type) {
  DVLOG(1) << GetClientInfoString(
      "DisableSyncForType(" +
      std::string(syncer::GetUserSelectableTypeName(type)) + ")");

  if (service() == nullptr) {
    LOG(ERROR) << "DisableSyncForType(): service() is null.";
    return false;
  }

  syncer::UserSelectableTypeSet selected_types =
      service()->GetUserSettings()->GetSelectedTypes();
  if (!selected_types.Has(type)) {
    DVLOG(1) << "DisableSyncForType(): Sync already disabled for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  selected_types.Remove(type);
  service()->GetUserSettings()->SetSelectedTypes(false, selected_types);
  if (AwaitSyncSetupCompletion()) {
    DVLOG(1) << "DisableSyncForType(): Disabled sync for type "
             << syncer::GetUserSelectableTypeName(type) << " on "
             << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("DisableSyncForDatatype failed");
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

  if (service() == nullptr) {
    LOG(ERROR) << "EnableSyncForRegisteredDatatypes(): service() is null.";
    return false;
  }

  service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true,
      service()->GetUserSettings()->GetRegisteredSelectableTypes());

  if (AwaitSyncSetupCompletion()) {
    DVLOG(1)
        << "EnableSyncForRegisteredDatatypes(): Enabled sync for all datatypes "
        << "on " << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableSyncForRegisteredDatatypes failed");
  return false;
}

bool SyncServiceImplHarness::DisableSyncForAllDatatypes() {
  DVLOG(1) << GetClientInfoString("DisableSyncForAllDatatypes");

  if (service() == nullptr) {
    LOG(ERROR) << "DisableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  DVLOG(1) << "DisableSyncForAllDatatypes(): Disabled sync for all "
           << "datatypes on " << profile_debug_name_;
  return true;
}

SyncCycleSnapshot SyncServiceImplHarness::GetLastCycleSnapshot() const {
  DCHECK(service() != nullptr) << "Sync service has not yet been set up.";
  if (service()->IsSyncFeatureActive()) {
    return service()->GetLastCycleSnapshotForDebugging();
  }
  return SyncCycleSnapshot();
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
