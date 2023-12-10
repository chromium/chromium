// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/wallet_helper.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_progress_marker.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"

using autofill::AutofillMetadata;
using autofill::AutofillTable;
using autofill::AutofillWebDataService;
using autofill::CreditCard;
using autofill::CreditCardCloudTokenData;
using autofill::PaymentsCustomerData;
using autofill::PersonalDataManager;
using autofill::data_util::TruncateUTF8;
using sync_datatype_helper::test;

namespace {

// Constants for the credit card.
const int kDefaultCardExpMonth = 8;
const int kDefaultCardExpYear = 2087;
const char kDefaultCardLastFour[] = "1234";
const char16_t kDefaultCardLastFour16[] = u"1234";
const char kDefaultCardName[] = "Patrick Valenzuela";
const char16_t kDefaultCardName16[] = u"Patrick Valenzuela";
const sync_pb::WalletMaskedCreditCard_WalletCardType kDefaultCardType =
    sync_pb::WalletMaskedCreditCard::AMEX;

template <class Item>
bool ListsMatch(int profile_a,
                const std::vector<Item*>& list_a,
                int profile_b,
                const std::vector<Item*>& list_b) {
  std::map<std::string, Item> list_a_map;
  for (Item* item : list_a) {
    list_a_map[item->server_id()] = *item;
  }

  // This seems to be a transient state that will eventually be rectified by
  // model type logic. We don't need to check b for duplicates directly because
  // after the first is erased from |autofill_profiles_a_map| the second will
  // not be found.
  if (list_a.size() != list_a_map.size()) {
    DVLOG(1) << "Profile " << profile_a << " contains duplicate server_ids.";
    return false;
  }

  for (Item* item : list_b) {
    if (!list_a_map.count(item->server_id())) {
      DVLOG(1) << "GUID " << item->server_id() << " not found in profile "
               << profile_b << ".";
      return false;
    }
    Item* expected_item = &list_a_map[item->server_id()];
    if (expected_item->Compare(*item) != 0 ||
        expected_item->use_count() != item->use_count() ||
        expected_item->use_date() != item->use_date()) {
      DVLOG(1) << "Mismatch in profile with server_id " << item->server_id()
               << ".";
      return false;
    }
    list_a_map.erase(item->server_id());
  }

  if (!list_a_map.empty()) {
    DVLOG(1) << "Entries present in profile " << profile_a
             << " but not in profile" << profile_b << ".";
    return false;
  }
  return true;
}

template <class Item>
void LogLists(const std::vector<Item*>& list_a,
              const std::vector<Item*>& list_b) {
  int x = 0;
  for (Item* item : list_a) {
    DVLOG(1) << "A#" << x++ << " " << *item;
  }
  x = 0;
  for (Item* item : list_b) {
    DVLOG(1) << "B#" << x++ << " " << *item;
  }
}

bool WalletDataAndMetadataMatch(
    int profile_a,
    const std::vector<CreditCard*>& server_cards_a,
    int profile_b,
    const std::vector<CreditCard*>& server_cards_b) {
  if (!ListsMatch(profile_a, server_cards_a, profile_b, server_cards_b)) {
    LogLists(server_cards_a, server_cards_b);
    return false;
  }
  return true;
}

void WaitForCurrentTasksToComplete(base::SequencedTaskRunner* task_runner) {
  // We are fine with the UI thread getting blocked. If using RunLoop here, in
  // some uses of this functions, we would get nested RunLoops that tend to
  // cause troubles. This is a more robust solution.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                                  base::Unretained(&event)));
  event.Wait();
}

void WaitForPDMToRefresh(int profile) {
  PersonalDataManager* pdm = wallet_helper::GetPersonalDataManager(profile);
  // Get the latest values from the DB: (1) post tasks on the DB sequence; (2)
  // wait for the DB sequence tasks to finish; (3) wait for the UI sequence
  // tasks to finish (that update the in-memory cache in PDM).
  pdm->Refresh();
  WaitForCurrentTasksToComplete(
      wallet_helper::GetProfileWebDataService(profile)->GetDBTaskRunner());
  base::RunLoop().RunUntilIdle();
}

void SetServerCardsOnDBSequence(AutofillWebDataService* wds,
                                const std::vector<CreditCard>& credit_cards) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  AutofillTable::FromWebDatabase(wds->GetDatabase())
      ->SetServerCreditCards(credit_cards);
}

void SetPaymentsCustomerDataOnDBSequence(
    AutofillWebDataService* wds,
    const PaymentsCustomerData& customer_data) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  AutofillTable::FromWebDatabase(wds->GetDatabase())
      ->SetPaymentsCustomerData(&customer_data);
}

