// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_xml_util.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/launcher/test_launcher.h"
#include "third_party/libxml/chromium/libxml_utils.h"
#include "third_party/libxml/chromium/xml_reader.h"

namespace base {

namespace {

// This is used for the xml parser to report errors. This assumes the context
// is a pointer to a std::string where the error message should be appended.
static void XmlErrorFunc(void *context, const char *message, ...) {
  va_list args;
  va_start(args, message);
  std::string* error = static_cast<std::string*>(context);
  StringAppendV(error, message, args);
  va_end(args);
}

}  // namespace

bool ProcessGTestOutput(const base::FilePath& output_file,
                        std::vector<TestResult>* results,
                        bool* crashed) {
  DCHECK(results);

  std::string xml_contents;
  if (!ReadFileToString(output_file, &xml_contents))
    return false;

  // Silence XML errors - otherwise they go to stderr.
  std::string xml_errors;
  ScopedXmlErrorFunc error_func(&xml_errors, &XmlErrorFunc);

  XmlReader xml_reader;
  if (!xml_reader.Load(xml_contents))
    return false;

  enum {
    STATE_INIT,
    STATE_TESTSUITE,
    STATE_TESTCASE,
    STATE_TEST_RESULT,
    STATE_FAILURE,
    STATE_END,
  } state = STATE_INIT;

  while (xml_reader.Read()) {
    xml_reader.SkipToElement();
    std::string node_name(xml_reader.NodeName());

    switch (state) {
      case STATE_INIT:
        if (node_name == "testsuites" && !xml_reader.IsClosingElement())
          state = STATE_TESTSUITE;
        else
          return false;
        break;
      case STATE_TESTSUITE:
        if (node_name == "testsuites" && xml_reader.IsClosingElement())
          state = STATE_END;
        else if (node_name == "testsuite" && !xml_reader.IsClosingElement())
          state = STATE_TESTCASE;
        else
          return false;
        break;
      case STATE_TESTCASE:
        if (node_name == "testsuite" && xml_reader.IsClosingElement()) {
          state = STATE_TESTSUITE;
        } else if (node_name == "x-teststart" &&
                   !xml_reader.IsClosingElement()) {
          // This is our custom extension that helps recognize which test was
          // running when the test binary crashed.
          TestResult result;

          std::string test_case_name;
          if (!xml_reader.NodeAttribute("classname", &test_case_name))
            return false;
          std::string test_name;
          if (!xml_reader.NodeAttribute("name", &test_name))
            return false;
          result.full_name = FormatFullTestName(test_case_name, test_name);

          result.elapsed_time = TimeDelta();

          // Assume the test crashed - we can correct that later.
          result.status = TestResult::TEST_CRASH;

          results->push_back(result);
        } else if (node_name == "testcase" && !xml_reader.IsClosingElement()) {
          std::string test_status;
          if (!xml_reader.NodeAttribute("status", &test_status))
            return false;

          if (test_status != "run" && test_status != "notrun")
            return false;
          if (test_status != "run")
            break;

          TestResult result;

          std::string test_case_name;
          if (!xml_reader.NodeAttribute("classname", &test_case_name))
            return false;
          std::string test_name;
          if (!xml_reader.NodeAttribute("name", &test_name))
            return false;
          result.full_name = test_case_name + "." + test_name;

          std::string test_time_str;
          if (!xml_reader.NodeAttribute("time", &test_time_str))
            return false;
          result.elapsed_time = TimeDelta::FromMicroseconds(
              static_cast<int64_t>(strtod(test_time_str.c_str(), nullptr) *
                                   Time::kMicrosecondsPerSecond));

          result.status = TestResult::TEST_SUCCESS;

          if (!results->empty() &&
              results->back().full_name == result.full_name &&
              results->back().status == TestResult::TEST_CRASH) {
            // Erase the fail-safe "crashed" result - now we know the test did
            // not crash.
            results->pop_back();
          }

          results->push_back(result);
        } else if (node_name == "failure" && !xml_reader.IsClosingElement()) {
          std::string failure_message;
          if (!xml_reader.NodeAttribute("message", &failure_message))
            return false;

          DCHECK(!results->empty());
          results->back().status = TestResult::TEST_FAILURE;

          state = STATE_FAILURE;
        } else if (node_name == "testcase" && xml_reader.IsClosingElement()) {
          // Deliberately empty.
        } else if (node_name == "x-test-result-part" &&
                   !xml_reader.IsClosingElement()) {
          std::string result_type;
          if (!xml_reader.NodeAttribute("type", &result_type))
            return false;

          std::string file_name;
          if (!xml_reader.NodeAttribute("file", &file_name))
            return false;

          std::string line_number_str;
          if (!xml_reader.NodeAttribute("line", &line_number_str))
            return false;

          int line_number;
          if (!StringToInt(line_number_str, &line_number))
            return false;

          TestResultPart::Type type;
          if (!TestResultPart::TypeFromString(result_type, &type))
            return false;

          TestResultPart test_result_part;
          test_result_part.type = type;
          test_result_part.file_name = file_name,
          test_result_part.line_number = line_number;
          DCHECK(!results->empty());
          results->back().test_result_parts.push_back(test_result_part);

          state = STATE_TEST_RESULT;
        } else {
          return false;
        }
        break;
      case STATE_TEST_RESULT:
        if (node_name == "summary" && !xml_reader.IsClosingElement()) {
          std::string summary;
          if (!xml_reader.ReadElementContent(&summary))
            return false;

          if (!Base64Decode(summary, &summary))
            return false;

          DCHECK(!results->empty());
          DCHECK(!results->back().test_result_parts.empty());
          results->back().test_result_parts.back().summary = summary;
        } else if (node_name == "summary" && xml_reader.IsClosingElement()) {
        } else if (node_name == "message" && !xml_reader.IsClosingElement()) {
          std::string message;
          if (!xml_reader.ReadElementContent(&message))
            return false;

          if (!Base64Decode(message, &message))
            return false;

          DCHECK(!results->empty());
          DCHECK(!results->back().test_result_parts.empty());
          results->back().test_result_parts.back().message = message;
        } else if (node_name == "message" && xml_reader.IsClosingElement()) {
        } else if (node_name == "x-test-result-part" &&
                   xml_reader.IsClosingElement()) {
          state = STATE_TESTCASE;
        } else {
          return false;
        }
        break;
      case STATE_FAILURE:
        if (node_name == "failure" && xml_reader.IsClosingElement())
          state = STATE_TESTCASE;
        else
          return false;
        break;
      case STATE_END:
        // If we are here and there are still XML elements, the file has wrong
        // format.
        return false;
    }
  }

  *crashed = (state != STATE_END);
  return true;
}

}  // namespace base
