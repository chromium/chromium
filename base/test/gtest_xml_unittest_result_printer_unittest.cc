// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_xml_unittest_result_printer.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/test/test_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(XmlUnitTestResultPrinterTest, LinkInXmlFile) {
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

TEST(XmlUnitTestResultPrinterTest, EscapedLinkInXmlFile) {
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

}  // namespace base
