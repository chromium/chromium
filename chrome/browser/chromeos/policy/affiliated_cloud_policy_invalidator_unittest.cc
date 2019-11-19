// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/affiliated_cloud_policy_invalidator.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/fake_affiliated_invalidation_service_provider.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "chrome/common/chrome_features.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "google/cacheinvalidation/include/types.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::Invoke;
using testing::Mock;
using testing::WithArgs;

namespace policy {

namespace {

const int kInvalidationSource = 123;
const char kInvalidationName[] = "invalidation";
const char kPolicyInvalidationTopic[] = "policy_invalidation_topic";

class FakeCloudPolicyStore : public CloudPolicyStore {
 public:
  FakeCloudPolicyStore();

  // CloudPolicyStore:
  void Store(const em::PolicyFetchResponse& policy) override;
  void Load() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeCloudPolicyStore);
};

FakeCloudPolicyStore::FakeCloudPolicyStore() {
}

void FakeCloudPolicyStore::Store(const em::PolicyFetchResponse& policy) {
  policy_.reset(new em::PolicyData);
  policy_->ParseFromString(policy.policy_data());
  Load();
}

void FakeCloudPolicyStore::Load() {
  NotifyStoreLoaded();
}

}  // namespace

// Accepts boolean param is_fcm_enabled.
// true if FCM (Firebase Cloud Messaging) is enabled,
// and false otherwise.
class AffiliatedCloudPolicyInvalidatorTest
    : public testing::TestWithParam<bool> {
 public:
  AffiliatedCloudPolicyInvalidatorTest();

 private:
  base::test::ScopedFeatureList feature_list_;
};

AffiliatedCloudPolicyInvalidatorTest::AffiliatedCloudPolicyInvalidatorTest() {
  feature_list_.InitWithFeatureState(features::kPolicyFcmInvalidations,
                                     GetParam() /* is_fcm_enabled */);
}

