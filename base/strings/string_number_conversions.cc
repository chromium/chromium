// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"

#include <iterator>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions_internal.h"

namespace base {

std::string NumberToString(int value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(int value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(unsigned value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(unsigned value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(long value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(long value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(unsigned long value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(unsigned long value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(long long value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(long long value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(unsigned long long value) {
  return internal::IntToStringT<std::string>(value);
}

std::u16string NumberToString16(unsigned long long value) {
  return internal::IntToStringT<std::u16string>(value);
}

std::string NumberToString(double value) {
  return internal::DoubleToStringT<std::string>(value);
}

std::u16string NumberToString16(double value) {
  return internal::DoubleToStringT<std::u16string>(value);
}

bool StringToInt(std::string_view input, int* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToInt(std::u16string_view input, int* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint(std::string_view input, unsigned* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint(std::u16string_view input, unsigned* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToInt64(std::string_view input, int64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToInt64(std::u16string_view input, int64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint64(std::string_view input, uint64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint64(std::u16string_view input, uint64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToSizeT(std::string_view input, size_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToSizeT(std::u16string_view input, size_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToDouble(std::string_view input, double* output) {
  return internal::StringToDoubleImpl(input, input.data(), *output);
}

bool StringToDouble(std::u16string_view input, double* output) {
  return internal::StringToDoubleImpl(
      input, reinterpret_cast<const uint16_t*>(input.data()), *output);
}

std::string HexEncode(const void* bytes, size_t size) {
  return HexEncode(
      // TODO(crbug.com/40284755): The pointer-based overload of HexEncode
      // should be removed.
      UNSAFE_TODO(span(static_cast<const uint8_t*>(bytes), size)));
}

std::string HexEncode(span<const uint8_t> bytes) {
  // Each input byte creates two output hex characters.
  std::string ret;
  ret.reserve(bytes.size() * 2);

  for (uint8_t byte : bytes) {
    AppendHexEncodedByte(byte, ret);
  }
  return ret;
}

std::string HexEncode(std::string_view chars) {
  return HexEncode(base::as_byte_span(chars));
}

bool HexStringToInt(std::string_view input, int* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToUInt(std::string_view input, uint32_t* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToInt64(std::string_view input, int64_t* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToUInt64(std::string_view input, uint64_t* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToBytes(std::string_view input, std::vector<uint8_t>* output) {
  DCHECK(output->empty());
  return internal::HexStringToByteContainer<uint8_t>(
      input, std::back_inserter(*output));
}

bool HexStringToString(std::string_view input, std::string* output) {
  DCHECK(output->empty());
  return internal::HexStringToByteContainer<char>(input,
                                                  std::back_inserter(*output));
}

bool HexStringToSpan(std::string_view input, span<uint8_t> output) {
  if (input.size() / 2 != output.size())
    return false;

  return internal::HexStringToByteContainer<uint8_t>(input, output.begin());
}

}  // namespace base
