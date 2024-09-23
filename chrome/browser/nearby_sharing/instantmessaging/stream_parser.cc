// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/stream_parser.h"

#include <string_view>

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/wire_format_lite.h"

namespace {

using ::google::protobuf::internal::WireFormatLite;

// A buffer spare capacity limits the amount of times we need to resize the
// buffer when copying over data, which involves reallocating memory.
// We chose 512 because it is one of the larger standard sizes for
// a buffer, and we expect a lot of data to be received in the WebRTC
// signaling process.
constexpr int kReadBufferSpareCapacity = 512;

// The minimum number of bytes to parse the messages or the noop field of
// the StreamBody proto is 2 because the size of the tag and wire type is a
// single byte, and the smallest size information would be contained in another
// single byte.
constexpr int kMinimumBytesToParseNextMessagesField = 2;

}  // namespace

StreamParser::StreamParser() = default;
StreamParser::~StreamParser() = default;

std::vector<
    chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
StreamParser::Append(std::string_view data) {
  if (!unparsed_data_buffer_) {
    unparsed_data_buffer_ = base::MakeRefCounted<net::GrowableIOBuffer>();
    unparsed_data_buffer_->SetCapacity(data.size() + kReadBufferSpareCapacity);
  } else if (unparsed_data_buffer_->RemainingCapacity() <
             static_cast<int>(data.size())) {
    unparsed_data_buffer_->SetCapacity(unparsed_data_buffer_->offset() +
                                       data.size() + kReadBufferSpareCapacity);
  }

  DCHECK_GE(unparsed_data_buffer_->RemainingCapacity(),
            static_cast<int>(data.size()));
  memcpy(unparsed_data_buffer_->data(), data.data(), data.size());
  unparsed_data_buffer_->set_offset(unparsed_data_buffer_->offset() +
                                    data.size());
  return ParseStreamIfAvailable();
}

std::vector<
    chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
StreamParser::ParseStreamIfAvailable() {
  DCHECK(unparsed_data_buffer_);
  std::vector<
      chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
      receive_messages_responses;

  base::span<uint8_t> unparsed_bytes_available =
      unparsed_data_buffer_->span_before_offset();
  if (unparsed_bytes_available.size() < kMinimumBytesToParseNextMessagesField) {
    return receive_messages_responses;
  }

  google::protobuf::io::CodedInputStream input_stream(
      unparsed_bytes_available.data(), unparsed_bytes_available.size());
  int bytes_consumed = 0;

  // We can't use StreamBody::ParseFromString() here, as it can't do partial
  // parsing, nor can it tell how many bytes are consumed.
  bool continue_parsing = unparsed_bytes_available.size() > 0;
  while (continue_parsing) {
    chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
        parsed_response;
    StreamParsingResult result =
        ParseNextMessagesFieldFromStream(&input_stream, &parsed_response);
    switch (result) {
      case StreamParser::StreamParsingResult::kSuccessfullyParsedResponse:
        receive_messages_responses.push_back(parsed_response);
        [[fallthrough]];
      case StreamParser::StreamParsingResult::kNoop:
        bytes_consumed = input_stream.CurrentPosition();
        continue_parsing = base::checked_cast<size_t>(bytes_consumed) <
                           unparsed_bytes_available.size();
        break;
      case StreamParser::StreamParsingResult::kNotEnoughDataYet:
      case StreamParser::StreamParsingResult::kParsingUnexpectedlyFailed:
        continue_parsing = false;
        break;
    }
  }

  if (bytes_consumed == 0)
    return receive_messages_responses;

  // Shift the unread data back to the beginning of the buffer for the next
  // iteration of reading data.
  base::span<uint8_t> bytes_not_consumed =
      unparsed_bytes_available.subspan(bytes_consumed);
  unparsed_bytes_available.copy_prefix_from(bytes_not_consumed);
  unparsed_data_buffer_->set_offset(bytes_not_consumed.size());

  return receive_messages_responses;
}

