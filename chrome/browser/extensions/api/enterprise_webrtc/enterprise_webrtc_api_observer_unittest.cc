// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_webrtc/enterprise_webrtc_api_observer.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/extensions/api/enterprise_webrtc.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kExtensionId[] = "abcdefghijklmnopqrstuvwxyzabcdef";
constexpr char kPeerConnectionId[] = "7-1";

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

}  // namespace extensions
