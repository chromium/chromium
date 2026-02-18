// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_driver.h"
#include "chrome/browser/record_replay/record_replay_driver_factory.h"
#include "chrome/browser/record_replay/record_replay_manager.h"
#include "chrome/browser/record_replay/recording_data_manager.h"
#include "chrome/common/record_replay/aliases.h"
#include "chrome/common/record_replay/record_replay_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace record_replay {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

// TODO(b/476101114): Replace with mojom::RecordReplayAgent.
class MockRecordReplayAgent : public RecordReplayDriver::TestRecordReplayAgent {
 public:
  MOCK_METHOD(void, StartRecording, (), (override));
  MOCK_METHOD(void, StopRecording, (), (override));
  MOCK_METHOD(void,
              GetElementSelector,
              (DomNodeId dom_node_id, base::OnceCallback<void(Selector)> cb),
              (override));
  MOCK_METHOD(void,
              GetMatchingElements,
              (Selector element_selector,
               base::OnceCallback<void(const std::vector<DomNodeId>&)> cb),
              (override));
  MOCK_METHOD(void,
              DoClick,
              (DomNodeId dom_node_id, base::OnceCallback<void(bool)> cb),
              (override));
  MOCK_METHOD(void,
              DoPaste,
              (DomNodeId dom_node_id,
               FieldValue text,
               base::OnceCallback<void(bool)> cb),
              (override));
  MOCK_METHOD(void,
              DoSelect,
              (DomNodeId dom_node_id,
               FieldValue value,
               base::OnceCallback<void(bool)> cb),
              (override));
};

class MockRecordReplayClient : public RecordReplayClient,
                               public content::WebContentsObserver {
 public:
  explicit MockRecordReplayClient(content::WebContents* web_contents) {
    driver_factory_.Observe(web_contents);
    ON_CALL(*this, GetPrimaryMainFrameUrl())
        .WillByDefault(Return(web_contents->GetLastCommittedURL()));
    ON_CALL(*this, GetManager()).WillByDefault(ReturnRef(manager_));
    ON_CALL(*this, GetDriverFactory())
        .WillByDefault(ReturnRef(driver_factory_));
    ON_CALL(*this, GetRecordingDataManager()).WillByDefault(Return(nullptr));
    web_contents->ForEachRenderFrameHost(
        [this](content::RenderFrameHost* rfh) { RenderFrameCreated(rfh); });
  }
  ~MockRecordReplayClient() override = default;

  MOCK_METHOD(RecordReplayManager&, GetManager, (), (override));
  MOCK_METHOD(RecordReplayDriverFactory&, GetDriverFactory, (), (override));
  MOCK_METHOD(RecordingDataManager*, GetRecordingDataManager, (), (override));
  MOCK_METHOD(GURL, GetPrimaryMainFrameUrl, (), (override));
  MOCK_METHOD(autofill::AutofillClient*, GetAutofillClient, (), (override));
  MOCK_METHOD(void, ReportToUser, (std::string_view message), (override));

  MockRecordReplayAgent& agent() { return agent_; }

 private:
  void RenderFrameCreated(content::RenderFrameHost* rfh) override {
    if (RecordReplayDriver* driver = driver_factory_.GetOrCreateDriver(rfh)) {
      driver->set_record_replay_agent_for_test(&agent_);
    }
  }

  RecordReplayManager manager_{this};
  MockRecordReplayAgent agent_;
  RecordReplayDriverFactory driver_factory_{*this};
};

class ReplayerTest : public ChromeRenderViewHostTestHarness {
 public:
  ReplayerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://foo.com"));
    client_.emplace(web_contents());
  }

  void TearDown() override {
    client_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  RecordReplayManager& manager() { return client_->GetManager(); }
  MockRecordReplayAgent& agent() { return client_->agent(); }
  base::MockOnceClosure& on_finish() { return on_finish_; }

 private:
  base::test::ScopedFeatureList feature_list_{
      record_replay::features::kRecordReplayBase};
  base::MockOnceClosure on_finish_;
  std::optional<NiceMock<MockRecordReplayClient>> client_;
};

// Tests that a recording without any action terminates right away.
TEST_F(ReplayerTest, EmptyRecording) {
  Recording recording;
  recording.set_url("https://foo.com");
  recording.set_start_time(1234);

  EXPECT_CALL(on_finish(), Run());

  Replayer replayer(&manager(), std::move(recording), on_finish().Get());
  replayer.Run();
}

// Tests that a simple recording is executed correctly.
// In particular, checks that the action is delayed the recorded delta.
TEST_F(ReplayerTest, SuccessfulRecording_SingleAction) {
  Recording recording;
  recording.set_url("https://foo.com");
  recording.set_start_time(1234);
  {
    Recording::Action* a = recording.add_actions();
    a->set_delta(123);
    a->set_element_selector("#action1");
    a->mutable_click_specifics();
  }

  MockFunction<void(std::string_view)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("insufficient time passed"));

    EXPECT_CALL(agent(), GetMatchingElements)
        .WillOnce(RunOnceCallback<1>(std::vector<DomNodeId>{DomNodeId(123)}));
    EXPECT_CALL(agent(), DoClick(DomNodeId(123), _))
        .WillOnce(RunOnceCallback<1>(true));

    EXPECT_CALL(on_finish(), Run());
    EXPECT_CALL(check, Call("sufficient time passed"));
  }

  Replayer replayer(&manager(), std::move(recording), on_finish().Get());
  replayer.Run();
  task_environment()->FastForwardBy(base::Microseconds(120));
  check.Call("insufficient time passed");
  task_environment()->FastForwardBy(base::Microseconds(5));
  check.Call("sufficient time passed");
}

