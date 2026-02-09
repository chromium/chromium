// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_profile_garbage_collection_service_factory.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/unexportable_keys/features.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/scoped_mock_unexportable_key_provider.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace unexportable_keys {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Test;

constexpr base::TimeDelta kGarbageCollectionDelay = base::Minutes(2);

class UnexportableKeyProfileGarbageCollectionServiceFactoryMacTest
    : public Test {
 public:
  UnexportableKeyProfileGarbageCollectionServiceFactoryMacTest() {
    UnexportableKeyServiceFactory::GetInstance()->SetServiceFactoryForTesting(
        base::BindLambdaForTesting(
            [this](crypto::UnexportableKeyProvider::Config config)
                -> std::unique_ptr<UnexportableKeyService> {
              auto service =
                  std::make_unique<StrictMock<MockUnexportableKeyService>>();
              created_services_[config.application_tag] = service.get();
              return service;
            }));
  }

  MockUnexportableKeyService* GetServiceForConfig(
      const crypto::UnexportableKeyProvider::Config& config) {
    return base::FindPtrOrNull(created_services_, config.application_tag);
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  // The service factory gets invoked for many purposes. Store handles to
  // created services keyed by their application_tag.
  absl::flat_hash_map<std::string, MockUnexportableKeyService*>
      created_services_;

  ScopedMockUnexportableKeyProvider scoped_key_provider_;
};

TEST_F(UnexportableKeyProfileGarbageCollectionServiceFactoryMacTest,
       GarbageCollectionOnRegularProfile) {
  base::test::ScopedFeatureList feature_list(kUnexportableKeyDeletion);
  base::HistogramTester histogram_tester;
  TestingProfile profile;

  MockUnexportableKeyService* created_service =
      GetServiceForConfig(GetConfigForProfilePath(profile.GetPath()));
  ASSERT_TRUE(created_service);

  UnexportableKeyId active_key_id;
  UnexportableKeyId orphaned_key_id;

  std::string profile_path_tag =
      GetConfigForProfilePath(profile.GetPath()).application_tag;
  std::string active_tag = GetConfigForProfile(profile).application_tag;

  // Create an OTR profile with the same path, which will be orphaned. Ensure
  // that no garbage collection service is created for this.
  Profile* otr_profile =
      TestingProfile::Builder()
          .SetPath(profile.GetPath())
          .AddTestingFactory(
              UnexportableKeyProfileGarbageCollectionServiceFactory::
                  GetInstance(),
              base::NullCallback())
          .BuildIncognito(&profile);

  std::string orphaned_tag = GetConfigForProfile(*otr_profile).application_tag;
  profile.DestroyOffTheRecordProfile(otr_profile);

  EXPECT_CALL(*created_service, GetKeyTag(active_key_id))
      .WillOnce(Return(active_tag));
  EXPECT_CALL(*created_service, GetKeyTag(orphaned_key_id))
      .WillOnce(Return(orphaned_tag));

  EXPECT_CALL(*created_service,
              GetAllSigningKeysForGarbageCollectionSlowlyAsync)
      .WillOnce(RunOnceCallback<1>(base::ToVector({
          active_key_id,
          orphaned_key_id,
      })));

  EXPECT_CALL(*created_service,
              DeleteKeysSlowlyAsync(ElementsAre(orphaned_key_id), _, _))
      .WillOnce(RunOnceCallback<2>(1));

  // Fast forward to trigger garbage collection.
  task_environment().FastForwardBy(kGarbageCollectionDelay);

  histogram_tester.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteOTRProfiles."
      "TotalKeyCount",
      2, 1);
  histogram_tester.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteOTRProfiles."
      "UsedKeyCount",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteOTRProfiles."
      "ObsoleteKeyCount",
      1, 1);
  histogram_tester.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.ObsoleteOTRProfiles."
      "ObsoleteKeyDeletionCount",
      1, 1);
}

TEST_F(UnexportableKeyProfileGarbageCollectionServiceFactoryMacTest,
       DeleteAllKeysOnOtrProfileShutdown) {
  base::test::ScopedFeatureList feature_list(kUnexportableKeyDeletion);
  base::HistogramTester histogram_tester;
  TestingProfile profile;
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);

  auto* created_service =
      GetServiceForConfig(GetConfigForProfile(*otr_profile));
  ASSERT_TRUE(created_service);

  EXPECT_CALL(*created_service, DeleteAllKeysSlowlyAsync)
      .WillOnce(RunOnceCallback<0>(3));
  profile.DestroyOffTheRecordProfile(otr_profile);

  histogram_tester.ExpectUniqueSample(
      "Crypto.UnexportableKeys.GarbageCollection.DestroyedOTRProfiles."
      "ObsoleteKeyDeletionCount",
      3, 1);
}

TEST_F(UnexportableKeyProfileGarbageCollectionServiceFactoryMacTest,
       FeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(kUnexportableKeyDeletion);
  TestingProfile profile;

  EXPECT_EQ(UnexportableKeyProfileGarbageCollectionServiceFactory::GetInstance()
                ->GetServiceForBrowserContext(&profile),
            nullptr);
}

}  // namespace

}  // namespace unexportable_keys
