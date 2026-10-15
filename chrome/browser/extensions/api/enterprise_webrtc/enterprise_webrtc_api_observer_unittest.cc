// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_webrtc/enterprise_webrtc_api_observer.h"

#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/extensions/api/enterprise_webrtc.h"
#include "content/public/browser/webrtc_diagnostics.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace {

constexpr char kExtensionId[] = "abcdefghijklmnopqrstuvwxyzabcdef";
constexpr char kExtensionId2[] = "abcdefghijklmnopqrstuvwxyzaaaaaa";
constexpr char kPeerConnectionId[] = "7-1";

// Captures the extension_id and args of the last onSnapshotTruncated
// dispatch.
class TruncationEventObserver : public TestEventRouter::EventObserver {
 public:
  explicit TruncationEventObserver(TestEventRouter* event_router)
      : event_router_(event_router) {
    event_router_->AddEventObserver(this);
  }
  TruncationEventObserver(const TruncationEventObserver&) = delete;
  TruncationEventObserver& operator=(const TruncationEventObserver&) = delete;
  ~TruncationEventObserver() override {
    event_router_->RemoveEventObserver(this);
  }

  void OnDispatchEventToExtension(const std::string& extension_id,
                                  const Event& event) override {
    if (event.event_name !=
        api::enterprise_webrtc::OnSnapshotTruncated::kEventName) {
      return;
    }
    dispatched_to_.push_back(extension_id);
    last_args_ = base::Value(event.args().Clone());
  }

  const std::vector<std::string>& dispatched_to() const {
    return dispatched_to_;
  }
  const base::Value& last_args() const { return last_args_; }

 private:
  const raw_ptr<TestEventRouter> event_router_;
  std::vector<std::string> dispatched_to_;
  base::Value last_args_;
};

// An add-peer-connection record with every field PeerConnectionRecord
// declares.
base::DictValue ValidRecord() {
  return base::test::ParseJsonDict(R"({
    "rid": 7,
    "lid": 1,
    "pid": 4242,
    "rtcConfiguration": "{ \"iceServers\": [], \"iceCandidatePoolSize\": 2 }",
    "url": "https://example.test/",
    "isOpen": true,
    "connected": false,
    "timestamp": 1758150000000.0
  })");
}

}  // namespace

class EnterpriseWebrtcApiObserverTest : public ExtensionServiceTestBase {
 public:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    event_router_ = CreateAndUseTestEventRouter(profile());
  }

  void TearDown() override {
    event_router_ = nullptr;
    ExtensionServiceTestBase::TearDown();
  }

 protected:
  EnterpriseWebrtcApiObserver* observer() {
    return EnterpriseWebrtcApiObserver::Get(profile());
  }

  int added_event_count() {
    return event_router_->GetEventCount(
        api::enterprise_webrtc::OnPeerConnectionAdded::kEventName);
  }

  int snapshot_truncated_event_count() {
    return event_router_->GetEventCount(
        api::enterprise_webrtc::OnSnapshotTruncated::kEventName);
  }

  TestEventRouter* event_router() { return event_router_; }

 private:
  raw_ptr<TestEventRouter> event_router_ = nullptr;
};

TEST_F(EnterpriseWebrtcApiObserverTest, PeerConnectionRecordIsDispatched) {
  ASSERT_TRUE(observer());

  observer()->OnPeerConnectionAdded(kPeerConnectionId,
                                    base::Value(ValidRecord()), {kExtensionId});

  EXPECT_EQ(1, added_event_count());
}

// WebRTCInternals builds the record, so a missing field means it and the
// webidl have drifted apart. Nothing reaches the extension in that case.
TEST_F(EnterpriseWebrtcApiObserverTest, RecordMissingARequiredFieldIsDropped) {
  ASSERT_TRUE(observer());
  base::DictValue record = ValidRecord();
  ASSERT_TRUE(record.Remove("url"));

  observer()->OnPeerConnectionAdded(
      kPeerConnectionId, base::Value(std::move(record)), {kExtensionId});

  EXPECT_EQ(0, added_event_count());
}

