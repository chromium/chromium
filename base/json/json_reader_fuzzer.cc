// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"

#include <optional>
#include <string_view>

#include "base/containers/heap_array.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace base {

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 2)
    return 0;

  // SAFETY: LibFuzzer provides a valid data/size pair.
  auto data_span = UNSAFE_BUFFERS(base::span(data, size));

  // Create a copy of input buffer, as otherwise we don't catch
  // overflow that touches the last byte (which is used in options).
  auto input = base::HeapArray<unsigned char>::Uninit(size - 1);
  input.copy_from(data_span.first(size - 1u));

  std::string_view input_string = base::as_string_view(input);

  const int options = data_span.back();

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
