// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_launcher_test_utils.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/gtest_util.h"
#include "base/test/launcher/test_result.h"

namespace base {

namespace test_launcher_utils {

namespace {

// Helper function to return |Value::Dict::FindString| by value instead of
// pointer to string, or empty string if nullptr.
std::string FindStringKeyOrEmpty(const Value::Dict& dict,
                                 const std::string& key) {
  const std::string* value = dict.FindString(key);
  return value ? *value : std::string();
}

// Find and return test case with name |test_suite_name|,
// return null if missing.
const testing::TestSuite* GetTestSuite(const std::string& test_suite_name) {
  testing::UnitTest* const unit_test = testing::UnitTest::GetInstance();
  for (int i = 0; i < unit_test->total_test_suite_count(); ++i) {
    const testing::TestSuite* test_case = unit_test->GetTestSuite(i);
    if (test_case->name() == test_suite_name) {
      return test_case;
    }
  }
  return nullptr;
}

}  // namespace

bool ValidateKeyValue(const Value::Dict& dict,
                      const std::string& key,
                      const std::string& expected_value) {
  std::string actual_value = FindStringKeyOrEmpty(dict, key);
  bool result = !actual_value.compare(expected_value);
  if (!result)
    ADD_FAILURE() << key << " expected value: " << expected_value
                  << ", actual: " << actual_value;
  return result;
}

bool ValidateKeyValue(const Value::Dict& dict,
                      const std::string& key,
                      int64_t expected_value) {
  int actual_value = dict.FindInt(key).value_or(0);
  bool result = (actual_value == expected_value);
  if (!result)
    ADD_FAILURE() << key << " expected value: " << expected_value
                  << ", actual: " << actual_value;
  return result;
}

bool ValidateTestResult(const Value::Dict& iteration_data,
                        const std::string& test_name,
                        const std::string& status,
                        size_t result_part_count,
                        bool have_running_info) {
  const Value::List* results = iteration_data.FindList(test_name);
  if (!results) {
    ADD_FAILURE() << "Cannot find result";
    return false;
  }
  if (1u != results->size()) {
    ADD_FAILURE() << "Expected one result";
    return false;
  }

  const Value::Dict* dict = (*results)[0].GetIfDict();
  if (!dict) {
    ADD_FAILURE() << "Value must be of type DICTIONARY";
    return false;
  }

  if (!ValidateKeyValue(*dict, "status", status))
    return false;

  // Verify the keys that only exists when have_running_info, if the test didn't
  // run, it wouldn't have these information.
  for (auto* key : {"process_num", "thread_id", "timestamp"}) {
    bool have_key = dict->Find(key);
    if (have_running_info && !have_key) {
      ADD_FAILURE() << "Result must contain '" << key << "' key";
      return false;
    }
    if (!have_running_info && have_key) {
      ADD_FAILURE() << "Result shouldn't contain '" << key << "' key";
      return false;
    }
  }

  const Value::List* list = dict->FindList("result_parts");
  if (!list) {
    ADD_FAILURE() << "Result must contain 'result_parts' key";
    return false;
  }

  if (result_part_count != list->size()) {
    ADD_FAILURE() << "result_parts count expected: " << result_part_count
                  << ", actual:" << list->size();
    return false;
  }
  return true;
}

bool ValidateTestLocations(const Value::Dict& test_locations,
                           const std::string& test_suite_name) {
  const testing::TestSuite* test_suite = GetTestSuite(test_suite_name);
  if (test_suite == nullptr) {
    ADD_FAILURE() << "Could not find test suite " << test_suite_name;
    return false;
  }
  bool result = true;
  for (int j = 0; j < test_suite->total_test_count(); ++j) {
    const testing::TestInfo* test_info = test_suite->GetTestInfo(j);
    std::string full_name =
        FormatFullTestName(test_suite->name(), test_info->name());
    result &= ValidateTestLocation(test_locations, full_name, test_info->file(),
                                   test_info->line());
  }
  return result;
}

bool ValidateTestLocation(const Value::Dict& test_locations,
                          const std::string& test_name,
                          const std::string& file,
                          int line) {
  const Value::Dict* dict =
      test_locations.FindDict(TestNameWithoutDisabledPrefix(test_name));
  if (!dict) {
    ADD_FAILURE() << "|test_locations| missing location for " << test_name;
    return false;
  }

  bool result = ValidateKeyValue(*dict, "file", file);
  result &= ValidateKeyValue(*dict, "line", line);
  return result;
}

std::optional<Value::Dict> ReadSummary(const FilePath& path) {
  std::optional<Value::Dict> result;
  File resultFile(path, File::FLAG_OPEN | File::FLAG_READ);
  const int size = 2e7;
  std::string json;
  CHECK(ReadFileToStringWithMaxSize(path, &json, size));
  std::optional<Value> value = JSONReader::Read(json);
  if (value && value->is_dict())
    result = std::move(*value).TakeDict();

  return result;
}

}  // namespace test_launcher_utils

}  // namespace base