StreamParser::StreamParsingResult
StreamParser::ParseNextMessagesFieldFromStream(
    google::protobuf::io::CodedInputStream* input_stream,
    chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse*
        parsed_response) {
  // The WireFormat nature of protos allows for key:value pairs, each which
  // contains the value of one proto field. The key (also called tag) for each
  // pair is actually two values: the field number and the wire type.
  //
  // A typical stream looks like:
  //      [message tag][field data][message tag][field data]...
  // where the message tag consists of the field id and the WireType, like so:
  //      [field id + WireType][field data][field id + WireType][field data]...
  //
  // In this case, we are only looking at the two fields of the StreamBody:
  // "messages", which is field 1, and "noop", which is field 15. The "messages"
  // field is the one containing the ReceiveMessageResponse proto, and the
  // "noop" field is sent by the Tachyon server to keep the connection alive.
  // Both of these fields we expect to be the 'bytes' data type, which the
  // wire type says it is a length delimited value. From this, we know that the
  // next bytes on should be a length followed by the actual data bytes (which
  // will be read by WireFormatLite::ReadBytes). Note: this is only true when
  // the wire type is set to  WIRETYPE_LENGTH_DELIMITED and we know the field
  // type is bytes.
  //
  // Therefore, for this specific instance, we expect our stream to look like
  // this when it contains the ReceiveMessagesResponse with an InboxMessage or
  // a FastPathReady message:
  //     [field id="messages"|WIRETYPE_LENGTH_DELIMITED][bytes size][byte data]
  // or it will look like this when we receive a noop message:
  //     [field id="noop"|WIRETYPE_LENGTH_DELIMITED][bytes size][byte data]
  // See https://developers.google.com/protocol-buffers/docs/encoding for
  // further explanation.

  // A message tag of zero means we don't have a valid tag or we don't have
  // enough bytes to read a tag. If we cannot read the tag, we likely need to
  // wait for more bytes to be appended to the input stream.
  uint32_t messages_tag = input_stream->ReadTag();
  if (messages_tag == 0)
    return StreamParser::StreamParsingResult::kNotEnoughDataYet;

  // If we were able to read the full tag above, and the field id does not
  // match the StreamBody messages field body or the noop field body we were
  // expecting then we are encountering a field we are not prepared to handle.
  // TODO(crbug.com/1217150) Add a way to read through bytes of the unknown
  // fields to skip it, in order to be more robost to StreamBody changes.
  int field_number = WireFormatLite::GetTagFieldNumber(messages_tag);
  if (field_number != chrome_browser_nearby_sharing_instantmessaging::
                          StreamBody::kMessagesFieldNumber &&
      field_number != chrome_browser_nearby_sharing_instantmessaging::
                          StreamBody::kNoopFieldNumber) {
    return StreamParser::StreamParsingResult::kParsingUnexpectedlyFailed;
  }

  // WireType specifies the format of the data to follow. Here, we are verifying
  // the data we are receiving matching the data we are expecting, which is in
  // the form of WIRETYPE_LENGTH_DELIMITED. We expect this to be
  // WIRETYPE_LENGTH_DELIMITED because the proto defines "messages" and "noop"
  // field as the "bytes" type.
  if (WireFormatLite::GetTagWireType(messages_tag) !=
      WireFormatLite::WireType::WIRETYPE_LENGTH_DELIMITED) {
    return StreamParser::StreamParsingResult::kParsingUnexpectedlyFailed;
  }

  // Read the byte field, not including tags of the StreamBody, which will
  // either be "StreamBody.messages" or "StreamBody.noop". If it is not
  // successful, we likely need to wait for more bytes to be appended to the
  // input stream to form a complete StreamBody. This function makes the
  // assumption that we already know the field and read the tag to determine
  // what field to read, which is why we need the checks above.
  std::string stream_body_field_bytes;
  if (!WireFormatLite::ReadBytes(input_stream, &stream_body_field_bytes))
    return StreamParser::StreamParsingResult::kNotEnoughDataYet;

  // Now that we have a complete "StreamBody.messages" or "StreamBody.noop"
  // bytes field, we want to properly handle it. If we have a
  // "StreamBody.messages", we want to transform the bytes into a
  // ReceiveMessagesResponse and append it to the vector we are returning, then
  // we can move along and read the next data from the buffer, if applicable.
  // "StreamBody.noop" messages may be generated as a way to keep the connection
  // to the server alive, and it is not an error. However, these messages do not
  // contain a ReceiveMessagesResponse, but we still want to remove this data
  // from the buffer and continue reading the next data, if applicable. We
  // update the |is_noop_field_| to true to tell ParseStreamIfAvailable that
  // although it receives an std::nullopt, it should still remove the bytes
  // from the buffer.
  if (field_number == chrome_browser_nearby_sharing_instantmessaging::
                          StreamBody::kNoopFieldNumber) {
    return StreamParser::StreamParsingResult::kNoop;
  }

  if (!parsed_response->ParseFromString(stream_body_field_bytes)) {
    LOG(ERROR) << "Failed to parse ReceiveMessagesResponse from stream body "
                  "message bytes.";
    return StreamParser::StreamParsingResult::kParsingUnexpectedlyFailed;
  }

  return StreamParser::StreamParsingResult::kSuccessfullyParsedResponse;
}
