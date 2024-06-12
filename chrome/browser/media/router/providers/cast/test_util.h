// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_TEST_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_TEST_UTIL_H_

#include <iosfwd>

#include "base/test/values_test_util.h"
#include "chrome/browser/media/router/providers/cast/cast_internal_message_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

std::ostream& operator<<(std::ostream&, CastInternalMessage::Type);
std::ostream& operator<<(std::ostream&, const CastInternalMessage&);

// Matcher for CastInternalMessage arguments.
MATCHER_P(IsCastInternalMessage, json, "") {
  auto message = CastInternalMessage::From(base::test::ParseJsonDict(json));
  DCHECK(message);
  if (arg.type() != message->type() ||
      arg.client_id() != message->client_id() ||
      arg.sequence_number() != message->sequence_number()) {
    return false;
  }

  if (arg.has_session_id() && arg.session_id() != message->session_id())
    return false;

  switch (arg.type()) {
    case CastInternalMessage::Type::kAppMessage:
      return arg.app_message_namespace() == message->app_message_namespace() &&
             arg.app_message_body() == message->app_message_body();
    case CastInternalMessage::Type::kV2Message:
      return arg.v2_message_type() == message->v2_message_type() &&
             testing::Matches(
                 base::test::IsJson(arg.v2_message_body().DebugString()))(
                 message->v2_message_body());
    default:
      return true;
  }
}

// Similar to Pointee, but works for instances of mojo::StructPtr, etc.
MATCHER_P(StructPtrTo, expected, "") {
  return arg && testing::Matches(expected)(*arg);
}

// Matcher for openscreen::cast::proto::CastMessage arguments.
MATCHER_P(IsCastChannelMessage, expected, "") {
  if (arg.has_source_id() != expected.has_source_id() ||
      arg.has_destination_id() != expected.has_destination_id() ||
      arg.has_namespace_() != expected.has_namespace_() ||
      arg.has_payload_utf8() != expected.has_payload_utf8() ||
      arg.has_payload_binary() != expected.has_payload_binary() ||
      arg.has_protocol_version() != expected.has_protocol_version() ||
      arg.has_payload_type() != expected.has_payload_type() ||
      (arg.has_source_id() && arg.source_id() != expected.source_id()) ||
      (arg.has_destination_id() &&
       arg.destination_id() != expected.destination_id()) ||
      (arg.has_namespace_() && arg.namespace_() != expected.namespace_()) ||
      (arg.has_payload_utf8() &&
       arg.payload_utf8() != expected.payload_utf8()) ||
      (arg.has_payload_binary() &&
       arg.payload_binary() != expected.payload_binary()) ||
      (arg.has_protocol_version() &&
       arg.protocol_version() != expected.protocol_version()) ||
      (arg.has_payload_type() &&
       arg.payload_type() != expected.payload_type())) {
    return false;
  }
  return true;
}

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_TEST_UTIL_H_