void SetCreditCardCloudTokenDataOnDBSequence(
    AutofillWebDataService* wds,
    const std::vector<CreditCardCloudTokenData>& cloud_token_data) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  AutofillTable::FromWebDatabase(wds->GetDatabase())
      ->SetCreditCardCloudTokenData(cloud_token_data);
}

void GetServerCardsMetadataOnDBSequence(
    AutofillWebDataService* wds,
    std::vector<AutofillMetadata>* cards_metadata) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  AutofillTable::FromWebDatabase(wds->GetDatabase())
      ->GetServerCardsMetadata(*cards_metadata);
}

void GetModelTypeStateOnDBSequence(syncer::ModelType model_type,
                                   AutofillWebDataService* wds,
                                   sync_pb::ModelTypeState* model_type_state) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  syncer::MetadataBatch metadata_batch;
  AutofillTable::FromWebDatabase(wds->GetDatabase())
      ->GetAllSyncMetadata(model_type, &metadata_batch);
  *model_type_state = metadata_batch.GetModelTypeState();
}

}  // namespace

namespace wallet_helper {

PersonalDataManager* GetPersonalDataManager(int index) {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      test()->GetProfile(index));
}

scoped_refptr<AutofillWebDataService> GetProfileWebDataService(int index) {
  return WebDataServiceFactory::GetAutofillWebDataForProfile(
      test()->GetProfile(index), ServiceAccessType::EXPLICIT_ACCESS);
}

scoped_refptr<AutofillWebDataService> GetAccountWebDataService(int index) {
  return WebDataServiceFactory::GetAutofillWebDataForAccount(
      test()->GetProfile(index), ServiceAccessType::EXPLICIT_ACCESS);
}

void SetServerCreditCards(
    int profile,
    const std::vector<autofill::CreditCard>& credit_cards) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SetServerCardsOnDBSequence,
                                base::Unretained(wds.get()), credit_cards));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

void SetPaymentsCustomerData(
    int profile,
    const autofill::PaymentsCustomerData& customer_data) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SetPaymentsCustomerDataOnDBSequence,
                                base::Unretained(wds.get()), customer_data));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

void SetCreditCardCloudTokenData(
    int profile,
    const std::vector<autofill::CreditCardCloudTokenData>& cloud_token_data) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SetCreditCardCloudTokenDataOnDBSequence,
                                base::Unretained(wds.get()), cloud_token_data));
}

void UpdateServerCardMetadata(int profile, const CreditCard& credit_card) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->UpdateServerCardMetadata(credit_card);
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

std::vector<AutofillMetadata> GetServerCardsMetadata(int profile) {
  std::vector<AutofillMetadata> cards_metadata;
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GetServerCardsMetadataOnDBSequence,
                                base::Unretained(wds.get()), &cards_metadata));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
  return cards_metadata;
}

sync_pb::ModelTypeState GetWalletModelTypeState(syncer::ModelType model_type,
                                                int profile) {
  DCHECK(model_type == syncer::AUTOFILL_WALLET_DATA ||
         model_type == syncer::AUTOFILL_WALLET_OFFER);
  sync_pb::ModelTypeState result;
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GetModelTypeStateOnDBSequence, model_type,
                                base::Unretained(wds.get()), &result));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
  return result;
}

void UnmaskServerCard(int profile,
                      const CreditCard& credit_card,
                      const std::u16string& full_number) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->UnmaskServerCreditCard(credit_card, full_number);
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

sync_pb::SyncEntity CreateDefaultSyncWalletCard() {
  return CreateSyncWalletCard(kDefaultCardID, kDefaultCardLastFour,
                              kDefaultBillingAddressID);
}

