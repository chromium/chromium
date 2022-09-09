// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_remoting_connector_messaging.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // All of the input is used as the text message.
  const std::string text_message(reinterpret_cast<const char*>(data), size);

  // Compute an arbitrary, but deterministic "expected session ID."
  const unsigned int session_id = size / 4;

  // Make sure the compiler does not try to optimize-away calls by storing their
  // results in a volatile variable.
  volatile unsigned int ignored_result;
  (void)ignored_result;

  using Messaging = CastRemotingConnectorMessaging;

  if (Messaging::IsMessageForSession(
          text_message, Messaging::kStartedStreamsMessageFormatPartial,
          session_id)) {
    ignored_result = Messaging::GetStreamIdFromStartedMessage(
        text_message, Messaging::kStartedStreamsMessageAudioIdSpecifier);
    ignored_result = Messaging::GetStreamIdFromStartedMessage(
        text_message, Messaging::kStartedStreamsMessageVideoIdSpecifier);
  }
  ignored_result = Messaging::IsMessageForSession(
      text_message, Messaging::kFailedMessageFormat, session_id);
  ignored_result = Messaging::IsMessageForSession(
      text_message, Messaging::kStoppedMessageFormat, session_id);

  return 0;
}
