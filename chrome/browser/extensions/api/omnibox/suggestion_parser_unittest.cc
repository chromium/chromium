// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/omnibox/suggestion_parser.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr api::omnibox::DescriptionStyleType kMatch =
    api::omnibox::DESCRIPTION_STYLE_TYPE_MATCH;
constexpr api::omnibox::DescriptionStyleType kDim =
    api::omnibox::DESCRIPTION_STYLE_TYPE_DIM;
constexpr api::omnibox::DescriptionStyleType kUrl =
    api::omnibox::DESCRIPTION_STYLE_TYPE_URL;

// A custom matcher for an omnibox::MatchClassification.
testing::Matcher<api::omnibox::MatchClassification> GetStyleMatcher(
    api::omnibox::DescriptionStyleType type,
    int offset,
    int length) {
  return testing::AllOf(
      ::testing::Field(&api::omnibox::MatchClassification::type,
                       ::testing::Eq(type)),
      ::testing::Field(&api::omnibox::MatchClassification::offset,
                       ::testing::Eq(offset)),
      ::testing::Field(&api::omnibox::MatchClassification::length,
                       ::testing::Pointee(::testing::Eq(length))));
}

}  // namespace

class SuggestionParserUnitTest : public testing::Test {
 public:
  SuggestionParserUnitTest() = default;
  ~SuggestionParserUnitTest() override = default;

  // A helper method to synchronously parses `str` as input and return the
  // result.
  std::unique_ptr<DescriptionAndStyles> ParseInput(base::StringPiece str) {
    std::unique_ptr<DescriptionAndStyles> result_out;
    std::string error;
    ParseImpl(str, &result_out, &error);
    return result_out;
  }

  // Returns the parsing error from attempting to parse `str`.
  std::string GetParseError(base::StringPiece str) {
    std::unique_ptr<DescriptionAndStyles> result_out;
    std::string error_out;
    ParseImpl(str, &result_out, &error_out);
    return error_out;
  }

 private:
  void ParseImpl(base::StringPiece str,
                 std::unique_ptr<DescriptionAndStyles>* result_out,
                 std::string* error_out) {
    base::RunLoop run_loop;
    auto get_result = [&run_loop, result_out, error_out](
                          std::unique_ptr<DescriptionAndStyles> result,
                          std::string error) {
      *result_out = std::move(result);
      *error_out = std::move(error);
      run_loop.Quit();
    };

    ParseDescriptionAndStyles(str, base::BindLambdaForTesting(get_result));
    run_loop.Run();

    // Exactly one of error and result should be populated.
    bool has_result = result_out->get();
    bool has_error = !error_out->empty();
    EXPECT_TRUE(has_result ^ has_error) << has_result << ", " << has_error;
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

// Tests a number of basic cases for XML suggestion parsing.
TEST_F(SuggestionParserUnitTest, BasicCases) {
  {
    std::unique_ptr<DescriptionAndStyles> result =
        ParseInput("hello <match>match</match> world");
    ASSERT_TRUE(result);
    EXPECT_EQ(u"hello match world", result->description);
    EXPECT_THAT(result->styles,
                testing::ElementsAre(GetStyleMatcher(kMatch, 6, 5)));
  }
  {
    std::unique_ptr<DescriptionAndStyles> result =
        ParseInput("<dim>hello</dim> <match>match</match> <url>world</url>");
    ASSERT_TRUE(result);
    EXPECT_EQ(u"hello match world", result->description);
    EXPECT_THAT(result->styles,
                testing::ElementsAre(GetStyleMatcher(kDim, 0, 5),
                                     GetStyleMatcher(kMatch, 6, 5),
                                     GetStyleMatcher(kUrl, 12, 5)));
  }
  {
    std::unique_ptr<DescriptionAndStyles> result =
        ParseInput("hello <dim>dim <match>dimmed match</match></dim>");
    ASSERT_TRUE(result);
    EXPECT_EQ(u"hello dim dimmed match", result->description);
    EXPECT_THAT(result->styles,
                testing::ElementsAre(GetStyleMatcher(kDim, 6, 16),
                                     GetStyleMatcher(kMatch, 10, 12)));
  }
}

// Tests cases where XML parsing is expected to fail.
TEST_F(SuggestionParserUnitTest, ParsingFails) {
  // Note: These aren't expected to be terribly robust tests, since XML parsing
  // is exercised significantly more in the XmlParser-related tests.
  EXPECT_THAT(GetParseError("<dim>no closing tag"),
              testing::HasSubstr("Opening and ending tag mismatch"));
  EXPECT_THAT(GetParseError("<dim>hello <url>foo</dim> world</url>"),
              testing::HasSubstr("Opening and ending tag mismatch"));
}

// Tests that XML strings are properly sanitized from any forbidden characters.
TEST_F(SuggestionParserUnitTest, Sanitization) {
  {
    std::unique_ptr<DescriptionAndStyles> result =
        ParseInput("  hello <match>match</match> world");
    ASSERT_TRUE(result);
    EXPECT_EQ(u"hello match world", result->description);
    EXPECT_THAT(result->styles,
                testing::ElementsAre(GetStyleMatcher(kMatch, 6, 5)));
  }
  {
    std::unique_ptr<DescriptionAndStyles> result =
        ParseInput("hell\ro <match>ma\ttch</match> wor\nld");
    ASSERT_TRUE(result);
    EXPECT_EQ(u"hello match world", result->description);
    EXPECT_THAT(result->styles,
                testing::ElementsAre(GetStyleMatcher(kMatch, 6, 5)));
  }
}

// Tests that unknown tag types and attributes are properly ignored.
TEST_F(SuggestionParserUnitTest, UnknownTagsAndAttributesAreIgnored) {
  {
    std::unique_ptr<DescriptionAndStyles> result =
        ParseInput("hello <match some-attr=\"foo\">match</match> world");
    ASSERT_TRUE(result);
    EXPECT_EQ(u"hello match world", result->description);
    EXPECT_THAT(result->styles,
                testing::ElementsAre(GetStyleMatcher(kMatch, 6, 5)));
  }
  {
    std::unique_ptr<DescriptionAndStyles> result =
        ParseInput("hello <unknown>match</unknown> world");
    ASSERT_TRUE(result);
    EXPECT_EQ(u"hello match world", result->description);
    EXPECT_THAT(result->styles, testing::IsEmpty());
  }
}

}  // namespace extensions
