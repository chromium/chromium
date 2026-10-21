// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace context_hub {
namespace {

class SkillRecordingServiceTest : public testing::Test {
 public:
  SkillRecordingServiceTest() = default;
  ~SkillRecordingServiceTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  SkillRecordingService service_;
};

TEST_F(SkillRecordingServiceTest, InitialState) {
  EXPECT_FALSE(service_.is_recording());
  EXPECT_FALSE(service_.start_time().has_value());
  auto result = service_.StopRecording();
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->actions.empty());
  EXPECT_FALSE(service_.is_recording());
}

TEST_F(SkillRecordingServiceTest, StartAndStopRecording) {
  service_.StartRecording();
  EXPECT_TRUE(service_.is_recording());
  EXPECT_EQ(service_.start_time(), base::TimeTicks::Now());

  auto result = service_.StopRecording();
  EXPECT_FALSE(service_.is_recording());
  EXPECT_FALSE(service_.start_time().has_value());
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->actions.empty());
}

TEST_F(SkillRecordingServiceTest, IgnoresMultipleStartRecording) {
  const base::TimeTicks initial_start_time = base::TimeTicks::Now();
  service_.StartRecording();
  EXPECT_TRUE(service_.is_recording());
  EXPECT_EQ(service_.start_time(), initial_start_time);

  task_environment_.FastForwardBy(base::Seconds(5));

  // Calling StartRecording again while already recording should be a no-op.
  service_.StartRecording();
  EXPECT_TRUE(service_.is_recording());
  // The start time should be unchanged.
  EXPECT_EQ(service_.start_time(), initial_start_time);

  auto result = service_.StopRecording();
  EXPECT_FALSE(service_.is_recording());
  EXPECT_FALSE(service_.start_time().has_value());
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->actions.empty());
}

}  // namespace
}  // namespace context_hub
