// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are unit tests for KcerRollbackHelper.
// KcerRollbackHelper is used for cleaning double written keys and certificates
// from software backed Chaps storage.

#include "chrome/browser/ash/kcer/nssdb_migration/kcer_rollback_helper.h"

#include <gmock/gmock.h>

#include <memory>

#include "ash/components/kcer/chaps/mock_high_level_chaps_client.h"
#include "ash/constants/ash_features.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using base::test::RunOnceCallback;
using base::test::RunOnceCallbackRepeatedly;
using testing::_;

namespace kcer::internal {
namespace {
const bool kDefaultFalse = false;
constexpr char kEmailId[] = "test@example.com";
constexpr uint32_t kSuccess = chromeos::PKCS11_CKR_OK;

class KcerRollbackHelperTest : public testing::Test {
 public:
  KcerRollbackHelperTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                          base::test::TaskEnvironment::MainThreadType::UI),
        scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {}
  ~KcerRollbackHelperTest() override = default;

  void SetUp() override {
    InitPrefStore();
    InitTpmState();
    InitDefaultExperimentState();
    pref_registry_->RegisterBooleanPref(prefs::kNssChapsDualWrittenCertsExist,
                                        kDefaultFalse);
    rollback_helper_ = std::make_unique<internal::KcerRollbackHelper>(
        &chaps_client_, pref_service_.get());
  }

  void PerformRollbackWithFastForward() {
    rollback_helper_->PerformRollback();
    task_environment_.FastForwardBy(base::Seconds(30));
    task_environment_.RunUntilIdle();
  }

  bool IsRollbackRequired() {
    return internal::KcerRollbackHelper::IsChapsRollbackRequired(
        pref_service_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  MockHighLevelChapsClient chaps_client_;
  scoped_refptr<PrefRegistrySimple> pref_registry_;
  std::unique_ptr<PrefService> pref_service_;
  std::unique_ptr<internal::KcerRollbackHelper> rollback_helper_;
  user_manager::ScopedUserManager scoped_user_manager_;
  base::HistogramTester histogram_tester_;

  void InitUser() {
    ash::FakeChromeUserManager* fake_user_manager =
        static_cast<ash::FakeChromeUserManager*>(
            user_manager::UserManager::Get());
    fake_user_manager->AddUser(AccountId::FromUserEmail(kEmailId));
  }

 private:
  void InitPrefStore() {
    scoped_refptr<TestingPrefStore> user_pref_store =
        base::MakeRefCounted<TestingPrefStore>();
    sync_preferences::PrefServiceMockFactory factory;
    factory.set_user_prefs(user_pref_store);
    pref_registry_ = base::MakeRefCounted<PrefRegistrySimple>();
    pref_service_ = factory.Create(pref_registry_.get());
  }

  void InitTpmState() {
    if (!ash::CryptohomePkcs11Client::Get()) {
      ash::CryptohomePkcs11Client::Get()->InitializeFake();
    }
    chromeos::TpmManagerClient::Get()->InitializeFake();
  }

  void InitDefaultExperimentState() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kEnableNssDbClientCertsRollback},
        /*disabled_features=*/{
            chromeos::features::kEnablePkcs12ToChapsDualWrite});
  }
};

// No user, calling PerformRollback() has aborted because no user id.
TEST_F(KcerRollbackHelperTest, NoUserInitialisedRollbackFailed) {
  PerformRollbackWithFastForward();

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kNssDbClientCertsRollback),
      BucketsInclude(
          Bucket(NssDbClientCertsRollbackEvent::kRollbackScheduled, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackStarted, 1),
          Bucket(NssDbClientCertsRollbackEvent::kFailedNoUserAccountId, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackSuccessful, 0)));
}

