// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class TemplateURLServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    if (GetTestPreCount() == 0) {
      command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry,
                                      "FR");
    } else {
      command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry,
                                      "DE");
    }
  }

  TemplateURLService* template_url_service() {
    return TemplateURLServiceFactory::GetForProfile(browser()->profile());
  }
};

// Checks the logic associated with changing countries when reloading the
// keywords data, using Yahoo! as an indicator. Initially, start the profile in
// Germany and set Yahoo! DE as default search engine. Then when we restart the
// profile when forcing the country as France, we expect that the DSE was
// changed to Yahoo! FR via prepopulate_id matching during the re-merge.
IN_PROC_BROWSER_TEST_F(TemplateURLServiceBrowserTest, PRE_LoadKeywordData) {
  TemplateURL* yahoo_de_turl = template_url_service()->GetTemplateURLForKeyword(
      TemplateURLPrepopulateData::yahoo_de.keyword);
  ASSERT_TRUE(yahoo_de_turl);
  EXPECT_NE(
      template_url_service()->GetDefaultSearchProvider()->prepopulate_id(),
      TemplateURLPrepopulateData::yahoo_de.id);

  template_url_service()->SetUserSelectedDefaultSearchProvider(yahoo_de_turl);

  const TemplateURL* updated_dse =
      template_url_service()->GetDefaultSearchProvider();
  EXPECT_EQ(updated_dse->prepopulate_id(),
            TemplateURLPrepopulateData::yahoo_de.id);
  EXPECT_EQ(updated_dse->keyword(),
            TemplateURLPrepopulateData::yahoo_de.keyword);
  EXPECT_EQ(updated_dse->prepopulate_id(),
            TemplateURLPrepopulateData::yahoo_fr.id);
  EXPECT_NE(updated_dse->keyword(),
            TemplateURLPrepopulateData::yahoo_fr.keyword);
}

// TODO(crbug.com/41493716): Fails in Mac builds.
// TODO(crbug.com/365747879): Flaky in Windows builds.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_LoadKeywordData DISABLED_LoadKeywordData
#else
#define MAYBE_LoadKeywordData LoadKeywordData
#endif
IN_PROC_BROWSER_TEST_F(TemplateURLServiceBrowserTest, MAYBE_LoadKeywordData) {
  const TemplateURL* loaded_dse =
      template_url_service()->GetDefaultSearchProvider();
  EXPECT_EQ(loaded_dse->prepopulate_id(),
            TemplateURLPrepopulateData::yahoo_fr.id);
  EXPECT_EQ(loaded_dse->keyword(),
            TemplateURLPrepopulateData::yahoo_fr.keyword);
  EXPECT_EQ(loaded_dse->prepopulate_id(),
            TemplateURLPrepopulateData::yahoo_de.id);
  EXPECT_NE(loaded_dse->keyword(),
            TemplateURLPrepopulateData::yahoo_de.keyword);
}
