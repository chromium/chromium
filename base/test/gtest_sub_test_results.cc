// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_sub_test_results.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_xml_unittest_result_printer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

void AddSubTestResult(std::string_view name,
                      testing::TimeInMillis elapsed_time,
                      std::optional<std::string_view> failure_message) {
  CHECK(!name.empty());
  CHECK(std::ranges::all_of(
      name, [](char c) { return IsAsciiAlphaNumeric(c) || c == '_'; }));
  XmlUnitTestResultPrinter::Get()->AddSubTestResult(name, elapsed_time,
                                                    std::move(failure_message));
}

}  // namespace base
