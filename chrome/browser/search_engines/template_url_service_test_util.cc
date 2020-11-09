// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_test_util.h"

#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/chrome_template_url_service_client.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
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
                                  base::string16* search_term)
      : ChromeTemplateURLServiceClient(history_service),
        search_term_(search_term) {}

  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURLID id,
                                   const base::string16& term) override {
    *search_term_ = term;
  }

 private:
  base::string16* search_term_;

  DISALLOW_COPY_AND_ASSIGN(TestingTemplateURLServiceClient);
};

}  // namespace

void SetManagedDefaultSearchPreferences(const TemplateURLData& managed_data,
                                        bool enabled,
                                        TestingProfile* profile) {
  auto dict = TemplateURLDataToDictionary(managed_data);
  dict->SetBoolean(DefaultSearchManager::kDisabledByPolicy, !enabled);

  profile->GetTestingPrefService()->SetManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      std::move(dict));
}

void RemoveManagedDefaultSearchPreferences(TestingProfile* profile) {
  profile->GetTestingPrefService()->RemoveManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
}

TemplateURLServiceTestUtil::TemplateURLServiceTestUtil() {
  // Make unique temp directory.
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  profile_.reset(new TestingProfile(temp_dir_.GetPath()));

  scoped_refptr<WebDatabaseService> web_database_service =
      new WebDatabaseService(temp_dir_.GetPath().AppendASCII("webdata"),
                             base::ThreadTaskRunnerHandle::Get(),
                             base::ThreadTaskRunnerHandle::Get());
  web_database_service->AddTable(
      std::unique_ptr<WebDatabaseTable>(new KeywordTable()));
  web_database_service->LoadDatabase();

  web_data_service_ = new KeywordWebDataService(
      web_database_service.get(), base::ThreadTaskRunnerHandle::Get());
  web_data_service_->Init(base::NullCallback());

  ResetModel(false);
}

TemplateURLServiceTestUtil::~TemplateURLServiceTestUtil() {
  ClearModel();
  web_data_service_->ShutdownOnUISequence();
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
  model_.reset(new TemplateURLService(
      profile()->GetPrefs(),
      std::make_unique<TestingSearchTermsData>("http://www.google.com/"),
      web_data_service_.get(),
      std::unique_ptr<TemplateURLServiceClient>(
          new TestingTemplateURLServiceClient(
              HistoryServiceFactory::GetForProfileIfExists(
                  profile(), ServiceAccessType::EXPLICIT_ACCESS),
              &search_term_)),
      base::BindLambdaForTesting(
          [&] { ++dsp_set_to_google_callback_count_; })));
  model()->AddObserver(this);
  changed_count_ = 0;
  if (verify_load)
    VerifyLoad();
}

base::string16 TemplateURLServiceTestUtil::GetAndClearSearchTerm() {
  base::string16 search_term;
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
