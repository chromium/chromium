// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/media/router/discovery/dial/safe_dial_app_info_parser.h"

#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

namespace {

constexpr char kValidAppInfoXml[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
       <service xmlns="urn:dial-multiscreen-org:schemas:dial">
        <name>YouTube</name>
        <options allowStop="false"/>
        <state>running</state>
        <link rel="run" href="run"/>
       </service>
    )";

constexpr char kValidAppInfoXmlExtraData[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
       <service xmlns="urn:dial-multiscreen-org:schemas:dial">
         <name>YouTube</name>
         <state>Running</state>
         <options allowStop="false"/>
         <link rel="run" href="run"/>
         <port>8080</port>
         <capabilities>websocket</capabilities>
         <additionalData>
           <screenId>e5n3112oskr42pg0td55b38nh4</screenId>
           <otherField>2</otherField>
         </additionalData>
       </service>
    )";

constexpr char kInvalidAppInfoXmlExtraData[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
       <service xmlns="urn:dial-multiscreen-org:schemas:dial">
         <name>YouTube</name>
         <state>Running</state>
         <options allowStop="false"/>
         <link rel="run" href="run"/>
         <additionalData>
           <>empty tags</>
         </additionalData>
       </service>
    )";

constexpr char kAppInfoXmlExtraDataWithEmptyValue[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
       <service xmlns="urn:dial-multiscreen-org:schemas:dial">
         <name>YouTube</name>
         <state>Running</state>
         <options allowStop="false"/>
         <link rel="run" href="run"/>
         <additionalData>
           <emptyValue></emptyValue>
           <value>1</value>
         </additionalData>
       </service>
    )";

constexpr char kAppInfoXmlExtraDataWithNestedValue[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
       <service xmlns="urn:dial-multiscreen-org:schemas:dial">
         <name>YouTube</name>
         <state>Running</state>
         <options allowStop="false"/>
         <link rel="run" href="run"/>
         <additionalData>
           <nested>
             <child1>1</child1>
             <child2>2</child2>
           </nested>
           <value>1</value>
         </additionalData>
       </service>
    )";

constexpr char kAppInfoXmlEmptyExtraData[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
       <service xmlns="urn:dial-multiscreen-org:schemas:dial">
         <name>YouTube</name>
         <state>Running</state>
         <options allowStop="false"/>
         <link rel="run" href="run"/>
         <additionalData></additionalData>
       </service>
    )";

constexpr char kInvalidXmlNoState[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
       <service xmlns="urn:dial-multiscreen-org:schemas:dial">
         <name>YouTube</name>
         <state></state>
         <options allowStop="false"/>
         <link rel="run" href="run"/>
       </service>
    )";

constexpr char kInvalidXmlInvalidState[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
       <service xmlns="urn:dial-multiscreen-org:schemas:dial">
         <name>YouTube</name>
         <options allowStop="false"/>
         <state>xyzzy</state>
         <link rel="run" href="run"/>
       </service>
    )";

constexpr char kInvalidXmlNoName[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
       <service xmlns="urn:dial-multiscreen-org:schemas:dial">
         <options allowStop="false"/>
         <state>running</state>
         <link rel="run" href="run"/>
       </service>
    )";

constexpr char kInvalidXmlMultipleServices[] =
    R"(<?xml version="1.0" encoding="UTF-8"?>
       <root>
         <service xmlns="urn:dial-multiscreen-org:schemas:dial">
           <name>YouTube</name>
           <options allowStop="false"/>
           <state>running</state>
         </service>
         <service xmlns="urn:dial-multiscreen-org:schemas:dial">
           <name>Netflix</name>
           <options allowStop="false"/>
           <state>running</state>
         </service>
       </root>
    )";

}  // namespace

class SafeDialAppInfoParserTest : public testing::Test {
 public:
  SafeDialAppInfoParserTest() = default;

  SafeDialAppInfoParserTest(const SafeDialAppInfoParserTest&) = delete;
  SafeDialAppInfoParserTest& operator=(const SafeDialAppInfoParserTest&) =
      delete;

  std::unique_ptr<ParsedDialAppInfo> Parse(
      const std::string& xml,
      SafeDialAppInfoParser::ParsingResult expected_result) {
    base::RunLoop run_loop;
    SafeDialAppInfoParser parser;
    parser.Parse(xml,
                 base::BindOnce(&SafeDialAppInfoParserTest::OnParsingCompleted,
                                base::Unretained(this), expected_result));
    base::RunLoop().RunUntilIdle();
    return std::move(app_info_);
  }

