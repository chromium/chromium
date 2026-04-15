// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/glic_experimental_triggering/glic_experimental_triggering_message_handler.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;

class GlicExperimentalTriggeringMessageHandlerTest : public testing::Test {
 public:
  GlicExperimentalTriggeringMessageHandlerTest() = default;
  ~GlicExperimentalTriggeringMessageHandlerTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_{
      features::kGlicExperimentalTriggering};
  TestingProfile profile_;
  GlicExperimentalTriggeringMessageHandler handler_{&profile_};
};

TEST_F(GlicExperimentalTriggeringMessageHandlerTest, HandlesMessageGracefully) {
  components_sharing_message::SharingMessage message;
  message.mutable_glic_experimental_triggering();

  base::MockOnceCallback<void(
      std::unique_ptr<components_sharing_message::ResponseMessage>)>
      done_callback;

  EXPECT_CALL(done_callback, Run(_)).Times(1);

  handler_.OnMessage(std::move(message), done_callback.Get());
}

}  // namespace
