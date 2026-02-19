// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_obsolete_profile_garbage_collector.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

namespace {

constexpr base::TimeDelta kGarbageCollectionDelay = base::Minutes(2);
constexpr char kProfileName[] = "profile1";
constexpr char kObsoleteProfileName[] = "obsolete";

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Test;

class UnexportableKeyObsoleteProfileGarbageCollectorMacTest : public Test {
 public:
  UnexportableKeyObsoleteProfileGarbageCollectorMacTest() {
    UnexportableKeyServiceFactory::GetInstance()->SetServiceFactoryForTesting(
        base::BindRepeating(
            &UnexportableKeyObsoleteProfileGarbageCollectorMacTest::
                CreateMockService,
            base::Unretained(this)));

    profile_manager_.emplace(TestingBrowserProcess::GetGlobal());
    CHECK(profile_manager_->SetUp());
    collector_.emplace(profile_manager_->profile_manager());
  }

  ~UnexportableKeyObsoleteProfileGarbageCollectorMacTest() override {
    UnexportableKeyServiceFactory::GetInstance()->SetServiceFactoryForTesting(
        base::NullCallback());
  }

  std::unique_ptr<UnexportableKeyService> CreateMockService(
      crypto::UnexportableKeyProvider::Config config) {
    auto service = std::make_unique<StrictMock<MockUnexportableKeyService>>();
    if (config.application_tag ==
        GetConfigForUserDataDir(
            profile_manager_->profile_manager()->user_data_dir())
            .application_tag) {
      user_data_dir_service_ = service.get();
    } else if (config.application_tag.ends_with(kProfileName)) {
      CHECK(on_profile_service_created_)
          << "set_on_profile_service_created() must be called before profile's "
             "service is created";
      std::move(on_profile_service_created_).Run(service.get());
    }
    return service;
  }

  MockUnexportableKeyService* user_data_dir_service() {
    return user_data_dir_service_;
  }

  void set_on_profile_service_created(
      base::OnceCallback<void(MockUnexportableKeyService*)> callback) {
    on_profile_service_created_ = std::move(callback);
  }

  TestingProfileManager& profile_manager() { return *profile_manager_; }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  UnexportableKeyObsoleteProfileGarbageCollector& collector() {
    return *collector_;
  }

  void DestroyProfileManager() { profile_manager_.reset(); }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::optional<TestingProfileManager> profile_manager_;
  std::optional<UnexportableKeyObsoleteProfileGarbageCollector> collector_;
  base::HistogramTester histogram_tester_;
  raw_ptr<MockUnexportableKeyService> user_data_dir_service_ = nullptr;
  base::OnceCallback<void(MockUnexportableKeyService*)>
      on_profile_service_created_;
};

TEST_F(UnexportableKeyObsoleteProfileGarbageCollectorMacTest,
       GarbageCollectionScheduled) {
  ASSERT_TRUE(user_data_dir_service());
  EXPECT_CALL(*user_data_dir_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync);

  task_environment().FastForwardBy(kGarbageCollectionDelay);
}

TEST_F(UnexportableKeyObsoleteProfileGarbageCollectorMacTest,
       GarbageCollectionNoKeys) {
  ASSERT_TRUE(user_data_dir_service());
  EXPECT_CALL(*user_data_dir_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync)
      .WillOnce(RunOnceCallback<1>(std::vector<UnexportableKeyId>()));
  EXPECT_CALL(*user_data_dir_service(), GetKeyTag).Times(0);
  EXPECT_CALL(*user_data_dir_service(), DeleteKeysSlowlyAsync).Times(0);

  task_environment().FastForwardBy(kGarbageCollectionDelay);

  histogram_tester().ExpectTotalCount(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "TotalKeyCount",
      0);
}

TEST_F(UnexportableKeyObsoleteProfileGarbageCollectorMacTest,
       GarbageCollectionActiveKey) {
  Profile* profile = profile_manager().CreateTestingProfile(kProfileName);
  std::string profile_tag = GetConfigForProfile(*profile).application_tag;

  ASSERT_TRUE(user_data_dir_service());
  UnexportableKeyId key_id;
  EXPECT_CALL(*user_data_dir_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync(
                  BackgroundTaskPriority::kBestEffort, _))
      .WillOnce(RunOnceCallback<1>(base::ToVector({key_id})));

  EXPECT_CALL(*user_data_dir_service(), GetKeyTag(key_id))
      .WillOnce(Return(profile_tag));
  EXPECT_CALL(*user_data_dir_service(), DeleteKeysSlowlyAsync(IsEmpty(), _, _))
      .WillOnce(
          RunOnceCallback<2>(base::unexpected(ServiceError::kKeyNotFound)));

  task_environment().FastForwardBy(kGarbageCollectionDelay);

  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "TotalKeyCount",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "UsedKeyCount",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "ObsoleteKeyCount",
      0, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "ObsoleteKeyDeletionCount",
      0, 1);
}

