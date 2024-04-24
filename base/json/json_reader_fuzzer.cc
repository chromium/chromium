// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"

#include <optional>
#include <string_view>

#include "base/json/json_writer.h"
#include "base/values.h"

namespace base {

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 2)
    return 0;

  // Create a copy of input buffer, as otherwise we don't catch
  // overflow that touches the last byte (which is used in options).
  std::unique_ptr<char[]> input(new char[size - 1]);
  memcpy(input.get(), data, size - 1);

  std::string_view input_string(input.get(), size - 1);

  const int options = data[size - 1];

  auto json_val =
      JSONReader::ReadAndReturnValueWithError(input_string, options);
  if (json_val.has_value()) {
    // Check that the value can be serialized and deserialized back to an
    // equivalent |Value|.
    const Value& value = *json_val;
    std::string serialized;
    CHECK(JSONWriter::Write(value, &serialized));

    std::optional<Value> deserialized =
        JSONReader::Read(std::string_view(serialized));
    CHECK(deserialized);
    CHECK_EQ(value, deserialized.value());
  }

  return 0;
}

}  // namespace base
