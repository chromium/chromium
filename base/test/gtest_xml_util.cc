// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_xml_util.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/launcher/test_launcher.h"
#include "third_party/libxml/chromium/libxml_utils.h"
#include "third_party/libxml/chromium/xml_reader.h"

namespace base {

namespace {

// No-op error handler that replaces libxml's default, which writes to stderr.
// The test launcher's worker threads speculatively parse results XML to detect
// timeouts in the processes they manage, so logging parsing errors could be
// noisy (e.g., crbug.com/1466897).
void NullXmlErrorFunc(void* context, const char* message, ...) {}

}  // namespace

struct Link {
  // The name of the test case.
  std::string name;
  // The name of the classname of the test.
  std::string classname;
  // The name of the link.
  std::string link_name;
  // The actual link.
  std::string link;
};

struct Property {
  // The name of the property.
  std::string name;
  // The value of the property.
  std::string value;
};

struct Tag {
  // The name of the test case.
  std::string name;
  // The name of the classname of the test.
  std::string classname;
  // The name of the tag.
  std::string tag_name;
  // The value of the tag.
  std::string tag_value;
};

bool ProcessGTestOutput(const base::FilePath& output_file,
                        std::vector<TestResult>* results,
                        bool* crashed) {
  DCHECK(results);

  std::string xml_contents;
  if (!ReadFileToString(output_file, &xml_contents))
    return false;

  // Silence XML errors - otherwise they go to stderr.
  ScopedXmlErrorFunc error_func(nullptr, &NullXmlErrorFunc);

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

  std::vector<Link> links;

  std::vector<Property> properties;

  std::vector<Tag> tags;

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

          std::string test_timestamp_str;
          Time test_timestamp;
          if (xml_reader.NodeAttribute("timestamp", &test_timestamp_str) &&
              Time::FromString(test_timestamp_str.c_str(), &test_timestamp)) {
            result.timestamp = test_timestamp;
          }

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
          result.elapsed_time = Microseconds(
              static_cast<int64_t>(strtod(test_time_str.c_str(), nullptr) *
                                   Time::kMicrosecondsPerSecond));

          // The timestamp attribute records the local date and time of the test
          // execution. It might be missing in the xml generated by older
          // version of test launcher or gtest.
          // https://github.com/google/googletest/blob/main/docs/advanced.md#generating-an-xml-report
          std::string test_timestamp_str;
          Time test_timestamp;
          if (xml_reader.NodeAttribute("timestamp", &test_timestamp_str) &&
              Time::FromString(test_timestamp_str.c_str(), &test_timestamp)) {
            result.timestamp = test_timestamp;
          }

          result.status = TestResult::TEST_SUCCESS;

          if (!results->empty() &&
              results->back().full_name == result.full_name &&
              results->back().status == TestResult::TEST_CRASH) {
            // Erase the fail-safe "crashed" result - now we know the test did
            // not crash.
            results->pop_back();
          }

          for (const Link& link : links) {
            if (link.name == test_name && link.classname == test_case_name) {
              result.AddLink(link.link_name, link.link);
            }
          }
          links.clear();
          for (const Property& property : properties) {
            result.AddProperty(property.name, property.value);
          }
          properties.clear();
          for (const Tag& tag : tags) {
            if (tag.name == test_name && tag.classname == test_case_name) {
              result.AddTag(tag.tag_name, tag.tag_value);
            }
          }
          tags.clear();
          results->push_back(result);
        } else if (node_name == "link" && !xml_reader.IsClosingElement()) {
          Link link;
          if (!xml_reader.NodeAttribute("name", &link.name))
            return false;
          if (!xml_reader.NodeAttribute("classname", &link.classname))
            return false;
          if (!xml_reader.NodeAttribute("link_name", &link.link_name))
            return false;
          if (!xml_reader.ReadElementContent(&link.link))
            return false;
          links.push_back(link);
        } else if (node_name == "link" && xml_reader.IsClosingElement()) {
          // Deliberately empty.
        } else if (node_name == "tag" && !xml_reader.IsClosingElement()) {
          Tag tag;
          if (!xml_reader.NodeAttribute("name", &tag.name))
            return false;
          if (!xml_reader.NodeAttribute("classname", &tag.classname))
            return false;
          if (!xml_reader.NodeAttribute("tag_name", &tag.tag_name))
            return false;
          if (!xml_reader.ReadElementContent(&tag.tag_value))
            return false;
          tags.push_back(tag);
        } else if (node_name == "tag" && xml_reader.IsClosingElement()) {
          // Deliberately empty.
        } else if (node_name == "properties" &&
                   !xml_reader.IsClosingElement()) {
          // Deliberately empty, begin of the test properties.
        } else if (node_name == "property" && !xml_reader.IsClosingElement()) {
          Property property;
          if (!xml_reader.NodeAttribute("name", &property.name))
            return false;
          if (!xml_reader.NodeAttribute("value", &property.value))
            return false;
          properties.push_back(property);
        } else if (node_name == "properties" && xml_reader.IsClosingElement()) {
          // Deliberately empty, end of the test properties.
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

  if (crashed) {
    *crashed = (state != STATE_END);
  }
  return true;
}

}  // namespace base