TEST_F(UnexportableKeyObsoleteProfileGarbageCollectorMacTest,
       GarbageCollectionWithVariousProfiles) {
  TestingProfile* active_profile =
      profile_manager().CreateTestingProfile(kProfileName);
  std::string active_profile_tag =
      GetConfigForProfile(*active_profile).application_tag;

  TestingProfile* obsolete_profile =
      profile_manager().CreateTestingProfile(kObsoleteProfileName);
  std::string obsolete_profile_tag =
      GetConfigForProfile(*obsolete_profile).application_tag;
  profile_manager().DeleteTestingProfile(kObsoleteProfileName);

  TestingProfile* guest_profile = profile_manager().CreateGuestProfile();
  std::string guest_profile_tag =
      GetConfigForProfile(*guest_profile).application_tag;

  TestingProfile* system_profile = profile_manager().CreateSystemProfile();
  std::string system_profile_tag =
      GetConfigForProfile(*system_profile).application_tag;

  UnexportableKeyId active_key_id;
  UnexportableKeyId obsolete_key_id;
  UnexportableKeyId guest_key_id;
  UnexportableKeyId system_key_id;

  ASSERT_TRUE(user_data_dir_service());
  EXPECT_CALL(*user_data_dir_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync(
                  BackgroundTaskPriority::kBestEffort, _))
      .WillOnce(RunOnceCallback<1>(base::ToVector({
          active_key_id,
          obsolete_key_id,
          guest_key_id,
          system_key_id,
      })));

  EXPECT_CALL(*user_data_dir_service(), GetKeyTag(active_key_id))
      .WillOnce(Return(active_profile_tag));
  EXPECT_CALL(*user_data_dir_service(), GetKeyTag(obsolete_key_id))
      .WillOnce(Return(obsolete_profile_tag));
  EXPECT_CALL(*user_data_dir_service(), GetKeyTag(guest_key_id))
      .WillOnce(Return(guest_profile_tag));
  EXPECT_CALL(*user_data_dir_service(), GetKeyTag(system_key_id))
      .WillOnce(Return(system_profile_tag));

  EXPECT_CALL(*user_data_dir_service(),
              DeleteKeysSlowlyAsync(ElementsAre(obsolete_key_id), _, _))
      .WillOnce(RunOnceCallback<2>(1));

  task_environment().FastForwardBy(kGarbageCollectionDelay);

  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "TotalKeyCount",
      4, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "UsedKeyCount",
      3, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "ObsoleteKeyCount",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "ObsoleteKeyDeletionCount",
      1, 1);
}

TEST_F(UnexportableKeyObsoleteProfileGarbageCollectorMacTest,
       GarbageCollectionKeyTagError) {
  ASSERT_TRUE(user_data_dir_service());
  UnexportableKeyId key_id;
  EXPECT_CALL(*user_data_dir_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync)
      .WillOnce(RunOnceCallback<1>(base::ToVector({key_id})));

  EXPECT_CALL(*user_data_dir_service(), GetKeyTag(key_id))
      .WillOnce(Return(base::unexpected(ServiceError::kKeyNotFound)));

  // If GetKeyTag fails, the key is assumed safe and not deleted.
  EXPECT_CALL(*user_data_dir_service(), DeleteKeysSlowlyAsync(IsEmpty(), _, _))
      .WillOnce(
          RunOnceCallback<2>(base::unexpected(ServiceError::kKeyNotFound)));

  task_environment().FastForwardBy(kGarbageCollectionDelay);

  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "TotalKeyCount",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "UsedKeyCount",
      1, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "ObsoleteKeyCount",
      0, 1);
  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "ObsoleteKeyDeletionCount",
      0, 1);
}

TEST_F(UnexportableKeyObsoleteProfileGarbageCollectorMacTest,
       OnProfileDeletion) {
  Profile* profile = profile_manager().CreateTestingProfile(kProfileName);

  // Setup expectation for the per-profile service.
  // NOTE: This logic needs to be a callback injected before returning the
  // service instance to the caller, since `DeleteAllKeysSlowlyAsync` is
  // immediately invoked after construction of the service, and thus there would
  // be no opportunity to set up the expectation otherwise.
  set_on_profile_service_created(
      base::BindOnce([](MockUnexportableKeyService* service) {
        EXPECT_CALL(*service, DeleteAllKeysSlowlyAsync)
            .WillOnce(RunOnceCallback<0>(3));
      }));

  // TestingProfileManager::DeleteTestingProfile() does not actually invoke
  // NotifyOnProfileMarkedForPermanentDeletion(), thus we need to invoke it
  // explicitly ourselves.
  // TODO(crbug.com/455538352): Add a browser test exercising these interactions
  // end-to-end.
  collector().OnProfileMarkedForPermanentDeletion(profile);

  histogram_tester().ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DestroyedProfiles."
      "ObsoleteKeyDeletionCount",
      3, 1);
}

TEST_F(UnexportableKeyObsoleteProfileGarbageCollectorMacTest,
       ProfileManagerDestroyedBeforeGarbageCollection) {
  ASSERT_TRUE(user_data_dir_service());

  UnexportableKeyId key_id;
  EXPECT_CALL(*user_data_dir_service(),
              GetAllSigningKeysForGarbageCollectionSlowlyAsync)
      .WillOnce([&](auto priority, auto callback) {
        // Destroy the profile manager before the garbage collection callback is
        // run.
        DestroyProfileManager();
        std::move(callback).Run(std::vector{key_id});
      });

  // Trigger the scheduled garbage collection task.
  task_environment().FastForwardBy(kGarbageCollectionDelay);

  // GetKeyTag etc. shouldn't be called because the ProfileManager was
  // destroyed.
  EXPECT_CALL(*user_data_dir_service(), GetKeyTag).Times(0);

  histogram_tester().ExpectTotalCount(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "TotalKeyCount",
      0);
  histogram_tester().ExpectTotalCount(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "UsedKeyCount",
      0);
  histogram_tester().ExpectTotalCount(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "ObsoleteKeyCount",
      0);
  histogram_tester().ExpectTotalCount(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteProfiles."
      "ObsoleteKeyDeletionCount",
      0);
}

}  // namespace
}  // namespace unexportable_keys
