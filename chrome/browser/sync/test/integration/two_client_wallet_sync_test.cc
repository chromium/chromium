// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/wallet_helper.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_http_post_provider.h"
#include "content/public/test/browser_test.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::CreditCard;
using wallet_helper::CreateDefaultSyncPaymentsCustomerData;
using wallet_helper::CreateSyncWalletCard;
using wallet_helper::GetServerCardsMetadata;
using wallet_helper::GetServerCreditCards;
using wallet_helper::kDefaultBillingAddressID;
using wallet_helper::UpdateServerCardMetadata;

const char kDifferentBillingAddressId[] = "another address entity ID";
constexpr char kLocalBillingAddressId[] =
    "local billing address ID has size 36";
constexpr char kLocalBillingAddressId2[] =
    "another local billing address id wow";
static_assert(sizeof(kLocalBillingAddressId) == autofill::kLocalGuidSize + 1,
              "|kLocalBillingAddressId| has to have the right length to be "
              "considered a local guid");
static_assert(sizeof(kLocalBillingAddressId) == sizeof(kLocalBillingAddressId2),
              "|kLocalBillingAddressId2| has to have the right length to be "
              "considered a local guid");

const base::Time kArbitraryDefaultTime =
    base::Time::FromSecondsSinceUnixEpoch(25);
const base::Time kLaterTime = base::Time::FromSecondsSinceUnixEpoch(5000);
const base::Time kEvenLaterTime = base::Time::FromSecondsSinceUnixEpoch(6000);

class TwoClientWalletSyncTest : public SyncTest {
 public:
  TwoClientWalletSyncTest() : SyncTest(TWO_CLIENT) {}

  TwoClientWalletSyncTest(const TwoClientWalletSyncTest&) = delete;
  TwoClientWalletSyncTest& operator=(const TwoClientWalletSyncTest&) = delete;

  ~TwoClientWalletSyncTest() override = default;

  // Needed for AwaitQuiescence().
  bool TestUsesSelfNotifications() override { return true; }

  bool SetupSyncAndInitialize() {
    test_clock_.SetNow(kArbitraryDefaultTime);

    if (!SetupSync()) {
      return false;
    }

    // As this test does not use self notifications, wait for the metadata to
    // converge with the specialized wallet checker.
    return AutofillWalletChecker(0, 1).Wait();
  }

 private:
  autofill::TestAutofillClock test_clock_;
};

IN_PROC_BROWSER_TEST_F(TwoClientWalletSyncTest, UpdateCreditCardMetadata) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSyncAndInitialize());

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1u, credit_cards.size());
  CreditCard card = *credit_cards[0];

  // Simulate using it -- increase both its use count and use date.
  ASSERT_EQ(1u, card.use_count());
  card.set_use_count(2);
  card.set_use_date(kLaterTime);
  UpdateServerCardMetadata(0, card);

  // Wait for the change to propagate.
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(2u, credit_cards[0]->use_count());
  EXPECT_EQ(kLaterTime, credit_cards[0]->use_date());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(2u, credit_cards[0]->use_count());
  EXPECT_EQ(kLaterTime, credit_cards[0]->use_date());
}

IN_PROC_BROWSER_TEST_F(TwoClientWalletSyncTest,
                       UpdateCreditCardMetadataWhileNotSyncing) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSyncAndInitialize());

  // Sumulate going offline on both clients.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1u, credit_cards.size());
  CreditCard card = *credit_cards[0];

  // Simulate using it -- increase both its use count and use date.
  ASSERT_EQ(1u, card.use_count());
  card.set_use_count(2);
  card.set_use_date(kLaterTime);
  UpdateServerCardMetadata(0, card);

  // Simulate going online again.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  // Wait for the change to propagate.
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(2u, credit_cards[0]->use_count());
  EXPECT_EQ(kLaterTime, credit_cards[0]->use_date());

  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(2u, credit_cards[0]->use_count());
  EXPECT_EQ(kLaterTime, credit_cards[0]->use_date());
}

