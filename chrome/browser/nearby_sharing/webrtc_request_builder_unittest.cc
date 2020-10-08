// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/webrtc_request_builder.h"

#include "base/i18n/timezone.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace {

const char kSelfId[] = "my_id";
const char kPeerId[] = "their_id";

}  // namespace

class WebRtcRequestBuilderTest : public testing::Test {
 public:
  void SetUp() override {
    icu::TimeZone::adoptDefault(
        icu::TimeZone::createTimeZone("America/Los_Angeles"));
  }

  void VerifyLocationHint(
      chrome_browser_nearby_sharing_instantmessaging::Id id) {
    EXPECT_EQ(chrome_browser_nearby_sharing_instantmessaging::
                  LocationStandard_Format_ISO_3166_1_ALPHA_2,
              id.location_hint().format());
    EXPECT_EQ("US", id.location_hint().location());
  }
};

TEST_F(WebRtcRequestBuilderTest, BuildSendRequest) {
  chrome_browser_nearby_sharing_instantmessaging::SendMessageExpressRequest
      request = BuildSendRequest(kSelfId, kPeerId);
  EXPECT_EQ(kSelfId, request.header().requester_id().id());
  EXPECT_EQ(kPeerId, request.dest_id().id());
  VerifyLocationHint(request.dest_id());
  VerifyLocationHint(request.header().requester_id());
}

TEST_F(WebRtcRequestBuilderTest, BuildReceiveRequest) {
  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesExpressRequest
      request = BuildReceiveRequest(kSelfId);
  EXPECT_EQ(kSelfId, request.header().requester_id().id());
  VerifyLocationHint(request.header().requester_id());
}
