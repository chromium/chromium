// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/kcer/nssdb_migration/kcer_rollback_helper.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/chaps_support.h"

namespace kcer::internal {
namespace {
const char kDefaultErrorMessage[] = "NssDbClientCertsRollback aborted ";
const char kNssDbClientCertsRollbackMessage[] = "NssDbClientCertsRollback ";

extern "C" SECStatus PK11_ClearCertsCache(PK11SlotInfo* slot_info)
    __attribute__((weak));

NssDbClientCertsRollbackEvent GetListSizeEvent(
    const std::vector<SessionChapsClient::ObjectHandle>& handles_list) {
  switch (handles_list.size()) {
    case 0u:
      return NssDbClientCertsRollbackEvent::kRollbackListSize0;
    case 1u:
      return NssDbClientCertsRollbackEvent::kRollbackListSize1;
    case 2u:
      return NssDbClientCertsRollbackEvent::kRollbackListSize2;
    case 3u:
      return NssDbClientCertsRollbackEvent::kRollbackListSize3;
    default:
      return NssDbClientCertsRollbackEvent::kRollbackListSizeAbove3;
  }
}
}  // namespace

void RecordUmaEvent(NssDbClientCertsRollbackEvent event) {
  base::UmaHistogramEnumeration(kNssDbClientCertsRollback, event);
}

void ResetCertCacheData(SessionChapsClient::SlotId slot_id) {
  if (!PK11_ClearCertsCache) {
    return;
  }
  crypto::ScopedPK11SlotList slot_list(PK11_GetAllTokens(
      CKM_INVALID_MECHANISM, PR_FALSE, PR_FALSE, /**wincx*/ nullptr));
  if (!slot_list) {
    return;
  }
  for (PK11SlotListElement* elem = slot_list->head; elem; elem = elem->next) {
    crypto::ScopedPK11Slot slot_info =
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(elem->slot));

    if (!crypto::IsSlotProvidedByChaps(slot_info.get())) {
      continue;
    }

    kcer::SessionChapsClient::SlotId cur_slot_id =
        kcer::SessionChapsClient::SlotId(PK11_GetSlotID(slot_info.get()));

    if (slot_id == cur_slot_id) {
      SECStatus result = PK11_ClearCertsCache(slot_info.get());
      if (result != SECSuccess) {
        RecordUmaEvent(NssDbClientCertsRollbackEvent::kCertCacheResetFailed);
        LOG(ERROR) << "Resetting slot certificates cache has failed with:"
                   << result;
      } else {
        RecordUmaEvent(
            NssDbClientCertsRollbackEvent::kCertCacheResetSuccessful);
      }
      break;
    }
  }
}

KcerRollbackHelper::KcerRollbackHelper(
    HighLevelChapsClient* high_level_chaps_client,
    PrefService* prefs_service)
    : high_level_chaps_client_(high_level_chaps_client),
      prefs_service_(prefs_service) {}

KcerRollbackHelper::~KcerRollbackHelper() = default;

// static
bool KcerRollbackHelper::IsChapsRollbackRequired(PrefService* pref_service) {
  if (!pref_service) {
    return false;
  }
  bool is_only_rollback_active =
      ash::features::IsNssDbClientCertsRollbackEnabled() &&
      !chromeos::features::IsPkcs12ToChapsDualWriteEnabled();

  if (is_only_rollback_active) {
    const PrefService::Preference* rollback_flag =
        pref_service->FindPreference(prefs::kNssChapsDualWrittenCertsExist);
    if (!rollback_flag) {
      RecordUmaEvent(NssDbClientCertsRollbackEvent::kRollbackFlagNotPresent);
      return false;
    }
    RecordUmaEvent(NssDbClientCertsRollbackEvent::kRollbackFlagPresent);
    return rollback_flag->GetValue()->GetBool();
  }
  return false;
}

std::optional<AccountId> GetAccountId() {
  if (!user_manager::UserManager::IsInitialized()) {
    LOG(ERROR) << kDefaultErrorMessage << "user manager is not initialised!";
    return std::nullopt;
  }

  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!active_user) {
    LOG(ERROR) << kDefaultErrorMessage << "no active user!";
    return std::nullopt;
  }
  return std::make_optional(active_user->GetAccountId());
}

void KcerRollbackHelper::PerformRollback() const {
  RecordUmaEvent(NssDbClientCertsRollbackEvent::kRollbackScheduled);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&KcerRollbackHelper::FindUserToken,
                     weak_factory_.GetWeakPtr()),
      base::Seconds(30));
}

