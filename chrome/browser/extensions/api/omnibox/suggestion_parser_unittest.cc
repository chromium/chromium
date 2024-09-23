// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/omnibox/suggestion_parser.h"

#include <memory>
#include <string_view>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr api::omnibox::DescriptionStyleType kMatch =
    api::omnibox::DescriptionStyleType::kMatch;
constexpr api::omnibox::DescriptionStyleType kDim =
    api::omnibox::DescriptionStyleType::kDim;
constexpr api::omnibox::DescriptionStyleType kUrl =
    api::omnibox::DescriptionStyleType::kUrl;

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
                       ::testing::Eq(length)));
}

}  // namespace

class SuggestionParserUnitTest : public testing::Test {
 public:
  SuggestionParserUnitTest() = default;
  ~SuggestionParserUnitTest() override = default;

  // A helper method to synchronously parses `str` as input and return the
  // result.
  DescriptionAndStyles ParseSingleInput(std::string_view str) {
    DescriptionAndStylesResult result;
    ParseImpl({str}, &result);
    if (result.descriptions_and_styles.size() != 1) {
      ADD_FAILURE() << "Failed to parse single input. Resulting size: "
                    << result.descriptions_and_styles.size();
      return DescriptionAndStyles();
    }

    return std::move(result.descriptions_and_styles[0]);
  }
  // Same as above, accepting multiple string inputs.
  std::vector<DescriptionAndStyles> ParseInputs(
      const std::vector<std::string_view>& strs) {
    DescriptionAndStylesResult result;
    ParseImpl(strs, &result);
    EXPECT_EQ(std::string(), result.error);
    return std::move(result.descriptions_and_styles);
  }

  // Returns the parsing error from attempting to parse `strs`.
  std::string GetParseError(const std::vector<std::string_view>& strs) {
    DescriptionAndStylesResult result;
    ParseImpl(strs, &result);
    return result.error;
  }

 private:
  void ParseImpl(const std::vector<std::string_view>& strs,
                 DescriptionAndStylesResult* result_out) {
    base::test::TestFuture<DescriptionAndStylesResult> parse_future;
    if (strs.size() == 1) {
      ParseDescriptionAndStyles(strs[0], parse_future.GetCallback());
    } else {
      ParseDescriptionsAndStyles(strs, parse_future.GetCallback());
    }

    *result_out = parse_future.Take();
    // Exactly one of error and result should be populated.
    bool has_parsed_entries = !result_out->descriptions_and_styles.empty();
    bool has_error = !result_out->error.empty();
    EXPECT_TRUE(has_parsed_entries ^ has_error)
        << has_parsed_entries << ", " << has_error;
  }

