// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/stream_parser.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stdint.h>

#include <tuple>

#include "base/logging.h"
#include "third_party/protobuf/src/google/protobuf/stubs/logging.h"

// Does initialization and holds state that's shared across all runs.
class Environment {
 public:
  Environment() {
    // Disable noisy logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }

 private:
  google::protobuf::LogSilencer log_silencer_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  FuzzedDataProvider provider(data, size);

  StreamParser parser;
  while (provider.remaining_bytes() > 0)
    std::ignore = parser.Append(provider.ConsumeRandomLengthString());

  return 0;
}
