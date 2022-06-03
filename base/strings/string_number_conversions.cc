// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"

#include <iterator>
#include <string>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions_internal.h"
#include "base/strings/string_piece.h"

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

bool StringToInt(StringPiece input, int* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToInt(StringPiece16 input, int* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint(StringPiece input, unsigned* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint(StringPiece16 input, unsigned* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToInt64(StringPiece input, int64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToInt64(StringPiece16 input, int64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint64(StringPiece input, uint64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint64(StringPiece16 input, uint64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToSizeT(StringPiece input, size_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToSizeT(StringPiece16 input, size_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToDouble(StringPiece input, double* output) {
  return internal::StringToDoubleImpl(input, input.data(), *output);
}

bool StringToDouble(StringPiece16 input, double* output) {
  return internal::StringToDoubleImpl(
      input, reinterpret_cast<const uint16_t*>(input.data()), *output);
}

std::string HexEncode(const void* bytes, size_t size) {
  static const char kHexChars[] = "0123456789ABCDEF";

  // Each input byte creates two output hex characters.
  std::string ret(size * 2, '\0');

  for (size_t i = 0; i < size; ++i) {
    char b = reinterpret_cast<const char*>(bytes)[i];
    ret[(i * 2)] = kHexChars[(b >> 4) & 0xf];
    ret[(i * 2) + 1] = kHexChars[b & 0xf];
  }
  return ret;
}

std::string HexEncode(base::span<const uint8_t> bytes) {
  return HexEncode(bytes.data(), bytes.size());
}

bool HexStringToInt(StringPiece input, int* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToUInt(StringPiece input, uint32_t* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToInt64(StringPiece input, int64_t* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToUInt64(StringPiece input, uint64_t* output) {
  return internal::HexStringToIntImpl(input, *output);
}

bool HexStringToBytes(StringPiece input, std::vector<uint8_t>* output) {
  DCHECK(output->empty());
  return internal::HexStringToByteContainer(input, std::back_inserter(*output));
}

bool HexStringToString(StringPiece input, std::string* output) {
  DCHECK(output->empty());
  return internal::HexStringToByteContainer(input, std::back_inserter(*output));
}

bool HexStringToSpan(StringPiece input, base::span<uint8_t> output) {
  if (input.size() / 2 != output.size())
    return false;

  return internal::HexStringToByteContainer(input, output.begin());
}

}  // namespace base
