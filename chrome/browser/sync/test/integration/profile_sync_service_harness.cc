// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"

#include <cstddef>
#include <sstream>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/quiesce_status_change_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/driver/about_sync_util.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/unified_consent_service.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_utils.h"

using browser_sync::ProfileSyncService;
using syncer::SyncCycleSnapshot;

namespace {

bool HasAuthError(ProfileSyncService* service) {
  return service->GetAuthError().state() ==
             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::SERVICE_ERROR ||
         service->GetAuthError().state() ==
             GoogleServiceAuthError::REQUEST_CANCELED;
}

class EngineInitializeChecker : public SingleClientStatusChangeChecker {
 public:
  explicit EngineInitializeChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    if (service()->IsEngineInitialized())
      return true;
    // Engine initialization is blocked by an auth error.
    if (HasAuthError(service()))
      return true;
    // Engine initialization is blocked by a failure to fetch Oauth2 tokens.
    if (service()->IsRetryingAccessTokenFetchForTest())
      return true;
    // Still waiting on engine initialization.
    return false;
  }

  std::string GetDebugMessage() const override { return "Engine Initialize"; }
};

class SyncSetupChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncSetupChecker(ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied() override {
    syncer::SyncService::TransportState state = service()->GetTransportState();
    if (state == syncer::SyncService::TransportState::ACTIVE)
      return true;
    // Sync is blocked by an auth error.
    if (HasAuthError(service()))
      return true;
    if (state != syncer::SyncService::TransportState::CONFIGURING)
      return false;
    // Sync is blocked because a custom passphrase is required.
    if (service()->passphrase_required_reason_for_test() ==
        syncer::REASON_DECRYPTION) {
      return true;
    }
    // Still waiting on sync setup.
    return false;
  }

  std::string GetDebugMessage() const override { return "Sync Setup"; }
};

}  // namespace

// static
std::unique_ptr<ProfileSyncServiceHarness> ProfileSyncServiceHarness::Create(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    SigninType signin_type) {
  return base::WrapUnique(
      new ProfileSyncServiceHarness(profile, username, password, signin_type));
}

ProfileSyncServiceHarness::ProfileSyncServiceHarness(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    SigninType signin_type)
    : profile_(profile),
      service_(ProfileSyncServiceFactory::GetForProfile(profile)),
      username_(username),
      password_(password),
      signin_type_(signin_type),
      profile_debug_name_(profile->GetDebugName()) {}

ProfileSyncServiceHarness::~ProfileSyncServiceHarness() { }

bool ProfileSyncServiceHarness::SignInPrimaryAccount() {
  // TODO(crbug.com/871221): This function should distinguish primary account
  // (aka sync account) from secondary accounts (content area signin). Let's
  // migrate tests that exercise transport-only sync to secondary accounts.
  DCHECK(!username_.empty());

  switch (signin_type_) {
    case SigninType::UI_SIGNIN: {
      Browser* browser = chrome::FindBrowserWithProfile(profile_);
      DCHECK(browser);
      if (!login_ui_test_utils::SignInWithUI(browser, username_, password_)) {
        LOG(ERROR) << "Could not sign in to GAIA servers.";
        return false;
      }
      return true;
    }

    case SigninType::FAKE_SIGNIN: {
      identity::IdentityManager* identity_manager =
          IdentityManagerFactory::GetForProfile(profile_);
      ProfileOAuth2TokenService* token_service =
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);

      // Verify HasPrimaryAccount() separately because
      // MakePrimaryAccountAvailable() below DCHECK fails if there is already
      // an authenticated account.
      if (identity_manager->HasPrimaryAccount()) {
        DCHECK_EQ(identity_manager->GetPrimaryAccountInfo().email, username_);
        // Don't update the refresh token if we already have one. The reason is
        // that doing so causes Sync (ServerConnectionManager in particular) to
        // mark the current access token as invalid. Since tests typically
        // always hand out the same access token string, any new access token
        // acquired later would also be considered invalid.
        if (!identity_manager->HasPrimaryAccountWithRefreshToken()) {
          identity::SetRefreshTokenForPrimaryAccount(token_service,
                                                     identity_manager);
        }
      } else {
        // Authenticate sync client using GAIA credentials.
        identity::MakePrimaryAccountAvailable(
            SigninManagerFactory::GetForProfile(profile_), token_service,
            identity_manager, username_);
      }
      return true;
    }
  }

  NOTREACHED();
  return false;
}

