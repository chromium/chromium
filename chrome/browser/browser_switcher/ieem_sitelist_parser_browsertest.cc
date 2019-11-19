// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_switcher {

namespace {

void OnXmlParsed(base::RepeatingClosure quit_run_loop,
                 ParsedXml expected,
                 ParsedXml actual) {
  base::ScopedClosureRunner runner(std::move(quit_run_loop));
  EXPECT_EQ(expected.rules, actual.rules);
  EXPECT_EQ(expected.error.has_value(), actual.error.has_value());
  if (expected.error.has_value() && actual.error.has_value())
    EXPECT_EQ(*expected.error, *actual.error);
}

void TestParseXml(const std::string& xml, ParsedXml expected) {
  base::RunLoop run_loop;
  ParseIeemXml(xml, base::BindOnce(&OnXmlParsed, run_loop.QuitClosure(),
                                   std::move(expected)));
  run_loop.Run();
}

}  // namespace

class IeemSitelistParserTest : public InProcessBrowserTest {
 public:
  IeemSitelistParserTest() = default;
  ~IeemSitelistParserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(IeemSitelistParserTest);
};

IN_PROC_BROWSER_TEST_F(IeemSitelistParserTest, BadXml) {
  TestParseXml("", ParsedXml({}, "Invalid XML: bad content"));
  TestParseXml("thisisnotxml", ParsedXml({}, "Invalid XML: bad content"));
}

IN_PROC_BROWSER_TEST_F(IeemSitelistParserTest, BadXmlParsed) {
  TestParseXml("<bogus></bogus>", ParsedXml({}, "Invalid XML root element"));
  TestParseXml("<rules version=\"424\"><unknown></unknown></rules>",
               ParsedXml({}, base::nullopt));
}

IN_PROC_BROWSER_TEST_F(IeemSitelistParserTest, V1OnlyBogusElements) {
  std::string xml =
      "<rules version=\"424\">"
      "<unknown><more><docMode><domain>ignore.com</domain></docMode>"
      "</more><emie><domain>ignoretoo.com<path>/ignored_path</path>"
      "</domain></emie><domain>onemoreignored.com</domain>"
      "<path>/ignore_outside_of_domain></path></unknown></rules>";
  TestParseXml(xml, ParsedXml({}, base::nullopt));
}

IN_PROC_BROWSER_TEST_F(IeemSitelistParserTest, V1Full) {
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
  std::vector<std::string> expected_sitelist = {
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
  TestParseXml(xml, ParsedXml(std::move(expected_sitelist), base::nullopt));
}

IN_PROC_BROWSER_TEST_F(IeemSitelistParserTest, V2Full) {
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
  std::vector<std::string> expected_sitelist = {
      "!google.com",  "!good.site",     "www.cpandl.com",
      "!contoso.com", "!relecloud.com", "!relecloud.com/about",
  };
  TestParseXml(xml, ParsedXml(std::move(expected_sitelist), base::nullopt));
}

}  // namespace browser_switcher