// Tests that a recording with two actions is executed correctly.
TEST_F(ReplayerTest, SuccessfulRecording_MultipleAction) {
  Recording recording;
  recording.set_url("https://foo.com");
  recording.set_start_time(1234);
  {
    Recording::Action* a = recording.add_actions();
    a->set_delta(125);
    a->set_element_selector("#action1");
    a->mutable_select_specifics()->set_value("select value");
  }
  {
    Recording::Action* a = recording.add_actions();
    a->set_delta(125);
    a->set_element_selector("#action2");
    a->mutable_text_specifics()->set_value("text value");
  }

  MockFunction<void(std::string_view)> check;
  {
    InSequence s;

    EXPECT_CALL(agent(), GetMatchingElements)
        .WillOnce(RunOnceCallback<1>(std::vector<DomNodeId>{DomNodeId(123)}));
    EXPECT_CALL(agent(),
                DoSelect(DomNodeId(123), FieldValue("select value"), _))
        .WillOnce(RunOnceCallback<2>(true));

    EXPECT_CALL(check, Call("first action done"));

    EXPECT_CALL(agent(), GetMatchingElements)
        .WillOnce(RunOnceCallback<1>(std::vector<DomNodeId>{DomNodeId(456)}));
    EXPECT_CALL(agent(), DoPaste(DomNodeId(456), FieldValue("text value"), _))
        .WillOnce(RunOnceCallback<2>(true));

    EXPECT_CALL(on_finish(), Run());
  }

  Replayer replayer(&manager(), std::move(recording), on_finish().Get());
  replayer.Run();
  task_environment()->FastForwardBy(base::Microseconds(150));
  check.Call("first action done");
  task_environment()->FastForwardBy(base::Microseconds(150));
}

// Tests that an action is repeated if it fails.
TEST_F(ReplayerTest, SuccessfulRecording_Repeat_Success) {
  Recording recording;
  recording.set_url("https://foo.com");
  recording.set_start_time(1234);
  {
    Recording::Action* a = recording.add_actions();
    a->set_delta(125);
    a->set_element_selector("#action1");
    a->mutable_click_specifics();
  }

  {
    InSequence s;

    EXPECT_CALL(agent(), GetMatchingElements)
        .WillOnce(RunOnceCallback<1>(std::vector<DomNodeId>{}));

    EXPECT_CALL(agent(), GetMatchingElements)
        .WillOnce(RunOnceCallback<1>(
            std::vector<DomNodeId>{DomNodeId(123), DomNodeId(456)}));

    EXPECT_CALL(agent(), GetMatchingElements)
        .WillOnce(RunOnceCallback<1>(std::vector<DomNodeId>{DomNodeId(123)}));
    EXPECT_CALL(agent(), DoClick(DomNodeId(123), _))
        .WillOnce(RunOnceCallback<1>(true));

    EXPECT_CALL(on_finish(), Run());
  }

  Replayer replayer(&manager(), std::move(recording), on_finish().Get());
  replayer.Run();
  task_environment()->FastForwardBy(base::Microseconds(125));
  task_environment()->FastForwardBy(base::Seconds(3));
  task_environment()->FastForwardBy(base::Seconds(3));
}

// Tests that if an action fails 3 times, the recording is stopped.
TEST_F(ReplayerTest, SuccessfulRecording_Repeat_Failure) {
  Recording recording;
  recording.set_url("https://foo.com");
  recording.set_start_time(1234);
  {
    Recording::Action* a = recording.add_actions();
    a->set_delta(125);
    a->set_element_selector("#action1");
    a->mutable_click_specifics();
  }
  {
    Recording::Action* a = recording.add_actions();
    a->set_delta(125);
    a->set_element_selector("#action2");
    a->mutable_click_specifics();
  }

  {
    InSequence s;

    EXPECT_CALL(agent(), GetMatchingElements)
        .WillOnce(RunOnceCallback<1>(std::vector<DomNodeId>{DomNodeId(123)}));
    EXPECT_CALL(agent(), DoClick(DomNodeId(123), _))
        .WillOnce(RunOnceCallback<1>(false));  // Failure!

    EXPECT_CALL(agent(), GetMatchingElements)
        .WillOnce(RunOnceCallback<1>(std::vector<DomNodeId>{DomNodeId(123)}));
    EXPECT_CALL(agent(), DoClick(DomNodeId(123), _))
        .WillOnce(RunOnceCallback<1>(false));  // Failure!

    EXPECT_CALL(agent(), GetMatchingElements)
        .WillOnce(RunOnceCallback<1>(std::vector<DomNodeId>{DomNodeId(123)}));
    EXPECT_CALL(agent(), DoClick(DomNodeId(123), _))
        .WillOnce(RunOnceCallback<1>(false));  // Failure!

    EXPECT_CALL(on_finish(), Run());
  }

  Replayer replayer(&manager(), std::move(recording), on_finish().Get());
  replayer.Run();
  task_environment()->FastForwardBy(base::Minutes(1));
}

}  // namespace
}  // namespace record_replay
