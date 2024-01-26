// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_switcher/browser_switcher_features.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_switcher {

namespace {

void OnXmlParsed(base::RepeatingClosure quit_run_loop,
                 ParsedXml expected,
                 ParsedXml actual) {
  base::ScopedClosureRunner runner(std::move(quit_run_loop));
  EXPECT_EQ(expected.rules.sitelist, actual.rules.sitelist);
  EXPECT_EQ(expected.rules.greylist, actual.rules.greylist);
  EXPECT_EQ(expected.error.has_value(), actual.error.has_value());
  if (expected.error.has_value() && actual.error.has_value()) {
    // The actual error may have more detail at the end. Ensure it starts with
    // the expected error.
    EXPECT_THAT(*actual.error, testing::StartsWith(*expected.error));
  }
}

}  // namespace

class IeemSitelistParserTest
    : public testing::TestWithParam<std::tuple<ParsingMode, bool>> {
 public:
  IeemSitelistParserTest() {
    parsing_mode_ = std::get<0>(GetParam());
    none_is_greylist_ = std::get<1>(GetParam());
    feature_list_.InitWithFeatureState(kBrowserSwitcherNoneIsGreylist,
                                       none_is_greylist());
  }
  ~IeemSitelistParserTest() override = default;

  ParsingMode parsing_mode() { return parsing_mode_; }
  bool none_is_greylist() { return none_is_greylist_; }

  void TestParseXml(const std::string& xml, ParsedXml expected) {
    base::RunLoop run_loop;
    ParseIeemXml(xml, parsing_mode(),
                 base::BindOnce(&OnXmlParsed, run_loop.QuitClosure(),
                                std::move(expected)));
    run_loop.Run();
  }

 private:
  ParsingMode parsing_mode_;
  bool none_is_greylist_;

  base::test::ScopedFeatureList feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

TEST_P(IeemSitelistParserTest, BadXml) {
  TestParseXml("", ParsedXml({}, {}, "Invalid XML: bad content"));
  TestParseXml("thisisnotxml", ParsedXml({}, {}, "Invalid XML: bad content"));
}

TEST_P(IeemSitelistParserTest, BadXmlParsed) {
  TestParseXml("<bogus></bogus>",
               ParsedXml({}, {}, "Invalid XML root element"));
  TestParseXml("<rules version=\"424\"><unknown></unknown></rules>",
               ParsedXml({}, {}, std::nullopt));
}

TEST_P(IeemSitelistParserTest, V1OnlyBogusElements) {
  std::string xml =
      "<rules version=\"424\">"
      "<unknown><more><docMode><domain>ignore.com</domain></docMode>"
      "</more><emie><domain>ignoretoo.com<path>/ignored_path</path>"
      "</domain></emie><domain>onemoreignored.com</domain>"
      "<path>/ignore_outside_of_domain></path></unknown></rules>";
  TestParseXml(xml, ParsedXml({}, {}, std::nullopt));
}

TEST_P(IeemSitelistParserTest, V1Full) {
  std::string xml =
      "<rules version=\"424\"><unknown><more><docMode><domain>ignore"
      "</domain></docMode></more><emie><domain>ignoretoo.com<path>/ignored_path"
      "</path></domain></emie><domain>onemoreingored.com</domain><path>"
      "/ignore_outside_of_domain></path></unknown><emie><other><more><docMode>"
      "<domain>ignore.com</domain></docMode></more><emie><domain>ignoretoo.com"
      "<path>/ignored_path</path></domain></emie><domain>onemoreingored.com"
      "</domain><path>/ignore_outside_of_domain></path></other><!--<domain "
      "exclude=\"false\">hotscanacc.dbch.b-source.net<path exclude=\"false\">"
      "/HotScan/</path></domain>--><domain>inside.com<more><docMode><domain>"
      "ignore.com</domain></docMode></more><emie><domain>ignoretoo.com<path>"
      "/ignored_path</path></domain></emie><domain>onemoreingored.com</domain>"
      "<path>/in_domain<more><docMode><domain>ignore.com</domain></docMode>"
      "</more><emie><domain>ignoretoo.com<path>/ignored_path</path></domain>"
      "</emie><domain>onemoreingored.com</domain><path>/ignore_nested_path>"
      "</path></path></domain><domain>   \ngoogle.com\t\t \t</domain><domain "
      "exclude=\"true\">good.com</domain><domain exclude=\"false\">more.com"
      "</domain><domain>e100.com<path>/path1</path><path exclude=\"true\">/pa2"
      "</path><path exclude=\"false\">/path3</path></domain><domain "
      "exclude=\"true\">e200.com<path>/path1</path><path exclude=\"true\">/pth2"
      "</path><path exclude=\"false\">/path3</path></domain><domain "
      "exclude=\"false\">e300.com<path>/path1</path><path exclude=\"true\">/pt2"
      "</path><path exclude=\"false\">/path3</path></domain><domain "
      "exclude=\"true\">random.com<path exclude=\"true\">/path1/</path><path "
      "exclude=\"false\" forceCompatView=\"true\">/path2<path exclude=\"true\">"
      "/TEST</path></path></domain></emie><docMode><domain docMode=\"8\">"
      "moredomains.com</domain><domain docMode=\"5\">evenmore.com<path "
      "docMode=\"5\">/r1</path><path docMode=\"5\">/r2</path></domain><domain "
      "docMode=\"5\" exclude=\"true\">domainz.com<path docMode=\"5\">/r2</path>"
      "<path docMode=\"5\" exclude=\"true\"> \n/r5\t</path><path docMode=\"5\" "
      "exclude=\"false\">/r6</path></domain><domain docMode=\"5\" "
      "exclude=\"false\">howmanydomainz.com<path docMode=\"5\">/r8</path><path "
      "docMode=\"5\" exclude=\"true\">/r9</path><path docMode=\"5\" "
      "exclude=\"false\">/r10</path></domain><domain exclude=\"true\" "
      "doNotTransition=\"true\">maybe.com<path>/yestransition</path>"
      "<path doNotTransition=\"true\">/guessnot</path></domain><domain>"
      "yes.com<path doNotTransition=\"true\">/actuallyno</path></domain>"
      "<domain doNotTransition=\"true\">no.com</domain></docMode></rules>";
  std::vector<std::string> expected_sitelist;
  std::vector<std::string> expected_greylist;
  if (none_is_greylist() && parsing_mode() == ParsingMode::kIESiteListMode) {
    expected_sitelist = {
        "inside.com",
        "inside.com/in_domain",
        "google.com",
        "more.com",
        "e100.com",
        "e100.com/path1",
        "e100.com/path3",
        "e200.com/path1",
        "e200.com/path3",
        "e300.com",
        "e300.com/path1",
        "e300.com/path3",
        "random.com/path2",
        "moredomains.com",
        "evenmore.com",
        "evenmore.com/r1",
        "evenmore.com/r2",
        "domainz.com/r2",
        "domainz.com/r6",
        "howmanydomainz.com",
        "howmanydomainz.com/r8",
        "howmanydomainz.com/r10",
        "maybe.com/yestransition",
        "yes.com",
    };
    expected_greylist = {
        "maybe.com/guessnot",
        "yes.com/actuallyno",
        "no.com",
    };
  } else {
    expected_sitelist = {
        "inside.com",
        "inside.com/in_domain",
        "google.com",
        "more.com",
        "e100.com",
        "e100.com/path1",
        "e100.com/path3",
        "e200.com/path1",
        "e200.com/path3",
        "e300.com",
        "e300.com/path1",
        "e300.com/path3",
        "random.com/path2",
        "moredomains.com",
        "evenmore.com",
        "evenmore.com/r1",
        "evenmore.com/r2",
        "domainz.com/r2",
        "domainz.com/r6",
        "howmanydomainz.com",
        "howmanydomainz.com/r8",
        "howmanydomainz.com/r10",
        "maybe.com/yestransition",
        "!maybe.com/guessnot",
        "yes.com",
        "!yes.com/actuallyno",
        "!no.com",
    };
  }
  TestParseXml(xml, ParsedXml(std::move(expected_sitelist),
                              std::move(expected_greylist), std::nullopt));
}

TEST_P(IeemSitelistParserTest, V2Full) {
  // Very subtle issue in the closing element for rules.
  std::string xml =
      "<site-list version=\"205\"><!-- File creation header -->"
      "<created-by><tool>EnterpriseSitelistManager</tool><version>10240"
      "</version><date-created>20150728.135021</date-created></created-by>"
      "<!-- unknown tags --><unknown><test><mest>test</mest></test>"
      "<!-- comments --></unknown><!-- no url attrib --><site><open-in>none"
      "</open-in></site><!-- nested site list --><site-list><site "
      "url=\"ignore!\"/></site-list><!-- nested site --><site "
      "url=\"google.com\"><site url=\"nested ignore!\"></site></site><!-- "
      "unknown tags in a site on multiple levels --><site url=\"good.site\">"
      "<!-- nested comments --><somethings>klj<other some=\"none\"/>jkh"
      "</somethings></site><!-- good sites --> <site url=\"www.cpandl.com\">"
      "<compat-mode>IE8Enterprise</compat-mode><open-in>MSEdge</open-in></site>"
      "<site url=\"contoso.com\"><compat-mode>default</compat-mode><open-in>"
      "None</open-in></site><site url=\"relecloud.com\"/><site "
      "url=\"relecloud.com/about\"><compat-mode>IE8Enterprise</compat-mode>"
      "</site></site-list><!-- trailing gibberish <trailing><site "
      "url=\"ignore after site list!\">  <compat-mode>IE8Enterprise\""
      "</compat-mode></site><gibberish>Lorem ipsum sit...</gibberish>"
      "</trailing>-->";
  std::vector<std::string> expected_sitelist;
  std::vector<std::string> expected_greylist;
  if (none_is_greylist() && parsing_mode() == ParsingMode::kIESiteListMode) {
    expected_sitelist = {"!www.cpandl.com"};
    expected_greylist = {
        "google.com",    "good.site",           "contoso.com",
        "relecloud.com", "relecloud.com/about",
    };
  } else {
    expected_sitelist = {
        "!google.com",  "!good.site",     "www.cpandl.com",
        "!contoso.com", "!relecloud.com", "!relecloud.com/about",
    };
  }
  TestParseXml(xml, ParsedXml(std::move(expected_sitelist),
                              std::move(expected_greylist), std::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    ParsingMode,
    IeemSitelistParserTest,
    testing::Combine(testing::Values(ParsingMode::kDefault,
                                     ParsingMode::kIESiteListMode,
                                     // 999 should behave like kDefault.
                                     static_cast<ParsingMode>(999)),
                     testing::Bool()));

}  // namespace browser_switcher
