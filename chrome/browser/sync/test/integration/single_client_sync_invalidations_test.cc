// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/device_info_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/sync/base/model_type.h"
#include "components/sync/invalidations/switches.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/bookmark_entity_builder.h"
#include "components/sync/test/fake_server/entity_builder_factory.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::AllOf;
using testing::ElementsAre;
using testing::Not;
using testing::NotNull;
using testing::UnorderedElementsAre;

const char kSyncedBookmarkURL[] = "http://www.mybookmark.com";

MATCHER_P(InterestedDataTypesAre, expected_data_types, "") {
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

MATCHER_P(InterestedDataTypesContain, expected_data_types, "") {
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
  return data_types.HasAll(expected_data_types);
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
  ASSERT_TRUE(sync_invalidations_service->GetInterestedDataTypes());
  const syncer::ModelTypeSet interested_data_types =
      *sync_invalidations_service->GetInterestedDataTypes();

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
          ElementsAre(AllOf(InterestedDataTypesAre(interested_data_types),
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
  ASSERT_TRUE(sync_invalidations_service->GetInterestedDataTypes());
  ASSERT_TRUE(sync_invalidations_service->GetFCMRegistrationToken());
  const syncer::ModelTypeSet interested_data_types =
      *sync_invalidations_service->GetInterestedDataTypes();
  const std::string fcm_token =
      *sync_invalidations_service->GetFCMRegistrationToken();

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
          ElementsAre(AllOf(InterestedDataTypesAre(interested_data_types),
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

  void InjectSyncedBookmark() {
    fake_server::BookmarkEntityBuilder bookmark_builder =
        entity_builder_factory_.NewBookmarkEntityBuilder("My Bookmark");
    GetFakeServer()->InjectEntity(
        bookmark_builder.BuildBookmark(GURL(kSyncedBookmarkURL)));
  }

 private:
  base::test::ScopedFeatureList override_features_;
  fake_server::EntityBuilderFactory entity_builder_factory_;

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
  ASSERT_TRUE(sync_invalidations_service->GetInterestedDataTypes());
  ASSERT_TRUE(sync_invalidations_service->GetFCMRegistrationToken());
  const syncer::ModelTypeSet interested_data_types =
      *sync_invalidations_service->GetInterestedDataTypes();
  const std::string fcm_token =
      *sync_invalidations_service->GetFCMRegistrationToken();

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
          ElementsAre(AllOf(InterestedDataTypesAre(interested_data_types),
                            HasInstanceIdToken(fcm_token))))
          .Wait());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientWithUseSyncInvalidationsForWalletAndOfferTest,
    EnableAndDisableADataType) {
  ASSERT_TRUE(SetupSync());

  // The local device should eventually be committed to the server. BOOKMARKS
  // should be included in interested types, since it's enabled by default.
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  GetFakeServer(),
                  ElementsAre(InterestedDataTypesContain(syncer::BOOKMARKS)))
                  .Wait());

  // Disable BOOKMARKS.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kBookmarks));
  // The local device should eventually be committed to the server. BOOKMARKS
  // should not be included in interested types, as it was disabled.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(),
          ElementsAre(Not(InterestedDataTypesContain(syncer::BOOKMARKS))))
          .Wait());

  // Create a bookmark on the server.
  InjectSyncedBookmark();
  // Enable BOOKMARKS again.
  ASSERT_TRUE(
      GetClient(0)->EnableSyncForType(syncer::UserSelectableType::kBookmarks));
  // The local device should eventually be committed to the server. BOOKMARKS
  // should now be included in interested types.
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  GetFakeServer(),
                  ElementsAre(InterestedDataTypesContain(syncer::BOOKMARKS)))
                  .Wait());
  // The bookmark should get synced now.
  EXPECT_TRUE(bookmarks_helper::GetBookmarkModel(0)->IsBookmarked(
      GURL(kSyncedBookmarkURL)));
}

// ChromeOS doesn't have the concept of sign-out.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(
    SingleClientWithUseSyncInvalidationsForWalletAndOfferTest,
    SignoutAndSignin) {
  ASSERT_TRUE(SetupSync());

  // The local device should eventually be committed to the server. The FCM
  // token should be present in device info.
  ASSERT_TRUE(SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
                  ->GetFCMRegistrationToken());
  const std::string old_token =
      *SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
           ->GetFCMRegistrationToken();
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  GetFakeServer(), ElementsAre(HasInstanceIdToken(old_token)))
                  .Wait());

  // Sign out. The FCM token should be cleared.
  GetClient(0)->SignOutPrimaryAccount();
  ASSERT_TRUE(SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
                  ->GetFCMRegistrationToken());
  EXPECT_TRUE(SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
                  ->GetFCMRegistrationToken()
                  ->empty());

  // Sign in again.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
                  ->GetFCMRegistrationToken());
  const std::string new_token =
      *SyncInvalidationsServiceFactory::GetForProfile(GetProfile(0))
           ->GetFCMRegistrationToken();
  EXPECT_NE(new_token, old_token);
  EXPECT_FALSE(new_token.empty());
  // New device info should eventually be committed to the server (but the old
  // device info will remain on the server). The FCM token should be present.
  EXPECT_TRUE(ServerDeviceInfoMatchChecker(
                  GetFakeServer(), UnorderedElementsAre(HasInstanceIdToken(old_token),
                                                        HasInstanceIdToken(new_token)))
                  .Wait());
}
#endif  // !OS_CHROMEOS

}  // namespace
