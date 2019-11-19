// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_parser.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_paths.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

// Filters any param which as an occurrence of |name_str| in its |key| or an
// occurrence of |value_str| in its |value|.
bool TestFilter(const std::string& name_str,
                const std::string& value_str,
                const std::string& key,
                const std::string& value) {
  return (name_str.empty() || key.find(name_str) == std::string::npos) &&
         (value_str.empty() || value.find(value_str) == std::string::npos);
}

class TemplateURLParserTest : public testing::Test {
 protected:
  TemplateURLParserTest();
  ~TemplateURLParserTest() override;

  void SetUp() override;

  // Parses the OpenSearch description document at file_name (relative to the
  // data dir). The TemplateURL is placed in |template_url_|.
  void ParseFile(const std::string& file_name,
                 const TemplateURLParser::ParameterFilter& filter);

  void ParseString(const std::string& data,
                   const TemplateURLParser::ParameterFilter& filter);

  // ParseFile parses the results into this template_url.
  std::unique_ptr<TemplateURL> template_url_;

 private:
  void OnTemplateURLParsed(base::OnceClosure quit_closure,
                           std::unique_ptr<TemplateURL> template_url) {
    template_url_ = std::move(template_url);
    std::move(quit_closure).Run();
  }

  base::FilePath osdd_dir_;
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

TemplateURLParserTest::TemplateURLParserTest() {}

TemplateURLParserTest::~TemplateURLParserTest() {
}

void TemplateURLParserTest::SetUp() {
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &osdd_dir_));
  osdd_dir_ = osdd_dir_.AppendASCII("osdd");
  ASSERT_TRUE(base::PathExists(osdd_dir_));
}

void TemplateURLParserTest::ParseFile(
    const std::string& file_name,
    const TemplateURLParser::ParameterFilter& filter) {
  base::FilePath full_path = osdd_dir_.AppendASCII(file_name);
  ASSERT_TRUE(base::PathExists(full_path));

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(full_path, &contents));
  ParseString(contents, filter);
}