// OnSnapshotTruncated dispatches to all currently capturing clients. The
// observer is invoked directly because forcing a real buffer overflow is
// impractical.
TEST_F(EnterpriseWebrtcApiObserverTest, SnapshotTruncatedIsDispatched) {
  ASSERT_TRUE(observer());
  ASSERT_EQ(content::WebRtcDiagnostics::GetInstance()->StartCaptureForClient(
                profile(), kExtensionId, {}),
            content::WebRtcDiagnostics::StartCaptureResult::kSuccess);
  TruncationEventObserver truncation_observer(event_router());

  observer()->OnSnapshotTruncated(5, 10, 15);

  EXPECT_THAT(truncation_observer.dispatched_to(),
              testing::ElementsAre(kExtensionId));
  ASSERT_TRUE(truncation_observer.last_args().is_list());
  const base::ListValue& args = truncation_observer.last_args().GetList();
  ASSERT_EQ(1u, args.size());
  const base::DictValue* info = args[0].GetIfDict();
  ASSERT_TRUE(info);
  EXPECT_EQ(5, info->FindInt("droppedLogEntries"));
  EXPECT_EQ(10, info->FindInt("droppedStatsEntries"));
  EXPECT_EQ(15, info->FindInt("droppedMediaEntries"));

  content::WebRtcDiagnostics::GetInstance()->StopCaptureForClient(profile(),
                                                                  kExtensionId);
}

// With no client capturing, GetCapturingClients() is empty, so there is
// nothing to dispatch to.
TEST_F(EnterpriseWebrtcApiObserverTest,
       SnapshotTruncatedSkipsNonCapturingExtensions) {
  ASSERT_TRUE(observer());
  TruncationEventObserver truncation_observer(event_router());

  observer()->OnSnapshotTruncated(5, 10, 15);

  EXPECT_TRUE(truncation_observer.dispatched_to().empty());
  EXPECT_EQ(0, snapshot_truncated_event_count());
}

// Same setup as SnapshotTruncatedIsDispatched, but with a second client
// (kExtensionId2) that is also capturing, to prove the dispatch set is not
// just "whatever the first client is" but genuinely tracks every capturing
// client.
TEST_F(EnterpriseWebrtcApiObserverTest,
       SnapshotTruncatedReachesEveryCapturingExtension) {
  ASSERT_TRUE(observer());
  ASSERT_EQ(content::WebRtcDiagnostics::GetInstance()->StartCaptureForClient(
                profile(), kExtensionId, {}),
            content::WebRtcDiagnostics::StartCaptureResult::kSuccess);
  ASSERT_EQ(content::WebRtcDiagnostics::GetInstance()->StartCaptureForClient(
                profile(), kExtensionId2, {}),
            content::WebRtcDiagnostics::StartCaptureResult::kSuccess);
  TruncationEventObserver truncation_observer(event_router());

  observer()->OnSnapshotTruncated(5, 10, 15);

  EXPECT_THAT(truncation_observer.dispatched_to(),
              testing::UnorderedElementsAre(kExtensionId, kExtensionId2));

  content::WebRtcDiagnostics::GetInstance()->StopCaptureForClient(profile(),
                                                                  kExtensionId);
  content::WebRtcDiagnostics::GetInstance()->StopCaptureForClient(
      profile(), kExtensionId2);
}

// Documents a known gap (crbug.com/571481238): unlike Added/Removed,
// onSnapshotTruncated does not consult each client's origin filter, so a
// client restricted to an origin that had nothing dropped still receives
// the event. GetCapturingClients() has no origin filtering to give it one.
TEST_F(EnterpriseWebrtcApiObserverTest, SnapshotTruncatedIgnoresOriginFilter) {
  ASSERT_TRUE(observer());
  ASSERT_EQ(content::WebRtcDiagnostics::GetInstance()->StartCaptureForClient(
                profile(), kExtensionId,
                {url::Origin::Create(GURL("https://a.example"))}),
            content::WebRtcDiagnostics::StartCaptureResult::kSuccess);

  observer()->OnSnapshotTruncated(5, 10, 15);

  EXPECT_EQ(1, snapshot_truncated_event_count());

  content::WebRtcDiagnostics::GetInstance()->StopCaptureForClient(profile(),
                                                                  kExtensionId);
}

}  // namespace extensions
