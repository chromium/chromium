// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/enterprise/enterprise_site_search_manager.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service_observer.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"

class KeywordWebDataService;
class TemplateURLService;

// Sets the managed preferences for the default search provider. `enabled`
// enables/disables use of the managed engine by `DefaultSearchManager`.
void SetManagedDefaultSearchPreferences(const TemplateURLData& managed_data,
                                        bool enabled,
                                        TestingProfile* profile);

// Removes all the managed preferences for the default search provider.
void RemoveManagedDefaultSearchPreferences(TestingProfile* profile);

// Sets the recommended preferences for the default search provider. `enabled`
// enables/disables use of the managed engine by `DefaultSearchManager`.
void SetRecommendedDefaultSearchPreferences(const TemplateURLData& data,
                                            bool enabled,
                                            TestingProfile* profile);

// Sets the managed preferences for site search providers.
void SetManagedSiteSearchSettingsPreference(
    const EnterpriseSiteSearchManager::OwnedTemplateURLDataVector&
        site_search_engines,
    TestingProfile* profile);

// Creates a TemplateURL with some test values. The caller owns the returned
// TemplateURL*.
std::unique_ptr<TemplateURL> CreateTestTemplateURL(
    const std::u16string& keyword,
    const std::string& url,
    const std::string& guid = std::string(),
    base::Time last_modified = base::Time::FromTimeT(100),
    bool safe_for_autoreplace = false,
    TemplateURLData::CreatedByPolicy created_by_policy =
        TemplateURLData::CreatedByPolicy::kNoPolicy,
    int prepopulate_id = 999999);

class TemplateURLServiceTestUtil : public TemplateURLServiceObserver {
 public:
  TemplateURLServiceTestUtil();

  explicit TemplateURLServiceTestUtil(PrefService& local_state);

  explicit TemplateURLServiceTestUtil(
      TestingProfile::TestingFactories testing_factories,
      PrefService* local_state = nullptr);

  TemplateURLServiceTestUtil(const TemplateURLServiceTestUtil&) = delete;
  TemplateURLServiceTestUtil& operator=(const TemplateURLServiceTestUtil&) =
      delete;

  ~TemplateURLServiceTestUtil() override;

  // TemplateURLServiceObserver implemementation.
  void OnTemplateURLServiceChanged() override;

  // Gets the observer count.
  int GetObserverCount();

  // Sets the observer count to 0.
  void ResetObserverCount();

  // Gets the number of times the DSP has been set to Google.
  int dsp_set_to_google_callback_count() const {
    return dsp_set_to_google_callback_count_;
  }

  // Makes sure the load was successful and sent the correct notification.
  void VerifyLoad();

  // Makes the model believe it has been loaded (without actually doing the
  // load). Since this avoids setting the built-in keyword version, the next
  // load will do a merge from prepopulated data.
  void ChangeModelToLoadState();

  // Deletes the current model (and doesn't create a new one).
  void ClearModel();

  // Creates a new TemplateURLService.
  void ResetModel(bool verify_load);

  // Returns the search term from the last invocation of
  // TemplateURLService::SetKeywordSearchTermsForURL and clears the search term.
  std::u16string GetAndClearSearchTerm();

  // Adds extension controlled TemplateURL to the model and overrides default
  // search pref in an extension controlled preferences, if extension wants to
  // be default.
  TemplateURL* AddExtensionControlledTURL(
      std::unique_ptr<TemplateURL> extension_turl);

  // Removes a TemplateURL controlled by |extension_id| from the model, and,
  // if necessary, from the extension-controlled default search preference.
  // This TemplateURL must exist.
  void RemoveExtensionControlledTURL(const std::string& extension_id);

  KeywordWebDataService* web_data_service() { return web_data_service_.get(); }
  TemplateURLService* model() { return model_.get(); }
  TestingProfile* profile() { return profile_.get(); }
  search_engines::SearchEngineChoiceService* search_engine_choice_service() {
    return search_engine_choice_service_.get();
  }

 private:
  // Populated only if the calling test did not previously set up a
  // local state. This object would then own the process-global local
  // state.
  // Don't access it directly, prefer using `local_state_` instead.
  std::unique_ptr<ScopedTestingLocalState> owned_local_state_;

  // We pass `local_state_` to the constructor in some cases where we can't
  // or don't want to use `g_browser_process->local_state()`.
  raw_ptr<PrefService> local_state_;
  std::unique_ptr<TestingProfile> profile_;
  int changed_count_ = 0;
  std::u16string search_term_;
  int dsp_set_to_google_callback_count_ = 0;
  scoped_refptr<KeywordWebDataService> web_data_service_;
  std::unique_ptr<search_engines::SearchEngineChoiceService>
      search_engine_choice_service_;
  std::unique_ptr<TemplateURLService> model_;
  data_decoder::test::InProcessDataDecoder data_decoder_;
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_
