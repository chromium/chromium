// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/remote_commands_invalidator.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/invalidation/impl/fake_ack_handler.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/invalidation/public/invalidator_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using ::testing::_;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::StrictMock;

namespace policy {

// TODO(crbug.com/1319443): Remove mock and test the actual invalidator.
class MockRemoteCommandInvalidator : public RemoteCommandsInvalidator {
 public:
  MockRemoteCommandInvalidator()
      : RemoteCommandsInvalidator("RemoteCommands.Test") {}
  MockRemoteCommandInvalidator(const MockRemoteCommandInvalidator&) = delete;
  MockRemoteCommandInvalidator& operator=(const MockRemoteCommandInvalidator&) =
      delete;

  MOCK_METHOD0(OnInitialize, void());
  MOCK_METHOD0(OnShutdown, void());
  MOCK_METHOD0(OnStart, void());
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD1(DoRemoteCommandsFetch, void(const invalidation::Invalidation&));

  void SetInvalidationTopic(const invalidation::Topic& topic) {
    em::PolicyData policy_data;
    policy_data.set_command_invalidation_topic(topic);
    ReloadPolicyData(&policy_data);
  }

  void ClearInvalidationTopic() {
    const em::PolicyData policy_data;
    ReloadPolicyData(&policy_data);
  }
};

class RemoteCommandsInvalidatorTest : public testing::Test {
 public:
  RemoteCommandsInvalidatorTest()
      : kTestingTopic1("abcdef"), kTestingTopic2("defabc") {}
  RemoteCommandsInvalidatorTest(const RemoteCommandsInvalidatorTest&) = delete;
  RemoteCommandsInvalidatorTest& operator=(
      const RemoteCommandsInvalidatorTest&) = delete;

  void EnableInvalidationService() {
    invalidation_service_.SetInvalidatorState(
        invalidation::INVALIDATIONS_ENABLED);
  }

  void DisableInvalidationService() {
    invalidation_service_.SetInvalidatorState(
        invalidation::TRANSIENT_INVALIDATION_ERROR);
  }

  invalidation::Invalidation CreateInvalidation(
      const invalidation::Topic& topic) {
    return invalidation::Invalidation::InitUnknownVersion(topic);
  }

  invalidation::Invalidation FireInvalidation(
      const invalidation::Topic& topic) {
    const invalidation::Invalidation invalidation = CreateInvalidation(topic);
    invalidation_service_.EmitInvalidationForTest(invalidation);
    return invalidation;
  }

  bool IsInvalidationSent(const invalidation::Invalidation& invalidation) {
    return !invalidation_service_.GetFakeAckHandler()->IsUnsent(invalidation);
  }

  bool IsInvalidationAcknowledged(
      const invalidation::Invalidation& invalidation) {
    return invalidation_service_.GetFakeAckHandler()->IsAcknowledged(
        invalidation);
  }

  bool IsInvalidatorRegistered() {
    return !invalidation_service_.invalidator_registrar()
                .GetRegisteredTopics(&invalidator_)
                .empty();
  }

  std::set<std::string> GetSubscribedTopics() {
    std::set<std::string> topics;
    for (const auto& topic : invalidation_service_.invalidator_registrar()
                                 .GetAllSubscribedTopics()) {
      topics.insert(topic.first);
    }

    return topics;
  }

  std::set<std::string> GetRegisteredTopics() {
    std::set<std::string> topics;
    for (const auto& topic :
         invalidation_service_.invalidator_registrar().GetRegisteredTopics(
             &invalidator_)) {
      topics.insert(topic.first);
    }

    return topics;
  }

  void VerifyExpectations() {
    Mock::VerifyAndClearExpectations(&invalidator_);
  }

 protected:
  // Initialize and start the invalidator.
  void InitializeAndStart() {
    EXPECT_CALL(invalidator_, OnInitialize()).Times(1);
    invalidator_.Initialize(&invalidation_service_);
    VerifyExpectations();

    EXPECT_CALL(invalidator_, OnStart()).Times(1);
    invalidator_.Start();

    VerifyExpectations();
  }

  // Stop and shutdown the invalidator.
  void StopAndShutdown() {
    EXPECT_CALL(invalidator_, OnStop()).Times(1);
    EXPECT_CALL(invalidator_, OnShutdown()).Times(1);
    invalidator_.Shutdown();

    VerifyExpectations();
  }

  // Fire an invalidation to verify that invalidation is not working.
  void VerifyInvalidationDisabled(const invalidation::Topic& topic) {
    const invalidation::Invalidation invalidation = FireInvalidation(topic);

    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(IsInvalidationSent(invalidation));
  }

  // Fire an invalidation to verify that invalidation works.
  void VerifyInvalidationEnabled(const invalidation::Topic& topic) {
    EXPECT_TRUE(invalidator_.invalidations_enabled());

    EXPECT_CALL(invalidator_,
                DoRemoteCommandsFetch(Eq(CreateInvalidation(topic))))
        .Times(1);
    const invalidation::Invalidation invalidation = FireInvalidation(topic);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(IsInvalidationSent(invalidation));
    EXPECT_TRUE(IsInvalidationAcknowledged(invalidation));
    VerifyExpectations();
  }

  // Fire an invalidation to verify that invalidation is not working. It is
  // expected that invalidator did not receive/acknowledged the invalidation.
  void VerifyTopicSubscribedButInvalidationDisabled(
      const invalidation::Topic& topic) {
    EXPECT_FALSE(invalidator_.invalidations_enabled());
    EXPECT_CALL(invalidator_, DoRemoteCommandsFetch(_)).Times(0);
    const invalidation::Invalidation invalidation = FireInvalidation(topic);

    EXPECT_TRUE(
        invalidation_service_.GetFakeAckHandler()->IsUnacked(invalidation));
    EXPECT_FALSE(invalidation_service_.GetFakeAckHandler()->IsAcknowledged(
        invalidation));
    VerifyExpectations();
  }

