// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_links.h"

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_xml_unittest_result_printer.h"

namespace base {
namespace {

bool IsValidUrl(const std::string& url) {
  // https://www.ietf.org/rfc/rfc3986.txt
  std::set<char> valid_characters{'-', '.', '_', '~', ':', '/', '?',  '#',
                                  '[', ']', '@', '!', '$', '&', '\'', '(',
                                  ')', '*', '+', ',', ';', '%', '='};
  for (const char& c : url) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') ||
          valid_characters.find(c) != valid_characters.end()))
      return false;
  }
  return true;
}

bool IsValidName(const std::string& name) {
  for (const char& c : name) {
    if (!(IsAsciiAlpha(c) || IsAsciiDigit(c) || c == '/' || c == '_'))
      return false;
  }
  return true;
}

}  // namespace

void AddLinkToTestResult(const std::string& name, const std::string& url) {
  DCHECK(IsValidName(name)) << name << " is not a valid name";
  DCHECK(IsValidUrl(url)) << url << " is not a valid link";
  XmlUnitTestResultPrinter::Get()->AddLink(name, url);
}

}  // namespace base
