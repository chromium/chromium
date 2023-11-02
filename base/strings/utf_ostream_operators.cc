// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_ostream_operators.h"

#include "base/strings/utf_string_conversions.h"
#include "base/types/supports_ostream_operator.h"

std::ostream& std::operator<<(std::ostream& out, const wchar_t* wstr) {
  return out << (wstr ? std::wstring_view(wstr) : std::wstring_view());
}

std::ostream& std::operator<<(std::ostream& out, std::wstring_view wstr) {
  return out << base::WideToUTF8(wstr);
}

std::ostream& std::operator<<(std::ostream& out, const std::wstring& wstr) {
  return out << std::wstring_view(wstr);
}

std::ostream& std::operator<<(std::ostream& out, const char16_t* str16) {
  return out << (str16 ? std::u16string_view(str16) : std::u16string_view());
}

std::ostream& std::operator<<(std::ostream& out, std::u16string_view str16) {
  return out << base::UTF16ToUTF8(str16);
}

std::ostream& std::operator<<(std::ostream& out, const std::u16string& str16) {
  return out << std::u16string_view(str16);
}
