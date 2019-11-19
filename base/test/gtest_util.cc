// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_util.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TestIdentifier::TestIdentifier() = default;

TestIdentifier::TestIdentifier(const TestIdentifier& other) = default;

std::string FormatFullTestName(const std::string& test_case_name,
                               const std::string& test_name) {
  return test_case_name + "." + test_name;
}

std::string TestNameWithoutDisabledPrefix(const std::string& full_test_name) {
  std::string test_name_no_disabled(full_test_name);
  ReplaceSubstringsAfterOffset(&test_name_no_disabled, 0, "DISABLED_", "");
  return test_name_no_disabled;
}

std::vector<TestIdentifier> GetCompiledInTests() {
  testing::UnitTest* const unit_test = testing::UnitTest::GetInstance();

  std::vector<TestIdentifier> tests;
  for (int i = 0; i < unit_test->total_test_case_count(); ++i) {
    const testing::TestCase* test_case = unit_test->GetTestCase(i);
    for (int j = 0; j < test_case->total_test_count(); ++j) {
      const testing::TestInfo* test_info = test_case->GetTestInfo(j);
      TestIdentifier test_data;
      test_data.test_case_name = test_case->name();
      test_data.test_name = test_info->name();
      test_data.file = test_info->file();
      test_data.line = test_info->line();
      tests.push_back(test_data);
    }
  }
  return tests;
}

bool WriteCompiledInTestsToFile(const FilePath& path) {
  std::vector<TestIdentifier> tests(GetCompiledInTests());

  ListValue root;
  for (const auto& i : tests) {
    std::unique_ptr<DictionaryValue> test_info(new DictionaryValue);
    test_info->SetStringKey("test_case_name", i.test_case_name);
    test_info->SetStringKey("test_name", i.test_name);
    test_info->SetStringKey("file", i.file);
    test_info->SetIntKey("line", i.line);
    root.Append(std::move(test_info));
  }

  JSONFileValueSerializer serializer(path);
  return serializer.Serialize(root);
}

bool ReadTestNamesFromFile(const FilePath& path,
                           std::vector<TestIdentifier>* output) {
  JSONFileValueDeserializer deserializer(path);
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_message);
  if (!value.get())
    return false;

  base::ListValue* tests = nullptr;
  if (!value->GetAsList(&tests))
    return false;

  std::vector<base::TestIdentifier> result;
  for (const auto& i : *tests) {
    const base::DictionaryValue* test = nullptr;
    if (!i.GetAsDictionary(&test))
      return false;

    TestIdentifier test_data;

    if (!test->GetStringASCII("test_case_name", &test_data.test_case_name))
      return false;

    if (!test->GetStringASCII("test_name", &test_data.test_name))
      return false;

    if (!test->GetStringASCII("file", &test_data.file))
      return false;

    if (!test->GetInteger("line", &test_data.line))
      return false;

    result.push_back(test_data);
  }

  output->swap(result);
  return true;
}

}  // namespace base