// kNssChapsDualWrittenCertsExist is registered and false by default.
// Calling PerformRollback() has no errors.
TEST_F(KcerRollbackHelperTest, IsChapsRollbackRequiredFalse) {
  InitUser();
  EXPECT_FALSE(IsRollbackRequired());
  std::vector<SessionChapsClient::ObjectHandle> object_list;

  EXPECT_CALL(chaps_client_, FindObjects(_, _, _))
      .WillOnce(RunOnceCallback<2>(object_list, kSuccess));
  EXPECT_CALL(chaps_client_, DestroyObjectsWithRetries(_, object_list, _))
      .WillOnce(RunOnceCallback<2>(kSuccess));

  PerformRollbackWithFastForward();

  EXPECT_FALSE(IsRollbackRequired());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kNssDbClientCertsRollback),
      BucketsInclude(
          Bucket(NssDbClientCertsRollbackEvent::kRollbackScheduled, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackStarted, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackListSize0, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackSuccessful, 1)));
}

// PerformRollback() is executed when rollback is required, rollback is no
// more required.
TEST_F(KcerRollbackHelperTest, DestroyDoubleWrittenObjectsInChapsTest) {
  InitUser();
  pref_service_->SetBoolean(prefs::kNssChapsDualWrittenCertsExist, true);
  EXPECT_TRUE(IsRollbackRequired());

  const std::vector<SessionChapsClient::ObjectHandle> kObjectHandles{
      SessionChapsClient::ObjectHandle(20),
      SessionChapsClient::ObjectHandle(30),
      SessionChapsClient::ObjectHandle(40)};
  EXPECT_CALL(chaps_client_, FindObjects(_, _, _))
      .WillOnce(RunOnceCallback<2>(kObjectHandles, kSuccess));
  EXPECT_CALL(chaps_client_, DestroyObjectsWithRetries(_, kObjectHandles, _))
      .WillOnce(RunOnceCallback<2>(kSuccess));

  PerformRollbackWithFastForward();

  EXPECT_FALSE(IsRollbackRequired());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kNssDbClientCertsRollback),
      BucketsInclude(
          Bucket(NssDbClientCertsRollbackEvent::kRollbackScheduled, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackStarted, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackListSize3, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackSuccessful, 1)));
}

// PerformRollback() is executed when rollback is required with 5 ObjectHandle,
// rollback is no more required, kRollbackListSizeAbove3 recorded.
TEST_F(KcerRollbackHelperTest, Destroy5DoubleWrittenObjectsInChapsTest) {
  InitUser();
  pref_service_->SetBoolean(prefs::kNssChapsDualWrittenCertsExist, true);
  EXPECT_TRUE(IsRollbackRequired());

  const std::vector<SessionChapsClient::ObjectHandle> kObjectHandles{
      SessionChapsClient::ObjectHandle(30),
      SessionChapsClient::ObjectHandle(40),
      SessionChapsClient::ObjectHandle(50),
      SessionChapsClient::ObjectHandle(60),
      SessionChapsClient::ObjectHandle(70)};
  EXPECT_CALL(chaps_client_, FindObjects(_, _, _))
      .WillOnce(RunOnceCallback<2>(kObjectHandles, kSuccess));
  EXPECT_CALL(chaps_client_, DestroyObjectsWithRetries(_, kObjectHandles, _))
      .WillOnce(RunOnceCallback<2>(kSuccess));

  PerformRollbackWithFastForward();

  EXPECT_FALSE(IsRollbackRequired());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kNssDbClientCertsRollback),
      BucketsInclude(
          Bucket(NssDbClientCertsRollbackEvent::kRollbackScheduled, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackStarted, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackListSizeAbove3, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackSuccessful, 1)));
}

// PerformRollback() is called when rollback is required, but waiting time was
// only 20 second, so execution has not happened. Then fast forward another 20
// sec and rollback has finished.
TEST_F(KcerRollbackHelperTest, RollbackTimeDidNotArrive) {
  InitUser();
  pref_service_->SetBoolean(prefs::kNssChapsDualWrittenCertsExist, true);
  EXPECT_TRUE(IsRollbackRequired());

  const std::vector<SessionChapsClient::ObjectHandle> kObjectHandles;
  rollback_helper_->PerformRollback();
  task_environment_.FastForwardBy(base::Seconds(20));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(IsRollbackRequired());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kNssDbClientCertsRollback),
      BucketsInclude(
          Bucket(NssDbClientCertsRollbackEvent::kRollbackScheduled, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackStarted, 0),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackListSize0, 0),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackSuccessful, 0)));

  std::vector<SessionChapsClient::ObjectHandle> object_list;
  EXPECT_CALL(chaps_client_, FindObjects(_, _, _))
      .WillOnce(RunOnceCallback<2>(object_list, kSuccess));
  EXPECT_CALL(chaps_client_, DestroyObjectsWithRetries(_, object_list, _))
      .WillOnce(RunOnceCallback<2>(kSuccess));
  task_environment_.FastForwardBy(base::Seconds(20));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(IsRollbackRequired());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kNssDbClientCertsRollback),
      BucketsInclude(
          Bucket(NssDbClientCertsRollbackEvent::kRollbackScheduled, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackStarted, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackListSize0, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackSuccessful, 1)));
}

