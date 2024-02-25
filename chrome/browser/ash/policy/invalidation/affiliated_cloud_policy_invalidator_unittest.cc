// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/invalidation/affiliated_cloud_policy_invalidator.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/invalidation/fake_affiliated_invalidation_service_provider.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::WithArgs;

namespace policy {

namespace {

const char kPolicyInvalidationTopic[] = "policy_invalidation_topic";

}  // namespace

// Verifies that an invalidator is created/destroyed as an invalidation service
// becomes available/unavailable. Also verifies that invalidations are handled
// correctly and the highest handled invalidation version is preserved when
// switching invalidation services.
TEST(AffiliatedCloudPolicyInvalidatorTest, CreateUseDestroy) {
  content::BrowserTaskEnvironment task_environment;

  // Set up a CloudPolicyCore backed by a simple CloudPolicyStore that does no
  // signature verification and stores policy in memory.
  MockCloudPolicyStore store;
  ON_CALL(store, Load()).WillByDefault([&store]() {
    store.NotifyStoreLoaded();
  });
  ON_CALL(store, Store(testing::_))
      .WillByDefault([&store](const em::PolicyFetchResponse& policy) {
        auto policy_data = std::make_unique<em::PolicyData>();
        ASSERT_TRUE(policy_data->ParseFromString(policy.policy_data()));
        store.set_policy_data_for_testing(std::move(policy_data));
        store.Load();
      });

  CloudPolicyCore core(dm_protocol::kChromeDevicePolicyType, std::string(),
                       &store,
                       base::SingleThreadTaskRunner::GetCurrentDefault(),
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

  policy.policy_data().set_policy_invalidation_topic(kPolicyInvalidationTopic);

  policy.Build();
  store.Store(policy.policy());

  FakeAffiliatedInvalidationServiceProvider provider;
  AffiliatedCloudPolicyInvalidator affiliated_invalidator(
      PolicyInvalidationScope::kDevice, &core, &provider);

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
  invalidation::Invalidation invalidation = invalidation::Invalidation(
      kPolicyInvalidationTopic, invalidation_version, "dummy payload");

  invalidator->OnIncomingInvalidation(invalidation);

  // Allow the invalidation to be handled.
  policy_client->SetFetchedInvalidationVersion(invalidation_version);
  policy.payload().mutable_reboot_on_shutdown()->set_reboot_on_shutdown(true);
  policy.Build();
  policy_client->SetPolicy(dm_protocol::kChromeDevicePolicyType, std::string(),
                           policy.policy());
  // TODO(b/298336121) Adjust expected argument once an appropriate
  // PolicyFetchReason can be passed through.
  EXPECT_CALL(*policy_client, FetchPolicy(_))
      .WillOnce(
          Invoke(policy_client, &MockCloudPolicyClient::NotifyPolicyFetched));
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

}  // namespace policy
