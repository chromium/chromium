// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_data_manager_android.h"

#include <memory>
#include <optional>

#include "base/android/jni_android.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/autofill/android/entity_data_manager_android_test_api.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/network/autofill_ai/mock_wallet_pass_access_manager.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/wallet/core/common/wallet_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::SaveArg;

class EntityDataManagerAndroidTest : public testing::Test {
 public:
  EntityDataManagerAndroidTest() {
    autofill::prefs::RegisterProfilePrefs(prefs_.registry());
    entity_data_manager_ = std::make_unique<EntityDataManager>(
        &prefs_, identity_test_env_.identity_manager(), &sync_service_,
        webdata_helper_.autofill_webdata_service(),
        /*history_service=*/nullptr, /*strike_database=*/nullptr,
        autofill::GeoIpCountryCode("US"));

    entity_data_manager_android_ = new EntityDataManagerAndroid(
        base::android::AttachCurrentThread(),
        /*obj=*/nullptr,
        /*google_groups_manager=*/nullptr, &prefs_,
        identity_test_env_.identity_manager(), &sync_service_,
        /*account_setting_service=*/nullptr, &consent_auditor_,
        /*is_off_the_record=*/false, &mock_wallet_pass_access_manager_,
        entity_data_manager_.get());
  }

  EntityDataManager& entity_data_manager() { return *entity_data_manager_; }
  MockWalletPassAccessManager& mock_wallet_pass_access_manager() {
    return mock_wallet_pass_access_manager_;
  }

  JNIEnv* env() { return env_.get(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  consent_auditor::FakeConsentAuditor consent_auditor_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  syncer::TestSyncService sync_service_;
  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};
  std::unique_ptr<EntityDataManager> entity_data_manager_;
  NiceMock<MockWalletPassAccessManager> mock_wallet_pass_access_manager_;
  raw_ptr<EntityDataManagerAndroid> entity_data_manager_android_;
  raw_ptr<JNIEnv> env_ = base::android::AttachCurrentThread();
};

// Test that when masked storage is not supported, Google Wallet servers are not
// called. Note that this does not mean the entity record type is `kLocal`.
// Public passes can have a record type of `kServerWallet` but upon saving the
// Wallet servers are not called. This is because these entities are later
// shared with Wallet via sync.
TEST_F(EntityDataManagerAndroidTest,
       AddOrUpdate_MaskedStorageNotSupported_DoNotCallWalletServers) {
  EntityInstance entity = test::GetVehicleEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});

  // Ensure it's NOT masked storage supported.
  ASSERT_FALSE(IsMaskedStorageSupported(entity.type(), entity.record_type()));
  // Wallet servers are not called when storing public passes.
  EXPECT_CALL(mock_wallet_pass_access_manager(), SaveWalletEntityInstance)
      .Times(0);

  test_api(*entity_data_manager_android_)
      .AddOrUpdateEntityInstance(entity, entity.record_type());
  webdata_helper_.WaitUntilIdle();

  EXPECT_THAT(entity_data_manager().GetEntityInstances(),
              testing::ElementsAre(entity));
}

// Test that when save to wallet fails, it falls back to local save.
TEST_F(EntityDataManagerAndroidTest,
       AddOrUpdate_WalletSaveFails_FallbackToLocal) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kAutofillAiWalletPrivatePasses,
       wallet::features::kWalletApiPrivatePassesConsent},
      {});

  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  EntityInstance entity = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});

  const int kDescriptionId = 123;
  const int kAcceptButtonId = 456;

  consent_auditor::ConsentAuditor::SessionId session_id_consent;
  consent_auditor::ConsentAuditor::SessionId session_id_api;
  sync_pb::UserConsentTypes::WalletPrivatePassConsent consent;
  {
    testing::InSequence s;
    EXPECT_CALL(consent_auditor_, RecordWalletPrivatePassConsent)
        .WillOnce(DoAll(SaveArg<1>(&session_id_consent), SaveArg<2>(&consent)));

    EXPECT_CALL(mock_wallet_pass_access_manager(),
                SaveWalletEntityInstance(entity, _, _))
        .WillOnce(DoAll(SaveArg<1>(&session_id_api),
                        RunOnceCallback<2>(std::nullopt)));
  }

  base::MockOnceClosure on_local_save_callback;
  EXPECT_CALL(on_local_save_callback, Run);

  test_api(*entity_data_manager_android_)
      .AddOrUpdateEntityInstance(entity, entity.record_type(), kDescriptionId,
                                 kAcceptButtonId, on_local_save_callback.Get());
  webdata_helper_.WaitUntilIdle();

  EXPECT_THAT(entity_data_manager().GetEntityInstances(),
              testing::ElementsAre(entity.CopyWithNewRecordType(
                  EntityInstance::RecordType::kLocal)));
  EXPECT_EQ(session_id_consent, session_id_api);
  EXPECT_EQ(consent.confirmation_grd_id(), kAcceptButtonId);
  EXPECT_EQ(consent.description_grd_ids(0), kDescriptionId);
}

