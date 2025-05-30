// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/test/gmock_expected_support.h"
#include "base/token.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestXml[] = "<hello>bonjour</hello>";
constexpr char kTestJson[] = R"(
    {"type": "element",
     "tag": "hello",
     "children": [{"type": "text", "text": "bonjour"}]
     } )";

class SafeXmlParserTest : public InProcessBrowserTest {
 public:
  SafeXmlParserTest() = default;

  SafeXmlParserTest(const SafeXmlParserTest&) = delete;
  SafeXmlParserTest& operator=(const SafeXmlParserTest&) = delete;

  ~SafeXmlParserTest() override = default;

 protected:
  // Parses |xml| and compares its parsed representation with |expected_json|.
  // If |expected_json| is empty, the XML parsing is expected to fail.
  void TestParse(std::string_view xml, const std::string& expected_json) {
    SCOPED_TRACE(xml);

    base::RunLoop run_loop;
    std::optional<base::Value> expected_value;
    if (!expected_json.empty()) {
      expected_value = base::JSONReader::Read(expected_json);
      DCHECK(expected_value) << "Bad test, incorrect JSON: " << expected_json;
    }

    data_decoder::DataDecoder::ParseXmlIsolated(
        std::string(xml),
        data_decoder::mojom::XmlParser::WhitespaceBehavior::kIgnore,
        base::BindOnce(&SafeXmlParserTest::XmlParsingDone,
                       base::Unretained(this), run_loop.QuitClosure(),
                       std::move(expected_value)));
    run_loop.Run();
  }

 private:
  void XmlParsingDone(base::OnceClosure quit_loop_closure,
                      std::optional<base::Value> expected_value,
                      data_decoder::DataDecoder::ValueOrError result) {
    base::ScopedClosureRunner runner(std::move(quit_loop_closure));
    if (expected_value) {
      ASSERT_THAT(result, base::test::ValueIs(::testing::Eq(
                              ::testing::ByRef(*expected_value))));
    } else {
      EXPECT_FALSE(result.has_value());
    }
  }
};

}  // namespace

// Tests that SafeXmlParser does parse. (actual XML parsing is tested in the
// service unit-tests).
IN_PROC_BROWSER_TEST_F(SafeXmlParserTest, Parse) {
  TestParse("[\"this is JSON not XML\"]", "");
  TestParse(kTestXml, kTestJson);
}
