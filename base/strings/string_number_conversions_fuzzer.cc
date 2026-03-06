// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

template <class NumberType, class StringPieceType, class StringType>
void CheckRoundtripsT(base::span<const uint8_t> data,
                      StringType (*num_to_string)(NumberType),
                      bool (*string_to_num)(StringPieceType, NumberType*)) {
  // Ensure we can read a NumberType from |data|
  if (data.size() < sizeof(NumberType)) {
    return;
  }
  NumberType v1;
  base::byte_span_from_ref(v1).copy_from_nonoverlapping(
      data.first<sizeof(NumberType)>());

  // Because we started with an arbitrary NumberType value, not an arbitrary
  // string, we expect that the function |string_to_num| (e.g. StringToInt) will
  // return true, indicating a perfect conversion.
  NumberType v2;
  CHECK(string_to_num(num_to_string(v1), &v2));

  // Given that this was a perfect conversion, we expect the original NumberType
  // value to equal the newly parsed one.
  CHECK_EQ(v1, v2);
}

template <class NumberType>
void CheckRoundtrips(base::span<const uint8_t> data,
                     bool (*string_to_num)(std::string_view, NumberType*)) {
  return CheckRoundtripsT<NumberType, std::string_view, std::string>(
      data, &base::NumberToString, string_to_num);
}

template <class NumberType>
void CheckRoundtrips16(base::span<const uint8_t> data,
                       bool (*string_to_num)(std::u16string_view,
                                             NumberType*)) {
  return CheckRoundtripsT<NumberType, std::u16string_view, std::u16string>(
      data, &base::NumberToString16, string_to_num);
}

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  // For each instantiation of NumberToString f and its corresponding StringTo*
  // function g, check that f(g(x)) = x holds for fuzzer-determined values of x.
  CheckRoundtrips<int>(data, &base::StringToInt);
  CheckRoundtrips16<int>(data, &base::StringToInt);
  CheckRoundtrips<unsigned int>(data, &base::StringToUint);
  CheckRoundtrips16<unsigned int>(data, &base::StringToUint);
  CheckRoundtrips<int64_t>(data, &base::StringToInt64);
  CheckRoundtrips16<int64_t>(data, &base::StringToInt64);
  CheckRoundtrips<uint64_t>(data, &base::StringToUint64);
  CheckRoundtrips16<uint64_t>(data, &base::StringToUint64);
  CheckRoundtrips<size_t>(data, &base::StringToSizeT);
  CheckRoundtrips16<size_t>(data, &base::StringToSizeT);

  const auto string_piece_input = base::as_string_view(data);

  int out_int;
  base::StringToInt(string_piece_input, &out_int);
  unsigned out_uint;
  base::StringToUint(string_piece_input, &out_uint);
  int64_t out_int64;
  base::StringToInt64(string_piece_input, &out_int64);
  uint64_t out_uint64;
  base::StringToUint64(string_piece_input, &out_uint64);
  size_t out_size;
  base::StringToSizeT(string_piece_input, &out_size);

  // Test for std::u16string_view if size is even.
  if (data.size() % 2 == 0) {
    std::u16string_view string_piece_input16(
        reinterpret_cast<const char16_t*>(data.data()), data.size() / 2);

    base::StringToInt(string_piece_input16, &out_int);
    base::StringToUint(string_piece_input16, &out_uint);
    base::StringToInt64(string_piece_input16, &out_int64);
    base::StringToUint64(string_piece_input16, &out_uint64);
    base::StringToSizeT(string_piece_input16, &out_size);
  }

  double out_double;
  base::StringToDouble(string_piece_input, &out_double);

  base::HexStringToInt(string_piece_input, &out_int);
  base::HexStringToUInt(string_piece_input, &out_uint);
  base::HexStringToInt64(string_piece_input, &out_int64);
  base::HexStringToUInt64(string_piece_input, &out_uint64);
  std::vector<uint8_t> out_bytes;
  base::HexStringToBytes(string_piece_input, &out_bytes);

  base::HexEncode(data);
  base::HexEncode(string_piece_input);

  // Convert the numbers back to strings.
  base::NumberToString(out_int);
  base::NumberToString16(out_int);
  base::NumberToString(out_uint);
  base::NumberToString16(out_uint);
  base::NumberToString(out_int64);
  base::NumberToString16(out_int64);
  base::NumberToString(out_uint64);
  base::NumberToString16(out_uint64);
  base::NumberToString(out_double);
  base::NumberToString16(out_double);

  return 0;
}