void KcerRollbackHelper::FindUserToken() const {
  RecordUmaEvent(NssDbClientCertsRollbackEvent::kRollbackStarted);
  std::optional<AccountId> account_id = GetAccountId();
  if (!account_id.has_value()) {
    LOG(ERROR) << kDefaultErrorMessage << "no account_id";
    RecordUmaEvent(NssDbClientCertsRollbackEvent::kFailedNoUserAccountId);
    return;
  }

  std::unique_ptr<ash::TPMTokenInfoGetter> scoped_user_token_info_getter =
      ash::TPMTokenInfoGetter::CreateForUserToken(
          account_id.value(), ash::CryptohomePkcs11Client::Get(),
          base::SingleThreadTaskRunner::GetCurrentDefault());

  ash::TPMTokenInfoGetter* user_token_info_getter =
      scoped_user_token_info_getter.get();
  // Pass scoped_user_token_info_getter to keep it alive.
  user_token_info_getter->Start(base::BindOnce(
      &KcerRollbackHelper::FindUserSlotId, weak_factory_.GetWeakPtr(),
      std::move(scoped_user_token_info_getter)));
}

void KcerRollbackHelper::FindUserSlotId(
    std::unique_ptr<ash::TPMTokenInfoGetter> scoped_user_token_info_getter,
    std::optional<user_data_auth::TpmTokenInfo> user_token_info) const {
  SessionChapsClient::SlotId user_slot_id;
  if (user_token_info) {
    user_slot_id = SessionChapsClient::SlotId(
        static_cast<uint64_t>(user_token_info->slot()));
  } else {
    LOG(ERROR) << kDefaultErrorMessage << "no slot info was found";
    RecordUmaEvent(NssDbClientCertsRollbackEvent::kFailedNoSlotInfoFound);
    return;
  }
  SelectAndDeleteDoubleWrittenObjects(user_slot_id);
}

void KcerRollbackHelper::SelectAndDeleteDoubleWrittenObjects(
    SessionChapsClient::SlotId slot_id) const {
  constexpr chromeos::PKCS11_CK_BBOOL kTrue = chromeos::PKCS11_CK_TRUE;

  chaps::AttributeList attributes;
  kcer::AddAttribute(attributes,
                     pkcs11_custom_attributes::kCkaChromeOsMigratedFromNss,
                     kcer::MakeSpan(&kTrue));

  SessionChapsClient::FindObjectsCallback find_callback =
      base::BindOnce(&KcerRollbackHelper::DestroyObjectsInSlot,
                     weak_factory_.GetWeakPtr(), slot_id);
  return high_level_chaps_client_->FindObjects(slot_id, std::move(attributes),
                                               std::move(find_callback));
}

void KcerRollbackHelper::DestroyObjectsInSlot(
    SessionChapsClient::SlotId slot_id,
    std::vector<SessionChapsClient::ObjectHandle> handles_list,
    uint32_t result_code) const {
  RecordUmaEvent(GetListSizeEvent(handles_list));
  SessionChapsClient::DestroyObjectCallback destroy_objects_callback =
      base::BindOnce(&KcerRollbackHelper::ResetCacheAndRollbackFlag,
                     weak_factory_.GetWeakPtr(), slot_id);

  return high_level_chaps_client_->DestroyObjectsWithRetries(
      slot_id, handles_list, std::move(destroy_objects_callback));
}

void KcerRollbackHelper::ResetCacheAndRollbackFlag(
    SessionChapsClient::SlotId slot_id,
    uint32_t result_code) const {
  ResetCertCacheData(slot_id);
  if (result_code != chromeos::PKCS11_CKR_OK) {
    LOG(ERROR) << "Not all objects were deleted due to" << result_code;
    RecordUmaEvent(NssDbClientCertsRollbackEvent::kFailedNotAllObjectsDeleted);
    return;
  }

  const PrefService::Preference* rollback_flag =
      prefs_service_->FindPreference(prefs::kNssChapsDualWrittenCertsExist);
  if (!rollback_flag) {
    LOG(ERROR) << "Resetting " << kNssDbClientCertsRollbackMessage
               << "flag while it was not set";
    RecordUmaEvent(
        NssDbClientCertsRollbackEvent::kFailedFlagResetNotSuccessful);
    return;
  }
  prefs_service_->ClearPref(prefs::kNssChapsDualWrittenCertsExist);
  RecordUmaEvent(NssDbClientCertsRollbackEvent::kRollbackSuccessful);
}

}  // namespace kcer::internal