IN_PROC_BROWSER_TEST_F(TwoClientWalletSyncTest,
                       UpdateCreditCardMetadataConflictsWhileNotSyncing) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSyncAndInitialize());

  // Sumulate going offline on both clients.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Increase use stats on both clients, make use count higher on the first
  // client and use date higher on the second client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1u, credit_cards.size());
  CreditCard card = *credit_cards[0];
  ASSERT_EQ(1u, card.use_count());
  card.set_use_count(3);
  card.set_use_date(kLaterTime);
  UpdateServerCardMetadata(0, card);

  credit_cards = GetServerCreditCards(1);
  ASSERT_EQ(1u, credit_cards.size());
  card = *credit_cards[0];
  ASSERT_EQ(1u, card.use_count());
  card.set_use_count(2);
  card.set_use_date(kEvenLaterTime);
  UpdateServerCardMetadata(1, card);

  // Simulate going online again.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  // Wait for the clients to coverge and both resolve the conflicts by taking
  // maxima in both components.
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(3u, credit_cards[0]->use_count());
  EXPECT_EQ(kEvenLaterTime, credit_cards[0]->use_date());
  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(3u, credit_cards[0]->use_count());
  EXPECT_EQ(kEvenLaterTime, credit_cards[0]->use_date());
}

IN_PROC_BROWSER_TEST_F(TwoClientWalletSyncTest,
                       UpdateCreditCardMetadataWithNewBillingAddressId) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            /*billing_address_id=*/""),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSyncAndInitialize());

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1U, credit_cards.size());
  CreditCard card = *credit_cards[0];
  ASSERT_TRUE(card.billing_address_id().empty());

  // Update the billing address.
  card.set_billing_address_id(kDefaultBillingAddressID);
  UpdateServerCardMetadata(0, card);
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  // Make sure both clients have the updated billing_address_id.
  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kDefaultBillingAddressID, credit_cards[0]->billing_address_id());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kDefaultBillingAddressID, credit_cards[0]->billing_address_id());
}

IN_PROC_BROWSER_TEST_F(TwoClientWalletSyncTest,
                       UpdateCreditCardMetadataWithChangedBillingAddressId) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSyncAndInitialize());

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1U, credit_cards.size());
  CreditCard card = *credit_cards[0];

  // Update the billing address.
  ASSERT_EQ(kDefaultBillingAddressID, card.billing_address_id());
  card.set_billing_address_id(kDifferentBillingAddressId);
  UpdateServerCardMetadata(0, card);
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  // Make sure both clients have the updated billing_address_id.
  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kDifferentBillingAddressId, credit_cards[0]->billing_address_id());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kDifferentBillingAddressId, credit_cards[0]->billing_address_id());
}

IN_PROC_BROWSER_TEST_F(
    TwoClientWalletSyncTest,
    UpdateCreditCardMetadataWithChangedBillingAddressId_RemoteToLocal) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSyncAndInitialize());

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1U, credit_cards.size());
  CreditCard card = *credit_cards[0];
  ASSERT_EQ(kDefaultBillingAddressID, card.billing_address_id());

  // Update the billing address (replace a remote profile by a local profile).
  card.set_billing_address_id(kLocalBillingAddressId);
  UpdateServerCardMetadata(0, card);
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  // Make sure both clients have the updated billing_address_id (local profile
  // wins).
  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kLocalBillingAddressId, credit_cards[0]->billing_address_id());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kLocalBillingAddressId, credit_cards[0]->billing_address_id());
}

IN_PROC_BROWSER_TEST_F(
    TwoClientWalletSyncTest,
    UpdateCreditCardMetadataWithChangedBillingAddressId_RemoteToLocalConflict) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSyncAndInitialize());

  // Sumulate going offline on both clients.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Update the billing address id on both clients to different local ids.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1u, credit_cards.size());
  CreditCard card = *credit_cards[0];
  ASSERT_EQ(kDefaultBillingAddressID, card.billing_address_id());
  card.set_billing_address_id(kLocalBillingAddressId);
  card.set_use_date(kLaterTime);
  // We treat the corner-case of merging data after initial sync (with
  // use_count==1) differently, set use-count to a higher value.
  card.set_use_count(2);
  UpdateServerCardMetadata(0, card);

  credit_cards = GetServerCreditCards(1);
  ASSERT_EQ(1u, credit_cards.size());
  card = *credit_cards[0];
  ASSERT_EQ(kDefaultBillingAddressID, card.billing_address_id());
  card.set_billing_address_id(kLocalBillingAddressId2);
  card.set_use_date(kEvenLaterTime);
  // We treat the corner-case of merging data after initial sync (with
  // use_count==1) differently, set use-count to a higher value.
  card.set_use_count(2);
  UpdateServerCardMetadata(1, card);

  // Simulate going online again.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  // Wait for the clients to coverge and both resolve the conflicts by taking
  // maxima in both components.
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kLocalBillingAddressId2, credit_cards[0]->billing_address_id());
  EXPECT_EQ(kEvenLaterTime, credit_cards[0]->use_date());
  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kLocalBillingAddressId2, credit_cards[0]->billing_address_id());
  EXPECT_EQ(kEvenLaterTime, credit_cards[0]->use_date());
}

}  // namespace
