// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_WALLET_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_WALLET_HELPER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
struct AutofillMetadata;
class AutofillProfile;
class AutofillWebDataService;
class CreditCard;
struct CreditCardCloudTokenData;
struct PaymentsCustomerData;
class PersonalDataManager;
}  // namespace autofill

namespace sync_pb {
class SyncEntity;
class ModelType;
class ModelTypeState;
}  // namespace sync_pb

namespace wallet_helper {

inline constexpr char kDefaultCardID[] = "wallet card ID";
inline constexpr char kDefaultAddressID[] = "wallet address ID";
inline constexpr char kDefaultCustomerID[] = "deadbeef";
inline constexpr char kDefaultBillingAddressID[] = "billing address entity ID";
inline constexpr char kDefaultCreditCardCloudTokenDataID[] =
    "cloud token data ID";

// Used to access the personal data manager within a particular sync profile.
[[nodiscard]] autofill::PersonalDataManager* GetPersonalDataManager(int index);

// Used to access the web data service within a particular sync profile.
[[nodiscard]] scoped_refptr<autofill::AutofillWebDataService>
GetProfileWebDataService(int index);

// Used to access the account-scoped web data service within a particular sync
// profile.
[[nodiscard]] scoped_refptr<autofill::AutofillWebDataService>
GetAccountWebDataService(int index);

void SetServerCreditCards(
    int profile,
    const std::vector<autofill::CreditCard>& credit_cards);

void SetServerProfiles(int profile,
                       const std::vector<autofill::AutofillProfile>& profiles);

void SetPaymentsCustomerData(
    int profile,
    const autofill::PaymentsCustomerData& customer_data);

void SetCreditCardCloudTokenData(
    int profile,
    const std::vector<autofill::CreditCardCloudTokenData>& cloud_token_data);

void UpdateServerCardMetadata(int profile,
                              const autofill::CreditCard& credit_card);

void UpdateServerAddressMetadata(
    int profile,
    const autofill::AutofillProfile& server_address);

std::map<std::string, autofill::AutofillMetadata> GetServerCardsMetadata(
    int profile);

std::map<std::string, autofill::AutofillMetadata> GetServerAddressesMetadata(
    int profile);

// Function supports AUTOFILL_WALLET_DATA and AUTOFILL_WALLET_OFFER.
sync_pb::ModelTypeState GetWalletModelTypeState(syncer::ModelType type,
                                                int profile);

void UnmaskServerCard(int profile,
                      const autofill::CreditCard& credit_card,
                      const std::u16string& full_number);

sync_pb::SyncEntity CreateDefaultSyncWalletCard();

sync_pb::SyncEntity CreateSyncWalletCard(const std::string& name,
                                         const std::string& last_four,
                                         const std::string& billing_address_id,
                                         const std::string& nickname = "",
                                         int64_t instrument_id = 1);

sync_pb::SyncEntity CreateSyncPaymentsCustomerData(
    const std::string& customer_id);

sync_pb::SyncEntity CreateDefaultSyncPaymentsCustomerData();

autofill::CreditCard GetDefaultCreditCard();

autofill::CreditCard GetCreditCard(const std::string& name,
                                   const std::string& last_four);

sync_pb::SyncEntity CreateDefaultSyncWalletAddress();

sync_pb::SyncEntity CreateSyncWalletAddress(const std::string& name,
                                            const std::string& company);

sync_pb::SyncEntity CreateSyncCreditCardCloudTokenData(
    const std::string& cloud_token_data_id);
sync_pb::SyncEntity CreateDefaultSyncCreditCardCloudTokenData();

// TODO(sebsg): Instead add a function to create a card, and one to inject in
// the server. Then compare the cards directly.
void ExpectDefaultCreditCardValues(const autofill::CreditCard& card);

// TODO(sebsg): Instead add a function to create a profile, and one to inject in
// the server. Then compare the profiles directly.
void ExpectDefaultProfileValues(const autofill::AutofillProfile& profile);

// Load current data from the database of profile |profile|.
std::vector<autofill::AutofillProfile*> GetServerProfiles(int profile);
std::vector<autofill::AutofillProfile*> GetLocalProfiles(int profile);
std::vector<autofill::CreditCard*> GetServerCreditCards(int profile);

}  // namespace wallet_helper

// Checker to block until autofill wallet & server profiles match on both
// profiles and until server profiles got converted to local profiles.
class AutofillWalletChecker : public StatusChangeChecker,
                              public autofill::PersonalDataManagerObserver {
 public:
  AutofillWalletChecker(int profile_a, int profile_b);
  ~AutofillWalletChecker() override;

  // StatusChangeChecker implementation.
  bool Wait() override;
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // autofill::PersonalDataManager implementation.
  void OnPersonalDataChanged() override;

 private:
  const int profile_a_;
  const int profile_b_;
};

// Checker to block until autofill server profiles got converted to local
// profiles.
class AutofillWalletConversionChecker
    : public StatusChangeChecker,
      public autofill::PersonalDataManagerObserver {
 public:
  explicit AutofillWalletConversionChecker(int profile);
  ~AutofillWalletConversionChecker() override;

  // StatusChangeChecker implementation.
  bool Wait() override;
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // autofill::PersonalDataManager implementation.
  void OnPersonalDataChanged() override;

 private:
  const int profile_;
};

// Checker to block until autofill wallet metadata sizes match on both profiles.
class AutofillWalletMetadataSizeChecker
    : public StatusChangeChecker,
      public autofill::PersonalDataManagerObserver {
 public:
  AutofillWalletMetadataSizeChecker(int profile_a, int profile_b);
  ~AutofillWalletMetadataSizeChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // autofill::PersonalDataManager implementation.
  void OnPersonalDataChanged() override;

 private:
  bool IsExitConditionSatisfiedImpl();

  const int profile_a_;
  const int profile_b_;
  bool checking_exit_condition_in_flight_ = false;
};

// Checker to block until a new progress marker with correct timestamp is
// received.
class FullUpdateTypeProgressMarkerChecker : public StatusChangeChecker,
                                            public syncer::SyncServiceObserver {
 public:
  FullUpdateTypeProgressMarkerChecker(
      base::Time min_required_progress_marker_timestamp,
      syncer::SyncService* service,
      syncer::ModelType model_type);
  ~FullUpdateTypeProgressMarkerChecker() override;

  FullUpdateTypeProgressMarkerChecker(
      const FullUpdateTypeProgressMarkerChecker&) = delete;
  FullUpdateTypeProgressMarkerChecker& operator=(
      const FullUpdateTypeProgressMarkerChecker&) = delete;

  // StatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // syncer::SyncServiceObserver:
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

 private:
  const base::Time min_required_progress_marker_timestamp_;
  const raw_ptr<const syncer::SyncService> service_;
  const syncer::ModelType model_type_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      scoped_observation_{this};
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_WALLET_HELPER_H_
