// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/stream_parser.h"

#include "base/strings/string_piece.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "net/base/io_buffer.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/wire_format_lite.h"

namespace {

using ::google::protobuf::internal::WireFormatLite;

// A buffer spare capacity limits the amount of times we need to resize the
// buffer when copying over data, which involves reallocating memory.
// We chose 512 is because it is one of the larger standard sizes for
// a buffer, and we expect a lot of data to be received in the WebRTC
// signaling process.
constexpr int kReadBufferSpareCapacity = 512;
constexpr int kMinimumBytesToParseNextMessagesField = 6;

}  // namespace

StreamParser::StreamParser(
    base::RepeatingCallback<void(const std::string& message)> listener,
    base::OnceClosure fastpath_ready_callback)
    : listener_(listener),
      fastpath_ready_callback_(std::move(fastpath_ready_callback)) {}
StreamParser::~StreamParser() = default;

void StreamParser::Append(base::StringPiece data) {
  if (!unparsed_data_buffer_) {
    unparsed_data_buffer_ = base::MakeRefCounted<net::GrowableIOBuffer>();
    unparsed_data_buffer_->SetCapacity(data.size() + kReadBufferSpareCapacity);
  } else if (unparsed_data_buffer_->RemainingCapacity() < data.size()) {
    unparsed_data_buffer_->SetCapacity(unparsed_data_buffer_->offset() +
                                       data.size() + kReadBufferSpareCapacity);
  }

  DCHECK_GE(unparsed_data_buffer_->RemainingCapacity(),
            static_cast<int>(data.size()));
  memcpy(unparsed_data_buffer_->data(), data.data(), data.size());
  unparsed_data_buffer_->set_offset(unparsed_data_buffer_->offset() +
                                    data.size());
  ParseStreamIfAvailable();
}

void StreamParser::ParseStreamIfAvailable() {
  DCHECK(unparsed_data_buffer_);
  int unparsed_bytes_available = unparsed_data_buffer_->offset();

  // If we don't at least 8 bytes, don't bother trying to parse yet and wait
  // for more data.
  if (unparsed_bytes_available < kMinimumBytesToParseNextMessagesField)
    return;

  google::protobuf::io::CodedInputStream input_stream(
      reinterpret_cast<const uint8_t*>(unparsed_data_buffer_->StartOfBuffer()),
      unparsed_bytes_available);
  int bytes_consumed = 0;

  // We can't use StreamBody::ParseFromString() here, as it can't do partial
  // parsing, nor can it tell how many bytes are consumed.
  while (bytes_consumed < unparsed_bytes_available) {
    bool is_successful = ParseNextMessagesFieldFromStream(&input_stream);
    if (is_successful) {
      // Only update |bytes_consumed| if the whole field is decoded.
      bytes_consumed = input_stream.CurrentPosition();
    } else {
      // The stream data can't be fully decoded yet.
      break;
    }
  }

  if (bytes_consumed == 0) {
    return;
  }

  CHECK_LE(bytes_consumed, unparsed_bytes_available);
  int bytes_not_consumed = unparsed_bytes_available - bytes_consumed;

  // Shift the unread data back to the beginning of the buffer for the next
  // iteration of reading data.
  memmove(unparsed_data_buffer_->StartOfBuffer(),
          unparsed_data_buffer_->StartOfBuffer() + bytes_consumed,
          bytes_not_consumed);
  unparsed_data_buffer_->set_offset(bytes_not_consumed);
}

void StreamParser::DelegateMessage(const std::string& messages) {
  // Security Note - The ReceiveMessagesResponse proto is coming from a trusted
  // Google server and hence can be parsed on the browser process. The message
  // contained within the proto is untrusted and should be parsed within a
  // sandbox process.
  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
      response;
  if (!response.ParseFromString(messages)) {
    NS_LOG(ERROR) << "Cannot read ReceiveMessagesResponse from string.";
    return;
  }

  switch (response.body_case()) {
    case chrome_browser_nearby_sharing_instantmessaging::
        ReceiveMessagesResponse::kFastPathReady:
      if (fastpath_ready_callback_) {
        std::move(fastpath_ready_callback_).Run();
      }
      break;
    case chrome_browser_nearby_sharing_instantmessaging::
        ReceiveMessagesResponse::kInboxMessage:
      listener_.Run(response.inbox_message().message());
      break;
    default:
      NS_LOG(ERROR) << __func__ << ": message body case was unexpected: "
                    << response.body_case();
      NOTREACHED();
  }
}

bool StreamParser::ParseNextMessagesFieldFromStream(
    google::protobuf::io::CodedInputStream* input_stream) {
  // The WireFormat nature of protos allows for key:value pairs, each which
  // contains the value of one proto field. The key (also called tag) for each
  // pair is actually two values: the field number and the wire type.
  //
  // A typical stream looks like:
  //      [message tag][field data][message tag][field data]...
  // where the message tag consists of the field id and the WireType, like so:
  //      [field id + WireType][field data][field id + WireType][field data]...
  //
  // In this case, we only considering the specific field (1 -- "messages") of
  // the StreamBody, and because we are expecting the 'bytes' data type, the
  // wire type says it is a length delimited value. From this, we know that the
  // next bytes on should be a length followed by the actual data bytes (which
  // will be read by WireFormatLite::ReadBytes). Note: this is only true when
  // the wire type is set to  WIRETYPE_LENGTH_DELIMITED and we know the field
  // type is bytes.
  //
  // Therefore, for this specific instance, we expect our stream to look like
  // this:
  //     [field id="messages"|WIRETYPE_LENGTH_DELIMITED][bytes size][byte data]
  // See https://developers.google.com/protocol-buffers/docs/encoding for
  // further explanation.

  // A message tag of zero means we don't have a valid tag or we don't have
  // enough bytes to read a tag. If we cannot read the tag, we likely need to
  // wait for more bytes to be appended to the input stream.
  uint32_t messages_tag = input_stream->ReadTag();
  if (messages_tag == 0)
    return false;

  // If we were able to read the full tag above, and the field id does not
  // match the StreamBody messages field body we were expecting, then we need
  // more data to continue.
  if (WireFormatLite::GetTagFieldNumber(messages_tag) !=
      chrome_browser_nearby_sharing_instantmessaging::StreamBody::
          kMessagesFieldNumber) {
    return false;
  }

  // WireType specifies the format of the data to follow. Here, we are verifying
  // the data we are receiving matching the data we are expecting, which is in
  // the form of WIRETYPE_LENGTH_DELIMITED. We expect this to be
  // WIRETYPE_LENGTH_DELIMITED because the proto defines "messages" field as
  // the "bytes" type.
  CHECK_EQ(WireFormatLite::WireType::WIRETYPE_LENGTH_DELIMITED,
           WireFormatLite::GetTagWireType(messages_tag));

  // Read the byte messages field, not including tags. If it is not successful,
  // we likely need to wait for more bytes to be appended to the input stream to
  // form a complete message. This function makes the assumption that we
  // already know the field and read the tag to determine what field to read,
  // which is why we need the checks above.
  std::string messages_field_bytes;
  if (!WireFormatLite::ReadBytes(input_stream, &messages_field_bytes))
    return false;

  // Now that we have a complete "StreamBody.messages" bytes field, we want to
  // properly handle it. DelegateMessage allows us to transform the bytes
  // message we received to a ReceiveMessagesResponse, and run the appropriate
  // callback depending on the response body. Then we can move along and read
  // the next data from the buffer, if applicable.
  DelegateMessage(messages_field_bytes);
  return true;
}
