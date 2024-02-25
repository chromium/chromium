// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/user_command_arc_job.h"

#include <memory>
#include <string>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/components/arc/test/fake_policy_instance.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

std::unique_ptr<RemoteCommandJob> CreateArcJob(Profile* profile,
                                               base::TimeTicks issued_time,
                                               const std::string& payload) {
  // Create the job proto.
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_USER_ARC_COMMAND);
  constexpr RemoteCommandJob::UniqueIDType kUniqueID = 123456789;
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(
      (base::TimeTicks::Now() - issued_time).InMilliseconds());
  command_proto.set_payload(payload);

  // Create the job and validate.
  auto job = std::make_unique<UserCommandArcJob>(profile);

  EXPECT_TRUE(job->Init(base::TimeTicks::Now(), command_proto,
                        enterprise_management::SignedData()));
  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());

  return job;
}

class UserCommandArcJobTest : public testing::Test {
 public:
  UserCommandArcJobTest(const UserCommandArcJobTest&) = delete;
  UserCommandArcJobTest& operator=(const UserCommandArcJobTest&) = delete;

 protected:
  UserCommandArcJobTest();
  ~UserCommandArcJobTest() override;

  content::BrowserTaskEnvironment task_environment_;

  // ArcServiceManager needs to be created before ArcPolicyBridge (since the
  // Bridge depends on the Manager), and it needs to be destroyed after Profile
  // (because BrowserContextKeyedServices are destroyed together with Profile,
  // and ArcPolicyBridge is such a service).
  const std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  const std::unique_ptr<TestingProfile> profile_;
  raw_ptr<arc::ArcPolicyBridge> arc_policy_bridge_;
  std::unique_ptr<arc::FakePolicyInstance> policy_instance_;
};

UserCommandArcJobTest::UserCommandArcJobTest()
    : arc_service_manager_(std::make_unique<arc::ArcServiceManager>()),
      profile_(std::make_unique<TestingProfile>()) {
  ash::ConciergeClient::InitializeFake(nullptr);
  arc_session_manager_ =
      CreateTestArcSessionManager(std::make_unique<arc::ArcSessionRunner>(
          base::BindRepeating(arc::FakeArcSession::Create)));
  arc_policy_bridge_ =
      arc::ArcPolicyBridge::GetForBrowserContextForTesting(profile_.get());
  policy_instance_ = std::make_unique<arc::FakePolicyInstance>();
  arc_service_manager_->arc_bridge_service()->policy()->SetInstance(
      policy_instance_.get());
}

UserCommandArcJobTest::~UserCommandArcJobTest() {
  arc_service_manager_->arc_bridge_service()->policy()->CloseInstance(
      policy_instance_.get());
}

TEST_F(UserCommandArcJobTest, TestPayloadReceiving) {
  const std::string kPayload = "testing payload";
  std::unique_ptr<RemoteCommandJob> job =
      CreateArcJob(profile_.get(), base::TimeTicks::Now(), kPayload);
  base::test::TestFuture<void> job_finished_future;
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       job_finished_future.GetCallback()));
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  EXPECT_EQ(RemoteCommandJob::SUCCEEDED, job->status());
  EXPECT_EQ(kPayload, policy_instance_->command_payload());
}

}  // namespace policy
