// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/test/integration/device_info_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/model_type.h"
#include "components/sync/invalidations/switches.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/protocol/sync.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::AllOf;
using testing::ElementsAre;
using testing::Not;
using testing::NotNull;

MATCHER_P(HasInterestedDataTypes, expected_data_types, "") {
  syncer::ModelTypeSet data_types;
  for (const int field_number : arg.specifics()
                                    .device_info()
                                    .invalidation_fields()
                                    .interested_data_type_ids()) {
    syncer::ModelType data_type =
        syncer::GetModelTypeFromSpecificsFieldNumber(field_number);
    if (!syncer::IsRealDataType(data_type)) {
      return false;
    }
    data_types.Put(data_type);
  }
  return data_types == expected_data_types;
}

MATCHER(HasInstanceIdToken, "") {
  return arg.specifics()
      .device_info()
      .invalidation_fields()
      .has_instance_id_token();
}

MATCHER_P(HasInstanceIdToken, expected_token, "") {
  return arg.specifics()
             .device_info()
             .invalidation_fields()
             .instance_id_token() == expected_token;
}

class SingleClientWithSyncSendInterestedDataTypesTest : public SyncTest {
 public:
  SingleClientWithSyncSendInterestedDataTypesTest() : SyncTest(SINGLE_CLIENT) {
    override_features_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncSendInterestedDataTypes},
        /*disabled_features=*/{
            switches::kUseSyncInvalidations,
            switches::kUseSyncInvalidationsForWalletAndOffer});
  }
  ~SingleClientWithSyncSendInterestedDataTypesTest() override = default;

 private:
  base::test::ScopedFeatureList override_features_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientWithSyncSendInterestedDataTypesTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientWithSyncSendInterestedDataTypesTest,
                       SendInterestedDataTypesAsPartOfDeviceInfo) {
  ASSERT_TRUE(SetupSync());

  syncer::SyncInvalidationsService* sync_invalidations_service =
      SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_THAT(sync_invalidations_service, NotNull());
  syncer::ModelTypeSet interested_data_types =
      sync_invalidations_service->GetInterestedDataTypes();

  // Check that some "standard" data types are included.
  EXPECT_TRUE(
      interested_data_types.HasAll({syncer::NIGORI, syncer::BOOKMARKS}));
  // Wallet and Offer data types are excluded unless
  // kUseSyncInvalidationsForWalletAndOffer is also enabled.
  EXPECT_FALSE(interested_data_types.Has(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_FALSE(interested_data_types.Has(syncer::AUTOFILL_WALLET_OFFER));

  // The local device should eventually be committed to the server.
  // The InstanceID token should only be uploaded if kUseSyncInvalidations is
  // also enabled.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(),
          ElementsAre(AllOf(HasInterestedDataTypes(interested_data_types),
                            Not(HasInstanceIdToken()))))
          .Wait());
}

class SingleClientWithUseSyncInvalidationsTest : public SyncTest {
 public:
  SingleClientWithUseSyncInvalidationsTest() : SyncTest(SINGLE_CLIENT) {
    override_features_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncSendInterestedDataTypes,
                              switches::kUseSyncInvalidations},
        /*disabled_features=*/{
            switches::kUseSyncInvalidationsForWalletAndOffer});
  }
  ~SingleClientWithUseSyncInvalidationsTest() override = default;

 private:
  base::test::ScopedFeatureList override_features_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientWithUseSyncInvalidationsTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientWithUseSyncInvalidationsTest,
                       SendInterestedDataTypesAndFCMTokenAsPartOfDeviceInfo) {
  ASSERT_TRUE(SetupSync());

  syncer::SyncInvalidationsService* sync_invalidations_service =
      SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_THAT(sync_invalidations_service, NotNull());
  syncer::ModelTypeSet interested_data_types =
      sync_invalidations_service->GetInterestedDataTypes();
  std::string fcm_token = sync_invalidations_service->GetFCMRegistrationToken();

  // Check that some "standard" data types are included.
  EXPECT_TRUE(
      interested_data_types.HasAll({syncer::NIGORI, syncer::BOOKMARKS}));
  // Wallet and Offer data types are excluded unless
  // kUseSyncInvalidationsForWalletAndOffer is also enabled.
  EXPECT_FALSE(interested_data_types.Has(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_FALSE(interested_data_types.Has(syncer::AUTOFILL_WALLET_OFFER));
  EXPECT_FALSE(fcm_token.empty());

  // The local device should eventually be committed to the server.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(),
          ElementsAre(AllOf(HasInterestedDataTypes(interested_data_types),
                            HasInstanceIdToken(fcm_token))))
          .Wait());
}

class SingleClientWithUseSyncInvalidationsForWalletAndOfferTest
    : public SyncTest {
 public:
  SingleClientWithUseSyncInvalidationsForWalletAndOfferTest()
      : SyncTest(SINGLE_CLIENT) {
    override_features_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncSendInterestedDataTypes,
                              switches::kUseSyncInvalidations,
                              switches::kUseSyncInvalidationsForWalletAndOffer},
        /*disabled_features=*/{});
  }
  ~SingleClientWithUseSyncInvalidationsForWalletAndOfferTest() override =
      default;

 private:
  base::test::ScopedFeatureList override_features_;

  DISALLOW_COPY_AND_ASSIGN(
      SingleClientWithUseSyncInvalidationsForWalletAndOfferTest);
};

IN_PROC_BROWSER_TEST_F(
    SingleClientWithUseSyncInvalidationsForWalletAndOfferTest,
    SendInterestedDataTypesAndFCMTokenAsPartOfDeviceInfo) {
  ASSERT_TRUE(SetupSync());

  syncer::SyncInvalidationsService* sync_invalidations_service =
      SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_THAT(sync_invalidations_service, NotNull());
  syncer::ModelTypeSet interested_data_types =
      sync_invalidations_service->GetInterestedDataTypes();
  std::string fcm_token = sync_invalidations_service->GetFCMRegistrationToken();

  // Check that some "standard" data types are included.
  EXPECT_TRUE(
      interested_data_types.HasAll({syncer::NIGORI, syncer::BOOKMARKS}));
  // Wallet data type should be included by default if
  // kUseSyncInvalidationsForWalletAndOffer is enabled.
  EXPECT_TRUE(interested_data_types.Has(syncer::AUTOFILL_WALLET_DATA));
  EXPECT_FALSE(fcm_token.empty());

  // The local device should eventually be committed to the server.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(),
          ElementsAre(AllOf(HasInterestedDataTypes(interested_data_types),
                            HasInstanceIdToken(fcm_token))))
          .Wait());
}

}  // namespace
