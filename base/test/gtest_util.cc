// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_util.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TestIdentifier::TestIdentifier() = default;

TestIdentifier::TestIdentifier(const TestIdentifier& other) = default;

TestIdentifier& TestIdentifier::operator=(const TestIdentifier& other) =
    default;

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
  for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
    const testing::TestSuite* test_suite = unit_test->GetTestSuite(i);
    for (int j = 0; j < test_suite->total_test_count(); ++j) {
      const testing::TestInfo* test_info = test_suite->GetTestInfo(j);
      TestIdentifier test_data;
      test_data.test_case_name = test_suite->name();
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

  Value::List storage;
  for (const TestIdentifier& i : tests) {
    Value::Dict test_info;
    test_info.Set("test_case_name", i.test_case_name);
    test_info.Set("test_name", i.test_name);
    test_info.Set("file", i.file);
    test_info.Set("line", i.line);
    storage.Append(std::move(test_info));
  }

  return base::test::WriteJsonFile(path, storage).has_value();
}

bool ReadTestNamesFromFile(const FilePath& path,
                           std::vector<TestIdentifier>* output) {
  JSONFileValueDeserializer deserializer(path);
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<Value> value =
      deserializer.Deserialize(&error_code, &error_message);
  if (!value.get())
    return false;

  if (!value->is_list())
    return false;

  std::vector<TestIdentifier> result;
  for (const Value& item : value->GetList()) {
    if (!item.is_dict())
      return false;

    const Value::Dict& dict = item.GetDict();
    const std::string* test_case_name = dict.FindString("test_case_name");
    if (!test_case_name || !IsStringASCII(*test_case_name))
      return false;

    const std::string* test_name = dict.FindString("test_name");
    if (!test_name || !IsStringASCII(*test_name))
      return false;

    const std::string* file = dict.FindString("file");
    if (!file || !IsStringASCII(*file))
      return false;

    std::optional<int> line = dict.FindInt("line");
    if (!line.has_value())
      return false;

    TestIdentifier test_data;
    test_data.test_case_name = *test_case_name;
    test_data.test_name = *test_name;
    test_data.file = *file;
    test_data.line = *line;
    result.push_back(test_data);
  }

  output->swap(result);
  return true;
}

}  // namespace base
