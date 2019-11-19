// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/test_util.h"

#include "components/cast_channel/enum_table.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

std::ostream& operator<<(std::ostream& out, CastInternalMessage::Type type) {
  return out << cast_util::EnumToString(type).value_or("<invalid>");
}

std::ostream& operator<<(std::ostream& out,
                         const CastInternalMessage& message) {
  out << "{type=" << message.type() << ", client_id=" << message.client_id();
  if (message.sequence_number()) {
    out << ", sequence_number=" << *message.sequence_number();
  }
  if (message.has_session_id()) {
    out << ", session_id=" << message.session_id();
  }

  switch (message.type()) {
    case CastInternalMessage::Type::kAppMessage:
      out << ", app_message_namespace=" << message.app_message_namespace()
          << ", app_message_body=" << message.app_message_body();
      break;
    case CastInternalMessage::Type::kV2Message:
      out << ", v2_message_type=" << message.v2_message_type()
          << ", v2_message_body=" << message.v2_message_body();
      break;
    default:
      break;
  }
  return out << "}";
}

}  // namespace media_router