  content::BrowserTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

// Tests a number of basic cases for XML suggestion parsing.
TEST_F(SuggestionParserUnitTest, BasicCases) {
  {
    DescriptionAndStyles result =
        ParseSingleInput("hello <match>match</match> world");
    EXPECT_EQ(u"hello match world", result.description);
    EXPECT_THAT(result.styles,
                testing::ElementsAre(GetStyleMatcher(kMatch, 6, 5)));
  }
  {
    DescriptionAndStyles result = ParseSingleInput(
        "<dim>hello</dim> <match>match</match> <url>world</url>");
    EXPECT_EQ(u"hello match world", result.description);
    EXPECT_THAT(result.styles,
                testing::ElementsAre(GetStyleMatcher(kDim, 0, 5),
                                     GetStyleMatcher(kMatch, 6, 5),
                                     GetStyleMatcher(kUrl, 12, 5)));
  }
  {
    DescriptionAndStyles result =
        ParseSingleInput("hello <dim>dim <match>dimmed match</match></dim>");
    EXPECT_EQ(u"hello dim dimmed match", result.description);
    EXPECT_THAT(result.styles,
                testing::ElementsAre(GetStyleMatcher(kDim, 6, 16),
                                     GetStyleMatcher(kMatch, 10, 12)));
  }
}

// Tests parsing multiple entries passed to the suggestion parsing.
TEST_F(SuggestionParserUnitTest, MultipleEntries) {
  {
    std::vector<DescriptionAndStyles> result = ParseInputs(
        {"first <match>match</match> entry", "second <url>url</url> entry",
         "final <dim>dim</dim> entry"});
    ASSERT_EQ(3u, result.size());
    EXPECT_EQ(u"first match entry", result[0].description);
    EXPECT_THAT(result[0].styles,
                testing::ElementsAre(GetStyleMatcher(kMatch, 6, 5)));
    EXPECT_EQ(u"second url entry", result[1].description);
    EXPECT_THAT(result[1].styles,
                testing::ElementsAre(GetStyleMatcher(kUrl, 7, 3)));
    EXPECT_EQ(u"final dim entry", result[2].description);
    EXPECT_THAT(result[2].styles,
                testing::ElementsAre(GetStyleMatcher(kDim, 6, 3)));
  }
  {
    // A fun "hack" that extensions can pull: When parsing multiple suggestions,
    // we join them together with each as an element with the
    // "internal-suggestion" tag. This means that, if an extension wanted to,
    // it could inject inner </internal-suggestion> tags to synthesize extra
    // suggestions. This isn't a security risk at all - it can't do anything
    // besides get extra suggestions, and we don't limit the number of
    // suggestions extensions can provide. There's no reason for extensions to
    // do this, but we add a test as documentation of this "quirk".
    constexpr char kJointSuggestion[] =
        "first <match>match</match></internal-suggestion><internal-suggestion>"
        "second <url>url</url>";
    std::vector<DescriptionAndStyles> result =
        ParseInputs({kJointSuggestion, "final <dim>dim</dim>"});
    ASSERT_EQ(3u, result.size());
    EXPECT_EQ(u"first match", result[0].description);
    EXPECT_THAT(result[0].styles,
                testing::ElementsAre(GetStyleMatcher(kMatch, 6, 5)));
    EXPECT_EQ(u"second url", result[1].description);
    EXPECT_THAT(result[1].styles,
                testing::ElementsAre(GetStyleMatcher(kUrl, 7, 3)));
    EXPECT_EQ(u"final dim", result[2].description);
    EXPECT_THAT(result[2].styles,
                testing::ElementsAre(GetStyleMatcher(kDim, 6, 3)));
  }
}

// Tests cases where XML parsing is expected to fail.
TEST_F(SuggestionParserUnitTest, ParsingFails) {
  // Note: These aren't expected to be terribly robust tests, since XML parsing
  // is exercised significantly more in the XmlParser-related tests.
  EXPECT_THAT(GetParseError({"<dim>no closing tag"}),
              testing::HasSubstr("Opening and ending tag mismatch"));
  EXPECT_THAT(GetParseError({"<dim>hello <url>foo</dim> world</url>"}),
              testing::HasSubstr("Opening and ending tag mismatch"));
  // Test an error in one of three inputs.
  EXPECT_THAT(GetParseError({"first <match>match</match> entry",
                             "second <url>url<url> entry",
                             "final <dim>dim</dim> entry"}),
              testing::HasSubstr("Opening and ending tag mismatch"));

  // Test "injection" attacks. Because we synthesize XML documents for and don't
  // do any escaping for the element tags we use ("fragment" and
  // "internal-suggestion"), extensions can prematurely end our tags. This is
  // safe; it just results in invalid XML.
  EXPECT_THAT(GetParseError({"first </fragment>DROP TABLE supersecret"}),
              testing::HasSubstr("Extra content at the end of the document"));
  EXPECT_THAT(
      GetParseError(
          {"first "
           "</internal-suggestion></fragment>fetch('https://example.com');",
           "second entry"}),
      testing::HasSubstr("Extra content at the end of the document"));

  // Test a suggestion that would add a second "fragment" element to the
  // parsed XML. The XML that ends up being parsed is:
  // <fragment>first suggestion</fragment>
  // <fragment>second</fragment>
  EXPECT_THAT(
      GetParseError({"first suggestion</fragment><fragment>second</fragment>"}),
      testing::HasSubstr("Extra content at the end of the document"));

  // Test an injection that inserts unexpected children in our synthesized XML
  // document. The XML that ends up being parsed is:
  // <fragment>
  //   <internal-suggestion>first</internal-suggestion>
  //   <other-class>Foobar</other-class>   <-- This was snuck in.
  //   <internal-suggestion>second></internal-suggestion>
  //   <internal-suggestion>final</internal-suggestion>
  // </fragment>
  // This is actually valid XML, and we reject it with our generic error in the
  // handling of the parsed value.
  {
    constexpr char kSneakyXML[] =
        "first</internal-suggestion><other-class>Foobar</other-class>"
        "<internal-suggestion>second";
    EXPECT_EQ("Invalid XML", GetParseError({kSneakyXML, "final suggestion"}));
  }
}

// Tests that XML strings are properly sanitized from any forbidden characters.
TEST_F(SuggestionParserUnitTest, Sanitization) {
  {
    DescriptionAndStyles result =
        ParseSingleInput("  hello <match>match</match> world");
    EXPECT_EQ(u"hello match world", result.description);
    EXPECT_THAT(result.styles,
                testing::ElementsAre(GetStyleMatcher(kMatch, 6, 5)));
  }
  {
    DescriptionAndStyles result =
        ParseSingleInput("hell\ro <match>ma\ttch</match> wor\nld");
    EXPECT_EQ(u"hello match world", result.description);
    EXPECT_THAT(result.styles,
                testing::ElementsAre(GetStyleMatcher(kMatch, 6, 5)));
  }
}

// Tests that unknown tag types and attributes are properly ignored.
TEST_F(SuggestionParserUnitTest, UnknownTagsAndAttributesAreIgnored) {
  {
    DescriptionAndStyles result =
        ParseSingleInput("hello <match some-attr=\"foo\">match</match> world");
    EXPECT_EQ(u"hello match world", result.description);
    EXPECT_THAT(result.styles,
                testing::ElementsAre(GetStyleMatcher(kMatch, 6, 5)));
  }
  {
    DescriptionAndStyles result =
        ParseSingleInput("hello <unknown>match</unknown> world");
    EXPECT_EQ(u"hello match world", result.description);
    EXPECT_THAT(result.styles, testing::IsEmpty());
  }
}

}  // namespace extensions
