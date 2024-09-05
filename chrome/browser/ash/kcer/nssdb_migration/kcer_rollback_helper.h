// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_KCER_NSSDB_MIGRATION_KCER_ROLLBACK_HELPER_H_
#define CHROME_BROWSER_ASH_KCER_NSSDB_MIGRATION_KCER_ROLLBACK_HELPER_H_

#include "ash/components/kcer/chaps/high_level_chaps_client.h"
#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"
#include "components/prefs/pref_service.h"

namespace kcer::internal {

const char kNssDbClientCertsRollback[] = "Ash.KcerRollbackHelper.Events";

// This enum should be kept in sync with the `NssDbClientCertsRollbackEvent`
// in tools/metrics/histograms/metadata/ash/enums.xml.
enum class NssDbClientCertsRollbackEvent {
  kRollbackScheduled = 0,
  kRollbackStarted = 1,
  kRollbackSuccessful = 2,
  kFailedNotAllObjectsDeleted = 3,
  kRollbackFlagPresent = 4,
  kRollbackFlagNotPresent = 5,
  kRollbackListSize0 = 6,
  kRollbackListSize1 = 7,
  kRollbackListSize2 = 8,
  kRollbackListSize3 = 9,
  kRollbackListSizeAbove3 = 10,
  kFailedNoSlotInfoFound = 11,
  kFailedNoUserAccountId = 12,
  kFailedFlagResetNotSuccessful = 13,
  kCertCacheResetSuccessful = 14,
  kCertCacheResetFailed = 15,
  kMaxValue = kCertCacheResetFailed,
};

// Helper class for scheduling and executing rollback from usage of software
// backed chaps client to software NSS DB.
class KcerRollbackHelper final {
 public:
  explicit KcerRollbackHelper(HighLevelChapsClient* high_level_chaps_client,
                              PrefService* prefs_service);
  ~KcerRollbackHelper();

  // Checks experiment status and presence of the rollback flag in
  // users preferences `prefs_service`.
  static bool IsChapsRollbackRequired(PrefService* prefs_service);

  // Schedules rollback execution.
  void PerformRollback() const;

 private:
  // Finds users token information and calls FindUserSlotId().
  void FindUserToken() const;

  // Extracts users slot id from `user_token_info` and pass it to
  // SelectAndDeleteDoubleWrittenObjects() .
  void FindUserSlotId(
      std::unique_ptr<ash::TPMTokenInfoGetter> scoped_user_token_info_getter,
      std::optional<user_data_auth::TpmTokenInfo> user_token_info) const;

  // Selects PKCS11 objects which have special attribute
  // 'kCkaChromeOsMigratedFromNss' from `slot_id` and calls
  // DestroyObjectsInSlot() on them.
  void SelectAndDeleteDoubleWrittenObjects(
      SessionChapsClient::SlotId slot_id) const;

  // Destroys objects from `slot_id` included into `handles` with retry.
  // Calls to callback function with `result` as parameter.
  void DestroyObjectsInSlot(
      SessionChapsClient::SlotId slot_id,
      std::vector<SessionChapsClient::ObjectHandle> handles,
      uint32_t result) const;

  // Resets flag in users preferences if rollback finished successfully based
  // on `result_code`.
  void ResetCacheAndRollbackFlag(SessionChapsClient::SlotId slot_id,
                                 uint32_t result_code) const;

  // This should outlives KcerRollbackHelper.
  raw_ptr<kcer::HighLevelChapsClient> high_level_chaps_client_;
  // This should outlives KcerRollbackHelper.
  raw_ptr<PrefService> prefs_service_;
  base::WeakPtrFactory<KcerRollbackHelper> weak_factory_{this};
};

}  // namespace kcer::internal

#endif  // CHROME_BROWSER_ASH_KCER_NSSDB_MIGRATION_KCER_ROLLBACK_HELPER_H_
