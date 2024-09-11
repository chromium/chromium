// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_test_util.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/chrome_template_url_service_client.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/testing_search_terms_data.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestingTemplateURLServiceClient : public ChromeTemplateURLServiceClient {
 public:
  TestingTemplateURLServiceClient(history::HistoryService* history_service,
                                  std::u16string* search_term)
      : ChromeTemplateURLServiceClient(history_service),
        search_term_(search_term) {}

  TestingTemplateURLServiceClient(const TestingTemplateURLServiceClient&) =
      delete;
  TestingTemplateURLServiceClient& operator=(
      const TestingTemplateURLServiceClient&) = delete;

  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURLID id,
                                   const std::u16string& term) override {
    *search_term_ = term;
  }

 private:
  raw_ptr<std::u16string> search_term_;
};

}  // namespace

void SetManagedDefaultSearchPreferences(const TemplateURLData& managed_data,
                                        bool enabled,
                                        TestingProfile* profile) {
  base::Value::Dict dict = TemplateURLDataToDictionary(managed_data);
  dict.Set(DefaultSearchManager::kDisabledByPolicy, !enabled);

  profile->GetTestingPrefService()->SetManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      std::move(dict));
}

void RemoveManagedDefaultSearchPreferences(TestingProfile* profile) {
  profile->GetTestingPrefService()->RemoveManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
}

void SetRecommendedDefaultSearchPreferences(const TemplateURLData& data,
                                            bool enabled,
                                            TestingProfile* profile) {
  base::Value::Dict dict = TemplateURLDataToDictionary(data);
  dict.Set(DefaultSearchManager::kDisabledByPolicy, !enabled);

  profile->GetTestingPrefService()->SetRecommendedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      std::move(dict));
}

void SetManagedSiteSearchSettingsPreference(
    const EnterpriseSiteSearchManager::OwnedTemplateURLDataVector&
        site_search_engines,
    TestingProfile* profile) {
  base::Value::List pref_value;
  for (auto& site_search_engine : site_search_engines) {
    pref_value.Append(
        base::Value(TemplateURLDataToDictionary(*site_search_engine)));
  }

  profile->GetTestingPrefService()->SetManagedPref(
      EnterpriseSiteSearchManager::kSiteSearchSettingsPrefName,
      std::move(pref_value));
}

std::unique_ptr<TemplateURL> CreateTestTemplateURL(
    const std::u16string& keyword,
    const std::string& url,
    const std::string& guid,
    base::Time last_modified,
    bool safe_for_autoreplace,
    TemplateURLData::CreatedByPolicy created_by_policy,
    int prepopulate_id) {
  DCHECK(!base::StartsWith(guid, "key"))
      << "Don't use test GUIDs with the form \"key1\". Use \"guid1\" instead "
         "for clarity.";

  TemplateURLData data;
  data.SetShortName(u"unittest");
  data.SetKeyword(keyword);
  data.SetURL(url);
  data.favicon_url = GURL("http://favicon.url");
  data.safe_for_autoreplace = safe_for_autoreplace;
  data.date_created = base::Time::FromTimeT(100);
  data.last_modified = last_modified;
  data.created_by_policy = created_by_policy;
  data.prepopulate_id = prepopulate_id;
  if (!guid.empty())
    data.sync_guid = guid;
  return std::make_unique<TemplateURL>(data);
}

TemplateURLServiceTestUtil::TemplateURLServiceTestUtil()
    : TemplateURLServiceTestUtil(TestingProfile::TestingFactories()) {}

TemplateURLServiceTestUtil::TemplateURLServiceTestUtil(PrefService& local_state)
    : TemplateURLServiceTestUtil(TestingProfile::TestingFactories(),
                                 &local_state) {}