sync_pb::SyncEntity CreateSyncWalletCard(const std::string& name,
                                         const std::string& last_four,
                                         const std::string& billing_address_id,
                                         const std::string& nickname,
                                         int64_t instrument_id) {
  sync_pb::SyncEntity entity;
  entity.set_name(name);
  entity.set_id_string(name);
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      entity.mutable_specifics()->mutable_autofill_wallet();
  wallet_specifics->set_type(
      sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* credit_card =
      wallet_specifics->mutable_masked_card();
  credit_card->set_id(name);
  credit_card->set_exp_month(kDefaultCardExpMonth);
  credit_card->set_exp_year(kDefaultCardExpYear);
  credit_card->set_last_four(last_four);
  credit_card->set_name_on_card(kDefaultCardName);
  credit_card->set_type(kDefaultCardType);
  credit_card->set_instrument_id(instrument_id);
  if (!billing_address_id.empty()) {
    credit_card->set_billing_address_id(billing_address_id);
  }
  if (!nickname.empty()) {
    credit_card->set_nickname(nickname);
  }
  return entity;
}

sync_pb::SyncEntity CreateSyncPaymentsCustomerData(
    const std::string& customer_id) {
  sync_pb::SyncEntity entity;
  entity.set_name(customer_id);
  entity.set_id_string(customer_id);
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      entity.mutable_specifics()->mutable_autofill_wallet();
  wallet_specifics->set_type(sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA);

  sync_pb::PaymentsCustomerData* customer_data =
      wallet_specifics->mutable_customer_data();
  customer_data->set_id(customer_id);
  return entity;
}

sync_pb::SyncEntity CreateDefaultSyncPaymentsCustomerData() {
  return CreateSyncPaymentsCustomerData(kDefaultCustomerID);
}

CreditCard GetDefaultCreditCard() {
  CreditCard card(CreditCard::RecordType::kMaskedServerCard, kDefaultCardID);
  card.SetExpirationMonth(kDefaultCardExpMonth);
  card.SetExpirationYear(kDefaultCardExpYear);
  card.SetNumber(kDefaultCardLastFour16);
  card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL, kDefaultCardName16);
  card.SetNetworkForMaskedCard(autofill::kAmericanExpressCard);
  card.set_billing_address_id(kDefaultBillingAddressID);
  return card;
}

sync_pb::SyncEntity CreateSyncCreditCardCloudTokenData(
    const std::string& cloud_token_data_id) {
  sync_pb::SyncEntity entity;
  entity.set_name(cloud_token_data_id);
  entity.set_id_string(cloud_token_data_id);
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      entity.mutable_specifics()->mutable_autofill_wallet();
  wallet_specifics->set_type(
      sync_pb::AutofillWalletSpecifics::CREDIT_CARD_CLOUD_TOKEN_DATA);
  sync_pb::WalletCreditCardCloudTokenData* cloud_token_data =
      wallet_specifics->mutable_cloud_token_data();
  cloud_token_data->set_instrument_token(cloud_token_data_id);
  return entity;
}

sync_pb::SyncEntity CreateDefaultSyncCreditCardCloudTokenData() {
  return CreateSyncCreditCardCloudTokenData(kDefaultCreditCardCloudTokenDataID);
}

void ExpectDefaultCreditCardValues(const CreditCard& card) {
  EXPECT_EQ(CreditCard::RecordType::kMaskedServerCard, card.record_type());
  EXPECT_EQ(kDefaultCardID, card.server_id());
  EXPECT_EQ(kDefaultCardLastFour16, card.LastFourDigits());
  EXPECT_EQ(autofill::kAmericanExpressCard, card.network());
  EXPECT_EQ(kDefaultCardExpMonth, card.expiration_month());
  EXPECT_EQ(kDefaultCardExpYear, card.expiration_year());
  EXPECT_EQ(kDefaultCardName16,
            card.GetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(kDefaultBillingAddressID, card.billing_address_id());
}

std::vector<CreditCard*> GetServerCreditCards(int profile) {
  WaitForPDMToRefresh(profile);
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  return pdm->GetServerCreditCards();
}

}  // namespace wallet_helper

AutofillWalletChecker::AutofillWalletChecker(int profile_a, int profile_b)
    : profile_a_(profile_a), profile_b_(profile_b) {
  wallet_helper::GetPersonalDataManager(profile_a_)->AddObserver(this);
  wallet_helper::GetPersonalDataManager(profile_b_)->AddObserver(this);
}

AutofillWalletChecker::~AutofillWalletChecker() {
  wallet_helper::GetPersonalDataManager(profile_a_)->RemoveObserver(this);
  wallet_helper::GetPersonalDataManager(profile_b_)->RemoveObserver(this);
}

bool AutofillWalletChecker::Wait() {
  // We need to make sure we are not reading before any locally instigated async
  // writes. This is run exactly one time before the first
  // IsExitConditionSatisfied() is called.
  WaitForPDMToRefresh(profile_a_);
  WaitForPDMToRefresh(profile_b_);
  return StatusChangeChecker::Wait();
}

bool AutofillWalletChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for matching autofill wallet cards and addresses";
  autofill::PersonalDataManager* pdm_a =
      wallet_helper::GetPersonalDataManager(profile_a_);
  autofill::PersonalDataManager* pdm_b =
      wallet_helper::GetPersonalDataManager(profile_b_);
  return WalletDataAndMetadataMatch(profile_a_, pdm_a->GetServerCreditCards(),
                                    profile_b_, pdm_b->GetServerCreditCards());
}