#if !defined(OS_CHROMEOS)
void ProfileSyncServiceHarness::SignOutPrimaryAccount() {
  DCHECK(!username_.empty());
  identity::ClearPrimaryAccount(
      SigninManagerFactory::GetForProfile(profile_),
      IdentityManagerFactory::GetForProfile(profile_),
      identity::ClearPrimaryAccountPolicy::REMOVE_ALL_ACCOUNTS);
}
#endif  // !OS_CHROMEOS

bool ProfileSyncServiceHarness::SetupSync() {
  bool result = SetupSync(syncer::UserSelectableTypes());
  if (!result) {
    LOG(ERROR) << profile_debug_name_ << ": SetupSync failed. Syncer status:\n"
               << GetServiceStatus();
  } else {
    DVLOG(1) << profile_debug_name_ << ": SetupSync successful.";
  }
  return result;
}

bool ProfileSyncServiceHarness::SetupSyncForClearingServerData() {
  bool result = SetupSyncImpl(syncer::UserSelectableTypes(),
                              /*skip_passphrase_verification=*/true,
                              /*encryption_passphrase=*/base::nullopt);
  if (!result) {
    LOG(ERROR) << profile_debug_name_
               << ": SetupSyncForClear failed. Syncer status:\n"
               << GetServiceStatus();
  } else {
    DVLOG(1) << profile_debug_name_ << ": SetupSyncForClear successful.";
  }
  return result;
}

bool ProfileSyncServiceHarness::SetupSync(
    syncer::ModelTypeSet synced_datatypes) {
  return SetupSyncImpl(synced_datatypes, /*skip_passphrase_verification=*/false,
                       /*encryption_passphrase=*/base::nullopt);
}

bool ProfileSyncServiceHarness::SetupSyncWithEncryptionPassphrase(
    syncer::ModelTypeSet synced_datatypes,
    const std::string& passphrase) {
  return SetupSyncImpl(synced_datatypes, /*skip_passphrase_verification=*/false,
                       passphrase);
}

bool ProfileSyncServiceHarness::SetupSyncWithDecryptionPassphrase(
    syncer::ModelTypeSet synced_datatypes,
    const std::string& passphrase) {
  if (!SetupSyncImpl(synced_datatypes, /*skip_passphrase_verification=*/true,
                     /*encryption_passphrase=*/base::nullopt)) {
    return false;
  }

  DVLOG(1) << "Setting decryption passphrase.";
  if (!service_->SetDecryptionPassphrase(passphrase)) {
    // This is not a fatal failure, as some tests intentionally pass an
    // incorrect passphrase. If this happens, Sync will be set up but will have
    // encountered cryptographer errors for the passphrase-encrypted datatypes.
    LOG(INFO) << "SetDecryptionPassphrase() failed.";
  }
  // Since SetupSyncImpl() was called with skip_passphrase_verification == true,
  // it will not have called FinishSyncSetup(). FinishSyncSetup() is in charge
  // of calling ProfileSyncService::SetFirstSetupComplete(), and without that,
  // Sync will still be in setup mode and Sync-the-feature will be disabled.
  // Therefore, we call FinishSyncSetup() here explicitly.
  FinishSyncSetup();
  return true;
}