// Verifies that an invalidator is created/destroyed as an invalidation service
// becomes available/unavailable. Also verifies that invalidations are handled
// correctly and the highest handled invalidation version is preserved when
// switching invalidation services.
TEST_P(AffiliatedCloudPolicyInvalidatorTest, CreateUseDestroy) {
  content::BrowserTaskEnvironment task_environment;

  // Set up a CloudPolicyCore backed by a simple CloudPolicyStore that does no
  // signature verification and stores policy in memory.
  FakeCloudPolicyStore store;
  CloudPolicyCore core(dm_protocol::kChromeDevicePolicyType, std::string(),
                       &store, base::ThreadTaskRunnerHandle::Get(),
                       network::TestNetworkConnectionTracker::CreateGetter());

  // Connect |core|. Expect it to send a registration request. Let the
  // registration succeed.
  std::unique_ptr<MockCloudPolicyClient> policy_client_owner(
      new MockCloudPolicyClient);
  MockCloudPolicyClient* policy_client = policy_client_owner.get();
  EXPECT_CALL(
      *policy_client,
      SetupRegistration(PolicyBuilder::kFakeToken, PolicyBuilder::kFakeDeviceId,
                        PolicyBuilder::GetUserAffiliationIds()))
      .WillOnce(WithArgs<1>(
          Invoke(policy_client, &MockCloudPolicyClient::SetDMToken)));
  core.Connect(std::move(policy_client_owner));
  Mock::VerifyAndClearExpectations(&policy_client);
  core.StartRefreshScheduler();

  DevicePolicyBuilder policy;

  const bool is_fcm_enabled = GetParam();
  if (is_fcm_enabled) {
    // Pass deprecated source if FCM (Firebase Cloud Messaging) is enabled,
    // because server does not support source field with FCM and
    // InvalidationService fills the source field with kDeprecatedSourceForFCM.
    policy.policy_data().set_invalidation_source(
        syncer::kDeprecatedSourceForFCM);
    policy.policy_data().set_policy_invalidation_topic(
        kPolicyInvalidationTopic);
  } else {
    policy.policy_data().set_invalidation_source(kInvalidationSource);
    policy.policy_data().set_invalidation_name(kInvalidationName);
  }
  policy.Build();
  store.Store(policy.policy());

  FakeAffiliatedInvalidationServiceProvider provider;
  AffiliatedCloudPolicyInvalidator affiliated_invalidator(
      em::DeviceRegisterRequest::DEVICE, &core, &provider);

  // Verify that no invalidator exists initially.
  EXPECT_FALSE(affiliated_invalidator.GetInvalidatorForTest());

  // Make a first invalidation service available.
  invalidation::FakeInvalidationService invalidation_service_1;
  affiliated_invalidator.OnInvalidationServiceSet(&invalidation_service_1);

  // Verify that an invalidator backed by the first invalidation service has
  // been created and its highest handled invalidation version starts out as
  // zero.
  CloudPolicyInvalidator* invalidator =
      affiliated_invalidator.GetInvalidatorForTest();
  ASSERT_TRUE(invalidator);
  EXPECT_EQ(0, invalidator->highest_handled_invalidation_version());
  EXPECT_EQ(&invalidation_service_1,
            invalidator->invalidation_service_for_test());

  // Trigger an invalidation. The invalidation version is interpreted as a
  // timestamp in microseconds. The policy blob contains a timestamp in
  // milliseconds. Convert from one to the other by multiplying by 1000.
  const int64_t invalidation_version = policy.policy_data().timestamp() * 1000;
  syncer::Invalidation invalidation =
      is_fcm_enabled
          ? syncer::Invalidation::Init(
                invalidation::ObjectId(syncer::kDeprecatedSourceForFCM,
                                       kPolicyInvalidationTopic),
                invalidation_version, "dummy payload")
          : syncer::Invalidation::Init(
                invalidation::ObjectId(kInvalidationSource, kInvalidationName),
                invalidation_version, "dummy payload");
  syncer::ObjectIdInvalidationMap invalidation_map;
  invalidation_map.Insert(invalidation);
  invalidator->OnIncomingInvalidation(invalidation_map);

  // Allow the invalidation to be handled.
  policy_client->SetFetchedInvalidationVersion(invalidation_version);
  policy.payload().mutable_reboot_on_shutdown()->set_reboot_on_shutdown(true);
  policy.Build();
  policy_client->SetPolicy(dm_protocol::kChromeDevicePolicyType,
                           std::string(),
                           policy.policy());
  EXPECT_CALL(*policy_client, FetchPolicy())
      .WillOnce(Invoke(policy_client,
                       &MockCloudPolicyClient::NotifyPolicyFetched));
  base::RunLoop().RunUntilIdle();

  // Verify that the invalidator's highest handled invalidation version was
  // updated and the new policy was stored.
  EXPECT_EQ(invalidation_version,
            invalidator->highest_handled_invalidation_version());
  ASSERT_TRUE(store.policy());
  em::ChromeDeviceSettingsProto device_policy;
  device_policy.ParseFromString(store.policy()->policy_value());
  EXPECT_EQ(true, device_policy.reboot_on_shutdown().reboot_on_shutdown());

  // Make the first invalidation service unavailable. Verify that the
  // invalidator is destroyed.
  affiliated_invalidator.OnInvalidationServiceSet(nullptr);
  EXPECT_FALSE(affiliated_invalidator.GetInvalidatorForTest());

  // Make a second invalidation service available.
  invalidation::FakeInvalidationService invalidation_service_2;
  affiliated_invalidator.OnInvalidationServiceSet(&invalidation_service_2);

  // Verify that an invalidator backed by the second invalidation service has
  // been created and its highest handled invalidation version does not start
  // out as zero.
  invalidator = affiliated_invalidator.GetInvalidatorForTest();
  ASSERT_TRUE(invalidator);
  EXPECT_EQ(invalidation_version,
            invalidator->highest_handled_invalidation_version());
  EXPECT_EQ(&invalidation_service_2,
            invalidator->invalidation_service_for_test());

  // Make the first invalidation service available again. This implies that the
  // second invalidation service is no longer available.
  affiliated_invalidator.OnInvalidationServiceSet(&invalidation_service_1);

  // Verify that the invalidator backed by the second invalidation service was
  // destroyed and an invalidation backed by the first invalidation service has
  // been created instead. Also verify that its highest handled invalidation
  // version does not start out as zero.
  invalidator = affiliated_invalidator.GetInvalidatorForTest();
  ASSERT_TRUE(invalidator);
  EXPECT_EQ(invalidation_version,
            invalidator->highest_handled_invalidation_version());
  EXPECT_EQ(&invalidation_service_1,
            invalidator->invalidation_service_for_test());

  provider.Shutdown();
  affiliated_invalidator.OnInvalidationServiceSet(nullptr);
}

INSTANTIATE_TEST_SUITE_P(FCMEnabledAndFCMDisabled,
                         AffiliatedCloudPolicyInvalidatorTest,
                         testing::Bool() /* is_fcm_enabled */);

}  // namespace policy