  invalidation::Topic kTestingTopic1;
  invalidation::Topic kTestingTopic2;

  base::test::SingleThreadTaskEnvironment task_environment_;

  invalidation::FakeInvalidationService invalidation_service_;
  StrictMock<MockRemoteCommandInvalidator> invalidator_;
};

// Verifies that only the fired invalidations will be received.
TEST_F(RemoteCommandsInvalidatorTest, FiredInvalidation) {
  InitializeAndStart();

  // Invalidator won't work at this point.
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  // Load the policy data, it should work now.
  invalidator_.SetInvalidationTopic(kTestingTopic1);
  EXPECT_TRUE(invalidator_.invalidations_enabled());

  base::RunLoop().RunUntilIdle();
  // No invalidation will be received if no invalidation is fired.
  VerifyExpectations();

  // Fire an invalidation with different object id, no invalidation will be
  // received.
  const invalidation::Invalidation invalidation1 =
      FireInvalidation(kTestingTopic2);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsInvalidationSent(invalidation1));
  VerifyExpectations();

  // Fire the invalidation, it should be acknowledged and trigger a remote
  // commands fetch.
  EXPECT_CALL(invalidator_,
              DoRemoteCommandsFetch(Eq(CreateInvalidation(kTestingTopic1))))
      .Times(1);
  const invalidation::Invalidation invalidation2 =
      FireInvalidation(kTestingTopic1);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsInvalidationSent(invalidation2));
  EXPECT_TRUE(IsInvalidationAcknowledged(invalidation2));
  VerifyExpectations();

  StopAndShutdown();
}

// Verifies that no invalidation will be received when invalidator is shutdown.
TEST_F(RemoteCommandsInvalidatorTest, ShutDown) {
  EXPECT_FALSE(invalidator_.invalidations_enabled());
  FireInvalidation(kTestingTopic1);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(invalidator_.invalidations_enabled());
}

// Verifies that no invalidation will be received when invalidator is stopped.
TEST_F(RemoteCommandsInvalidatorTest, Stopped) {
  EXPECT_CALL(invalidator_, OnInitialize()).Times(1);
  invalidator_.Initialize(&invalidation_service_);
  VerifyExpectations();

  EXPECT_FALSE(invalidator_.invalidations_enabled());
  FireInvalidation(kTestingTopic2);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  EXPECT_CALL(invalidator_, OnShutdown()).Times(1);
  invalidator_.Shutdown();
}

// Verifies that stated/stopped state changes work as expected.
TEST_F(RemoteCommandsInvalidatorTest, StartedStateChange) {
  InitializeAndStart();

  // Invalidator requires topic to work.
  VerifyInvalidationDisabled(kTestingTopic1);
  EXPECT_FALSE(invalidator_.invalidations_enabled());
  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);
  EXPECT_EQ(GetSubscribedTopics(), std::set<std::string>{kTestingTopic1});
  EXPECT_EQ(GetRegisteredTopics(), std::set<std::string>{kTestingTopic1});

  // Stop and restart invalidator.
  EXPECT_CALL(invalidator_, OnStop()).Times(1);
  invalidator_.Stop();
  VerifyExpectations();

  VerifyTopicSubscribedButInvalidationDisabled(kTestingTopic1);
  EXPECT_FALSE(invalidator_.invalidations_enabled());
  EXPECT_EQ(GetSubscribedTopics(), std::set<std::string>{kTestingTopic1});
  EXPECT_EQ(GetRegisteredTopics(), std::set<std::string>{});

  EXPECT_CALL(invalidator_, OnStart()).Times(1);
  invalidator_.Start();
  VerifyExpectations();

  // Invalidator requires topic to work.
  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);
  EXPECT_EQ(GetSubscribedTopics(), std::set<std::string>{kTestingTopic1});
  EXPECT_EQ(GetRegisteredTopics(), std::set<std::string>{kTestingTopic1});

  StopAndShutdown();
}

// Verifies that registered state changes work as expected.
TEST_F(RemoteCommandsInvalidatorTest, RegistedStateChange) {
  InitializeAndStart();

  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);

  invalidator_.SetInvalidationTopic(kTestingTopic2);
  VerifyInvalidationEnabled(kTestingTopic2);
  VerifyInvalidationDisabled(kTestingTopic1);

  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);
  VerifyInvalidationDisabled(kTestingTopic2);

  invalidator_.ClearInvalidationTopic();
  VerifyInvalidationDisabled(kTestingTopic1);
  VerifyInvalidationDisabled(kTestingTopic2);
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  invalidator_.SetInvalidationTopic(kTestingTopic2);
  VerifyInvalidationEnabled(kTestingTopic2);
  VerifyInvalidationDisabled(kTestingTopic1);

  StopAndShutdown();
}

// Verifies that invalidation service enabled state changes work as expected.
TEST_F(RemoteCommandsInvalidatorTest, InvalidationServiceEnabledStateChanged) {
  InitializeAndStart();

  invalidator_.SetInvalidationTopic(kTestingTopic1);
  VerifyInvalidationEnabled(kTestingTopic1);

  DisableInvalidationService();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  EnableInvalidationService();
  VerifyInvalidationEnabled(kTestingTopic1);

  EnableInvalidationService();
  VerifyInvalidationEnabled(kTestingTopic1);

  DisableInvalidationService();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  DisableInvalidationService();
  EXPECT_FALSE(invalidator_.invalidations_enabled());

  StopAndShutdown();
}

}  // namespace policy
