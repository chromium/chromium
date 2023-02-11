// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_xml_unittest_result_printer.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/test/test_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class XmlUnitTestResultPrinterTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kTestLauncherOutput)) {
      GTEST_SKIP() << "XmlUnitTestResultPrinterTest is not initialized "
                   << "for single process tests.";
    }
  }
};

TEST_F(XmlUnitTestResultPrinterTest, LinkInXmlFile) {
  XmlUnitTestResultPrinter::Get()->AddLink("unique_link", "http://google.com");
  std::string file_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTestLauncherOutput);
  std::string content;
  ASSERT_TRUE(
      base::ReadFileToString(FilePath::FromUTF8Unsafe(file_path), &content));
  std::string expected_content =
      base::StrCat({"<link name=\"LinkInXmlFile\" "
                    "classname=\"XmlUnitTestResultPrinterTest\" "
                    "link_name=\"unique_link\">",
                    "http://google.com", "</link>"});
  EXPECT_TRUE(content.find(expected_content) != std::string::npos)
      << expected_content << " not found in " << content;
}

TEST_F(XmlUnitTestResultPrinterTest, EscapedLinkInXmlFile) {
  XmlUnitTestResultPrinter::Get()->AddLink(
      "unique_link", "http://google.com/path?id=\"'<>&\"");
  std::string file_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTestLauncherOutput);
  std::string content;
  ASSERT_TRUE(
      base::ReadFileToString(FilePath::FromUTF8Unsafe(file_path), &content));
  std::string expected_content = base::StrCat(
      {"<link name=\"EscapedLinkInXmlFile\" "
       "classname=\"XmlUnitTestResultPrinterTest\" "
       "link_name=\"unique_link\">",
       "http://google.com/path?id=&quot;&apos;&lt;&gt;&amp;&quot;", "</link>"});
  EXPECT_TRUE(content.find(expected_content) != std::string::npos)
      << expected_content << " not found in " << content;
}

TEST_F(XmlUnitTestResultPrinterTest, TagInXmlFile) {
  XmlUnitTestResultPrinter::Get()->AddTag("tag_name", "tag_value");
  std::string file_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTestLauncherOutput);
  std::string content;
  ASSERT_TRUE(
      base::ReadFileToString(FilePath::FromUTF8Unsafe(file_path), &content));
  std::string expected_content =
      base::StrCat({"<tag name=\"TagInXmlFile\" "
                    "classname=\"XmlUnitTestResultPrinterTest\" "
                    "tag_name=\"tag_name\">",
                    "tag_value", "</tag>"});
  EXPECT_TRUE(content.find(expected_content) != std::string::npos)
      << expected_content << " not found in " << content;
}

TEST_F(XmlUnitTestResultPrinterTest, MultiTagsInXmlFile) {
  XmlUnitTestResultPrinter::Get()->AddTag("tag_name1", "tag_value1");
  XmlUnitTestResultPrinter::Get()->AddTag("tag_name2", "tag_value2");
  std::string file_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTestLauncherOutput);
  std::string content;
  ASSERT_TRUE(
      base::ReadFileToString(FilePath::FromUTF8Unsafe(file_path), &content));
  std::string expected_content_1 =
      "<tag name=\"MultiTagsInXmlFile\" "
      "classname=\"XmlUnitTestResultPrinterTest\" "
      "tag_name=\"tag_name1\">tag_value1</tag>";
  EXPECT_TRUE(content.find(expected_content_1) != std::string::npos)
      << expected_content_1 << " not found in " << content;

  std::string expected_content_2 =
      "<tag name=\"MultiTagsInXmlFile\" "
      "classname=\"XmlUnitTestResultPrinterTest\" "
      "tag_name=\"tag_name2\">tag_value2</tag>";
  EXPECT_TRUE(content.find(expected_content_2) != std::string::npos)
      << expected_content_2 << " not found in " << content;
}

TEST_F(XmlUnitTestResultPrinterTest, MultiTagsWithSameNameInXmlFile) {
  XmlUnitTestResultPrinter::Get()->AddTag("tag_name", "tag_value1");
  XmlUnitTestResultPrinter::Get()->AddTag("tag_name", "tag_value2");
  std::string file_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTestLauncherOutput);
  std::string content;
  ASSERT_TRUE(
      base::ReadFileToString(FilePath::FromUTF8Unsafe(file_path), &content));
  std::string expected_content_1 =
      "<tag name=\"MultiTagsWithSameNameInXmlFile\" "
      "classname=\"XmlUnitTestResultPrinterTest\" "
      "tag_name=\"tag_name\">tag_value1</tag>";
  EXPECT_TRUE(content.find(expected_content_1) != std::string::npos)
      << expected_content_1 << " not found in " << content;

  std::string expected_content_2 =
      "<tag name=\"MultiTagsWithSameNameInXmlFile\" "
      "classname=\"XmlUnitTestResultPrinterTest\" "
      "tag_name=\"tag_name\">tag_value2</tag>";
  EXPECT_TRUE(content.find(expected_content_2) != std::string::npos)
      << expected_content_2 << " not found in " << content;
}

class XmlUnitTestResultPrinterTimestampTest : public ::testing::Test {
 public:
  static void TearDownTestSuite() {
    // <testcase ...> should generated after test case finishes. After
    // TearDown().
    std::string file_path =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kTestLauncherOutput);
    if (file_path.empty()) {
      GTEST_SKIP() << "Test has to run with --" << switches::kTestLauncherOutput
                   << " switch.";
    }
    std::string content;
    ASSERT_TRUE(
        base::ReadFileToString(FilePath::FromUTF8Unsafe(file_path), &content));
    EXPECT_THAT(content, ::testing::ContainsRegex("<testcase.*timestamp="));
  }
};

TEST_F(XmlUnitTestResultPrinterTimestampTest, TimestampInXmlFile) {
  // <x-teststart ... /> should generated at this point
  std::string file_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTestLauncherOutput);
  if (file_path.empty()) {
    GTEST_SKIP() << "Test has to run with --" << switches::kTestLauncherOutput
                 << " switch.";
  }
  std::string content;
  ASSERT_TRUE(
      base::ReadFileToString(FilePath::FromUTF8Unsafe(file_path), &content));
  EXPECT_THAT(content, ::testing::ContainsRegex("<x-teststart.*timestamp="));
}

}  // namespace base
