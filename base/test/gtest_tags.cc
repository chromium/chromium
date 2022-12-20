// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_tags.h"

#include "base/check.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_xml_unittest_result_printer.h"

namespace base {

void AddTagToTestResult(const std::string& name, const std::string& value) {
  DCHECK(!name.empty()) << name << " is an empty name";
  XmlUnitTestResultPrinter::Get()->AddTag(name, value);
}

void AddFeatureIdTagToTestResult(const std::string& value) {
  AddTagToTestResult("feature_id", value);
}

}  // namespace base