// PerformRollback() is executed when rollback is
// required, found object list is empty, rollback is no more required.
TEST_F(KcerRollbackHelperTest, RollbackWithEmptyObjectsList) {
  InitUser();
  pref_service_->SetBoolean(prefs::kNssChapsDualWrittenCertsExist, true);
  EXPECT_TRUE(IsRollbackRequired());

  std::vector<SessionChapsClient::ObjectHandle> object_list;

  EXPECT_CALL(chaps_client_, FindObjects(_, _, _))
      .WillOnce(RunOnceCallback<2>(object_list, kSuccess));
  EXPECT_CALL(chaps_client_, DestroyObjectsWithRetries(_, object_list, _))
      .WillOnce(RunOnceCallback<2>(kSuccess));

  PerformRollbackWithFastForward();

  EXPECT_FALSE(IsRollbackRequired());
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kNssDbClientCertsRollback),
      BucketsInclude(
          Bucket(NssDbClientCertsRollbackEvent::kRollbackScheduled, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackStarted, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackListSize0, 1),
          Bucket(NssDbClientCertsRollbackEvent::kRollbackSuccessful, 1)));
}

// IsChapsRollbackRequired() is called when rollback is
// required and experiment is active. True is returned.
TEST_F(KcerRollbackHelperTest, IsChapsRollbackRequiredTrueReturned) {
  EXPECT_FALSE(IsRollbackRequired());

  pref_service_->SetBoolean(prefs::kNssChapsDualWrittenCertsExist, true);
  EXPECT_TRUE(IsRollbackRequired());
}

// IsChapsRollbackRequired() is called when rollback is
// required but experiment is not active. False is returned.
TEST_F(KcerRollbackHelperTest, ExperimentIsNotActiveFalseReturned) {
  EXPECT_FALSE(IsRollbackRequired());

  pref_service_->SetBoolean(prefs::kNssChapsDualWrittenCertsExist, true);
  EXPECT_TRUE(IsRollbackRequired());

  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{chromeos::features::kEnablePkcs12ToChapsDualWrite,
                             ash::features::kEnableNssDbClientCertsRollback});

  EXPECT_FALSE(IsRollbackRequired());
}

// IsChapsRollbackRequired() is called when rollback is
// required but import experiment is still active. False is returned.
TEST_F(KcerRollbackHelperTest, ImportExperimentIsActiveFalseReturned) {
  EXPECT_FALSE(IsRollbackRequired());

  pref_service_->SetBoolean(prefs::kNssChapsDualWrittenCertsExist, true);
  EXPECT_TRUE(IsRollbackRequired());

  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kEnablePkcs12ToChapsDualWrite,
                            ash::features::kEnableNssDbClientCertsRollback},
      /*disabled_features=*/{});

  EXPECT_FALSE(IsRollbackRequired());
}

// IsChapsRollbackRequired() is called when rollback is
// required. Import experiment is still active, rollback experiment is disabled.
// False is returned.
TEST_F(KcerRollbackHelperTest,
       ImportExperimentIsActiveRollbackIsDisabledFalseReturned) {
  EXPECT_FALSE(IsRollbackRequired());

  pref_service_->SetBoolean(prefs::kNssChapsDualWrittenCertsExist, true);
  EXPECT_TRUE(IsRollbackRequired());

  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kEnablePkcs12ToChapsDualWrite},
      /*disabled_features=*/{ash::features::kEnableNssDbClientCertsRollback});

  EXPECT_FALSE(IsRollbackRequired());
}

}  // namespace
}  // namespace kcer::internal