TemplateURLServiceTestUtil::TemplateURLServiceTestUtil(
    TestingProfile::TestingFactories testing_factories,
    PrefService* local_state)
    : local_state_(local_state) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactories(std::move(testing_factories));
  profile_ = profile_builder.Build();

  scoped_refptr<WebDatabaseService> web_database_service =
      new WebDatabaseService(profile_->GetPath().AppendASCII("webdata"),
                             base::SingleThreadTaskRunner::GetCurrentDefault(),
                             base::SingleThreadTaskRunner::GetCurrentDefault());
  web_database_service->AddTable(
      std::unique_ptr<WebDatabaseTable>(new KeywordTable()));
  web_database_service->LoadDatabase(g_browser_process->os_crypt_async());

  web_data_service_ = new KeywordWebDataService(
      web_database_service.get(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  web_data_service_->Init(base::NullCallback());

  if (!local_state_) {
    if (g_browser_process->local_state()) {
      local_state_ = g_browser_process->local_state();
    } else {
      // `g_browser_process->local_state()` might be null in unit tests.
      owned_local_state_ = std::make_unique<ScopedTestingLocalState>(
          TestingBrowserProcess::GetGlobal());
      local_state_ = owned_local_state_->Get();
    }
  }

  search_engine_choice_service_ =
      std::make_unique<search_engines::SearchEngineChoiceService>(
          *profile_->GetPrefs(), local_state_,
          /*is_profile_eligible_for_dse_guest_propagation=*/false);

  ResetModel(false);
}

TemplateURLServiceTestUtil::~TemplateURLServiceTestUtil() {
  ClearModel();
  web_data_service_->ShutdownOnUISequence();
  search_engine_choice_service_.reset();
  profile_.reset();

  // Flush the message loop to make application verifiers happy.
  base::RunLoop().RunUntilIdle();
}

void TemplateURLServiceTestUtil::OnTemplateURLServiceChanged() {
  changed_count_++;
}

int TemplateURLServiceTestUtil::GetObserverCount() {
  return changed_count_;
}

void TemplateURLServiceTestUtil::ResetObserverCount() {
  changed_count_ = 0;
}

void TemplateURLServiceTestUtil::VerifyLoad() {
  ASSERT_FALSE(model()->loaded());
  model()->Load();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetObserverCount());
  ResetObserverCount();
}

void TemplateURLServiceTestUtil::ChangeModelToLoadState() {
  model()->ChangeToLoadedState();
  // Initialize the web data service so that the database gets updated with
  // any changes made.

  model()->web_data_service_ = web_data_service_;
  base::RunLoop().RunUntilIdle();
}

void TemplateURLServiceTestUtil::ClearModel() {
  model_->Shutdown();
  model_.reset();
}

void TemplateURLServiceTestUtil::ResetModel(bool verify_load) {
  if (model_)
    ClearModel();
  model_ = std::make_unique<TemplateURLService>(
      *profile()->GetPrefs(), *search_engine_choice_service_,
      std::make_unique<TestingSearchTermsData>("http://www.google.com/"),
      web_data_service_.get(),
      std::unique_ptr<TemplateURLServiceClient>(
          new TestingTemplateURLServiceClient(
              HistoryServiceFactory::GetForProfileIfExists(
                  profile(), ServiceAccessType::EXPLICIT_ACCESS),
              &search_term_)),
      base::BindLambdaForTesting([&] { ++dsp_set_to_google_callback_count_; })
#if BUILDFLAG(IS_CHROMEOS_LACROS)
          ,
      profile()->IsMainProfile()
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  );
  model()->AddObserver(this);
  changed_count_ = 0;
  if (verify_load)
    VerifyLoad();
}

std::u16string TemplateURLServiceTestUtil::GetAndClearSearchTerm() {
  std::u16string search_term;
  search_term.swap(search_term_);
  return search_term;
}

TemplateURL* TemplateURLServiceTestUtil::AddExtensionControlledTURL(
    std::unique_ptr<TemplateURL> extension_turl) {
  TemplateURL* result = model()->Add(std::move(extension_turl));
  DCHECK(result);
  DCHECK(result->GetExtensionInfoForTesting());
  if (result->GetExtensionInfoForTesting()->wants_to_be_default_engine) {
    SetExtensionDefaultSearchInPrefs(profile()->GetTestingPrefService(),
                                     result->data());
  }
  return result;
}

void TemplateURLServiceTestUtil::RemoveExtensionControlledTURL(
    const std::string& extension_id) {
  TemplateURL* turl = model()->FindTemplateURLForExtension(
      extension_id, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION);
  ASSERT_TRUE(turl);
  ASSERT_TRUE(turl->GetExtensionInfoForTesting());
  if (turl->GetExtensionInfoForTesting()->wants_to_be_default_engine)
    RemoveExtensionDefaultSearchFromPrefs(profile()->GetTestingPrefService());
  model()->RemoveExtensionControlledTURL(
      extension_id, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION);
}