bool ProfileSyncServiceHarness::SetupSyncImpl(
    syncer::ModelTypeSet synced_datatypes,
    bool skip_passphrase_verification,
    const base::Optional<std::string>& encryption_passphrase) {
  DCHECK(!profile_->IsLegacySupervised())
      << "SetupSync should not be used for legacy supervised users.";

  if (service() == nullptr) {
    LOG(ERROR) << "SetupSync(): service() is null.";
    return false;
  }

  // Tell the sync service that setup is in progress so we don't start syncing
  // until we've finished configuration.
  sync_blocker_ = service()->GetSetupInProgressHandle();

  if (!SignInPrimaryAccount()) {
    return false;
  }

  // Now that auth is completed, request that sync actually start.
  service()->RequestStart();

  if (!AwaitEngineInitialization(skip_passphrase_verification)) {
    return false;
  }
  // Choose the datatypes to be synced. If all datatypes are to be synced,
  // set sync_everything to true; otherwise, set it to false.
  bool sync_everything = (synced_datatypes == syncer::UserSelectableTypes());
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // When unified consent given is set to |true|, the unified consent service
    // enables syncing all datatypes.
    UnifiedConsentServiceFactory::GetForProfile(profile_)
        ->SetUnifiedConsentGiven(sync_everything);
    if (!sync_everything) {
      service()->OnUserChoseDatatypes(sync_everything, synced_datatypes);
    }
  } else {
    service()->OnUserChoseDatatypes(sync_everything, synced_datatypes);
  }

  if (encryption_passphrase.has_value()) {
    service()->SetEncryptionPassphrase(encryption_passphrase.value());
  }

  // Notify ProfileSyncService that we are done with configuration.
  if (skip_passphrase_verification) {
    sync_blocker_.reset();
  } else {
    FinishSyncSetup();
  }

  if ((signin_type_ == SigninType::UI_SIGNIN) &&
      !login_ui_test_utils::DismissSyncConfirmationDialog(
          chrome::FindBrowserWithProfile(profile_),
          base::TimeDelta::FromSeconds(30))) {
    LOG(ERROR) << "Failed to dismiss sync confirmation dialog.";
    return false;
  }

  // OneClickSigninSyncStarter observer is created with a real user sign in.
  // It is deleted on certain conditions which are not satisfied by our tests,
  // and this causes the SigninTracker observer to stay hanging at shutdown.
  // Calling LoginUIService::SyncConfirmationUIClosed forces the observer to
  // be removed. http://crbug.com/484388
  if (signin_type_ == SigninType::UI_SIGNIN) {
    LoginUIServiceFactory::GetForProfile(profile_)->SyncConfirmationUIClosed(
        LoginUIService::SYNC_WITH_DEFAULT_SETTINGS);
  }

  if (!skip_passphrase_verification &&
      service()->IsUsingSecondaryPassphrase()) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }

  // Wait for initial sync cycle to be completed.
  if (!AwaitSyncSetupCompletion(skip_passphrase_verification)) {
    return false;
  }

  return true;
}

void ProfileSyncServiceHarness::FinishSyncSetup() {
  sync_blocker_.reset();
  service()->SetFirstSetupComplete();
}

void ProfileSyncServiceHarness::StopSyncService(
    syncer::SyncService::SyncStopDataFate data_fate) {
  DVLOG(1) << "Requesting stop for service.";
  service()->RequestStop(data_fate);
}

bool ProfileSyncServiceHarness::StartSyncService() {
  std::unique_ptr<syncer::SyncSetupInProgressHandle> blocker =
      service()->GetSetupInProgressHandle();
  DVLOG(1) << "Requesting start for service";
  service()->RequestStart();

  if (!AwaitEngineInitialization()) {
    LOG(ERROR) << "AwaitEngineInitialization failed.";
    return false;
  }
  DVLOG(1) << "Engine Initialized successfully.";

  if (service()->IsUsingSecondaryPassphrase()) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }
  DVLOG(1) << "Passphrase decryption success.";

  blocker.reset();
  service()->SetFirstSetupComplete();

  if (!AwaitSyncSetupCompletion(/*skip_passphrase_verification=*/false)) {
    LOG(FATAL) << "AwaitSyncSetupCompletion failed.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::HasUnsyncedItems() {
  base::RunLoop loop;
  bool result = false;
  service()->HasUnsyncedItemsForTest(
      base::BindLambdaForTesting([&](bool has_unsynced_items) {
        result = has_unsynced_items;
        loop.Quit();
      }));
  loop.Run();
  return result;
}

bool ProfileSyncServiceHarness::AwaitMutualSyncCycleCompletion(
    ProfileSyncServiceHarness* partner) {
  std::vector<ProfileSyncServiceHarness*> harnesses;
  harnesses.push_back(this);
  harnesses.push_back(partner);
  return AwaitQuiescence(harnesses);
}

// static
bool ProfileSyncServiceHarness::AwaitQuiescence(
    const std::vector<ProfileSyncServiceHarness*>& clients) {
  if (clients.empty()) {
    return true;
  }

  std::vector<ProfileSyncService*> services;
  for (const ProfileSyncServiceHarness* harness : clients) {
    services.push_back(harness->service());
  }
  return QuiesceStatusChangeChecker(services).Wait();
}