void TemplateURLParserTest::ParseString(
    const std::string& data,
    const TemplateURLParser::ParameterFilter& filter) {
  base::RunLoop run_loop;
  SearchTermsData search_terms_data;
  TemplateURLParser::Parse(
      &search_terms_data, data, filter,
      base::BindOnce(&TemplateURLParserTest::OnTemplateURLParsed,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
}

// Actual tests ---------------------------------------------------------------

TEST_F(TemplateURLParserTest, FailOnBogusURL) {
  ASSERT_NO_FATAL_FAILURE(
      ParseFile("bogus.xml", TemplateURLParser::ParameterFilter()));
  EXPECT_FALSE(template_url_);
}

TEST_F(TemplateURLParserTest, PassOnHTTPS) {
  ASSERT_NO_FATAL_FAILURE(
      ParseFile("https.xml", TemplateURLParser::ParameterFilter()));
  EXPECT_TRUE(template_url_);
}

TEST_F(TemplateURLParserTest, FailOnPost) {
  ASSERT_NO_FATAL_FAILURE(
      ParseFile("post.xml", TemplateURLParser::ParameterFilter()));
  EXPECT_FALSE(template_url_);
}

TEST_F(TemplateURLParserTest, TestDictionary) {
  ASSERT_NO_FATAL_FAILURE(
      ParseFile("dictionary.xml", TemplateURLParser::ParameterFilter()));
  ASSERT_TRUE(template_url_);
  EXPECT_EQ(ASCIIToUTF16("Dictionary.com"), template_url_->short_name());
  EXPECT_EQ(GURL("http://cache.lexico.com/g/d/favicon.ico"),
            template_url_->favicon_url());
  EXPECT_TRUE(template_url_->url_ref().SupportsReplacement(SearchTermsData()));
  EXPECT_EQ("http://dictionary.reference.com/browse/{searchTerms}?r=75",
            template_url_->url());
}

TEST_F(TemplateURLParserTest, TestMSDN) {
  ASSERT_NO_FATAL_FAILURE(
      ParseFile("msdn.xml", TemplateURLParser::ParameterFilter()));
  ASSERT_TRUE(template_url_);
  EXPECT_EQ(ASCIIToUTF16("Search \" MSDN"), template_url_->short_name());
  EXPECT_EQ(GURL("http://search.msdn.microsoft.com/search/favicon.ico"),
            template_url_->favicon_url());
  EXPECT_TRUE(template_url_->url_ref().SupportsReplacement(SearchTermsData()));
  EXPECT_EQ("http://search.msdn.microsoft.com/search/default.aspx?"
            "Query={searchTerms}&brand=msdn&locale=en-US",
            template_url_->url());
}

TEST_F(TemplateURLParserTest, TestWikipedia) {
  ASSERT_NO_FATAL_FAILURE(
      ParseFile("wikipedia.xml", TemplateURLParser::ParameterFilter()));
  ASSERT_TRUE(template_url_);
  EXPECT_EQ(ASCIIToUTF16("Wikipedia (English)"), template_url_->short_name());
  EXPECT_EQ(GURL("http://en.wikipedia.org/favicon.ico"),
            template_url_->favicon_url());
  EXPECT_TRUE(template_url_->url_ref().SupportsReplacement(SearchTermsData()));
  EXPECT_EQ("http://en.wikipedia.org/w/index.php?"
            "title=Special:Search&search={searchTerms}",
            template_url_->url());
  EXPECT_TRUE(template_url_->suggestions_url_ref().SupportsReplacement(
      SearchTermsData()));
  EXPECT_EQ("http://en.wikipedia.org/w/api.php?"
            "action=opensearch&search={searchTerms}",
            template_url_->suggestions_url());
  ASSERT_EQ(2U, template_url_->input_encodings().size());
  EXPECT_EQ("UTF-8", template_url_->input_encodings()[0]);
  EXPECT_EQ("Shift_JIS", template_url_->input_encodings()[1]);
}

TEST_F(TemplateURLParserTest, NoCrashOnEmptyAttributes) {
  ASSERT_NO_FATAL_FAILURE(ParseFile("url_with_no_attributes.xml",
                                    TemplateURLParser::ParameterFilter()));
}

TEST_F(TemplateURLParserTest, TestFirefoxEbay) {
  // This file uses the Parameter extension
  // (see http://www.opensearch.org/Specifications/OpenSearch/Extensions/Parameter/1.0)
  TemplateURLParser::ParameterFilter filter =
      base::BindRepeating(&TestFilter, "ebay", "ebay");
  ASSERT_NO_FATAL_FAILURE(ParseFile("firefox_ebay.xml", filter));
  ASSERT_TRUE(template_url_);
  EXPECT_EQ(ASCIIToUTF16("eBay"), template_url_->short_name());
  EXPECT_TRUE(template_url_->url_ref().SupportsReplacement(SearchTermsData()));
  EXPECT_EQ("http://search.ebay.com/search/search.dll?query={searchTerms}&"
            "MfcISAPICommand=GetResult&ht=1&srchdesc=n&maxRecordsReturned=300&"
            "maxRecordsPerPage=50&SortProperty=MetaEndSort",
            template_url_->url());
  ASSERT_EQ(1U, template_url_->input_encodings().size());
  EXPECT_EQ("ISO-8859-1", template_url_->input_encodings()[0]);
  EXPECT_EQ(GURL("http://search.ebay.com/favicon.ico"),
            template_url_->favicon_url());
}

TEST_F(TemplateURLParserTest, TestFirefoxWebster) {
  // This XML file uses a namespace.
  TemplateURLParser::ParameterFilter filter =
      base::BindRepeating(&TestFilter, std::string(), "Mozilla");
  ASSERT_NO_FATAL_FAILURE(ParseFile("firefox_webster.xml", filter));
  ASSERT_TRUE(template_url_);
  EXPECT_EQ(ASCIIToUTF16("Webster"), template_url_->short_name());
  EXPECT_TRUE(template_url_->url_ref().SupportsReplacement(SearchTermsData()));
  EXPECT_EQ("http://www.webster.com/cgi-bin/dictionary?va={searchTerms}",
            template_url_->url());
  ASSERT_EQ(1U, template_url_->input_encodings().size());
  EXPECT_EQ("ISO-8859-1", template_url_->input_encodings()[0]);
  EXPECT_EQ(GURL("http://www.webster.com/favicon.ico"),
            template_url_->favicon_url());
}

TEST_F(TemplateURLParserTest, TestFirefoxYahoo) {
  // This XML file uses a namespace.
  TemplateURLParser::ParameterFilter filter =
      base::BindRepeating(&TestFilter, std::string(), "Mozilla");
  ASSERT_NO_FATAL_FAILURE(ParseFile("firefox_yahoo.xml", filter));
  ASSERT_TRUE(template_url_);
  EXPECT_EQ(ASCIIToUTF16("Yahoo"), template_url_->short_name());
  EXPECT_TRUE(template_url_->url_ref().SupportsReplacement(SearchTermsData()));
  EXPECT_EQ("http://ff.search.yahoo.com/gossip?"
            "output=fxjson&command={searchTerms}",
            template_url_->suggestions_url());
  EXPECT_EQ("http://search.yahoo.com/search?p={searchTerms}&ei=UTF-8",
            template_url_->url());
  ASSERT_EQ(1U, template_url_->input_encodings().size());
  EXPECT_EQ("UTF-8", template_url_->input_encodings()[0]);
  EXPECT_EQ(GURL("http://search.yahoo.com/favicon.ico"),
            template_url_->favicon_url());
}

// Make sure we ignore POST suggestions (this is the same XML file as
// firefox_yahoo.xml, the suggestion method was just changed to POST).
TEST_F(TemplateURLParserTest, TestPostSuggestion) {
  // This XML file uses a namespace.
  TemplateURLParser::ParameterFilter filter =
      base::BindRepeating(&TestFilter, std::string(), "Mozilla");
  ASSERT_NO_FATAL_FAILURE(ParseFile("post_suggestion.xml", filter));
  ASSERT_TRUE(template_url_);
  EXPECT_EQ(ASCIIToUTF16("Yahoo"), template_url_->short_name());
  EXPECT_TRUE(template_url_->url_ref().SupportsReplacement(SearchTermsData()));
  EXPECT_TRUE(template_url_->suggestions_url().empty());
  EXPECT_EQ("http://search.yahoo.com/search?p={searchTerms}&ei=UTF-8",
            template_url_->url());
  ASSERT_EQ(1U, template_url_->input_encodings().size());
  EXPECT_EQ("UTF-8", template_url_->input_encodings()[0]);
  EXPECT_EQ(GURL("http://search.yahoo.com/favicon.ico"),
            template_url_->favicon_url());
}

// <Alias> tags are parsed and used as keyword for the template URL.
TEST_F(TemplateURLParserTest, TestKeyword) {
  ASSERT_NO_FATAL_FAILURE(
      ParseFile("keyword.xml", TemplateURLParser::ParameterFilter()));
  ASSERT_TRUE(template_url_);
  EXPECT_EQ(ASCIIToUTF16("Example"), template_url_->short_name());
  EXPECT_EQ("https://www.example.com/search?q={searchTerms}",
            template_url_->url());
  EXPECT_EQ(ASCIIToUTF16("moose"), template_url_->keyword());
}

// Empty <Alias> tags are ignored and the default keyword is used instead
// (because empty keywords are not allowed).
TEST_F(TemplateURLParserTest, TestEmptyKeyword) {
  ASSERT_NO_FATAL_FAILURE(
      ParseFile("empty_keyword.xml", TemplateURLParser::ParameterFilter()));
  ASSERT_TRUE(template_url_);
  EXPECT_EQ(ASCIIToUTF16("Example"), template_url_->short_name());
  EXPECT_EQ("https://www.example.com/search?q={searchTerms}",
            template_url_->url());
  EXPECT_EQ(ASCIIToUTF16("example.com"), template_url_->keyword());
}

// An invalid template URL should not crash the parser.
// crbug.com/770734
TEST_F(TemplateURLParserTest, InvalidInput) {
  TemplateURLParser::ParameterFilter filter = base::BindRepeating(
      [](const std::string&, const std::string&) -> bool { return true; });
  constexpr char char_data[] = R"(
    <OpenSearchDescription>
    <Url template=")R:RRR?>RRR0" type="application/x-suggestions+json">
      <Param name="name" value="value"/>
    </Url>
    </OpenSearchDescription>
  )";
  ParseString(char_data, filter);
}
