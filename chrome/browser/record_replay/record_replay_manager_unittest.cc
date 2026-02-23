// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/record_replay_manager.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_driver_factory.h"
#include "chrome/browser/record_replay/recording_data_manager.h"
#include "chrome/common/record_replay/record_replay_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace record_replay {
namespace {

using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class MockRecordingDataManager : public RecordingDataManager {
 public:
  MOCK_METHOD(void, AddRecording, (Recording recording), (override));
  MOCK_METHOD(base::optional_ref<const Recording>,
              GetRecording,
              (const std::string& url),
              (const override));
};

class MockRecordReplayClient : public RecordReplayClient {
 public:
  explicit MockRecordReplayClient(content::WebContents* web_contents) {
    driver_factory_.Observe(web_contents);
    ON_CALL(*this, GetPrimaryMainFrameUrl())
        .WillByDefault(Return(web_contents->GetLastCommittedURL()));
    ON_CALL(*this, GetManager()).WillByDefault(ReturnRef(manager_));
    ON_CALL(*this, GetDriverFactory())
        .WillByDefault(ReturnRef(driver_factory_));
    ON_CALL(*this, GetRecordingDataManager())
        .WillByDefault(Return(&data_manager_));
  }
  ~MockRecordReplayClient() override = default;

  MOCK_METHOD(RecordReplayManager&, GetManager, (), (override));
  MOCK_METHOD(RecordReplayDriverFactory&, GetDriverFactory, (), (override));
  MOCK_METHOD(MockRecordingDataManager*,
              GetRecordingDataManager,
              (),
              (override));
  MOCK_METHOD(GURL, GetPrimaryMainFrameUrl, (), (override));
  MOCK_METHOD(autofill::AutofillClient*, GetAutofillClient, (), (override));
  MOCK_METHOD(void, ReportToUser, (std::string_view message), (override));

 private:
  MockRecordingDataManager data_manager_;
  RecordReplayManager manager_{this};
  RecordReplayDriverFactory driver_factory_{*this};
};

class RecordReplayManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  using State = RecordReplayManager::State;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://foo.com"));
    client_.emplace(web_contents());
  }

  void TearDown() override {
    client_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockRecordReplayClient& client() { return *client_; }
  MockRecordingDataManager& data_manager() {
    return *client_->GetRecordingDataManager();
  }
  RecordReplayManager& manager() { return client_->GetManager(); }

 private:
  base::test::ScopedFeatureList feature_list_{
      record_replay::features::kRecordReplayBase};
  std::optional<NiceMock<MockRecordReplayClient>> client_;
};

// Tests that StartRecording() starts a recording and, when called again, it
// finishes the ongoing recording without implicitly saving it.
// TODO(b/476101114): Expect StartRecording() on the current and future drivers.
TEST_F(RecordReplayManagerTest, StartRecording) {
  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("first StartRecording() call"));
    EXPECT_CALL(client(), ReportToUser);

    EXPECT_CALL(check, Call("second StartRecording() call"));
    EXPECT_CALL(client(), ReportToUser);
  }

  EXPECT_EQ(manager().state(), State::kIdle);

  check.Call("first StartRecording() call");
  manager().StartRecording();
  EXPECT_EQ(manager().state(), State::kRecording);

  check.Call("second StartRecording() call");
  manager().StartRecording();
  EXPECT_EQ(manager().state(), State::kRecording);
}

// Tests that StopRecording() stores a recording after StartRecording().
// TODO(b/476101114): Expect StopRecording() on the current and future drivers.
TEST_F(RecordReplayManagerTest, StopRecording) {
  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("StartRecording() call"));
    EXPECT_CALL(client(), ReportToUser);

    EXPECT_CALL(check, Call("first StopRecording() call"));

    EXPECT_CALL(check, Call("second StopRecording() call"));
  }

  EXPECT_EQ(manager().state(), State::kIdle);

  check.Call("StartRecording() call");
  manager().StartRecording();
  EXPECT_EQ(manager().state(), State::kRecording);

  check.Call("first StopRecording() call");
  manager().StopRecording();
  EXPECT_EQ(manager().state(), State::kIdle);

  check.Call("second StopRecording() call");
  manager().StopRecording();
  EXPECT_EQ(manager().state(), State::kIdle);
}

// Tests that StopRecording() without preceding StartRecording() is a no-op.
TEST_F(RecordReplayManagerTest, StopRecording_NoOp) {
  EXPECT_CALL(data_manager(), AddRecording).Times(0);
  EXPECT_EQ(manager().state(), State::kIdle);
  manager().StopRecording();
  EXPECT_EQ(manager().state(), State::kIdle);
}

}  // namespace
}  // namespace record_replay
