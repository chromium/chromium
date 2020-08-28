// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/stream_parser.h"

#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"

StreamParser::StreamParser(
    base::RepeatingCallback<void(const std::string& message)> listener)
    : listener_(listener) {}
StreamParser::~StreamParser() = default;

void StreamParser::Append(base::StringPiece data) {
  size_t size = data.as_string().size();
  data_.append(data.as_string().data(), size);

  base::Optional<chrome_browser_nearby_sharing_instantmessaging::StreamBody>
      stream_body = GetNextMessage();
  while (stream_body) {
    DelegateMessage(stream_body.value());
    stream_body = GetNextMessage();
  }
}

base::Optional<chrome_browser_nearby_sharing_instantmessaging::StreamBody>
StreamParser::GetNextMessage() {
  // The incoming stream may not be a valid StreamBody proto as it might be
  // split into various OnDataReceived calls. The easy way is to append all
  // incoming data and parse byte by byte to check if it forms a valid
  // StreamBody proto.
  // Security Note - The StreamBody proto is coming from a trusted Google server
  // and hence can be parsed on the browser process.

  // TODO(crbug.com/1123172) - Add metrics to figure out which code paths are
  // more used and the time taken to parse the incoming messages.
  if (data_.empty())
    return base::nullopt;

  // There's a good chance that the entire message is a valid proto since the
  // individual messages sent by WebRTC are small, so check that first to
  // speed up parsing.
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body;
  if (stream_body.ParseFromString(data_)) {
    data_.clear();
    return stream_body;
  }

  int end_pos = 1;
  int size = data_.size();
  while (end_pos < size) {
    // TODO(crbug.com/1123169) - Optimize this function to use header
    // information to figure out the start and end of proto instead of checking
    // for every length.
    if (stream_body.ParseFromArray(data_.data(), end_pos)) {
      data_.erase(data_.begin(), data_.begin() + end_pos);
      return stream_body;
    }
    end_pos++;
  }

  return base::nullopt;
}

void StreamParser::DelegateMessage(
    const chrome_browser_nearby_sharing_instantmessaging::StreamBody&
        stream_body) {
  // Security Note - The ReceiveMessagesResponse proto is coming from a trusted
  // Google server and hence can be parsed on the browser process. The message
  // contained within the proto is untrusted and should be parsed within a
  // sandbox process.
  for (int i = 0; i < stream_body.messages_size(); i++) {
    chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
        response;
    response.ParseFromString(stream_body.messages(i));
    if (response.body_case() != chrome_browser_nearby_sharing_instantmessaging::
                                    ReceiveMessagesResponse::kInboxMessage)
      continue;
    listener_.Run(response.inbox_message().message());
  }
}
