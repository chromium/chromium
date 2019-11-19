// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/user_command_arc_job.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/connection_holder.h"
#include "components/arc/test/fake_policy_instance.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

std::unique_ptr<policy::RemoteCommandJob> CreateArcJob(
    Profile* profile,
    base::TimeTicks issued_time,
    const std::string& payload) {
  // Create the job proto.
  enterprise_management::RemoteCommand command_proto;
  command_proto.set_type(
      enterprise_management::RemoteCommand_Type_USER_ARC_COMMAND);
  constexpr policy::RemoteCommandJob::UniqueIDType kUniqueID = 123456789;
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(
      (base::TimeTicks::Now() - issued_time).InMilliseconds());
  command_proto.set_payload(payload);

  // Create the job and validate.
  auto job = std::make_unique<policy::UserCommandArcJob>(profile);

  EXPECT_TRUE(job->Init(base::TimeTicks::Now(), command_proto, nullptr));
  EXPECT_EQ(kUniqueID, job->unique_id());
  EXPECT_EQ(policy::RemoteCommandJob::NOT_STARTED, job->status());

  return job;
}

class UserCommandArcJobTest : public testing::Test {
 protected:
  UserCommandArcJobTest();
  ~UserCommandArcJobTest() override;

  content::BrowserTaskEnvironment task_environment_;

  // ArcServiceManager needs to be created before ArcPolicyBridge (since the
  // Bridge depends on the Manager), and it needs to be destroyed after Profile
  // (because BrowserContextKeyedServices are destroyed together with Profile,
  // and ArcPolicyBridge is such a service).
  const std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  const std::unique_ptr<TestingProfile> profile_;
  arc::ArcPolicyBridge* const arc_policy_bridge_;
  const std::unique_ptr<arc::FakePolicyInstance> policy_instance_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserCommandArcJobTest);
};

UserCommandArcJobTest::UserCommandArcJobTest()
    : arc_service_manager_(std::make_unique<arc::ArcServiceManager>()),
      profile_(std::make_unique<TestingProfile>()),
      arc_policy_bridge_(
          arc::ArcPolicyBridge::GetForBrowserContextForTesting(profile_.get())),
      policy_instance_(std::make_unique<arc::FakePolicyInstance>()) {
  arc_service_manager_->arc_bridge_service()->policy()->SetInstance(
      policy_instance_.get());
}

UserCommandArcJobTest::~UserCommandArcJobTest() {
  arc_service_manager_->arc_bridge_service()->policy()->CloseInstance(
      policy_instance_.get());
}

TEST_F(UserCommandArcJobTest, TestPayloadReceiving) {
  const std::string kPayload = "testing payload";
  std::unique_ptr<policy::RemoteCommandJob> job =
      CreateArcJob(profile_.get(), base::TimeTicks::Now(), kPayload);
  base::RunLoop run_loop;

  auto check_result_callback = base::AdaptCallbackForRepeating(base::BindOnce(
      [](base::RunLoop* run_loop, policy::RemoteCommandJob* job,
         arc::FakePolicyInstance* policy_instance,
         std::string expected_payload) {
        EXPECT_EQ(policy::RemoteCommandJob::SUCCEEDED, job->status());
        EXPECT_EQ(expected_payload, policy_instance->command_payload());
        run_loop->Quit();
      },
      &run_loop, job.get(), policy_instance_.get(), kPayload));
  EXPECT_TRUE(job->Run(base::Time::Now(), base::TimeTicks::Now(),
                       check_result_callback));
  run_loop.Run();
}

}  // namespace policy