  void OnParsingCompleted(SafeDialAppInfoParser::ParsingResult expected_result,
                          std::unique_ptr<ParsedDialAppInfo> app_info,
                          SafeDialAppInfoParser::ParsingResult result) {
    app_info_ = std::move(app_info);
    EXPECT_EQ(expected_result, result);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<ParsedDialAppInfo> app_info_;
};

TEST_F(SafeDialAppInfoParserTest, TestInvalidXmlNoService) {
  std::unique_ptr<ParsedDialAppInfo> app_info =
      Parse("", SafeDialAppInfoParser::ParsingResult::kInvalidXML);
  EXPECT_FALSE(app_info);
}

TEST_F(SafeDialAppInfoParserTest, TestValidXml) {
  std::string xml_text(kValidAppInfoXml);
  std::unique_ptr<ParsedDialAppInfo> app_info =
      Parse(xml_text, SafeDialAppInfoParser::ParsingResult::kSuccess);

  EXPECT_EQ("YouTube", app_info->name);
  EXPECT_EQ(DialAppState::kRunning, app_info->state);
  EXPECT_FALSE(app_info->allow_stop);
  EXPECT_EQ("run", app_info->href);
  EXPECT_TRUE(app_info->extra_data.empty());
}

TEST_F(SafeDialAppInfoParserTest, TestValidXmlExtraData) {
  std::string xml_text(kValidAppInfoXmlExtraData);
  std::unique_ptr<ParsedDialAppInfo> app_info =
      Parse(xml_text, SafeDialAppInfoParser::ParsingResult::kSuccess);

  EXPECT_EQ("YouTube", app_info->name);
  EXPECT_EQ(DialAppState::kRunning, app_info->state);
  EXPECT_EQ(2u, app_info->extra_data.size());
  EXPECT_EQ("e5n3112oskr42pg0td55b38nh4", app_info->extra_data["screenId"]);
  EXPECT_EQ("2", app_info->extra_data["otherField"]);
}

TEST_F(SafeDialAppInfoParserTest, TestInvalidXmlExtraData) {
  std::string xml_text(kInvalidAppInfoXmlExtraData);
  std::unique_ptr<ParsedDialAppInfo> app_info =
      Parse(xml_text, SafeDialAppInfoParser::ParsingResult::kInvalidXML);
  // Empty tag names in <additionalData> would invalidate the entire XML.
  EXPECT_EQ(nullptr, app_info);
}

TEST_F(SafeDialAppInfoParserTest, TestExtraDataWithEmptyValue) {
  std::string xml_text(kAppInfoXmlExtraDataWithEmptyValue);
  std::unique_ptr<ParsedDialAppInfo> app_info =
      Parse(xml_text, SafeDialAppInfoParser::ParsingResult::kSuccess);
  EXPECT_EQ(1u, app_info->extra_data.size());
  EXPECT_EQ("1", app_info->extra_data["value"]);
}

TEST_F(SafeDialAppInfoParserTest, TestExtraDataWithNestedValue) {
  std::string xml_text(kAppInfoXmlExtraDataWithNestedValue);
  std::unique_ptr<ParsedDialAppInfo> app_info =
      Parse(xml_text, SafeDialAppInfoParser::ParsingResult::kSuccess);
  EXPECT_EQ(1u, app_info->extra_data.size());
  EXPECT_EQ("1", app_info->extra_data["value"]);
}

TEST_F(SafeDialAppInfoParserTest, TestEmptyExtraData) {
  std::string xml_text(kAppInfoXmlEmptyExtraData);
  std::unique_ptr<ParsedDialAppInfo> app_info =
      Parse(xml_text, SafeDialAppInfoParser::ParsingResult::kSuccess);
  EXPECT_EQ(0u, app_info->extra_data.size());
}

TEST_F(SafeDialAppInfoParserTest, TestInvalidXmlNoState) {
  std::string xml_text(kInvalidXmlNoState);
  std::unique_ptr<ParsedDialAppInfo> app_info =
      Parse(xml_text, SafeDialAppInfoParser::ParsingResult::kFailToReadState);
  EXPECT_FALSE(app_info);
}

TEST_F(SafeDialAppInfoParserTest, TestInvalidXmlInvalidState) {
  std::string xml_text(kInvalidXmlInvalidState);
  std::unique_ptr<ParsedDialAppInfo> app_info =
      Parse(xml_text, SafeDialAppInfoParser::ParsingResult::kInvalidState);
  EXPECT_FALSE(app_info);
}

TEST_F(SafeDialAppInfoParserTest, TestInvalidXmlNoName) {
  std::string xml_text(kInvalidXmlNoName);
  std::unique_ptr<ParsedDialAppInfo> app_info =
      Parse(xml_text, SafeDialAppInfoParser::ParsingResult::kMissingName);
  EXPECT_FALSE(app_info);
}

TEST_F(SafeDialAppInfoParserTest, TestInvalidXmlMultipleServices) {
  std::string xml_text(kInvalidXmlMultipleServices);
  std::unique_ptr<ParsedDialAppInfo> app_info =
      Parse(xml_text, SafeDialAppInfoParser::ParsingResult::kInvalidXML);
  EXPECT_FALSE(app_info);
}

}  // namespace media_router
