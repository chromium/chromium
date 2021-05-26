// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_STREAM_PARSER_H_
#define CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_STREAM_PARSER_H_

#include <string>

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

// Parses incoming stream of data into valid proto objects and delegates them to
// the registered callback.
class StreamParser {
 public:
  explicit StreamParser(
      base::RepeatingCallback<void(const std::string& message)> listener,
      base::OnceClosure fastpath_ready);
  ~StreamParser();

  // Appends the stream data (which should be the partial or full serialized
  // StreamBody).
  void Append(base::StringPiece data);

 private:
  void DelegateMessage(const std::string& message);
  void ParseStreamIfAvailable();

  // Only supports reading single field type and assumes the StreamBody messages
  // field is the only one we will ever see.
  bool ParseNextMessagesFieldFromStream(
      google::protobuf::io::CodedInputStream* input_stream);

  base::RepeatingCallback<void(const std::string& message)> listener_;
  base::OnceClosure fastpath_ready_callback_;
  scoped_refptr<net::GrowableIOBuffer> unparsed_data_buffer_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_STREAM_PARSER_H_
