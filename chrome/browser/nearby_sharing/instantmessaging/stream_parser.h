// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_STREAM_PARSER_H_
#define CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_STREAM_PARSER_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"

namespace google {
namespace protobuf {
namespace io {
class CodedInputStream;
}  // namespace io
}  // namespace protobuf
}  // namespace google

namespace net {
class GrowableIOBuffer;
}  // namespace net

namespace chrome_browser_nearby_sharing_instantmessaging {
class StreamBody;
}  // namespace chrome_browser_nearby_sharing_instantmessaging

// Parses incoming stream of data into valid proto objects.
class StreamParser {
 public:
  StreamParser();
  ~StreamParser();

  // Appends the stream data (which should be the partial or full serialized
  // StreamBody) and returns a vector of response protos that can be parsed
  // from the available stream data. If no responses can be parsed yet, an
  // empty vector is returned.
  std::vector<
      chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
  Append(std::string_view data);

 private:
  enum class StreamParsingResult {
    kSuccessfullyParsedResponse,
    kNoop,
    kNotEnoughDataYet,
    kParsingUnexpectedlyFailed
  };

  std::vector<
      chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
  ParseStreamIfAvailable();

  // Only supports reading single field type and assumes the StreamBody messages
  // field and the noop field are the only ones we will ever see.
  StreamParsingResult ParseNextMessagesFieldFromStream(
      google::protobuf::io::CodedInputStream* input_stream,
      chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse*
          parsed_response);

  scoped_refptr<net::GrowableIOBuffer> unparsed_data_buffer_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_STREAM_PARSER_H_