// Test that when save to wallet succeeds, it is saved as a wallet
// entity.
TEST_F(EntityDataManagerAndroidTest,
       AddOrUpdate_WalletSaveSucceeds_SavedAsWallet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kAutofillAiWalletPrivatePasses,
       wallet::features::kWalletApiPrivatePassesConsent},
      {});

  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  EntityInstance entity = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  // Simulate the server returning a masked version of the entity.
  EntityInstance saved_entity = test::MaskEntityInstance(entity);

  const int kDescriptionId = 123;
  const int kAcceptButtonId = 456;

  // Expect that consent is logged and that the correct session ID if forward
  // to the Wallet API.
  consent_auditor::ConsentAuditor::SessionId session_id_consent;
  consent_auditor::ConsentAuditor::SessionId session_id_api;
  sync_pb::UserConsentTypes::WalletPrivatePassConsent consent;
  EXPECT_CALL(consent_auditor_, RecordWalletPrivatePassConsent)
      .WillOnce(DoAll(SaveArg<1>(&session_id_consent), SaveArg<2>(&consent)));

  EXPECT_CALL(mock_wallet_pass_access_manager(),
              SaveWalletEntityInstance(entity, _, _))
      .WillOnce(
          DoAll(SaveArg<1>(&session_id_api), RunOnceCallback<2>(saved_entity)));

  base::MockOnceClosure on_local_save_callback;
  EXPECT_CALL(on_local_save_callback, Run).Times(0);

  test_api(*entity_data_manager_android_)
      .AddOrUpdateEntityInstance(entity, entity.record_type(), kDescriptionId,
                                 kAcceptButtonId, on_local_save_callback.Get());
  webdata_helper_.WaitUntilIdle();

  base::span<const EntityInstance> instances =
      entity_data_manager().GetEntityInstances();
  ASSERT_EQ(instances.size(), 1u);
  EXPECT_EQ(instances[0].guid(), entity.guid());
  EXPECT_EQ(session_id_consent, session_id_api);
  EXPECT_EQ(consent.confirmation_grd_id(), kAcceptButtonId);
  EXPECT_EQ(consent.description_grd_ids(0), kDescriptionId);
}

// Test that when targeted record type is wallet but the entity is local
// (e.g. due to ineligibility after user turning of sync), it is saved locally.
TEST_F(EntityDataManagerAndroidTest,
       AddOrUpdate_UserTargetedWalletButBecameIneligible_SavedLocally) {
  base::test::ScopedFeatureList feature_list{
      features::kAutofillAiWalletPrivatePasses};

  EntityInstance entity = test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kLocal});

  EXPECT_CALL(mock_wallet_pass_access_manager(), SaveWalletEntityInstance)
      .Times(0);

  base::MockOnceClosure on_local_save_callback;
  EXPECT_CALL(on_local_save_callback, Run);

  // Targeted record type was wallet, but entity is local.
  test_api(*entity_data_manager_android_)
      .AddOrUpdateEntityInstance(
          entity, EntityInstance::RecordType::kServerWallet,
          /*description_string_id=*/0,
          /*accept_button_string_id=*/0, on_local_save_callback.Get());
  webdata_helper_.WaitUntilIdle();

  base::span<const EntityInstance> instances =
      entity_data_manager().GetEntityInstances();
  ASSERT_EQ(instances.size(), 1u);
  EXPECT_EQ(instances[0].record_type(), EntityInstance::RecordType::kLocal);
}

TEST_F(EntityDataManagerAndroidTest, LogEntityAddedFromSettings) {
  base::HistogramTester histogram_tester;
  EntityInstance entity = test::GetPassportEntityInstance();

  test_api(*entity_data_manager_android_)
      .AddOrUpdateEntityInstance(entity, entity.record_type());
  webdata_helper_.WaitUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.EntityAddedFromSettings.Passport.Local",
      autofill::EntityTypeName::kPassport, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.EntityAddedFromSettings.Local",
      autofill::EntityTypeName::kPassport, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.EntityAddedFromSettings.Passport",
      autofill::EntityTypeName::kPassport, 1);
  histogram_tester.ExpectUniqueSample("Autofill.Ai.EntityAddedFromSettings",
                                      autofill::EntityTypeName::kPassport, 1);
}

TEST_F(EntityDataManagerAndroidTest, LogEntityUpdatedFromSettings) {
  base::HistogramTester histogram_tester;
  EntityInstance entity = test::GetPassportEntityInstance();

  // First add the entity.
  test_api(*entity_data_manager_android_)
      .AddOrUpdateEntityInstance(entity, entity.record_type());
  webdata_helper_.WaitUntilIdle();

  // Now update it.
  test_api(*entity_data_manager_android_)
      .AddOrUpdateEntityInstance(entity, entity.record_type());
  webdata_helper_.WaitUntilIdle();

  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.EntityUpdatedFromSettings.Passport.Local",
      autofill::EntityTypeName::kPassport, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.EntityUpdatedFromSettings.Local",
      autofill::EntityTypeName::kPassport, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Ai.EntityUpdatedFromSettings.Passport",
      autofill::EntityTypeName::kPassport, 1);
  histogram_tester.ExpectBucketCount("Autofill.Ai.EntityUpdatedFromSettings",
                                     autofill::EntityTypeName::kPassport, 1);
}

TEST_F(EntityDataManagerAndroidTest, LogEntityDeletedFromSettings) {
  base::HistogramTester histogram_tester;
  EntityInstance entity = test::GetPassportEntityInstance();

  // First add the entity.
  test_api(*entity_data_manager_android_)
      .AddOrUpdateEntityInstance(entity, entity.record_type());
  webdata_helper_.WaitUntilIdle();

  // Now delete it.
  entity_data_manager_android_->RemoveEntityInstance(env(),
                                                     entity.guid().value());
  webdata_helper_.WaitUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.EntityDeletedFromSettings.Passport.Local",
      autofill::EntityTypeName::kPassport, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.EntityDeletedFromSettings.Local",
      autofill::EntityTypeName::kPassport, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Ai.EntityDeletedFromSettings.Passport",
      autofill::EntityTypeName::kPassport, 1);
  histogram_tester.ExpectUniqueSample("Autofill.Ai.EntityDeletedFromSettings",
                                      autofill::EntityTypeName::kPassport, 1);
}

}  // namespace
}  // namespace autofill
