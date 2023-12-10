// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions_win.h"

#include <string>
#include <string_view>

#include "base/strings/string_number_conversions_internal.h"

namespace base {

std::wstring NumberToWString(int value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(unsigned value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(long value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(unsigned long value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(long long value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(unsigned long long value) {
  return internal::IntToStringT<std::wstring>(value);
}

std::wstring NumberToWString(double value) {
  return internal::DoubleToStringT<std::wstring>(value);
}

bool StringToInt(std::wstring_view input, int* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint(std::wstring_view input, unsigned* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToInt64(std::wstring_view input, int64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToUint64(std::wstring_view input, uint64_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToSizeT(std::wstring_view input, size_t* output) {
  return internal::StringToIntImpl(input, *output);
}

bool StringToDouble(std::wstring_view input, double* output) {
  return internal::StringToDoubleImpl(
      input, reinterpret_cast<const uint16_t*>(input.data()), *output);
}

}  // namespace base
