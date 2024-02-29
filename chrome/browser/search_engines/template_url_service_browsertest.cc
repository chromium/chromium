// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class DefaultSearchEngineObserver : public TemplateURLServiceObserver {
 public:
  explicit DefaultSearchEngineObserver(
      TemplateURLService& template_url_service,
      base::OnceClosure on_default_search_engine_changed,
      int engine_prepopulated_id)
      : template_url_service_(template_url_service),
        on_default_search_engine_changed_(
            std::move(on_default_search_engine_changed)),
        engine_prepopulated_id_(engine_prepopulated_id) {
    if (MaybeRunDefaultSearchEngineChangedCallback()) {
      return;
    }

    observations_.AddObservation(&template_url_service_.get());
  }

  ~DefaultSearchEngineObserver() override = default;

  void OnTemplateURLServiceChanged() override {
    MaybeRunDefaultSearchEngineChangedCallback();
  }

 private:
  bool MaybeRunDefaultSearchEngineChangedCallback() {
    const TemplateURL* default_search_engine =
        template_url_service_->GetDefaultSearchProvider();
    if (default_search_engine->prepopulate_id() == engine_prepopulated_id_) {
      std::move(on_default_search_engine_changed_).Run();
      return true;
    }

    return false;
  }

  raw_ref<TemplateURLService> template_url_service_;
  base::OnceClosure on_default_search_engine_changed_;
  int engine_prepopulated_id_;

  base::ScopedMultiSourceObservation<TemplateURLService,
                                     TemplateURLServiceObserver>
      observations_{this};
};
class TemplateURLServiceBrowserTest : public InProcessBrowserTest {
 public:
  TemplateURLServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        switches::kSearchEngineChoiceTrigger,
        {{switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name,
          "false"}});
  }

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

  base::test::ScopedFeatureList scoped_feature_list_;
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

IN_PROC_BROWSER_TEST_F(TemplateURLServiceBrowserTest, LoadKeywordData) {
  // We wait for the expected search engine to load because the test was flaky.
  // See crbug.com/1520740
  base::RunLoop runloop;
  DefaultSearchEngineObserver observer(CHECK_DEREF(template_url_service()),
                                       runloop.QuitClosure(),
                                       TemplateURLPrepopulateData::yahoo_fr.id);
  runloop.Run();

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
