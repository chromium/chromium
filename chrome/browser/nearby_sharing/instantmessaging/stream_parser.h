// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_STREAM_PARSER_H_
#define CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_STREAM_PARSER_H_

#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"

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

  void Append(base::StringPiece data);

 private:
  base::Optional<chrome_browser_nearby_sharing_instantmessaging::StreamBody>
  GetNextMessage();
  void DelegateMessage(
      const chrome_browser_nearby_sharing_instantmessaging::StreamBody&
          stream_body);

  base::RepeatingCallback<void(const std::string& message)> listener_;
  base::OnceClosure fastpath_ready_callback_;
  std::string data_;
  int parsing_counter_for_metrics_ = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INSTANTMESSAGING_STREAM_PARSER_H_