bool ProfileSyncServiceHarness::AwaitEngineInitialization(
    bool skip_passphrase_verification) {
  if (!EngineInitializeChecker(service()).Wait()) {
    LOG(ERROR) << "EngineInitializeChecker timed out.";
    return false;
  }

  if (!service()->IsEngineInitialized()) {
    LOG(ERROR) << "Service engine not initialized.";
    return false;
  }

  // Make sure that initial sync wasn't blocked by a missing passphrase.
  if (!skip_passphrase_verification &&
      service()->passphrase_required_reason_for_test() ==
          syncer::REASON_DECRYPTION) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }

  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::AwaitSyncSetupCompletion(
    bool skip_passphrase_verification) {
  if (!SyncSetupChecker(service()).Wait()) {
    LOG(ERROR) << "SyncSetupChecker timed out.";
    return false;
  }

  // If passphrase verification is not skipped, make sure that initial sync
  // wasn't blocked by a missing passphrase.
  if (!skip_passphrase_verification &&
      service()->passphrase_required_reason_for_test() ==
          syncer::REASON_DECRYPTION) {
    LOG(ERROR) << "A passphrase is required for decryption. Sync cannot proceed"
                  " until SetDecryptionPassphrase is called.";
    return false;
  }

  if (HasAuthError(service())) {
    LOG(ERROR) << "Credentials were rejected. Sync cannot proceed.";
    return false;
  }

  return true;
}

