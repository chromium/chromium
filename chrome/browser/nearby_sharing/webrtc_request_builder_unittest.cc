// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/webrtc_request_builder.h"

#include "base/i18n/timezone.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc_signaling_messenger.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc_signaling_messenger.mojom.h"
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

  ::sharing::mojom::LocationHintPtr CountryCodeLocationHint(
      const std::string& country_code) {
    ::sharing::mojom::LocationHintPtr location_hint_ptr =
        ::sharing::mojom::LocationHint::New();
    location_hint_ptr->location = country_code;
    location_hint_ptr->format =
        ::sharing::mojom::LocationStandardFormat::ISO_3166_1_ALPHA_2;
    return location_hint_ptr;
  }

  ::sharing::mojom::LocationHintPtr CallingCodeLocationHint(
      const std::string& calling_code) {
    ::sharing::mojom::LocationHintPtr location_hint_ptr =
        ::sharing::mojom::LocationHint::New();
    location_hint_ptr->location = calling_code;
    location_hint_ptr->format =
        ::sharing::mojom::LocationStandardFormat::E164_CALLING;
    return location_hint_ptr;
  }

  void VerifyLocationHint(
      ::sharing::mojom::LocationHintPtr expected_location_hint,
      chrome_browser_nearby_sharing_instantmessaging::Id id) {
    EXPECT_EQ(static_cast<int>(expected_location_hint->format),
              static_cast<int>(id.location_hint().format()));
    EXPECT_EQ(expected_location_hint->location, id.location_hint().location());
  }
};

TEST_F(WebRtcRequestBuilderTest, BuildSendRequest) {
  ::sharing::mojom::LocationHintPtr location_hint =
      CountryCodeLocationHint("ZZ");
  chrome_browser_nearby_sharing_instantmessaging::SendMessageExpressRequest
      request = BuildSendRequest(kSelfId, kPeerId, location_hint.Clone());
  EXPECT_NE("", request.header().request_id());
  EXPECT_EQ(kSelfId, request.header().requester_id().id());
  EXPECT_EQ(kPeerId, request.dest_id().id());
  VerifyLocationHint(location_hint.Clone(), request.dest_id());
  VerifyLocationHint(location_hint.Clone(), request.header().requester_id());
}

TEST_F(WebRtcRequestBuilderTest, BuildReceiveRequest) {
  ::sharing::mojom::LocationHintPtr location_hint =
      CallingCodeLocationHint("+1");
  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesExpressRequest
      request = BuildReceiveRequest(kSelfId, location_hint.Clone());
  EXPECT_NE("", request.header().request_id());
  EXPECT_EQ(kSelfId, request.header().requester_id().id());
  VerifyLocationHint(location_hint.Clone(), request.header().requester_id());
}

TEST_F(WebRtcRequestBuilderTest, RequestIdsAreUnique) {
  ::sharing::mojom::LocationHintPtr location_hint =
      CountryCodeLocationHint("ZZ");
  chrome_browser_nearby_sharing_instantmessaging::SendMessageExpressRequest
      request_a = BuildSendRequest(kSelfId, kPeerId, location_hint.Clone());
  chrome_browser_nearby_sharing_instantmessaging::SendMessageExpressRequest
      request_b = BuildSendRequest(kSelfId, kPeerId, location_hint.Clone());
  EXPECT_NE(request_a.header().request_id(), request_b.header().request_id());
}