void AutofillWalletChecker::OnPersonalDataChanged() {
  CheckExitCondition();
}

AutofillWalletMetadataSizeChecker::AutofillWalletMetadataSizeChecker(
    int profile_a,
    int profile_b)
    : profile_a_(profile_a), profile_b_(profile_b) {
  wallet_helper::GetPersonalDataManager(profile_a_)->AddObserver(this);
  wallet_helper::GetPersonalDataManager(profile_b_)->AddObserver(this);
}

AutofillWalletMetadataSizeChecker::~AutofillWalletMetadataSizeChecker() {
  wallet_helper::GetPersonalDataManager(profile_a_)->RemoveObserver(this);
  wallet_helper::GetPersonalDataManager(profile_b_)->RemoveObserver(this);
}

bool AutofillWalletMetadataSizeChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for matching autofill wallet metadata sizes";
  // This checker used to be flaky (crbug.com/921386) because of using RunLoops
  // to load synchronously data from the DB in IsExitConditionSatisfiedImpl.
  // Such a waiting RunLoop often processed another OnPersonalDataChanged() call
  // resulting in nested RunLoops. This should be avoided now by blocking using
  // WaitableEvent, instead. This check enforces that we do not nest it anymore.
  DCHECK(!checking_exit_condition_in_flight_)
      << "There should be no nested calls for "
         "IsExitConditionSatisfied(std::ostream* os)";
  checking_exit_condition_in_flight_ = true;
  bool exit_condition_is_satisfied = IsExitConditionSatisfiedImpl();
  checking_exit_condition_in_flight_ = false;
  return exit_condition_is_satisfied;
}

void AutofillWalletMetadataSizeChecker::OnPersonalDataChanged() {
  CheckExitCondition();
}

bool AutofillWalletMetadataSizeChecker::IsExitConditionSatisfiedImpl() {
  // There could be trailing metadata left on one of the clients. Check that
  // metadata.size() is the same on both clients.
  std::vector<AutofillMetadata> cards_metadata_a =
      wallet_helper::GetServerCardsMetadata(profile_a_);
  std::vector<AutofillMetadata> cards_metadata_b =
      wallet_helper::GetServerCardsMetadata(profile_b_);
  if (cards_metadata_a.size() != cards_metadata_b.size()) {
    DVLOG(1) << "Server cards metadata mismatch, expected "
             << cards_metadata_a.size() << ", found "
             << cards_metadata_b.size();
    return false;
  }
  return true;
}

FullUpdateTypeProgressMarkerChecker::FullUpdateTypeProgressMarkerChecker(
    base::Time min_required_progress_marker_timestamp,
    syncer::SyncService* service,
    syncer::ModelType model_type)
    : min_required_progress_marker_timestamp_(
          min_required_progress_marker_timestamp),
      service_(service),
      model_type_(model_type) {
  scoped_observation_.Observe(service);
}

FullUpdateTypeProgressMarkerChecker::~FullUpdateTypeProgressMarkerChecker() =
    default;

bool FullUpdateTypeProgressMarkerChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  // GetLastCycleSnapshot() returns by value, so make sure to capture it for
  // iterator use.
  const syncer::SyncCycleSnapshot snap =
      service_->GetLastCycleSnapshotForDebugging();
  const syncer::ProgressMarkerMap& progress_markers =
      snap.download_progress_markers();
  auto marker_it = progress_markers.find(model_type_);
  if (marker_it == progress_markers.end()) {
    *os << "Waiting for an updated progress marker timestamp "
        << min_required_progress_marker_timestamp_
        << "; actual: no progress marker in last sync cycle";
    return false;
  }

  sync_pb::DataTypeProgressMarker progress_marker;
  bool success = progress_marker.ParseFromString(marker_it->second);
  DCHECK(success);

  const base::Time actual_timestamp =
      fake_server::FakeServer::GetProgressMarkerTimestamp(progress_marker);

  *os << "Waiting for an updated progress marker timestamp "
      << min_required_progress_marker_timestamp_ << "; actual "
      << actual_timestamp;

  return actual_timestamp >= min_required_progress_marker_timestamp_;
}

// syncer::SyncServiceObserver implementation.
void FullUpdateTypeProgressMarkerChecker::OnSyncCycleCompleted(
    syncer::SyncService* sync) {
  CheckExitCondition();
}