bool ProfileSyncServiceHarness::EnableSyncForDatatype(
    syncer::ModelType datatype) {
  DVLOG(1) << GetClientInfoString(
      "EnableSyncForDatatype("
      + std::string(syncer::ModelTypeToString(datatype)) + ")");

  if (!IsSyncEnabledByUser()) {
    bool result = SetupSync(syncer::ModelTypeSet(datatype));
    // If SetupSync() succeeded, then Sync must now be enabled.
    DCHECK(!result || IsSyncEnabledByUser());
    return result;
  }

  if (service() == nullptr) {
    LOG(ERROR) << "EnableSyncForDatatype(): service() is null.";
    return false;
  }

  if (!syncer::UserSelectableTypes().Has(datatype)) {
    LOG(ERROR) << "Can only enable user selectable types, requested "
               << syncer::ModelTypeToString(datatype);
    return false;
  }

  syncer::ModelTypeSet synced_datatypes = service()->GetPreferredDataTypes();
  if (synced_datatypes.Has(datatype)) {
    DVLOG(1) << "EnableSyncForDatatype(): Sync already enabled for datatype "
             << syncer::ModelTypeToString(datatype)
             << " on " << profile_debug_name_ << ".";
    return true;
  }

  synced_datatypes.Put(syncer::ModelTypeFromInt(datatype));
  synced_datatypes.RetainAll(syncer::UserSelectableTypes());
  service()->OnUserChoseDatatypes(false, synced_datatypes);
  if (AwaitSyncSetupCompletion(/*skip_passphrase_verification=*/false)) {
    DVLOG(1) << "EnableSyncForDatatype(): Enabled sync for datatype "
             << syncer::ModelTypeToString(datatype)
             << " on " << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableSyncForDatatype failed");
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForDatatype(
    syncer::ModelType datatype) {
  DVLOG(1) << GetClientInfoString(
      "DisableSyncForDatatype("
      + std::string(syncer::ModelTypeToString(datatype)) + ")");

  if (service() == nullptr) {
    LOG(ERROR) << "DisableSyncForDatatype(): service() is null.";
    return false;
  }

  if (!syncer::UserSelectableTypes().Has(datatype)) {
    LOG(ERROR) << "Can only disable user selectable types, requested "
               << syncer::ModelTypeToString(datatype);
    return false;
  }

  syncer::ModelTypeSet synced_datatypes = service()->GetPreferredDataTypes();
  if (!synced_datatypes.Has(datatype)) {
    DVLOG(1) << "DisableSyncForDatatype(): Sync already disabled for datatype "
             << syncer::ModelTypeToString(datatype)
             << " on " << profile_debug_name_ << ".";
    return true;
  }

  // Disable unified consent first as otherwise disabling sync is not possible.
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    UnifiedConsentServiceFactory::GetForProfile(profile_)
        ->SetUnifiedConsentGiven(false);
  }

  synced_datatypes.RetainAll(syncer::UserSelectableTypes());
  synced_datatypes.Remove(datatype);
  service()->OnUserChoseDatatypes(false, synced_datatypes);
  if (AwaitSyncSetupCompletion(/*skip_passphrase_verification=*/false)) {
    DVLOG(1) << "DisableSyncForDatatype(): Disabled sync for datatype "
             << syncer::ModelTypeToString(datatype)
             << " on " << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("DisableSyncForDatatype failed");
  return false;
}

bool ProfileSyncServiceHarness::EnableSyncForAllDatatypes() {
  DVLOG(1) << GetClientInfoString("EnableSyncForAllDatatypes");

  if (!IsSyncEnabledByUser()) {
    bool result = SetupSync();
    // If SetupSync() succeeded, then Sync must now be enabled.
    DCHECK(!result || IsSyncEnabledByUser());
    return result;
  }

  if (service() == nullptr) {
    LOG(ERROR) << "EnableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // Setting unified consent given to true will enable all sync data types.
    UnifiedConsentServiceFactory::GetForProfile(profile_)
        ->SetUnifiedConsentGiven(true);
  } else {
    service()->OnUserChoseDatatypes(true, syncer::UserSelectableTypes());
  }
  if (AwaitSyncSetupCompletion(/*skip_passphrase_verification=*/false)) {
    DVLOG(1) << "EnableSyncForAllDatatypes(): Enabled sync for all datatypes "
             << "on " << profile_debug_name_ << ".";
    return true;
  }

  DVLOG(0) << GetClientInfoString("EnableSyncForAllDatatypes failed");
  return false;
}

bool ProfileSyncServiceHarness::DisableSyncForAllDatatypes() {
  DVLOG(1) << GetClientInfoString("DisableSyncForAllDatatypes");

  if (service() == nullptr) {
    LOG(ERROR) << "DisableSyncForAllDatatypes(): service() is null.";
    return false;
  }

  service()->RequestStop(ProfileSyncService::CLEAR_DATA);

  DVLOG(1) << "DisableSyncForAllDatatypes(): Disabled sync for all "
           << "datatypes on " << profile_debug_name_;
  return true;
}

SyncCycleSnapshot ProfileSyncServiceHarness::GetLastCycleSnapshot() const {
  DCHECK(service() != nullptr) << "Sync service has not yet been set up.";
  if (service()->IsSyncFeatureActive()) {
    return service()->GetLastCycleSnapshot();
  }
  return SyncCycleSnapshot();
}

std::string ProfileSyncServiceHarness::GetServiceStatus() {
  std::unique_ptr<base::DictionaryValue> value(
      syncer::sync_ui_util::ConstructAboutInformation(service(),
                                                      chrome::GetChannel()));
  std::string service_status;
  base::JSONWriter::WriteWithOptions(
      *value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &service_status);
  return service_status;
}

// TODO(sync): Clean up this method in a separate CL. Remove all snapshot fields
// and log shorter, more meaningful messages.
std::string ProfileSyncServiceHarness::GetClientInfoString(
    const std::string& message) const {
  std::stringstream os;
  os << profile_debug_name_ << ": " << message << ": ";
  if (service()) {
    const SyncCycleSnapshot& snap = GetLastCycleSnapshot();
    syncer::SyncStatus status;
    service()->QueryDetailedSyncStatus(&status);
    // Capture select info from the sync session snapshot and syncer status.
    os << ", has_unsynced_items: " << snap.has_remaining_local_changes()
       << ", did_commit: "
       << (snap.model_neutral_state().num_successful_commits == 0 &&
           snap.model_neutral_state().commit_result == syncer::SYNCER_OK)
       << ", encryption conflicts: " << snap.num_encryption_conflicts()
       << ", hierarchy conflicts: " << snap.num_hierarchy_conflicts()
       << ", server conflicts: " << snap.num_server_conflicts()
       << ", num_updates_downloaded : "
       << snap.model_neutral_state().num_updates_downloaded_total
       << ", passphrase_required_reason: "
       << syncer::PassphraseRequiredReasonToString(
              service()->passphrase_required_reason_for_test())
       << ", notifications_enabled: " << status.notifications_enabled
       << ", service_is_active: " << service()->IsSyncFeatureActive();
  } else {
    os << "Sync service not available";
  }
  return os.str();
}

bool ProfileSyncServiceHarness::IsSyncEnabledByUser() const {
  return service()->IsFirstSetupComplete() &&
         !service()->HasDisableReason(
             ProfileSyncService::DISABLE_REASON_USER_CHOICE);
}
