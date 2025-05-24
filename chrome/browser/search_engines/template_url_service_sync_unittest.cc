// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data_resolver_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/search_engines/util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::Time;
using testing::AllOf;
using testing::Contains;
using testing::ElementsAre;
using testing::Eq;
using testing::Field;
using testing::IsEmpty;
using testing::IsNull;
using testing::Not;
using testing::NotNull;
using testing::Property;
using testing::ResultOf;

const char kOmniboxScheme[] = "omnibox";

// Extract the GUID from a search engine syncer::SyncData.
std::string GetGUID(const syncer::SyncData& sync_data) {
  return sync_data.GetSpecifics().search_engine().sync_guid();
}

// Extract the URL from a search engine syncer::SyncData.
std::string GetURL(const syncer::SyncData& sync_data) {
  return sync_data.GetSpecifics().search_engine().url();
}

// Extract the keyword from a search engine syncer::SyncData.
std::string GetKeyword(const syncer::SyncData& sync_data) {
  return sync_data.GetSpecifics().search_engine().keyword();
}

// Much like TemplateURLService::CreateSyncDataFromTemplateURL(), but allows the
// caller to override the keyword, URL, or GUID fields with empty strings, in
// order to create custom data that should be handled specially when synced to a
// client.
syncer::SyncData CreateCustomSyncData(const TemplateURL& turl,
                                      const std::u16string& keyword,
                                      const std::string& url,
                                      const std::string& sync_guid,
                                      int prepopulate_id = -1) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::SearchEngineSpecifics* se_specifics =
      specifics.mutable_search_engine();
  se_specifics->set_short_name(base::UTF16ToUTF8(turl.short_name()));
  se_specifics->set_keyword(base::UTF16ToUTF8(keyword));
  se_specifics->set_favicon_url(turl.favicon_url().spec());
  se_specifics->set_url(url);
  se_specifics->set_safe_for_autoreplace(turl.safe_for_autoreplace());
  se_specifics->set_originating_url(turl.originating_url().spec());
  se_specifics->set_date_created(turl.date_created().ToInternalValue());
  se_specifics->set_input_encodings(
      base::JoinString(turl.input_encodings(), ";"));
  se_specifics->set_suggestions_url(turl.suggestions_url());
  se_specifics->set_prepopulate_id(prepopulate_id == -1 ? turl.prepopulate_id()
                                                        : prepopulate_id);
  se_specifics->set_last_modified(turl.last_modified().ToInternalValue());
  se_specifics->set_sync_guid(sync_guid);
  return syncer::SyncData::CreateLocalData(turl.sync_guid(),  // Must be valid!
                                   se_specifics->keyword(), specifics);
}

// TestChangeProcessor --------------------------------------------------------

// Dummy SyncChangeProcessor used to help review what SyncChanges are pushed
// back up to Sync.
class TestChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  TestChangeProcessor();

  TestChangeProcessor(const TestChangeProcessor&) = delete;
  TestChangeProcessor& operator=(const TestChangeProcessor&) = delete;

  ~TestChangeProcessor() override;

  // Store a copy of all the changes passed in so we can examine them later.
  std::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  bool contains_guid(const std::string& guid) const {
    return change_map_.count(guid) != 0;
  }

  syncer::SyncChange change_for_guid(const std::string& guid) const {
    DCHECK(contains_guid(guid));
    return change_map_.find(guid)->second;
  }

  size_t change_list_size() { return change_map_.size(); }

  void set_erroneous(bool erroneous) { erroneous_ = erroneous; }

 private:
  // Track the changes received in ProcessSyncChanges.
  std::map<std::string, syncer::SyncChange> change_map_;
  bool erroneous_;
};

TestChangeProcessor::TestChangeProcessor() : erroneous_(false) {
}

TestChangeProcessor::~TestChangeProcessor() = default;

std::optional<syncer::ModelError> TestChangeProcessor::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (erroneous_)
    return syncer::ModelError(FROM_HERE, "Some error.");

  change_map_.erase(change_map_.begin(), change_map_.end());
  for (auto iter = change_list.begin(); iter != change_list.end(); ++iter)
    change_map_.emplace(GetGUID(iter->sync_data()), *iter);

  return std::nullopt;
}

class TestTemplateURLServiceClient : public TemplateURLServiceClient {
 public:
  ~TestTemplateURLServiceClient() override = default;

  void Shutdown() override {}
  void SetOwner(TemplateURLService* owner) override {}
  void DeleteAllSearchTermsForKeyword(TemplateURLID id) override {}
  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURLID id,
                                   const std::u16string& term) override {}
  void AddKeywordGeneratedVisit(const GURL& url) override {}
};

class KeywordsConsumer
    : public WebDataServiceConsumer,
      public base::test::TestFuture<std::vector<TemplateURLData>> {
 public:
  ~KeywordsConsumer() override = default;

  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override {
    CHECK_EQ(KEYWORDS_RESULT, result->GetType());
    SetValue(reinterpret_cast<const WDResult<WDKeywordsResult>*>(result.get())
                 ->GetValue()
                 .keywords);
  }
};

}  // namespace

// TemplateURLServiceSyncTest -------------------------------------------------
// TODO(crbug.com/40276119): Remove this test when the default search provider
// preference stops being synced.
class TemplateURLServiceSyncTest : public testing::Test {
 public:
  typedef TemplateURLService::SyncDataMap SyncDataMap;

  TemplateURLServiceSyncTest();

  TemplateURLServiceSyncTest(const TemplateURLServiceSyncTest&) = delete;
  TemplateURLServiceSyncTest& operator=(const TemplateURLServiceSyncTest&) =
      delete;

  void SetUp() override;
  void TearDown() override;

  TemplateURLService* model() { return test_util_a_->model(); }
  // For readability, we redefine an accessor for Model A for use in tests that
  // involve syncing two models.
  TemplateURLService* model_a() { return test_util_a_->model(); }
  TemplateURLService* model_b() { return test_util_b_->model(); }
  TestingProfile* profile_a() { return test_util_a_->profile(); }
  TestChangeProcessor* processor() { return sync_processor_.get(); }
  std::unique_ptr<syncer::SyncChangeProcessor> PassProcessor();

  // Verifies the two TemplateURLs are equal.
  // TODO(stevet): Share this with TemplateURLServiceTest.
  void AssertEquals(const TemplateURL& expected,
                    const TemplateURL& actual) const;

  // Expect that two syncer::SyncDataLists have equal contents, in terms of the
  // sync_guid, keyword, and url fields.
  void AssertEquals(const syncer::SyncDataList& data1,
                    const syncer::SyncDataList& data2) const;

  // Convenience helper for creating SyncChanges. Takes ownership of |turl|.
  syncer::SyncChange CreateTestSyncChange(
      syncer::SyncChange::SyncChangeType type,
      std::unique_ptr<TemplateURL> turl) const;

  // Helper that creates some initial sync data. We cheat a little by specifying
  // GUIDs for easy identification later. We also make the last_modified times
  // slightly older than CreateTestTemplateURL's default, to test conflict
  // resolution.
  syncer::SyncDataList CreateInitialSyncData(
      base::Time last_modified = base::Time::FromTimeT(90)) const;

  // Syntactic sugar.
  std::unique_ptr<TemplateURL> Deserialize(const syncer::SyncData& sync_data);

  // Creates a new TemplateURL copying the fields of |turl| but replacing
  // the |url| and |guid| and initializing the date_created and last_modified
  // timestamps to a default value of 100.
  std::unique_ptr<TemplateURL> CopyTemplateURL(const TemplateURLData* turl,
                                               const std::string& url,
                                               const std::string& guid);

  // Executes MergeDataAndStartSyncing and ProcessSyncChanges respectively, and
  // verifies the expected number of calls were made to notify observers. These
  // will clear out previous notify call counts beforehand.
  std::optional<syncer::ModelError> MergeAndExpectNotify(
      syncer::SyncDataList initial_sync_data,
      int expected_notify_count);
  std::optional<syncer::ModelError> MergeAndExpectNotifyAtLeast(
      syncer::SyncDataList initial_sync_data);
  std::optional<syncer::ModelError> ProcessAndExpectNotify(
      syncer::SyncChangeList changes,
      int expected_notify_count);
  std::optional<syncer::ModelError> ProcessAndExpectNotifyAtLeast(
      syncer::SyncChangeList changes);

 protected:
  content::BrowserTaskEnvironment task_environment_;

  // We have two `TestingPrefServiceSimple` to initialize two
  // `TemplateURLServiceTestUtil`.
  TestingPrefServiceSimple local_state_a_;
  TestingPrefServiceSimple local_state_b_;

  // We keep two TemplateURLServices to test syncing between them.
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_a_;
  std::unique_ptr<TemplateURLServiceTestUtil> test_util_b_;

  // Our dummy ChangeProcessor used to inspect changes pushed to Sync.
  std::unique_ptr<TestChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncChangeProcessorWrapperForTest>
      sync_processor_wrapper_;

  // List of features that we want to enable or disable in the test.
  base::test::ScopedFeatureList feature_list_;
};

TemplateURLServiceSyncTest::TemplateURLServiceSyncTest()
    : sync_processor_(new TestChangeProcessor),
      sync_processor_wrapper_(new syncer::SyncChangeProcessorWrapperForTest(
          sync_processor_.get())) {
  // We disable the search engine choice feature because, when enabled, the
  // default search provider pref is not synced. This test can be removed when
  // the feature flag is inlined."
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{switches::kSearchEngineChoiceTrigger});
}

void TemplateURLServiceSyncTest::SetUp() {
  DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(true);
  test_util_a_ = std::make_unique<TemplateURLServiceTestUtil>(local_state_a_);
  // Use ChangeToLoadState() instead of VerifyLoad() so we don't actually pull
  // in the prepopulate data, which the sync tests don't care about (and would
  // just foul them up).
  test_util_a_->ChangeModelToLoadState();
  test_util_a_->ResetObserverCount();

  test_util_b_ = std::make_unique<TemplateURLServiceTestUtil>(local_state_b_);
  test_util_b_->VerifyLoad();
}

void TemplateURLServiceSyncTest::TearDown() {
  test_util_a_.reset();
  DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(false);
}

std::unique_ptr<syncer::SyncChangeProcessor>
TemplateURLServiceSyncTest::PassProcessor() {
  return std::move(sync_processor_wrapper_);
}

void TemplateURLServiceSyncTest::AssertEquals(const TemplateURL& expected,
                                              const TemplateURL& actual) const {
  ASSERT_EQ(expected.short_name(), actual.short_name());
  ASSERT_EQ(expected.keyword(), actual.keyword());
  ASSERT_EQ(expected.url(), actual.url());
  ASSERT_EQ(expected.suggestions_url(), actual.suggestions_url());
  ASSERT_EQ(expected.favicon_url(), actual.favicon_url());
  ASSERT_EQ(expected.safe_for_autoreplace(), actual.safe_for_autoreplace());
  ASSERT_EQ(expected.input_encodings(), actual.input_encodings());
  ASSERT_EQ(expected.date_created(), actual.date_created());
  ASSERT_EQ(expected.last_modified(), actual.last_modified());
}

void TemplateURLServiceSyncTest::AssertEquals(
    const syncer::SyncDataList& data1,
    const syncer::SyncDataList& data2) const {
  SyncDataMap map1 = TemplateURLService::CreateGUIDToSyncDataMap(data1);
  SyncDataMap map2 = TemplateURLService::CreateGUIDToSyncDataMap(data2);

  for (auto iter1 = map1.cbegin(); iter1 != map1.cend(); ++iter1) {
    auto iter2 = map2.find(iter1->first);
    if (iter2 != map2.end()) {
      ASSERT_EQ(GetKeyword(iter1->second), GetKeyword(iter2->second));
      ASSERT_EQ(GetURL(iter1->second), GetURL(iter2->second));
      map2.erase(iter2);
    }
  }
  EXPECT_EQ(0U, map2.size());
}

syncer::SyncChange TemplateURLServiceSyncTest::CreateTestSyncChange(
    syncer::SyncChange::SyncChangeType type,
    std::unique_ptr<TemplateURL> turl) const {
  return syncer::SyncChange(
      FROM_HERE, type,
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data()));
}

syncer::SyncDataList TemplateURLServiceSyncTest::CreateInitialSyncData(
    base::Time last_modified) const {
  syncer::SyncDataList list;

  std::unique_ptr<TemplateURL> turl =
      CreateTestTemplateURL(u"key1", "http://key1.com", "guid1", last_modified);
  list.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data()));
  turl =
      CreateTestTemplateURL(u"key2", "http://key2.com", "guid2", last_modified);
  list.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data()));
  turl =
      CreateTestTemplateURL(u"key3", "http://key3.com", "guid3", last_modified);
  list.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data()));

  return list;
}

std::unique_ptr<TemplateURL> TemplateURLServiceSyncTest::Deserialize(
    const syncer::SyncData& sync_data) {
  syncer::SyncChangeList dummy;
  TestTemplateURLServiceClient client;
  return TemplateURLService::CreateTemplateURLFromTemplateURLAndSyncData(
      &client,
      CHECK_DEREF(TemplateURLPrepopulateData::ResolverFactory::GetForProfile(
          profile_a())),
      SearchTermsData(),
      /*existing_turl=*/nullptr, sync_data, &dummy);
}

std::unique_ptr<TemplateURL> TemplateURLServiceSyncTest::CopyTemplateURL(
    const TemplateURLData* turl,
    const std::string& url,
    const std::string& guid) {
  TemplateURLData data = *turl;
  data.SetURL(url);
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.sync_guid = guid;
  return std::make_unique<TemplateURL>(data);
}

std::optional<syncer::ModelError>
TemplateURLServiceSyncTest::MergeAndExpectNotify(
    syncer::SyncDataList initial_sync_data,
    int expected_notify_count) {
  test_util_a_->ResetObserverCount();
  std::optional<syncer::ModelError> error = model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_sync_data, PassProcessor());
  EXPECT_EQ(expected_notify_count, test_util_a_->GetObserverCount());
  return error;
}

std::optional<syncer::ModelError>
TemplateURLServiceSyncTest::MergeAndExpectNotifyAtLeast(
    syncer::SyncDataList initial_sync_data) {
  test_util_a_->ResetObserverCount();
  std::optional<syncer::ModelError> error = model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_sync_data, PassProcessor());
  EXPECT_LE(1, test_util_a_->GetObserverCount());
  return error;
}

std::optional<syncer::ModelError>
TemplateURLServiceSyncTest::ProcessAndExpectNotify(
    syncer::SyncChangeList changes,
    int expected_notify_count) {
  test_util_a_->ResetObserverCount();
  std::optional<syncer::ModelError> error =
      model()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_EQ(expected_notify_count, test_util_a_->GetObserverCount());
  return error;
}

std::optional<syncer::ModelError>
TemplateURLServiceSyncTest::ProcessAndExpectNotifyAtLeast(
    syncer::SyncChangeList changes) {
  test_util_a_->ResetObserverCount();
  std::optional<syncer::ModelError> error =
      model()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_LE(1, test_util_a_->GetObserverCount());
  return error;
}

// Actual tests ---------------------------------------------------------------

TEST_F(TemplateURLServiceSyncTest, SerializeDeserialize) {
  // Create a TemplateURL and convert it into a sync specific type.
  std::unique_ptr<TemplateURL> turl(
      CreateTestTemplateURL(u"unittest", "http://www.unittest.com/"));
  syncer::SyncData sync_data =
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data());
  // Convert the specifics back to a TemplateURL.
  std::unique_ptr<TemplateURL> deserialized(Deserialize(sync_data));
  EXPECT_TRUE(deserialized.get());
  // Ensure that the original and the deserialized TURLs are equal in values.
  AssertEquals(*turl, *deserialized);
}

TEST_F(TemplateURLServiceSyncTest, StartSyncEmpty) {
  ASSERT_TRUE(model()->GetAllSyncData(syncer::SEARCH_ENGINES).empty());
  MergeAndExpectNotify(syncer::SyncDataList(), 0);

  EXPECT_EQ(0U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_EQ(0U, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTest, MergeIntoEmpty) {
  ASSERT_TRUE(model()->GetAllSyncData(syncer::SEARCH_ENGINES).empty());
  syncer::SyncDataList initial_data = CreateInitialSyncData();
  MergeAndExpectNotify(initial_data, 1);

  EXPECT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  // We expect the model to have accepted all of the initial sync data. Search
  // through the model using the GUIDs to ensure that they're present.
  for (syncer::SyncDataList::const_iterator iter = initial_data.begin();
      iter != initial_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    EXPECT_TRUE(model()->GetTemplateURLForGUID(guid));
  }

  EXPECT_EQ(0U, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTest, ProcessChangesEmptyModel) {
  // We initially have no data.
  MergeAndExpectNotify({}, 0);

  // Set up a bunch of ADDs.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"key1", "http://key1.com", "guid1")));
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"key2", "http://key2.com", "guid2")));
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"key3", "http://key3.com", "guid3")));
  ProcessAndExpectNotify(changes, 1);

  EXPECT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_EQ(0U, processor()->change_list_size());
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid1"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid2"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid3"));
}

TEST_F(TemplateURLServiceSyncTest, ProcessChangesNoConflicts) {
  MergeAndExpectNotify(CreateInitialSyncData(), 1);

  // Process different types of changes, without conflicts.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"key4", "http://key4.com", "guid4")));
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(u"newkeyword", "http://new.com", "guid2")));
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_DELETE,
      CreateTestTemplateURL(u"key3", "http://key3.com", "guid3")));
  ProcessAndExpectNotify(changes, 1);

  // Add one, remove one, update one, so the number shouldn't change.
  EXPECT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_EQ(0U, processor()->change_list_size());
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid1"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid2"));
  const TemplateURL* turl = model()->GetTemplateURLForGUID("guid2");
  EXPECT_TRUE(turl);
  EXPECT_EQ(u"newkeyword", turl->keyword());
  EXPECT_EQ("http://new.com", turl->url());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("guid3"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid4"));
}

TEST_F(TemplateURLServiceSyncTest,
       ProcessChangesWithDuplicateKeywordsSyncWins) {
  MergeAndExpectNotify(CreateInitialSyncData(), 1);

  // Process different types of changes, with duplicate keywords. Note that all
  // this data has a newer timestamp, so Sync will win in these scenarios.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"key2", "http://new.com", "aaa")));
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(u"key3", "http://key3.com", "guid1")));
  ProcessAndExpectNotify(changes, 1);

  // Add one, update one, so we're up to 4.
  ASSERT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // aaa duplicates the keyword of guid2 and wins. guid2 still has its keyword,
  // but is shadowed by aaa.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("aaa"));
  EXPECT_EQ(model()->GetTemplateURLForGUID("aaa"),
            model()->GetTemplateURLForKeyword(u"key2"));
  TemplateURL* guid2_turl = model()->GetTemplateURLForGUID("guid2");
  ASSERT_TRUE(guid2_turl);
  ASSERT_EQ(u"key2", guid2_turl->keyword());
  // guid1 update duplicates the keyword of guid3 and wins. guid3 still has its
  // keyword but is shadowed by guid3 now.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid1"));
  EXPECT_EQ(model()->GetTemplateURLForGUID("guid1"),
            model()->GetTemplateURLForKeyword(u"key3"));
  TemplateURL* guid3_turl = model()->GetTemplateURLForGUID("guid3");
  ASSERT_TRUE(guid3_turl);
  EXPECT_EQ(u"key3", guid3_turl->keyword());

  // Sync is always newer here, so it should always win. But we DO NOT create
  // new sync updates in response to processing sync changes. That could cause
  // an infinite loop. Instead, on next startup, we will merge changes anyways.
  EXPECT_EQ(0U, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTest,
       ProcessChangesWithDuplicateKeywordsLocalWins) {
  MergeAndExpectNotify(CreateInitialSyncData(), 1);

  // Process different types of changes, with duplicate keywords. Note that all
  // this data has an older timestamp, so the local data will win in this case.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"key2", "http://new.com", "aaa",
                            base::Time::FromTimeT(10))));
  // Update the keyword of engine with GUID "guid1" to "key3", which will
  // duplicate the keyword of engine with GUID "guid3".
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(u"key3", "http://key3.com", "guid1",
                            base::Time::FromTimeT(10))));
  ProcessAndExpectNotify(changes, 1);

  // Add one, update one, so we're up to 4.
  ASSERT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // aaa duplicates the keyword of guid2 and loses. It still exists, and kept
  // its keyword as "key2", but it's NOT best TemplateURL for "key2".
  TemplateURL* aaa_turl = model()->GetTemplateURLForGUID("aaa");
  ASSERT_TRUE(aaa_turl);
  EXPECT_EQ(u"key2", aaa_turl->keyword());

  TemplateURL* guid2_turl = model()->GetTemplateURLForGUID("guid2");
  ASSERT_TRUE(guid2_turl);
  EXPECT_NE(aaa_turl, guid2_turl);
  EXPECT_EQ(guid2_turl, model()->GetTemplateURLForKeyword(u"key2"));

  // guid1 update duplicates the keyword of guid3 and loses. It updates its
  // keyword to "key3", but is NOT the best TemplateURL for "key3".
  TemplateURL* guid1_turl = model()->GetTemplateURLForGUID("guid1");
  ASSERT_TRUE(guid1_turl);
  EXPECT_EQ(u"key3", guid1_turl->keyword());
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid3"));
  EXPECT_EQ(model()->GetTemplateURLForGUID("guid3"),
            model()->GetTemplateURLForKeyword(u"key3"));

  // Local data wins twice, but we specifically DO NOT push updates to Sync
  // in response to processing sync updates. That can cause an infinite loop.
  EXPECT_EQ(0U, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTest, ProcessTemplateURLChange) {
  // Ensure that ProcessTemplateURLChange is called and pushes the correct
  // changes to Sync whenever local changes are made to TemplateURLs.
  MergeAndExpectNotify(CreateInitialSyncData(), 1);

  // Add a new search engine.
  model()->Add(CreateTestTemplateURL(u"baidu", "http://baidu.cn", "new"));
  EXPECT_EQ(1U, processor()->change_list_size());
  ASSERT_TRUE(processor()->contains_guid("new"));
  syncer::SyncChange change = processor()->change_for_guid("new");
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change.change_type());
  EXPECT_EQ("baidu", GetKeyword(change.sync_data()));
  EXPECT_EQ("http://baidu.cn", GetURL(change.sync_data()));

  // Change a keyword.
  TemplateURL* existing_turl = model()->GetTemplateURLForGUID("guid1");
  model()->ResetTemplateURL(existing_turl, existing_turl->short_name(), u"k",
                            existing_turl->url());
  EXPECT_EQ(1U, processor()->change_list_size());
  ASSERT_TRUE(processor()->contains_guid("guid1"));
  change = processor()->change_for_guid("guid1");
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change.change_type());
  EXPECT_EQ("k", GetKeyword(change.sync_data()));

  // Remove an existing search engine.
  existing_turl = model()->GetTemplateURLForGUID("guid2");
  model()->Remove(existing_turl);
  EXPECT_EQ(1U, processor()->change_list_size());
  ASSERT_TRUE(processor()->contains_guid("guid2"));
  change = processor()->change_for_guid("guid2");
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, change.change_type());
}

TEST_F(TemplateURLServiceSyncTest, ProcessChangesWithLocalExtensions) {
  MergeAndExpectNotify(CreateInitialSyncData(), 1);

  // Add some extension keywords locally.
  model()->RegisterExtensionControlledTURL("extension1", "unittest", "keyword1",
                                           "http://extension1", Time(), false);
  TemplateURL* extension1 = model()->GetTemplateURLForKeyword(u"keyword1");
  ASSERT_TRUE(extension1);
  EXPECT_EQ(0U, processor()->change_list_size());

  model()->RegisterExtensionControlledTURL("extension2", "unittest", "keyword2",
                                           "http://extension2", Time(), false);
  TemplateURL* extension2 = model()->GetTemplateURLForKeyword(u"keyword2");
  ASSERT_TRUE(extension2);
  EXPECT_EQ(0U, processor()->change_list_size());

  // Create some sync changes that will conflict with the extension keywords.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"keyword1", "http://aaa.com", std::string(),
                            base::Time::FromTimeT(100), true,
                            TemplateURLData::PolicyOrigin::kNoPolicy, 0)));
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"keyword2", "http://bbb.com")));
  ProcessAndExpectNotify(changes, 1);

  // Because aaa.com was marked as replaceable, it was removed in favor of the
  // extension engine.
  EXPECT_FALSE(model()->GetTemplateURLForHost("aaa.com"));
  // But bbb.com was marked as non-replaceable, so it coexists with extension2.
  EXPECT_TRUE(model()->GetTemplateURLForHost("bbb.com"));

  // The extensions should continue to take precedence over the normal
  // user-created engines.
  EXPECT_EQ(extension1, model()->GetTemplateURLForKeyword(u"keyword1"));
  EXPECT_EQ(extension2, model()->GetTemplateURLForKeyword(u"keyword2"));
}

TEST_F(TemplateURLServiceSyncTest, DuplicateEncodingsRemoved) {
  // Create a sync entry with duplicate encodings.
  syncer::SyncDataList initial_data;

  TemplateURLData data;
  data.SetShortName(u"test");
  data.SetKeyword(u"keyword");
  data.SetURL("http://test/%s");
  data.input_encodings.push_back("UTF-8");
  data.input_encodings.push_back("UTF-8");
  data.input_encodings.push_back("UTF-16");
  data.input_encodings.push_back("UTF-8");
  data.input_encodings.push_back("Big5");
  data.input_encodings.push_back("UTF-16");
  data.input_encodings.push_back("Big5");
  data.input_encodings.push_back("Windows-1252");
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.sync_guid = "keyword";
  std::unique_ptr<TemplateURL> turl(new TemplateURL(data));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data()));

  // Now try to sync the data locally.
  MergeAndExpectNotify(initial_data, 1);

  // The entry should have been added, with duplicate encodings removed.
  TemplateURL* keyword = model()->GetTemplateURLForKeyword(u"keyword");
  ASSERT_FALSE(keyword == nullptr);
  EXPECT_EQ(4U, keyword->input_encodings().size());

  // We should also have gotten a corresponding UPDATE pushed upstream.
  EXPECT_GE(processor()->change_list_size(), 1U);
  ASSERT_TRUE(processor()->contains_guid("keyword"));
  syncer::SyncChange keyword_change = processor()->change_for_guid("keyword");
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, keyword_change.change_type());
  EXPECT_EQ("UTF-8;UTF-16;Big5;Windows-1252", keyword_change.sync_data().
      GetSpecifics().search_engine().input_encodings());
}

TEST_F(TemplateURLServiceSyncTest, MergeTwoClientsBasic) {
  // Start off B with some empty data.
  model_b()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                      CreateInitialSyncData(), PassProcessor());

  // Merge A and B. All of B's data should transfer over to A, which initially
  // has no data.
  std::unique_ptr<syncer::SyncChangeProcessorWrapperForTest> delegate_b(
      new syncer::SyncChangeProcessorWrapperForTest(model_b()));
  model_a()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, model_b()->GetAllSyncData(syncer::SEARCH_ENGINES),
      std::move(delegate_b));

  // They should be consistent.
  AssertEquals(model_a()->GetAllSyncData(syncer::SEARCH_ENGINES),
               model_b()->GetAllSyncData(syncer::SEARCH_ENGINES));
}

TEST_F(TemplateURLServiceSyncTest, MergeTwoClientsDupesAndConflicts) {
  // Start off B with some empty data.
  model_b()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                      CreateInitialSyncData(), PassProcessor());

  // Set up A so we have some interesting duplicates and conflicts.
  model_a()->Add(CreateTestTemplateURL(u"key4", "http://key4.com",
                                       "guid4"));  // Added
  model_a()->Add(CreateTestTemplateURL(u"key2", "http://key2.com",
                                       "guid2"));  // Merge - Copy of guid2.
  model_a()->Add(CreateTestTemplateURL(
      u"key3", "http://key3.com", "guid5",
      base::Time::FromTimeT(10)));  // Merge - Dupe of guid3.
  model_a()->Add(
      CreateTestTemplateURL(u"key1", "http://key6.com", "guid6",
                            base::Time::FromTimeT(10)));  // Conflict with guid1

  // Merge A and B.
  std::unique_ptr<syncer::SyncChangeProcessorWrapperForTest> delegate_b(
      new syncer::SyncChangeProcessorWrapperForTest(model_b()));
  model_a()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, model_b()->GetAllSyncData(syncer::SEARCH_ENGINES),
      std::move(delegate_b));

  // They should be consistent.
  AssertEquals(model_a()->GetAllSyncData(syncer::SEARCH_ENGINES),
               model_b()->GetAllSyncData(syncer::SEARCH_ENGINES));
}

TEST_F(TemplateURLServiceSyncTest, StopSyncing) {
  std::optional<syncer::ModelError> merge_error =
      MergeAndExpectNotify(CreateInitialSyncData(), 1);
  ASSERT_FALSE(merge_error.has_value());
  model()->StopSyncing(syncer::SEARCH_ENGINES);

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(u"newkeyword", "http://new.com", "guid2")));
  // Because the sync data is never applied locally, there should not be any
  // notification.
  std::optional<syncer::ModelError> process_error =
      ProcessAndExpectNotify(changes, 0);
  EXPECT_TRUE(process_error.has_value());

  // Ensure that the sync changes were not accepted.
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"newkeyword"));
}

TEST_F(TemplateURLServiceSyncTest, SyncErrorOnInitialSync) {
  processor()->set_erroneous(true);
  // Error happens after local changes are applied, still expect a notify.
  std::optional<syncer::ModelError> merge_error =
      MergeAndExpectNotify(CreateInitialSyncData(), 1);
  EXPECT_TRUE(merge_error.has_value());

  // Ensure that if the initial merge was erroneous, then subsequence attempts
  // to push data into the local model are rejected, since the model was never
  // successfully associated with Sync in the first place.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(u"newkeyword", "http://new.com", "guid2")));
  processor()->set_erroneous(false);
  std::optional<syncer::ModelError> process_error =
      ProcessAndExpectNotify(changes, 0);
  EXPECT_TRUE(process_error.has_value());

  // Ensure that the sync changes were not accepted.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid2"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"newkeyword"));
}

TEST_F(TemplateURLServiceSyncTest, SyncErrorOnLaterSync) {
  // Ensure that if the SyncProcessor succeeds in the initial merge, but fails
  // in future ProcessSyncChanges, we still return an error.
  std::optional<syncer::ModelError> merge_error =
      MergeAndExpectNotify(CreateInitialSyncData(), 1);
  ASSERT_FALSE(merge_error.has_value());

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(u"newkeyword", "http://new.com", "guid2")));
  processor()->set_erroneous(true);
  // Because changes make it to local before the error, still need to notify.
  std::optional<syncer::ModelError> process_error =
      ProcessAndExpectNotify(changes, 1);
  EXPECT_TRUE(process_error.has_value());
}

TEST_F(TemplateURLServiceSyncTest, SyncedDefaultGUIDArrivesFirst) {
  syncer::SyncDataList initial_data = CreateInitialSyncData();
  // The default search provider should support replacement.
  std::unique_ptr<TemplateURL> turl(
      CreateTestTemplateURL(u"key2", "http://key2.com/{searchTerms}", "guid2",
                            base::Time::FromTimeT(90)));
  initial_data[1] =
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data());
  MergeAndExpectNotify(initial_data, 1);
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID("guid2"));

  EXPECT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);

  // Change kSyncedDefaultSearchProviderGUID to a GUID that does not exist in
  // the model yet. Ensure that the default has not changed in any way.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, "newdefault");

  ASSERT_EQ(default_search, model()->GetDefaultSearchProvider());

  // Bring in a random new search engine with a different GUID. Ensure that
  // it doesn't change the default.
  syncer::SyncChangeList changes1;
  changes1.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"random", "http://random.com", "random")));
  test_util_a_->ResetObserverCount();
  ProcessAndExpectNotify(changes1, 1);

  EXPECT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  ASSERT_EQ(default_search, model()->GetDefaultSearchProvider());

  // Finally, bring in the expected entry with the right GUID. Ensure that
  // the default has changed to the new search engine.
  syncer::SyncChangeList changes2;
  changes2.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"new", "http://new.com/{searchTerms}",
                            "newdefault")));
  // When the default changes, a second notify is triggered.
  ProcessAndExpectNotifyAtLeast(changes2);

  EXPECT_EQ(5U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  ASSERT_NE(default_search, model()->GetDefaultSearchProvider());
  ASSERT_EQ("newdefault", model()->GetDefaultSearchProvider()->sync_guid());
}

TEST_F(TemplateURLServiceSyncTest, DefaultGuidDeletedBeforeNewDSPArrives) {
  syncer::SyncDataList initial_data;
  // The default search provider should support replacement.
  std::unique_ptr<TemplateURL> turl1 =
      CreateTestTemplateURL(u"key1", "http://key1.com/{searchTerms}", "guid1",
                            base::Time::FromTimeT(90));
  // Create a second default search provider for the
  // FindNewDefaultSearchProvider method to find.
  TemplateURLData data;
  data.SetShortName(u"unittest");
  data.SetKeyword(u"key2");
  data.SetURL("http://key2.com/{searchTerms}");
  data.favicon_url = GURL("http://favicon.url");
  data.safe_for_autoreplace = false;
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.policy_origin = TemplateURLData::PolicyOrigin::kNoPolicy;
  data.prepopulate_id = 999999;
  data.sync_guid = "guid2";
  std::unique_ptr<TemplateURL> turl2(new TemplateURL(data));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl1->data()));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl2->data()));
  MergeAndExpectNotify(initial_data, 1);
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID("guid1"));
  ASSERT_EQ("guid1", model()->GetDefaultSearchProvider()->sync_guid());

  EXPECT_EQ(2U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);

  // Change kSyncedDefaultSearchProviderGUID to a GUID that does not exist in
  // the model yet. Ensure that the default has not changed in any way.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, "newdefault");

  ASSERT_EQ("guid1", model()->GetDefaultSearchProvider()->sync_guid());
  EXPECT_EQ("newdefault", GetDefaultSearchProviderGuidFromPrefs(
                              *profile_a()->GetTestingPrefService()));

  // Simulate a situation where an ACTION_DELETE on the default arrives before
  // the new default search provider entry. This should fail to delete the
  // target entry. The synced default GUID should not be changed so that when
  // the expected default entry arrives, it can still be set as the default.
  syncer::SyncChangeList changes1;
  changes1.push_back(CreateTestSyncChange(syncer::SyncChange::ACTION_DELETE,
                                          std::move(turl1)));
  ProcessAndExpectNotify(changes1, 0);

  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"key1"));
  EXPECT_EQ(2U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_EQ("guid1", model()->GetDefaultSearchProvider()->sync_guid());
  EXPECT_EQ("newdefault", GetDefaultSearchProviderGuidFromPrefs(
                              *profile_a()->GetTestingPrefService()));

  // Finally, bring in the expected entry with the right GUID. Ensure that
  // the default has changed to the new search engine.
  syncer::SyncChangeList changes2;
  changes2.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"new", "http://new.com/{searchTerms}",
                            "newdefault")));

  // When the default changes, a second notify is triggered and the previous
  // default search engine has been deleted.
  ProcessAndExpectNotifyAtLeast(changes2);

  EXPECT_EQ(2U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_EQ("newdefault", model()->GetDefaultSearchProvider()->sync_guid());
  EXPECT_EQ("newdefault", GetDefaultSearchProviderGuidFromPrefs(
                              *profile_a()->GetTestingPrefService()));
  EXPECT_THAT(model()->GetTemplateURLForGUID("guid1"), IsNull());
}

TEST_F(TemplateURLServiceSyncTest,
       DefaultGuidDeletedAndUpdatedBeforeNewDSPArrives) {
  syncer::SyncDataList initial_data;
  // The default search provider should support replacement.
  std::unique_ptr<TemplateURL> turl1 =
      CreateTestTemplateURL(u"key1", "http://key1.com/{searchTerms}", "guid1",
                            base::Time::FromTimeT(90));
  // Create a second default search provider for the
  // FindNewDefaultSearchProvider method to find.
  TemplateURLData data;
  data.SetShortName(u"unittest");
  data.SetKeyword(u"key2");
  data.SetURL("http://key2.com/{searchTerms}");
  data.favicon_url = GURL("http://favicon.url");
  data.safe_for_autoreplace = false;
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.policy_origin = TemplateURLData::PolicyOrigin::kNoPolicy;
  data.prepopulate_id = 999999;
  data.sync_guid = "guid2";
  std::unique_ptr<TemplateURL> turl2(new TemplateURL(data));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl1->data()));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl2->data()));
  MergeAndExpectNotify(initial_data, 1);
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID("guid1"));
  ASSERT_EQ("guid1", model()->GetDefaultSearchProvider()->sync_guid());

  ASSERT_EQ(2U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_THAT(default_search, NotNull());

  // Change kSyncedDefaultSearchProviderGUID to a GUID that does not exist in
  // the model yet. Ensure that the default has not changed in any way.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_THAT(prefs, NotNull());
  SetDefaultSearchProviderGuidToPrefs(*prefs, "newdefault");

  ASSERT_EQ("guid1", model()->GetDefaultSearchProvider()->sync_guid());
  ASSERT_EQ("newdefault", GetDefaultSearchProviderGuidFromPrefs(
                              *profile_a()->GetTestingPrefService()));

  // Simulate a situation where an ACTION_DELETE on the default arrives before
  // the new default search provider entry. This should fail to delete the
  // target entry. The synced default GUID should not be changed so that when
  // the expected default entry arrives, it can still be set as the default.
  syncer::SyncChangeList changes1;
  changes1.push_back(CreateTestSyncChange(syncer::SyncChange::ACTION_DELETE,
                                          std::move(turl1)));
  ProcessAndExpectNotify(changes1, 0);

  ASSERT_TRUE(model()->GetTemplateURLForKeyword(u"key1"));
  ASSERT_EQ(2U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  ASSERT_EQ("guid1", model()->GetDefaultSearchProvider()->sync_guid());
  ASSERT_EQ("newdefault", GetDefaultSearchProviderGuidFromPrefs(
                              *profile_a()->GetTestingPrefService()));

  // Update the default search engine before a new search engine arrives.
  TemplateURL* existing_turl = model()->GetTemplateURLForGUID("guid1");
  ASSERT_EQ(existing_turl, default_search);
  model()->ResetTemplateURL(existing_turl, existing_turl->short_name(),
                            /*keyword=*/u"k", existing_turl->url());

  // Finally, bring in the expected entry with the right GUID. Ensure that
  // the default has changed to the new search engine.
  syncer::SyncChangeList changes2;
  changes2.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"new", "http://new.com/{searchTerms}",
                            "newdefault")));

  // When the default changes, a second notify is triggered and the previous
  // default search engine should be kept.
  ProcessAndExpectNotifyAtLeast(changes2);

  EXPECT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_EQ("newdefault", model()->GetDefaultSearchProvider()->sync_guid());
  EXPECT_EQ("newdefault", GetDefaultSearchProviderGuidFromPrefs(
                              *profile_a()->GetTestingPrefService()));
  EXPECT_THAT(model()->GetTemplateURLForGUID("guid1"), NotNull());
}

TEST_F(TemplateURLServiceSyncTest, SyncedDefaultArrivesAfterStartup) {
  // Start with the default set to something in the model before we start
  // syncing.
  model()->Add(CreateTestTemplateURL(
      u"what", "http://thewhat.com/{searchTerms}", "initdefault"));
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID("initdefault"));

  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);

  // Set kSyncedDefaultSearchProviderGUID to something that is not yet in
  // the model but is expected in the initial sync. Ensure that this doesn't
  // change our default since we're not quite syncing yet.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, "guid2");

  EXPECT_EQ(default_search, model()->GetDefaultSearchProvider());

  // Now sync the initial data, which will include the search engine entry
  // destined to become the new default.
  syncer::SyncDataList initial_data = CreateInitialSyncData();
  // The default search provider should support replacement.
  std::unique_ptr<TemplateURL> turl(
      CreateTestTemplateURL(u"key2", "http://key2.com/{searchTerms}", "guid2",
                            base::Time::FromTimeT(90)));
  initial_data[1] =
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data());

  // When the default changes, a second notify is triggered.
  MergeAndExpectNotifyAtLeast(initial_data);

  // Ensure that the new default has been set.
  ASSERT_NE(default_search, model()->GetDefaultSearchProvider());
  ASSERT_EQ("guid2", model()->GetDefaultSearchProvider()->sync_guid());
}

TEST_F(TemplateURLServiceSyncTest, SyncedDefaultAlreadySetOnStartup) {
  // Start with the default set to something in the model before we start
  // syncing.
  const char kGUID[] = "initdefault";
  model()->Add(CreateTestTemplateURL(
      u"what", "http://thewhat.com/{searchTerms}", kGUID));
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID(kGUID));

  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);

  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  // Set kSyncedDefaultSearchProviderGUID to the current default.
  SetDefaultSearchProviderGuidToPrefs(*prefs, kGUID);

  EXPECT_EQ(default_search, model()->GetDefaultSearchProvider());

  // Now sync the initial data.
  MergeAndExpectNotify(CreateInitialSyncData(), 1);

  // Ensure that the new entries were added and the default has not changed.
  ASSERT_EQ(default_search, model()->GetDefaultSearchProvider());
  ASSERT_EQ(kGUID, model()->GetDefaultSearchProvider()->sync_guid());
}

TEST_F(TemplateURLServiceSyncTest, SyncWithManagedDefaultSearch) {
  // First start off with a few entries and make sure we can set an unmanaged
  // default search provider.
  MergeAndExpectNotify(CreateInitialSyncData(), 1);
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID("guid2"));

  EXPECT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  ASSERT_FALSE(model()->is_default_search_managed());
  ASSERT_TRUE(model()->GetDefaultSearchProvider());

  // Change the default search provider to a managed one.
  TemplateURLData managed;
  managed.SetShortName(u"manageddefault");
  managed.SetKeyword(u"manageddefault");
  managed.SetURL("http://manageddefault.com/search?t={searchTerms}");
  managed.favicon_url = GURL("http://manageddefault.com/icon.jpg");
  managed.input_encodings = {"UTF-16", "UTF-32"};
  managed.alternate_urls = {"http://manageddefault.com/search#t={searchTerms}"};

  SetManagedDefaultSearchPreferences(managed, true, test_util_a_->profile());
  const TemplateURL* dsp_turl = model()->GetDefaultSearchProvider();

  EXPECT_TRUE(model()->is_default_search_managed());

  // Add a new entry from Sync. It should still sync in despite the default
  // being managed.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"newkeyword", "http://new.com/{searchTerms}",
                            "newdefault")));
  ProcessAndExpectNotify(changes, 1);

  EXPECT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // Change kSyncedDefaultSearchProviderGUID to point to the new entry and
  // ensure that the DSP remains managed.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, "newdefault");

  EXPECT_EQ(dsp_turl, model()->GetDefaultSearchProvider());
  EXPECT_TRUE(model()->is_default_search_managed());

  // Go unmanaged. Ensure that the DSP changes to the expected pending entry
  // from Sync.
  const TemplateURL* expected_default =
      model()->GetTemplateURLForGUID("newdefault");
  RemoveManagedDefaultSearchPreferences(test_util_a_->profile());

  EXPECT_EQ(expected_default, model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceSyncTest, OverrideSyncPrefWithExtensionDefaultSearch) {
  // Add third-party default search engine.
  TemplateURL* user_dse = model()->Add(CreateTestTemplateURL(
      u"some_keyword", "http://new.com/{searchTerms}", "guid"));
  ASSERT_TRUE(user_dse);
  model()->SetUserSelectedDefaultSearchProvider(user_dse);
  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());

  // Change the default search provider to an extension one.
  std::unique_ptr<TemplateURLData> extension =
      GenerateDummyTemplateURLData("extensiondefault");
  const TemplateURL* ext_dse =
      test_util_a_->AddExtensionControlledTURL(std::make_unique<TemplateURL>(
          *extension, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, "ext_id",
          Time(), true));
  EXPECT_EQ(ext_dse, model()->GetDefaultSearchProvider());

  // Update the custom search engine that was default but now is hidden by
  // |ext_dse|.
  model()->ResetTemplateURL(user_dse, u"New search engine", u"new_keyword",
                            "http://new.com/{searchTerms}");

  // Change kSyncedDefaultSearchProviderGUID to point to an nonexisting entry.
  // It can happen when the prefs are synced but the search engines are not.
  // That step is importnt because otherwise RemoveExtensionControlledTURL below
  // will not overwrite the GUID and won't trigger a recursion call.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, "remote_default_guid");

  // The search engine is still the same.
  EXPECT_EQ(ext_dse, model()->GetDefaultSearchProvider());

  // Remove extension DSE. Ensure that the DSP changes to the existing search
  // engine. It should not cause a crash.
  test_util_a_->RemoveExtensionControlledTURL("ext_id");

  EXPECT_EQ(user_dse, model()->GetDefaultSearchProvider());
}

// Check that keyword conflict between synced engine and extension engine is
// resolved correctly.
TEST_F(TemplateURLServiceSyncTest, ExtensionAndNormalEngineConflict) {
  // Start with empty model.
  MergeAndExpectNotify({}, 0);
  const std::u16string kCommonKeyword = u"common_keyword";
  // Change the default search provider to an extension one.
  std::unique_ptr<TemplateURLData> extension =
      GenerateDummyTemplateURLData("common_keyword");
  auto ext_dse = std::make_unique<TemplateURL>(
      *extension, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, "ext", Time(),
      true);
  const TemplateURL* extension_turl =
      test_util_a_->AddExtensionControlledTURL(std::move(ext_dse));
  EXPECT_TRUE(model()->IsExtensionControlledDefaultSearch());
  EXPECT_EQ(extension_turl, model()->GetTemplateURLForKeyword(kCommonKeyword));

  // Add through sync normal engine with the same keyword as extension.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(kCommonKeyword, "http://normal.com", "normal_guid",
                            base::Time::FromTimeT(10))));
  ProcessAndExpectNotify(changes, 1);

  // Expect new engine synced in and kept keyword.
  const TemplateURL* normal_turl =
      model()->GetTemplateURLForGUID("normal_guid");
  ASSERT_TRUE(normal_turl);
  EXPECT_EQ(kCommonKeyword, normal_turl->keyword());
  EXPECT_EQ(TemplateURL::NORMAL, normal_turl->type());

  // Check that extension engine remains default and is accessible by keyword.
  EXPECT_TRUE(model()->IsExtensionControlledDefaultSearch());
  EXPECT_EQ(extension_turl, model()->GetTemplateURLForKeyword(kCommonKeyword));

  // Update through sync normal engine changing keyword to nonconflicting value.
  changes.clear();
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(u"nonconflicting_keyword", "http://normal.com",
                            "normal_guid", base::Time::FromTimeT(11))));
  ProcessAndExpectNotify(changes, 1);
  normal_turl = model()->GetTemplateURLForGUID("normal_guid");
  ASSERT_TRUE(normal_turl);
  EXPECT_EQ(u"nonconflicting_keyword", normal_turl->keyword());
  // Check that extension engine remains default and is accessible by keyword.
  EXPECT_TRUE(model()->IsExtensionControlledDefaultSearch());
  EXPECT_EQ(extension_turl, model()->GetTemplateURLForKeyword(kCommonKeyword));

  // Update through sync normal engine changing keyword back to conflicting
  // value.
  changes.clear();
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(kCommonKeyword, "http://normal.com", "normal_guid",
                            base::Time::FromTimeT(12))));
  ProcessAndExpectNotify(changes, 1);
  normal_turl = model()->GetTemplateURLForGUID("normal_guid");
  ASSERT_TRUE(normal_turl);
  EXPECT_EQ(kCommonKeyword, normal_turl->keyword());

  // Check extension engine still remains default.
  EXPECT_TRUE(model()->IsExtensionControlledDefaultSearch());
  EXPECT_EQ(extension_turl, model()->GetTemplateURLForKeyword(kCommonKeyword));

  // Remove extension engine and expect that normal engine can be acessed by
  // keyword.
  test_util_a_->RemoveExtensionControlledTURL("ext");
  EXPECT_EQ(model()->GetTemplateURLForGUID("normal_guid"),
            model()->GetTemplateURLForKeyword(kCommonKeyword));
}

TEST_F(TemplateURLServiceSyncTest, DeleteBogusData) {
  // Create a couple of bogus entries to sync.
  syncer::SyncDataList initial_data;
  std::unique_ptr<TemplateURL> turl =
      CreateTestTemplateURL(u"key1", "http://key1.com", "guid1");
  initial_data.push_back(CreateCustomSyncData(
      *turl, turl->keyword(), std::string(), turl->sync_guid()));
  turl = CreateTestTemplateURL(u"key2", "http://key2.com");
  initial_data.push_back(
      CreateCustomSyncData(*turl, turl->keyword(), turl->url(), std::string()));
  turl = CreateTestTemplateURL(u"key3", "http://key3.com", "guid3");
  initial_data.push_back(CreateCustomSyncData(*turl, std::u16string(),
                                              turl->url(), turl->sync_guid()));

  // Now try to sync the data locally.
  MergeAndExpectNotify(initial_data, 0);

  // Nothing should have been added, and all bogus entries should be marked for
  // deletion.
  EXPECT_EQ(0U, model()->GetTemplateURLs().size());
  EXPECT_EQ(3U, processor()->change_list_size());
  ASSERT_TRUE(processor()->contains_guid("guid1"));
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            processor()->change_for_guid("guid1").change_type());
  ASSERT_TRUE(processor()->contains_guid(std::string()));
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            processor()->change_for_guid(std::string()).change_type());
  ASSERT_TRUE(processor()->contains_guid("guid3"));
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE,
            processor()->change_for_guid("guid3").change_type());
}

TEST_F(TemplateURLServiceSyncTest, SyncBaseURLs) {
  // Verify that bringing in a remote TemplateURL that uses Google base URLs
  // causes it to get a local keyword that matches the local base URL.
  syncer::SyncDataList initial_data;
  std::unique_ptr<TemplateURL> turl(CreateTestTemplateURL(
      u"google.co.uk", "{google:baseURL}search?q={searchTerms}", "guid"));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());
  TemplateURL* synced_turl = model()->GetTemplateURLForGUID("guid");
  ASSERT_TRUE(synced_turl);
  EXPECT_EQ(u"google.com", synced_turl->keyword());
  EXPECT_EQ(0U, processor()->change_list_size());

  // Remote updates to this URL's keyword should be silently ignored.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(u"google.de",
                            "{google:baseURL}search?q={searchTerms}", "guid")));
  ProcessAndExpectNotify(changes, 1);
  EXPECT_EQ(u"google.com", synced_turl->keyword());
  EXPECT_EQ(0U, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTest, MergePrepopulatedEngine) {
  std::unique_ptr<TemplateURLData> default_turl(
      TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
          ->GetFallbackSearch());

  // Merge with an initial list containing a prepopulated engine with a wrong
  // URL.
  syncer::SyncDataList list;
  std::unique_ptr<TemplateURL> sync_turl = CopyTemplateURL(
      default_turl.get(), "http://wrong.url.com?q={searchTerms}", "default");
  list.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(sync_turl->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, list,
                                    PassProcessor());

  const TemplateURL* result_turl = model()->GetTemplateURLForGUID("default");
  EXPECT_TRUE(result_turl);
  EXPECT_EQ(default_turl->keyword(), result_turl->keyword());
  EXPECT_EQ(default_turl->short_name(), result_turl->short_name());
  EXPECT_EQ(default_turl->url(), result_turl->url());
}

TEST_F(TemplateURLServiceSyncTest, AddPrepopulatedEngine) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList(), PassProcessor());

  std::unique_ptr<TemplateURLData> default_turl(
      TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
          ->GetFallbackSearch());
  std::unique_ptr<TemplateURL> sync_turl = CopyTemplateURL(
      default_turl.get(), "http://wrong.url.com?q={searchTerms}", "default");

  // Add a prepopulated engine with a wrong URL.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(syncer::SyncChange::ACTION_ADD,
                                         std::move(sync_turl)));
  ProcessAndExpectNotify(changes, 1);

  const TemplateURL* result_turl = model()->GetTemplateURLForGUID("default");
  EXPECT_TRUE(result_turl);
  EXPECT_EQ(default_turl->keyword(), result_turl->keyword());
  EXPECT_EQ(default_turl->short_name(), result_turl->short_name());
  EXPECT_EQ(default_turl->url(), result_turl->url());
}

TEST_F(TemplateURLServiceSyncTest, UpdatePrepopulatedEngine) {
  std::unique_ptr<TemplateURLData> default_turl(
      TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
          ->GetFallbackSearch());

  TemplateURLData data = *default_turl;
  data.SetURL("http://old.wrong.url.com?q={searchTerms}");
  data.sync_guid = "default";
  model()->Add(std::make_unique<TemplateURL>(data));

  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList(), PassProcessor());

  std::unique_ptr<TemplateURL> sync_turl =
      CopyTemplateURL(default_turl.get(),
                      "http://new.wrong.url.com?q={searchTerms}", "default");

  // Update the engine in the model, which is prepopulated, with a new one.
  // Both have wrong URLs, but it should still get corrected.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(syncer::SyncChange::ACTION_UPDATE,
                                         std::move(sync_turl)));
  ProcessAndExpectNotify(changes, 1);

  const TemplateURL* result_turl = model()->GetTemplateURLForGUID("default");
  EXPECT_TRUE(result_turl);
  EXPECT_EQ(default_turl->keyword(), result_turl->keyword());
  EXPECT_EQ(default_turl->short_name(), result_turl->short_name());
  EXPECT_EQ(default_turl->url(), result_turl->url());
}

TEST_F(TemplateURLServiceSyncTest, MergeEditedPrepopulatedEngine) {
  std::unique_ptr<TemplateURLData> default_turl(
      TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
          ->GetFallbackSearch());

  TemplateURLData data(*default_turl);
  data.safe_for_autoreplace = false;
  data.SetKeyword(u"new_kw");
  data.SetShortName(u"my name");
  data.SetURL("http://wrong.url.com?q={searchTerms}");
  data.date_created = Time::FromTimeT(50);
  data.last_modified = Time::FromTimeT(50);
  data.sync_guid = "default";
  model()->Add(std::make_unique<TemplateURL>(data));

  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  std::unique_ptr<TemplateURL> sync_turl(new TemplateURL(data));
  syncer::SyncDataList list;
  list.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(sync_turl->data()));
  MergeAndExpectNotify(list, 1);

  const TemplateURL* result_turl = model()->GetTemplateURLForGUID("default");
  EXPECT_TRUE(result_turl);
  EXPECT_EQ(u"new_kw", result_turl->keyword());
  EXPECT_EQ(u"my name", result_turl->short_name());
  EXPECT_EQ(default_turl->url(), result_turl->url());
}

TEST_F(TemplateURLServiceSyncTest, MergeConflictingPrepopulatedEngine) {
  std::unique_ptr<TemplateURLData> default_turl(
      TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
          ->GetFallbackSearch());

  TemplateURLData data(*default_turl);
  data.SetKeyword(u"old_kw");
  data.SetShortName(u"my name");
  data.SetURL("http://wrong.url.com?q={searchTerms}");
  data.safe_for_autoreplace = true;
  data.date_created = Time::FromTimeT(50);
  data.last_modified = Time::FromTimeT(50);
  data.prepopulate_id = 1;
  data.sync_guid = "default";
  model()->Add(std::make_unique<TemplateURL>(data));

  TemplateURLData new_data(*default_turl);
  new_data.SetKeyword(u"new_kw");
  new_data.SetShortName(u"my name");
  new_data.SetURL("http://wrong.url.com?q={searchTerms}");
  new_data.safe_for_autoreplace = false;
  new_data.date_created = Time::FromTimeT(100);
  new_data.last_modified = Time::FromTimeT(100);
  new_data.prepopulate_id = 1;
  new_data.sync_guid = "different_guid";

  // Test that a remote TemplateURL can override a local TemplateURL not yet
  // known to sync.
  std::unique_ptr<TemplateURL> sync_turl =
      std::make_unique<TemplateURL>(new_data);
  syncer::SyncDataList list;
  list.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(sync_turl->data()));
  MergeAndExpectNotify(list, 1);

  TemplateURL* result_turl = model()->GetTemplateURLForGUID("different_guid");
  EXPECT_TRUE(result_turl);
  EXPECT_EQ(u"new_kw", result_turl->keyword());
  EXPECT_EQ(u"my name", result_turl->short_name());
  EXPECT_EQ(default_turl->url(), result_turl->url());

  // Reset the state of the service.
  model()->Remove(result_turl);
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  sync_processor_wrapper_ =
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          sync_processor_.get());

  // Now test that a remote TemplateURL can override the attributes of the local
  // default search provider.
  TemplateURL* existing_default = new TemplateURL(data);
  model()->Add(base::WrapUnique(existing_default));
  model()->SetUserSelectedDefaultSearchProvider(existing_default);

  // Default changing code invokes notify multiple times, difficult to fix.
  MergeAndExpectNotifyAtLeast(list);

  const TemplateURL* final_turl = model()->GetDefaultSearchProvider();
  EXPECT_TRUE(final_turl);
  EXPECT_EQ(u"new_kw", final_turl->keyword());
  EXPECT_EQ(u"my name", final_turl->short_name());
  EXPECT_EQ(default_turl->url(), final_turl->url());
}

TEST_F(TemplateURLServiceSyncTest, MergePrepopulatedEngineWithChangedKeyword) {
  const TemplateURLData default_data =
      *TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
           ->GetFallbackSearch();

  // Add a prepopulated search engine and mark it as default.
  model()->Add(std::make_unique<TemplateURL>(default_data));
  ASSERT_EQ(1u, model()->GetTemplateURLs().size());
  model()->SetUserSelectedDefaultSearchProvider(model()->GetTemplateURLs()[0]);
  ASSERT_EQ(model()->GetTemplateURLs()[0], model()->GetDefaultSearchProvider());

  // Now Sync data comes in changing the keyword.
  TemplateURLData changed_data(default_data);
  changed_data.SetKeyword(u"new_kw");
  changed_data.last_modified += base::Minutes(10);
  // It's important to set |safe_for_autoreplace| to false, which marks the
  // update as a manual user update. Without this,
  // TemplateURLService::UpdateTemplateURLIfPrepopulated would reset changes to
  // the keyword or search URL.
  changed_data.safe_for_autoreplace = false;
  // Since we haven't synced on this device before, the incoming data will have
  // a different guid (even though it's based on the exact same prepopulated
  // engine).
  changed_data.sync_guid = "different_guid";

  syncer::SyncDataList list{
      TemplateURLService::CreateSyncDataFromTemplateURLData(changed_data)};
  MergeAndExpectNotify(list, 1);

  // Make sure that no duplicate was created, that the local GUID was updated to
  // the one from Sync, and the keyword was updated.
  EXPECT_EQ(1u, model()->GetTemplateURLs().size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID(default_data.sync_guid));
  TemplateURL* result_turl = model()->GetTemplateURLForGUID("different_guid");
  ASSERT_TRUE(result_turl);
  EXPECT_EQ(u"new_kw", result_turl->keyword());
  // Also make sure that prefs::kSyncedDefaultSearchProviderGUID was updated to
  // point to the new GUID.
  EXPECT_EQ(GetDefaultSearchProviderGuidFromPrefs(
                *profile_a()->GetTestingPrefService()),
            "different_guid");
}

// The following tests check the case where, when turning on Sync, we get the
// following incoming changes: a) The default prepopulated engine (usually
// google.com) was modified (new keyword), and b) a new custom engine is chosen
// as the default. This maps to three events: adding an engine, changing the
// prepopulated engine, and changing the pref that defines the default engine.
// These can happen in any order, so there are multiple tests to verify that all
// orders work correctly.

TEST_F(TemplateURLServiceSyncTest, MergePrepopulatedEngine_Pref_Change_Add) {
  const TemplateURLData default_data =
      *TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
           ->GetFallbackSearch();

  // Add a prepopulated search engine and mark it as default.
  model()->Add(std::make_unique<TemplateURL>(default_data));
  ASSERT_EQ(1u, model()->GetTemplateURLs().size());
  model()->SetUserSelectedDefaultSearchProvider(model()->GetTemplateURLs()[0]);
  ASSERT_EQ(model()->GetTemplateURLs()[0], model()->GetDefaultSearchProvider());

  // Three changes come in from Sync:
  // 1) prefs::kSyncedDefaultSearchProviderGUID gets changed to point to a
  //    newly-added search engine (which doesn't exist yet at that point).
  // 2) The keyword of the existing prepopulated default engine gets changed.
  // 3) A new custom engine is added, which matches the pref change from 1).

  // Search engine changes are applied in order of their GUIDs. Make sure the
  // GUID for the change comes before the GUID for the add.
  const std::string kChangedGuid = "changed_guid";
  const std::string kAddedGuid = "zadded_guid";
  ASSERT_LT(kChangedGuid, kAddedGuid);

  // Step 1: Change the default search engine pref.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, kAddedGuid);

  TemplateURLData changed_data(default_data);
  changed_data.SetKeyword(u"new_kw");
  changed_data.last_modified += base::Minutes(10);
  // It's important to set |safe_for_autoreplace| to false, which marks the
  // update as a manual user update. Without this,
  // TemplateURLService::UpdateTemplateURLIfPrepopulated would reset changes to
  // the keyword or search URL.
  changed_data.safe_for_autoreplace = false;
  // Since we haven't synced on this device before, the incoming data will have
  // a different guid (even though it's based on the exact same prepopulated
  // engine).
  changed_data.sync_guid = kChangedGuid;

  TemplateURLData added_data;
  added_data.SetShortName(u"CustomEngine");
  added_data.SetKeyword(u"custom_kw");
  added_data.SetURL("https://custom.search?q={searchTerms}");
  added_data.date_created = Time::FromTimeT(100);
  added_data.last_modified = Time::FromTimeT(100);
  added_data.sync_guid = kAddedGuid;

  // Steps 2 and 3: Change the keyword of the existing engine, and add a new
  // custom one.
  syncer::SyncDataList list{
      TemplateURLService::CreateSyncDataFromTemplateURLData(changed_data),
      TemplateURLService::CreateSyncDataFromTemplateURLData(added_data)};
  MergeAndExpectNotify(list, 1);

  // Verify that the keyword change to the previous default engine was applied,
  // and that the newly-added engine is now the default.
  EXPECT_EQ(2u, model()->GetTemplateURLs().size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID(default_data.sync_guid));
  TemplateURL* changed_turl = model()->GetTemplateURLForGUID(kChangedGuid);
  ASSERT_TRUE(changed_turl);
  EXPECT_EQ(u"new_kw", changed_turl->keyword());
  TemplateURL* added_turl = model()->GetTemplateURLForGUID(kAddedGuid);
  ASSERT_TRUE(added_turl);
  EXPECT_EQ(model()->GetDefaultSearchProvider(), added_turl);
  EXPECT_EQ(u"custom_kw", added_turl->keyword());
}

TEST_F(TemplateURLServiceSyncTest, MergePrepopulatedEngine_Pref_Add_Change) {
  const TemplateURLData default_data =
      *TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
           ->GetFallbackSearch();

  // Add a prepopulated search engine and mark it as default.
  model()->Add(std::make_unique<TemplateURL>(default_data));
  ASSERT_EQ(1u, model()->GetTemplateURLs().size());
  model()->SetUserSelectedDefaultSearchProvider(model()->GetTemplateURLs()[0]);
  ASSERT_EQ(model()->GetTemplateURLs()[0], model()->GetDefaultSearchProvider());

  // Three changes come in from Sync:
  // 1) prefs::kSyncedDefaultSearchProviderGUID gets changed to point to a
  //    newly-added search engine (which doesn't exist yet at that point).
  // 2) A new custom engine is added, which matches the pref change from 1).
  // 3) The keyword of the existing prepopulated default engine gets changed.

  // Search engine changes are applied in order of their GUIDs. Make sure the
  // GUID for the add comes before the GUID for the change.
  const std::string kChangedGuid = "changed_guid";
  const std::string kAddedGuid = "added_guid";
  ASSERT_LT(kAddedGuid, kChangedGuid);

  // Step 1: Change the default search engine pref.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, kAddedGuid);

  TemplateURLData changed_data(default_data);
  changed_data.SetKeyword(u"new_kw");
  changed_data.last_modified += base::Minutes(10);
  // It's important to set |safe_for_autoreplace| to false, which marks the
  // update as a manual user update. Without this,
  // TemplateURLService::UpdateTemplateURLIfPrepopulated would reset changes to
  // the keyword or search URL.
  changed_data.safe_for_autoreplace = false;
  // Since we haven't synced on this device before, the incoming data will have
  // a different guid (even though it's based on the exact same prepopulated
  // engine).
  changed_data.sync_guid = kChangedGuid;

  TemplateURLData added_data;
  added_data.SetShortName(u"CustomEngine");
  added_data.SetKeyword(u"custom_kw");
  added_data.SetURL("https://custom.search?q={searchTerms}");
  added_data.date_created = Time::FromTimeT(100);
  added_data.last_modified = Time::FromTimeT(100);
  added_data.sync_guid = kAddedGuid;

  // Steps 2 and 3: Add a new custom engine, and change the keyword of the
  // existing one.
  syncer::SyncDataList list{
      TemplateURLService::CreateSyncDataFromTemplateURLData(added_data),
      TemplateURLService::CreateSyncDataFromTemplateURLData(changed_data)};
  MergeAndExpectNotify(list, 1);

  // Verify that the keyword change to the previous default engine was applied,
  // and that the newly-added engine is now the default.
  EXPECT_EQ(2u, model()->GetTemplateURLs().size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID(default_data.sync_guid));
  TemplateURL* changed_turl = model()->GetTemplateURLForGUID(kChangedGuid);
  ASSERT_TRUE(changed_turl);
  EXPECT_EQ(u"new_kw", changed_turl->keyword());
  TemplateURL* added_turl = model()->GetTemplateURLForGUID(kAddedGuid);
  ASSERT_TRUE(added_turl);
  EXPECT_EQ(model()->GetDefaultSearchProvider(), added_turl);
  EXPECT_EQ(u"custom_kw", added_turl->keyword());
}

TEST_F(TemplateURLServiceSyncTest, MergePrepopulatedEngine_Change_Add_Pref) {
  const TemplateURLData default_data =
      *TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
           ->GetFallbackSearch();

  // Add a prepopulated search engine and mark it as default.
  model()->Add(std::make_unique<TemplateURL>(default_data));
  ASSERT_EQ(1u, model()->GetTemplateURLs().size());
  model()->SetUserSelectedDefaultSearchProvider(model()->GetTemplateURLs()[0]);
  ASSERT_EQ(model()->GetTemplateURLs()[0], model()->GetDefaultSearchProvider());

  // Three changes come in from Sync:
  // 1) The keyword of the existing prepopulated default engine gets changed.
  // 2) A new custom engine is added.
  // 3) prefs::kSyncedDefaultSearchProviderGUID gets changed to point to the
  //    newly-added search engine from 2).

  // Search engine changes are applied in order of their GUIDs. Make sure the
  // GUID for the change comes before the GUID for the add.
  const std::string kChangedGuid = "changed_guid";
  const std::string kAddedGuid = "zadded_guid";
  ASSERT_LT(kChangedGuid, kAddedGuid);

  TemplateURLData changed_data(default_data);
  changed_data.SetKeyword(u"new_kw");
  changed_data.last_modified += base::Minutes(10);
  // It's important to set |safe_for_autoreplace| to false, which marks the
  // update as a manual user update. Without this,
  // TemplateURLService::UpdateTemplateURLIfPrepopulated would reset changes to
  // the keyword or search URL.
  changed_data.safe_for_autoreplace = false;
  // Since we haven't synced on this device before, the incoming data will have
  // a different guid (even though it's based on the exact same prepopulated
  // engine).
  changed_data.sync_guid = kChangedGuid;

  TemplateURLData added_data;
  added_data.SetShortName(u"CustomEngine");
  added_data.SetKeyword(u"custom_kw");
  added_data.SetURL("https://custom.search?q={searchTerms}");
  added_data.date_created = Time::FromTimeT(100);
  added_data.last_modified = Time::FromTimeT(100);
  added_data.sync_guid = kAddedGuid;

  // Steps 1 and 2: Change the keyword of the existing engine, and add a new
  // custom one.
  syncer::SyncDataList list{
      TemplateURLService::CreateSyncDataFromTemplateURLData(changed_data),
      TemplateURLService::CreateSyncDataFromTemplateURLData(added_data)};
  MergeAndExpectNotify(list, 1);

  // Step 3: Change the default search engine pref.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, kAddedGuid);

  // Verify that the keyword change to the previous default engine was applied,
  // and that the newly-added engine is now the default.
  EXPECT_EQ(2u, model()->GetTemplateURLs().size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID(default_data.sync_guid));
  TemplateURL* changed_turl = model()->GetTemplateURLForGUID(kChangedGuid);
  ASSERT_TRUE(changed_turl);
  EXPECT_EQ(u"new_kw", changed_turl->keyword());
  TemplateURL* added_turl = model()->GetTemplateURLForGUID(kAddedGuid);
  ASSERT_TRUE(added_turl);
  EXPECT_EQ(model()->GetDefaultSearchProvider(), added_turl);
  EXPECT_EQ(u"custom_kw", added_turl->keyword());
}

TEST_F(TemplateURLServiceSyncTest, MergePrepopulatedEngine_Add_Change_Pref) {
  const TemplateURLData default_data =
      *TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
           ->GetFallbackSearch();

  // Add a prepopulated search engine and mark it as default.
  model()->Add(std::make_unique<TemplateURL>(default_data));
  ASSERT_EQ(1u, model()->GetTemplateURLs().size());
  model()->SetUserSelectedDefaultSearchProvider(model()->GetTemplateURLs()[0]);
  ASSERT_EQ(model()->GetTemplateURLs()[0], model()->GetDefaultSearchProvider());

  // Three changes come in from Sync:
  // 1) A new custom engine is added.
  // 2) The keyword of the existing prepopulated default engine gets changed.
  // 3) prefs::kSyncedDefaultSearchProviderGUID gets changed to point to the
  //    newly-added search engine from 1).

  // Search engine changes are applied in order of their GUIDs. Make sure the
  // GUID for the add comes before the GUID for the change.
  const std::string kChangedGuid = "changed_guid";
  const std::string kAddedGuid = "added_guid";
  ASSERT_LT(kAddedGuid, kChangedGuid);

  TemplateURLData changed_data(default_data);
  changed_data.SetKeyword(u"new_kw");
  changed_data.last_modified += base::Minutes(10);
  // It's important to set |safe_for_autoreplace| to false, which marks the
  // update as a manual user update. Without this,
  // TemplateURLService::UpdateTemplateURLIfPrepopulated would reset changes to
  // the keyword or search URL.
  changed_data.safe_for_autoreplace = false;
  // Since we haven't synced on this device before, the incoming data will have
  // a different guid (even though it's based on the exact same prepopulated
  // engine).
  changed_data.sync_guid = kChangedGuid;

  TemplateURLData added_data;
  added_data.SetShortName(u"CustomEngine");
  added_data.SetKeyword(u"custom_kw");
  added_data.SetURL("https://custom.search?q={searchTerms}");
  added_data.date_created = Time::FromTimeT(100);
  added_data.last_modified = Time::FromTimeT(100);
  added_data.sync_guid = kAddedGuid;

  // Steps 1 and 2: Add a new custom engine, and change the keyword of the
  // existing one.
  syncer::SyncDataList list{
      TemplateURLService::CreateSyncDataFromTemplateURLData(added_data),
      TemplateURLService::CreateSyncDataFromTemplateURLData(changed_data)};
  MergeAndExpectNotify(list, 1);

  // Step 3: Change the default search engine pref.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, kAddedGuid);

  // Verify that the keyword change to the previous default engine was applied,
  // and that the newly-added engine is now the default.
  EXPECT_EQ(2u, model()->GetTemplateURLs().size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID(default_data.sync_guid));
  TemplateURL* changed_turl = model()->GetTemplateURLForGUID(kChangedGuid);
  ASSERT_TRUE(changed_turl);
  EXPECT_EQ(u"new_kw", changed_turl->keyword());
  TemplateURL* added_turl = model()->GetTemplateURLForGUID(kAddedGuid);
  ASSERT_TRUE(added_turl);
  EXPECT_EQ(model()->GetDefaultSearchProvider(), added_turl);
  EXPECT_EQ(u"custom_kw", added_turl->keyword());
}

TEST_F(TemplateURLServiceSyncTest, MergeNonEditedPrepopulatedEngine) {
  std::unique_ptr<TemplateURLData> default_turl(
      TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
          ->GetFallbackSearch());

  TemplateURLData data(*default_turl);
  data.safe_for_autoreplace = true;  // Can be replaced with built-in values.
  data.SetKeyword(u"new_kw");
  data.SetShortName(u"my name");
  data.SetURL("http://wrong.url.com?q={searchTerms}");
  data.date_created = Time::FromTimeT(50);
  data.last_modified = Time::FromTimeT(50);
  data.sync_guid = "default";
  model()->Add(std::make_unique<TemplateURL>(data));

  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  std::unique_ptr<TemplateURL> sync_turl(new TemplateURL(data));
  syncer::SyncDataList list;
  list.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(sync_turl->data()));
  MergeAndExpectNotify(list, 1);

  const TemplateURL* result_turl = model()->GetTemplateURLForGUID("default");
  EXPECT_TRUE(result_turl);
  EXPECT_EQ(default_turl->keyword(), result_turl->keyword());
  EXPECT_EQ(default_turl->short_name(), result_turl->short_name());
  EXPECT_EQ(default_turl->url(), result_turl->url());
}

TEST_F(TemplateURLServiceSyncTest, MergePrepopulatedEngineIgnoresId0) {
  // The newly registered keyword will have prepulate_id 0 since that is the
  // default value.
  model()->RegisterExtensionControlledTURL("extension1", "unittest", "keyword1",
                                           "http://extension1", Time(), false);

  // Try to merge in a turl with preopulate_id also set to 0. This should work.
  syncer::SyncDataList initial_data;
  std::unique_ptr<TemplateURL> turl(
      CreateTestTemplateURL(u"what", "http://thewhat.com/{searchTerms}",
                            "normal_guid", base::Time::FromTimeT(10), true,
                            TemplateURLData::PolicyOrigin::kNoPolicy, 0));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data()));

  MergeAndExpectNotify(initial_data, 1);
}

TEST_F(TemplateURLServiceSyncTest, MergeStarterPackEngine) {
  // Create a starter pack engine to ensure it is merged correctly.
  TemplateURLData data;
  data.SetShortName(u"Bookmarks");
  data.SetKeyword(u"@bookmarks");
  data.SetURL("chrome://bookmarks/?q={searchTerms}");
  data.starter_pack_id = TemplateURLStarterPackData::kBookmarks;
  data.date_created = Time::FromTimeT(100);
  data.last_modified = Time::FromTimeT(100);
  data.sync_guid = "bookmarks_guid";

  // Create another starter pack engine with an invalid starter pack id. This
  // should not be merged into the model.
  TemplateURLData invalid_data;
  invalid_data.SetShortName(u"Invalid starter pack");
  invalid_data.SetKeyword(u"@invalid");
  invalid_data.SetURL("chrome://bookmarks/?q={searchTerms}");
  invalid_data.starter_pack_id = TemplateURLStarterPackData::kMaxStarterPackID;
  invalid_data.date_created = Time::FromTimeT(100);
  invalid_data.last_modified = Time::FromTimeT(100);
  invalid_data.sync_guid = "invalid_guid";

  syncer::SyncDataList list{
      TemplateURLService::CreateSyncDataFromTemplateURLData(data),
      TemplateURLService::CreateSyncDataFromTemplateURLData(invalid_data)};
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, list,
                                    PassProcessor());

  // Ensure that the @bookmarks engine gets merged correctly.
  const TemplateURL* result_turl =
      model()->GetTemplateURLForGUID("bookmarks_guid");
  EXPECT_TRUE(result_turl);
  EXPECT_EQ(data.keyword(), result_turl->keyword());
  EXPECT_EQ(data.short_name(), result_turl->short_name());
  EXPECT_EQ(data.url(), result_turl->url());
  EXPECT_EQ(data.starter_pack_id, result_turl->starter_pack_id());

  // The @invalid entry has an invalid starter pack ID, ensure that it gets
  // thrown out when received from sync.
  const TemplateURL* invalid_result_turl =
      model()->GetTemplateURLForGUID("invalid_guid");
  EXPECT_FALSE(invalid_result_turl);
}

TEST_F(TemplateURLServiceSyncTest, GUIDUpdatedOnDefaultSearchChange) {
  const char kGUID[] = "initdefault";
  model()->Add(CreateTestTemplateURL(
      u"what", "http://thewhat.com/{searchTerms}", kGUID));
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID(kGUID));

  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);

  const char kNewGUID[] = "newdefault";
  model()->Add(CreateTestTemplateURL(
      u"what", "http://thewhat.com/{searchTerms}", kNewGUID));
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID(kNewGUID));

  EXPECT_EQ(kNewGUID, GetDefaultSearchProviderGuidFromPrefs(
                          *profile_a()->GetTestingPrefService()));
}

TEST_F(TemplateURLServiceSyncTest, NonAsciiKeywordDoesNotCrash) {
  model()->Add(CreateTestTemplateURL(u"\U0002f98d", "http://key1.com"));
  MergeAndExpectNotify(CreateInitialSyncData(), 1);
}

TEST_F(TemplateURLServiceSyncTest,
       GetAllSyncDataSkipsUntouchedAutogeneratedEngines) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    /*initial_sync_data=*/{}, PassProcessor());

  // `safe_for_autoreplace` is false. This represents an autogenerated keyword
  // which the user has modified. These should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "guid1", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));

  // `safe_for_autoreplace` is false. This represents a keyword which the user
  // has modified. These should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key2", "http://key2.com", "guid2", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kFalse));

  // `safe_for_autoreplace` is true. This represents a keyword which the user
  // has modified. These should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key3", "http://key3.com", "guid3", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue));

  // `safe_for_autoreplace` is true and `is_active` is `kUnspecified`. This
  // represents an autogenerated keyword which the user has not touched. These
  // should not be synced.
  model()->Add(CreateTestTemplateURL(
      u"key4", "http://key4.com", "guid4", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));

  // `safe_for_autoreplace` is true and `is_active` is `kFalse`. This represents
  // an autogenerated keyword which the user has manually deactivated. These
  // should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key5", "http://key5.com", "guid5", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kFalse));

  // `safe_for_autoreplace` is true and `is_active` is `kTrue`. This represents
  // an autogenerated keyword which the user has manually activated. These
  // should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key6", "http://key6.com", "guid6", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue));

  // Only the autogenerated untouched keyword(guid4) is missing.
  EXPECT_THAT(
      model()->GetAllSyncData(syncer::SEARCH_ENGINES),
      ElementsAre(ResultOf(GetGUID, "guid1"), ResultOf(GetGUID, "guid2"),
                  ResultOf(GetGUID, "guid3"), ResultOf(GetGUID, "guid5"),
                  ResultOf(GetGUID, "guid6")));
}

TEST_F(TemplateURLServiceSyncTest, MergeIgnoresUntouchedAutogeneratedKeywords) {
  syncer::SyncDataList initial_data;

  // `safe_for_autoreplace` is false. This represents an autogenerated keyword
  // which the user has modified. These should be synced.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"key1", "http://key1.com", "guid1", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));

  // `safe_for_autoreplace` is false. This represents a keyword which the user
  // has modified. These should be synced.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"key2", "http://key2.com", "guid2", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kFalse)
          ->data()));

  // `safe_for_autoreplace` is true. This represents a keyword which the user
  // has modified. These should be synced.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"key3", "http://key3.com", "guid3", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kTrue)
          ->data()));

  // `safe_for_autoreplace` is true and `is_active` is `kUnspecified`. This
  // represents an autogenerated keyword which the user has not touched. These
  // should not be synced.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"key4", "http://key4.com", "guid4", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));

  // `safe_for_autoreplace` is true and `is_active` is `kFalse`. This represents
  // an autogenerated keyword which the user has manually deactivated. These
  // should be synced.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"key5", "http://key5.com", "guid5", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kFalse)
          ->data()));

  // `safe_for_autoreplace` is true and `is_active` is `kTrue`. This represents
  // an autogenerated keyword which the user has manually activated. These
  // should be synced.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"key6", "http://key6.com", "guid6", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kTrue)
          ->data()));

  base::HistogramTester histogram_tester;

  // Now try to sync the data locally.
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  EXPECT_EQ(5U, model()->GetTemplateURLs().size());
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid1"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid2"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid3"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid5"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid6"));
  // Untouched autogenerated keyword is ignored.
  EXPECT_FALSE(model()->GetTemplateURLForGUID("guid4"));

  // No sync change was committed to the processor.
  EXPECT_EQ(0U, processor()->change_list_size());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Sync.SearchEngine.RemoteSearchEngineIsUntouchedAutogenerated"),
      ElementsAre(base::Bucket(false, 5), base::Bucket(true, 1)));
}

TEST_F(TemplateURLServiceSyncTest,
       ShouldLogRemoteUntouchedAutogeneratedKeywordsDuringMerge) {
  syncer::SyncDataList initial_data;

  // All the below keywords are untouched autogenerated keywords, given that
  // `safe_for_autoreplace` is true (implying that the keyword is autogenerated)
  // and `is_active` is `kUnspecified` (implying that the keyword is untouched).
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"key1", "http://key1.com", "guid1", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));

  // Prepopulated keyword.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"key2", "http://key2.com", "guid2", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/99999,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));

  // Starter pack keyword.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"key3", "http://key3.com", "guid3", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/1, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));

  base::HistogramTester histogram_tester;

  // Now try to sync the data locally.
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  // All the above keywords are untouched autogenerated keywords. All such
  // except for prepopulated engines are ignored (see crbug.com/404407977).
  EXPECT_EQ(1u, model()->GetTemplateURLs().size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("guid1"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid2"));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("guid3"));
  // No sync change was committed to the processor.
  EXPECT_EQ(0U, processor()->change_list_size());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Sync.SearchEngine.RemoteSearchEngineIsUntouchedAutogenerated"),
      base::BucketsAre(base::Bucket(false, 0), base::Bucket(true, 3)));
  // Only one of the above keywords is a prepopulated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.RemoteUntouchedAutogenerated."
                  "IsPrepopulatedEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
  // Only one of the above keywords is a starter pack keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.RemoteUntouchedAutogenerated."
                  "IsStarterPackEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
}

TEST_F(TemplateURLServiceSyncTest,
       ProcessSyncChangesIgnoresUntouchedAutogeneratedKeywords) {
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, syncer::SyncDataList{}, PassProcessor()));

  syncer::SyncChangeList changes;
  // `safe_for_autoreplace` is false. This represents an autogenerated keyword
  // which the user has modified. These should be synced.
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          u"key1", "http://key1.com", "guid1", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)));

  // `safe_for_autoreplace` is false. This represents a keyword which the user
  // has modified. These should be synced.
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          u"key2", "http://key2.com", "guid2", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kFalse)));

  // `safe_for_autoreplace` is true. This represents a keyword which the user
  // has modified. These should be synced.
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          u"key3", "http://key3.com", "guid3", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kTrue)));

  // `safe_for_autoreplace` is true and `is_active` is `kUnspecified`. This
  // represents an autogenerated keyword which the user has not touched. These
  // should not be synced.
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          u"key4", "http://key4.com", "guid4", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)));

  // `safe_for_autoreplace` is true and `is_active` is `kFalse`. This represents
  // an autogenerated keyword which the user has manually deactivated. These
  // should be synced.
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          u"key5", "http://key5.com", "guid5", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kFalse)));

  // `safe_for_autoreplace` is true and `is_active` is `kTrue`. This represents
  // an autogenerated keyword which the user has manually activated. These
  // should be synced.
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          u"key6", "http://key6.com", "guid6", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kTrue)));

  base::HistogramTester histogram_tester;

  // Now try to sync the data locally.
  ASSERT_FALSE(model()->ProcessSyncChanges(FROM_HERE, changes));

  EXPECT_EQ(5U, model()->GetTemplateURLs().size());
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid1"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid2"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid3"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid5"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid6"));
  // Untouched autogenerated keyword is ignored.
  EXPECT_FALSE(model()->GetTemplateURLForGUID("guid4"));

  // No sync change was committed to the processor.
  EXPECT_EQ(0U, processor()->change_list_size());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Sync.SearchEngine.RemoteSearchEngineIsUntouchedAutogenerated"),
      ElementsAre(base::Bucket(false, 5), base::Bucket(true, 1)));
}

TEST_F(TemplateURLServiceSyncTest,
       ShouldLogRemoteUntouchedAutogeneratedKeywordsUponProcessSyncChanges) {
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, syncer::SyncDataList{}, PassProcessor()));

  syncer::SyncChangeList changes;
  // All the below keywords are untouched autogenerated keywords, given that
  // `safe_for_autoreplace` is true (implying that the keyword is autogenerated)
  // and `is_active` is `kUnspecified` (implying that the keyword is untouched).
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          u"key1", "http://key1.com", "guid1", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)));

  // Prepopulated keyword.
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          u"key2", "http://key2.com", "guid2", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/99999,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)));

  // Starter pack keyword.
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          u"key3", "http://key3.com", "guid3", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/1, TemplateURLData::ActiveStatus::kUnspecified)));

  base::HistogramTester histogram_tester;

  // Now try to sync the data locally.
  ASSERT_FALSE(model()->ProcessSyncChanges(FROM_HERE, changes));

  // All the above keywords are untouched autogenerated keywords. All such
  // except prepopulated engines are ignored (see crbug.com/404407977).
  EXPECT_EQ(1u, model()->GetTemplateURLs().size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("guid1"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid2"));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("guid3"));

  // No sync change was committed to the processor.
  EXPECT_EQ(0U, processor()->change_list_size());

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Sync.SearchEngine.RemoteSearchEngineIsUntouchedAutogenerated"),
      base::BucketsAre(base::Bucket(false, 0), base::Bucket(true, 3)));
  // Only one of the above keywords is a prepopulated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.RemoteUntouchedAutogenerated."
                  "IsPrepopulatedEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
  // Only one of the above keywords is a starter pack keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.RemoteUntouchedAutogenerated."
                  "IsStarterPackEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
}

TEST_F(TemplateURLServiceSyncTest,
       AddingUntouchedAutogeneratedKeywordsSendsNoUpdate) {
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, syncer::SyncDataList{}, PassProcessor()));

  // `safe_for_autoreplace` is false. This represents an autogenerated keyword
  // which the user has modified. These should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "guid1", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  EXPECT_EQ(1U, processor()->change_list_size());
  EXPECT_TRUE(processor()->contains_guid("guid1"));

  // `safe_for_autoreplace` is false. This represents a keyword which the user
  // has modified. These should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key2", "http://key2.com", "guid2", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kFalse));
  EXPECT_EQ(1U, processor()->change_list_size());
  EXPECT_TRUE(processor()->contains_guid("guid2"));

  // `safe_for_autoreplace` is true. This represents a keyword which the user
  // has modified. These should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key3", "http://key3.com", "guid3", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue));
  EXPECT_EQ(1U, processor()->change_list_size());
  EXPECT_TRUE(processor()->contains_guid("guid3"));

  // `safe_for_autoreplace` is true and `is_active` is `kUnspecified`. This
  // represents an autogenerated keyword which the user has not touched. These
  // should not be synced.
  model()->Add(CreateTestTemplateURL(
      u"key4", "http://key4.com", "guid4", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  EXPECT_FALSE(processor()->contains_guid("guid4"));

  // `safe_for_autoreplace` is true and `is_active` is `kFalse`. This represents
  // an autogenerated keyword which the user has manually deactivated. These
  // should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key5", "http://key5.com", "guid5", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kFalse));
  EXPECT_EQ(1U, processor()->change_list_size());
  EXPECT_TRUE(processor()->contains_guid("guid5"));

  // `safe_for_autoreplace` is true and `is_active` is `kTrue`. This represents
  // an autogenerated keyword which the user has manually activated. These
  // should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key6", "http://key6.com", "guid6", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue));
  EXPECT_EQ(1U, processor()->change_list_size());
  EXPECT_TRUE(processor()->contains_guid("guid6"));
}

TEST_F(TemplateURLServiceSyncTest,
       UpdatingUntouchedAutogeneratedKeywordsSendsUpdate) {
  // Ensure that ProcessTemplateURLChange is called and pushes the correct
  // changes to Sync whenever local changes are made to TemplateURLs.
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, syncer::SyncDataList{}, PassProcessor()));

  // `safe_for_autoreplace` is true and `is_active` is `kUnspecified`. This
  // represents an autogenerated keyword which the user has not touched. These
  // should not be synced.
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      u"key", "http://key.com", "guid", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  EXPECT_EQ(0U, processor()->change_list_size());
  EXPECT_FALSE(processor()->contains_guid("guid"));

  // Change a keyword.
  ASSERT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  model()->ResetTemplateURL(turl, turl->short_name(), u"newkey", turl->url());

  EXPECT_FALSE(turl->safe_for_autoreplace());
  EXPECT_EQ(turl->is_active(), TemplateURLData::ActiveStatus::kTrue);
  EXPECT_EQ(1U, processor()->change_list_size());
  ASSERT_TRUE(processor()->contains_guid("guid"));
  EXPECT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
}

TEST_F(TemplateURLServiceSyncTest,
       DeletingUntouchedAutogeneratedKeywordsSendsNoUpdate) {
  // Ensure that ProcessTemplateURLChange is called and pushes the correct
  // changes to Sync whenever local changes are made to TemplateURLs.
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, syncer::SyncDataList{}, PassProcessor()));

  // `safe_for_autoreplace` is false. This represents an autogenerated keyword
  // which the user has modified. These should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "guid1", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  // `safe_for_autoreplace` is false. This represents a keyword which the user
  // has modified. These should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key2", "http://key2.com", "guid2", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kFalse));
  // `safe_for_autoreplace` is true. This represents a keyword which the user
  // has modified. These should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key3", "http://key3.com", "guid3", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue));
  // `safe_for_autoreplace` is true and `is_active` is `kUnspecified`. This
  // represents an autogenerated keyword which the user has not touched. These
  // should not be synced.
  model()->Add(CreateTestTemplateURL(
      u"key4", "http://key4.com", "guid4", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  // `safe_for_autoreplace` is true and `is_active` is `kFalse`. This represents
  // an autogenerated keyword which the user has manually deactivated. These
  // should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key5", "http://key5.com", "guid5", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kFalse));
  // `safe_for_autoreplace` is true and `is_active` is `kTrue`. This represents
  // an autogenerated keyword which the user has manually activated. These
  // should be synced.
  model()->Add(CreateTestTemplateURL(
      u"key6", "http://key6.com", "guid6", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue));

  // Remove the search engines.
  for (const std::string& guid :
       {"guid1", "guid2", "guid3", "guid5", "guid6"}) {
    ASSERT_TRUE(model()->GetTemplateURLForGUID(guid));
    model()->Remove(model()->GetTemplateURLForGUID(guid));
    EXPECT_EQ(1U, processor()->change_list_size());
    ASSERT_TRUE(processor()->contains_guid(guid));
    EXPECT_EQ(processor()->change_for_guid(guid).change_type(),
              syncer::SyncChange::ACTION_DELETE);
  }

  // Removing the autogenerated untouched keyword sends no sync update.
  ASSERT_TRUE(model()->GetTemplateURLForGUID("guid4"));
  model()->Remove(model()->GetTemplateURLForGUID("guid4"));
  EXPECT_FALSE(processor()->contains_guid("guid4"));
}

class TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines
    : public TemplateURLServiceSyncTest {
 public:
  TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines() {
    feature_list_.InitAndDisableFeature(
        syncer::kSeparateLocalAndAccountSearchEngines);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       GetAllSyncDataBasic) {
  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL(u"key2", "http://key2.com"));
  model()->Add(CreateTestTemplateURL(u"key3", "http://key3.com"));
  syncer::SyncDataList all_sync_data =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES);

  EXPECT_EQ(3U, all_sync_data.size());

  for (syncer::SyncDataList::const_iterator iter = all_sync_data.begin();
       iter != all_sync_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    const TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    std::unique_ptr<TemplateURL> deserialized(Deserialize(*iter));
    AssertEquals(*service_turl, *deserialized);
  }
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       GetAllSyncDataNoManagedEngines) {
  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL(u"key2", "http://key2.com"));
  model()->Add(CreateTestTemplateURL(
      u"key3", "http://key3.com", std::string(), base::Time::FromTimeT(100),
      false, TemplateURLData::PolicyOrigin::kDefaultSearchProvider));
  syncer::SyncDataList all_sync_data =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES);

  EXPECT_EQ(2U, all_sync_data.size());

  for (syncer::SyncDataList::const_iterator iter = all_sync_data.begin();
       iter != all_sync_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    std::unique_ptr<TemplateURL> deserialized(Deserialize(*iter));
    ASSERT_FALSE(service_turl->CreatedByPolicy());
    AssertEquals(*service_turl, *deserialized);
  }
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       GetAllSyncDataWithOmniboxExtension) {
  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL(u"key2", "http://key2.com"));
  std::string fake_id("blahblahblah");
  std::string fake_url = std::string(kOmniboxScheme) + "://" + fake_id;
  model()->RegisterExtensionControlledTURL(fake_id, "unittest", "key3",
                                           fake_url, Time(), false);
  syncer::SyncDataList all_sync_data =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES);

  EXPECT_EQ(2U, all_sync_data.size());

  for (syncer::SyncDataList::const_iterator iter = all_sync_data.begin();
       iter != all_sync_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    const TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    std::unique_ptr<TemplateURL> deserialized(Deserialize(*iter));
    AssertEquals(*service_turl, *deserialized);
  }
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       GetAllSyncDataWithSearchOverrideExtension) {
  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL(u"key2", "http://key2.com"));

  // Change default search provider to an extension one.
  std::unique_ptr<TemplateURLData> extension =
      GenerateDummyTemplateURLData("extension");
  auto ext_dse = std::make_unique<TemplateURL>(
      *extension, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, "ext", Time(),
      true);
  test_util_a_->AddExtensionControlledTURL(std::move(ext_dse));

  const TemplateURL* ext_turl = model()->GetDefaultSearchProvider();
  EXPECT_TRUE(model()->IsExtensionControlledDefaultSearch());

  // Extension default search must not be synced across browsers.
  syncer::SyncDataList all_sync_data =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES);
  EXPECT_EQ(2U, all_sync_data.size());

  for (auto sync_data : all_sync_data) {
    std::string guid = GetGUID(sync_data);
    const TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    std::unique_ptr<TemplateURL> deserialized = Deserialize(sync_data);
    AssertEquals(*service_turl, *deserialized);
    EXPECT_NE(TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION,
              deserialized->type());
    EXPECT_NE(ext_turl->keyword(), deserialized->keyword());
    EXPECT_NE(ext_turl->short_name(), deserialized->short_name());
    EXPECT_NE(ext_turl->url(), deserialized->url());
  }
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       LocalDefaultWinsConflict) {
  // We expect that the local default always wins keyword conflict resolution.
  const std::u16string keyword(u"key1");
  const std::string url("http://whatever.com/{searchTerms}");
  TemplateURL* default_turl = model()->Add(CreateTestTemplateURL(
      keyword, url, "whateverguid", base::Time::FromTimeT(10)));
  model()->SetUserSelectedDefaultSearchProvider(default_turl);

  syncer::SyncDataList initial_data = CreateInitialSyncData();
  // The guid1 entry should be different from the default but conflict in the
  // keyword.
  std::unique_ptr<TemplateURL> turl =
      CreateTestTemplateURL(keyword, "http://key1.com/{searchTerms}", "guid1",
                            base::Time::FromTimeT(90));
  initial_data[0] =
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data());
  MergeAndExpectNotify(initial_data, 1);

  // Since the local default was not yet synced, it should be merged with the
  // conflicting TemplateURL. However, its values should have been preserved
  // since it would have won conflict resolution due to being the default.
  EXPECT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  const TemplateURL* winner = model()->GetTemplateURLForGUID("guid1");
  ASSERT_TRUE(winner);
  EXPECT_EQ(model()->GetDefaultSearchProvider(), winner);
  EXPECT_EQ(keyword, winner->keyword());
  EXPECT_EQ(url, winner->url());
  ASSERT_TRUE(processor()->contains_guid("guid1"));
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            processor()->change_for_guid("guid1").change_type());
  EXPECT_EQ(url, GetURL(processor()->change_for_guid("guid1").sync_data()));

  // There is no loser, as the two were merged together. The local sync_guid
  // should no longer be found in the model.
  const TemplateURL* loser = model()->GetTemplateURLForGUID("whateverguid");
  ASSERT_FALSE(loser);
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       MergeAddFromOlderSyncData) {
  // GUIDs all differ, so this is data to be added from Sync, but the timestamps
  // from Sync are older. Set up the local data so that one is a dupe, one has a
  // conflicting keyword, and the last has no conflicts (a clean ADD).
  // Duplicate keyword, same hostname
  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com", "localguid1",
                                     base::Time::FromTimeT(100)));

  // Duplicate keyword, different hostname
  model()->Add(CreateTestTemplateURL(u"key2", "http://expected.com",
                                     "localguid2", base::Time::FromTimeT(100)));

  // Add
  model()->Add(
      CreateTestTemplateURL(u"unique", "http://unique.com", "localguid3"));

  ASSERT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  MergeAndExpectNotify(CreateInitialSyncData(), 1);

  // The dupe and conflict results in merges, as local values are always merged
  // with sync values if there is a keyword conflict. The unique keyword should
  // be added.
  EXPECT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // The key1 duplicate keyword results in the local copy winning. Ensure that
  // Sync's copy was not added, and the local copy is pushed upstream to Sync as
  // an update. The local copy should have received the sync data's GUID.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid1"));
  // Check changes for the UPDATE.
  ASSERT_TRUE(processor()->contains_guid("guid1"));
  syncer::SyncChange guid1_change = processor()->change_for_guid("guid1");
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, guid1_change.change_type());
  // The local sync_guid should no longer be found.
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid1"));

  // The key2 duplicate keyword results in a merge, with the values of the local
  // copy winning, so ensure it retains the original URL, and that an update to
  // the sync guid is pushed upstream to Sync.
  const TemplateURL* guid2 = model()->GetTemplateURLForGUID("guid2");
  ASSERT_TRUE(guid2);
  EXPECT_EQ(u"key2", guid2->keyword());
  EXPECT_EQ("http://expected.com", guid2->url());
  // Check changes for the UPDATE.
  ASSERT_TRUE(processor()->contains_guid("guid2"));
  syncer::SyncChange guid2_change = processor()->change_for_guid("guid2");
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, guid2_change.change_type());
  EXPECT_EQ("key2", GetKeyword(guid2_change.sync_data()));
  EXPECT_EQ("http://expected.com", GetURL(guid2_change.sync_data()));
  // The local sync_guid should no longer be found.
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid2"));

  // The last TemplateURL should have had no conflicts and was just added. It
  // should not have replaced the third local TemplateURL.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("localguid3"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid3"));

  // Two UPDATEs and one ADD.
  EXPECT_EQ(3U, processor()->change_list_size());
  // One ADDs should be pushed up to Sync.
  ASSERT_TRUE(processor()->contains_guid("localguid3"));
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            processor()->change_for_guid("localguid3").change_type());
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       MergeAddFromNewerSyncData) {
  // GUIDs all differ, so Sync may overtake some entries, but the timestamps
  // from Sync are newer. Set up the local data so that one is a dupe, one has a
  // conflicting keyword, and the last has no conflicts (a clean ADD).
  // Duplicate keyword, same hostname
  model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "localguid1", base::Time::FromTimeT(10),
      false, TemplateURLData::PolicyOrigin::kNoPolicy, 111));

  // Duplicate keyword, different hostname
  model()->Add(CreateTestTemplateURL(
      u"key2", "http://expected.com", "localguid2", base::Time::FromTimeT(10),
      false, TemplateURLData::PolicyOrigin::kNoPolicy, 112));

  // Add
  model()->Add(CreateTestTemplateURL(
      u"unique", "http://unique.com", "localguid3", base::Time::FromTimeT(10),
      false, TemplateURLData::PolicyOrigin::kNoPolicy, 113));

  ASSERT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  MergeAndExpectNotify(CreateInitialSyncData(), 1);

  // The duplicate keywords results in merges. The unique keyword be added to
  // the model.
  EXPECT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // The key1 duplicate keyword results in Sync's copy winning. Ensure that
  // Sync's copy replaced the local copy.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid1"));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid1"));
  EXPECT_FALSE(processor()->contains_guid("guid1"));
  EXPECT_FALSE(processor()->contains_guid("localguid1"));

  // The key2 duplicate keyword results in Sync's copy winning, so ensure it
  // retains the original keyword and is added. The local copy should be
  // removed.
  const TemplateURL* guid2_sync = model()->GetTemplateURLForGUID("guid2");
  ASSERT_TRUE(guid2_sync);
  EXPECT_EQ(u"key2", guid2_sync->keyword());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid2"));

  // The last TemplateURL should have had no conflicts and was just added. It
  // should not have replaced the third local TemplateURL.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("localguid3"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid3"));

  // One ADD.
  EXPECT_EQ(1U, processor()->change_list_size());
  // One ADDs should be pushed up to Sync.
  ASSERT_TRUE(processor()->contains_guid("localguid3"));
  EXPECT_EQ(syncer::SyncChange::ACTION_ADD,
            processor()->change_for_guid("localguid3").change_type());
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       MergeIgnoresPolicyAndPlayAPIEngines) {
  // Add a policy-created engine.
  model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "localguid1", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false,
      /*policy_origin=*/
      TemplateURLData::PolicyOrigin::kDefaultSearchProvider));

  {
    auto play_api_engine = CreateTestTemplateURL(
        u"key2", "http://key2.com", "localguid2", base::Time::FromTimeT(100));
    TemplateURLData data(play_api_engine->data());
    data.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
    play_api_engine = std::make_unique<TemplateURL>(data);
    model()->Add(std::move(play_api_engine));
  }

  ASSERT_EQ(1U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  MergeAndExpectNotify(CreateInitialSyncData(), 1);

  // The policy engine should be ignored when it comes to conflict resolution.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid1"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("localguid1"));

  // The Play API engine should be ignored when it comes to conflict resolution.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid2"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("localguid2"));
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       MergeInAllNewData) {
  model()->Add(CreateTestTemplateURL(u"abc.com", "http://abc.com", "abc"));
  model()->Add(CreateTestTemplateURL(u"def.com", "http://def.com", "def"));
  model()->Add(CreateTestTemplateURL(u"xyz.com", "http://xyz.com", "xyz"));
  ASSERT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  syncer::SyncDataList initial_data = CreateInitialSyncData();
  MergeAndExpectNotify(initial_data, 1);

  EXPECT_EQ(6U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  // We expect the model to have accepted all of the initial sync data. Search
  // through the model using the GUIDs to ensure that they're present.
  for (syncer::SyncDataList::const_iterator iter = initial_data.begin();
       iter != initial_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    EXPECT_TRUE(model()->GetTemplateURLForGUID(guid));
  }
  // All the original TemplateURLs should also remain in the model.
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"abc.com"));
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"def.com"));
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"xyz.com"));
  // Ensure that Sync received the expected changes.
  EXPECT_EQ(3U, processor()->change_list_size());
  EXPECT_TRUE(processor()->contains_guid("abc"));
  EXPECT_TRUE(processor()->contains_guid("def"));
  EXPECT_TRUE(processor()->contains_guid("xyz"));
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       MergeSyncIsTheSame) {
  // The local data is the same as the sync data merged in. i.e. - There have
  // been no changes since the last time we synced. Even the last_modified
  // timestamps are the same.
  syncer::SyncDataList initial_data = CreateInitialSyncData();
  for (syncer::SyncDataList::const_iterator iter = initial_data.begin();
       iter != initial_data.end(); ++iter) {
    model()->Add(Deserialize(*iter));
  }
  ASSERT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  MergeAndExpectNotify(initial_data, 0);

  EXPECT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  for (syncer::SyncDataList::const_iterator iter = initial_data.begin();
       iter != initial_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    EXPECT_TRUE(model()->GetTemplateURLForGUID(guid));
  }
  EXPECT_EQ(0U, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       MergeInSyncTemplateURL) {
  // An enumeration used to indicate which TemplateURL test value is expected
  // for a particular test result.
  enum ExpectedTemplateURL {
    LOCAL,
    SYNC,
    BOTH,
    NEITHER,
  };

  // Sets up and executes a MergeInSyncTemplateURL test given a number of
  // expected start and end states:
  //  * |conflict_winner| denotes which TemplateURL should win the
  //    conflict.
  //  * |synced_at_start| denotes which of the TemplateURLs should known
  //    to Sync.
  //  * |update_sent| denotes which TemplateURL should have an
  //    ACTION_UPDATE sent to the server after the merge.
  //  * |present_in_model| denotes which TemplateURL should be found in
  //    the model after the merge.
  //  * If |keywords_conflict| is true, the TemplateURLs are set up with
  //    the same keyword.
  struct TestCases {
    ExpectedTemplateURL conflict_winner;
    ExpectedTemplateURL synced_at_start;
    ExpectedTemplateURL update_sent;
    ExpectedTemplateURL present_in_model;
    bool keywords_conflict;
    size_t final_num_turls;
  };
  const auto test_cases = std::to_array<TestCases>({
      // Both are synced and the new sync entry is better: Local is left as-is,
      // and the Sync is added.
      {SYNC, BOTH, NEITHER, BOTH, true, 2},
      // Both are synced and the local entry is better: Sync is still added to
      // the model.
      {LOCAL, BOTH, NEITHER, BOTH, true, 2},
      // Local was not known to Sync and the new sync entry is better: Sync is
      // added. Local is removed. No updates.
      {SYNC, SYNC, NEITHER, SYNC, true, 1},
      // Local was not known to sync and the local entry is better: Local is
      // updated with sync GUID, Sync is not added. UPDATE sent for Sync.
      {LOCAL, SYNC, SYNC, SYNC, true, 1},
      // No conflicting keyword. Both should be added with their original
      // keywords, with no updates sent. Note that MergeDataAndStartSyncing is
      // responsible for creating the ACTION_ADD for the local TemplateURL.
      {NEITHER, SYNC, NEITHER, BOTH, false, 2},
  });

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Case #" << i << std::endl);

    // Assert all the valid states of ExpectedTemplateURLs.
    ASSERT_FALSE(test_cases[i].conflict_winner == BOTH);
    ASSERT_FALSE(test_cases[i].synced_at_start == NEITHER);
    ASSERT_FALSE(test_cases[i].synced_at_start == LOCAL);
    ASSERT_FALSE(test_cases[i].update_sent == BOTH);
    ASSERT_FALSE(test_cases[i].present_in_model == NEITHER);

    const std::u16string local_keyword = u"localkeyword";
    const std::u16string sync_keyword =
        test_cases[i].keywords_conflict ? local_keyword : u"synckeyword";
    const std::string local_url = "www.localurl.com";
    const std::string sync_url = "www.syncurl.com";
    const base::Time local_last_modified = base::Time::FromTimeT(100);
    const base::Time sync_last_modified =
        base::Time::FromTimeT(test_cases[i].conflict_winner == SYNC ? 110 : 90);
    const std::string local_guid = "local_guid";
    const std::string sync_guid = "sync_guid";

    // Initialize expectations.
    std::u16string expected_local_keyword = local_keyword;
    std::u16string expected_sync_keyword = sync_keyword;

    // Create the data and run the actual test.
    TemplateURL* local_turl = model()->Add(CreateTestTemplateURL(
        local_keyword, local_url, local_guid, local_last_modified));
    std::unique_ptr<TemplateURL> sync_turl(CreateTestTemplateURL(
        sync_keyword, sync_url, sync_guid, sync_last_modified));

    SyncDataMap sync_data;
    if (test_cases[i].synced_at_start == SYNC ||
        test_cases[i].synced_at_start == BOTH) {
      sync_data[sync_turl->sync_guid()] =
          TemplateURLService::CreateSyncDataFromTemplateURLData(
              sync_turl->data());
    }
    if (test_cases[i].synced_at_start == BOTH) {
      sync_data[local_turl->sync_guid()] =
          TemplateURLService::CreateSyncDataFromTemplateURLData(
              local_turl->data());
    }
    SyncDataMap initial_data;
    initial_data[local_turl->sync_guid()] =
        TemplateURLService::CreateSyncDataFromTemplateURLData(
            local_turl->data());

    syncer::SyncChangeList change_list;
    test_util_a_->ResetObserverCount();
    ASSERT_EQ(1u, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
    model()->MergeInSyncTemplateURL(sync_turl.get(), sync_data, &change_list,
                                    &initial_data);
    EXPECT_EQ(test_cases[i].final_num_turls,
              model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
    EXPECT_EQ(1, test_util_a_->GetObserverCount());

    // Check for expected updates, if any.
    std::string expected_update_guid;
    if (test_cases[i].update_sent == LOCAL) {
      expected_update_guid = local_guid;
    } else if (test_cases[i].update_sent == SYNC) {
      expected_update_guid = sync_guid;
    }
    if (!expected_update_guid.empty()) {
      ASSERT_EQ(1U, change_list.size());
      EXPECT_EQ(expected_update_guid, GetGUID(change_list[0].sync_data()));
      EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
                change_list[0].change_type());
    } else {
      EXPECT_EQ(0U, change_list.size());
    }

    // Check for TemplateURLs expected in the model. Note that this is checked
    // by GUID rather than the initial pointer, as a merge could occur (the
    // Sync TemplateURL overtakes the local one). Also remove the present
    // TemplateURL when done so the next test case starts with a clean slate.
    if (test_cases[i].present_in_model == LOCAL ||
        test_cases[i].present_in_model == BOTH) {
      ASSERT_TRUE(model()->GetTemplateURLForGUID(local_guid));
      EXPECT_EQ(expected_local_keyword, local_turl->keyword());
      EXPECT_EQ(local_url, local_turl->url());
      EXPECT_EQ(local_last_modified, local_turl->last_modified());
      model()->Remove(model()->GetTemplateURLForGUID(local_guid));
    }
    if (test_cases[i].present_in_model == SYNC ||
        test_cases[i].present_in_model == BOTH) {
      ASSERT_TRUE(model()->GetTemplateURLForGUID(sync_guid));
      EXPECT_EQ(expected_sync_keyword, sync_turl->keyword());
      EXPECT_EQ(sync_url, sync_turl->url());
      EXPECT_EQ(sync_last_modified, sync_turl->last_modified());
      model()->Remove(model()->GetTemplateURLForGUID(sync_guid));
    }
  }
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       MergeTwiceWithSameSyncData) {
  // Ensure that a second merge with the same data as the first does not
  // actually update the local data.
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateInitialSyncData()[0]);

  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com", "guid1",
                                     base::Time::FromTimeT(10)));  // earlier

  std::optional<syncer::ModelError> error =
      MergeAndExpectNotify(initial_data, 1);
  ASSERT_FALSE(error.has_value());

  // We should have updated the original TemplateURL with Sync's version.
  // Keep a copy of it so we can compare it after we re-merge.
  TemplateURL* guid1_url = model()->GetTemplateURLForGUID("guid1");
  ASSERT_TRUE(guid1_url);
  std::unique_ptr<TemplateURL> updated_turl(new TemplateURL(guid1_url->data()));
  EXPECT_EQ(Time::FromTimeT(90), updated_turl->last_modified());

  // Modify a single field of the initial data. This should not be updated in
  // the second merge, as the last_modified timestamp remains the same.
  std::unique_ptr<TemplateURL> temp_turl(Deserialize(initial_data[0]));
  TemplateURLData data(temp_turl->data());
  data.SetShortName(u"SomethingDifferent");
  temp_turl = std::make_unique<TemplateURL>(data);
  initial_data.clear();
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(temp_turl->data()));

  // Remerge the data again. This simulates shutting down and syncing again
  // at a different time, but the cloud data has not changed.
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  sync_processor_wrapper_ =
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          sync_processor_.get());
  error = MergeAndExpectNotify(initial_data, 0);
  ASSERT_FALSE(error.has_value());

  // Check that the TemplateURL was not modified.
  const TemplateURL* reupdated_turl = model()->GetTemplateURLForGUID("guid1");
  ASSERT_TRUE(reupdated_turl);
  AssertEquals(*updated_turl, *reupdated_turl);
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       MergeUpdateFromSync) {
  // The local data is the same as the sync data merged in, but timestamps have
  // changed. Ensure the right fields are merged in.
  syncer::SyncDataList initial_data;
  TemplateURL* turl1 = model()->Add(CreateTestTemplateURL(
      u"abc.com", "http://abc.com", "abc", base::Time::FromTimeT(9000)));
  model()->Add(CreateTestTemplateURL(u"xyz.com", "http://xyz.com", "xyz",
                                     base::Time::FromTimeT(9000)));

  std::unique_ptr<TemplateURL> turl1_newer = CreateTestTemplateURL(
      u"abc.com", "http://abc.ca", "abc", base::Time::FromTimeT(9999));
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      turl1_newer->data()));

  std::unique_ptr<TemplateURL> turl2_older = CreateTestTemplateURL(
      u"xyz.com", "http://xyz.ca", "xyz", base::Time::FromTimeT(8888));
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      turl2_older->data()));

  ASSERT_EQ(2U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  MergeAndExpectNotify(initial_data, 1);

  // Both were local updates, so we expect the same count.
  EXPECT_EQ(2U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // Check that the first replaced the initial abc TemplateURL.
  EXPECT_EQ(turl1, model()->GetTemplateURLForGUID("abc"));
  EXPECT_EQ("http://abc.ca", turl1->url());

  // Check that the second produced an upstream update to the xyz TemplateURL.
  EXPECT_EQ(1U, processor()->change_list_size());
  ASSERT_TRUE(processor()->contains_guid("xyz"));
  syncer::SyncChange change = processor()->change_for_guid("xyz");
  EXPECT_TRUE(change.change_type() == syncer::SyncChange::ACTION_UPDATE);
  EXPECT_EQ("http://xyz.com", GetURL(change.sync_data()));
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       PreSyncDeletes) {
  model()->pre_sync_deletes_.insert("guid1");
  model()->pre_sync_deletes_.insert("guid2");
  model()->pre_sync_deletes_.insert("aaa");
  model()->Add(CreateTestTemplateURL(u"whatever", "http://key1.com", "bbb"));
  ASSERT_EQ(1U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  MergeAndExpectNotify(CreateInitialSyncData(), 1);
  EXPECT_EQ(2U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // We expect the model to have GUIDs {bbb, guid3} after our initial merge.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("bbb"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid3"));
  syncer::SyncChange change = processor()->change_for_guid("guid1");
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, change.change_type());
  change = processor()->change_for_guid("guid2");
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, change.change_type());
  // "aaa" should have been pruned out on account of not being from Sync.
  EXPECT_FALSE(processor()->contains_guid("aaa"));
  // The set of pre-sync deletes should be cleared so they're not reused if
  // MergeDataAndStartSyncing gets called again.
  EXPECT_TRUE(model()->pre_sync_deletes_.empty());
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       PreSyncUpdates) {
  const char kNewKeyword[] = "somethingnew";
  const char16_t kNewKeyword16[] = u"somethingnew";
  // Fetch the prepopulate search engines so we know what they are.
  std::vector<std::unique_ptr<TemplateURLData>> prepop_turls =
      TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
          ->GetPrepopulatedEngines();

  std::vector<std::unique_ptr<TemplateURLData>> starter_pack_turls =
      TemplateURLStarterPackData::GetStarterPackEngines();

  // We have to prematurely exit this test if for some reason this machine does
  // not have any prepopulate TemplateURLs.
  ASSERT_FALSE(prepop_turls.empty());

  // Create a copy of the first TemplateURL with a really old timestamp and a
  // new keyword. Add it to the model.
  TemplateURLData data_copy(*prepop_turls[0]);
  data_copy.last_modified = Time::FromTimeT(10);
  std::u16string original_keyword = data_copy.keyword();
  data_copy.SetKeyword(kNewKeyword16);
  // Set safe_for_autoreplace to false so our keyword survives.
  data_copy.safe_for_autoreplace = false;
  model()->Add(std::make_unique<TemplateURL>(data_copy));

  // Merge the prepopulate search engines.
  base::Time pre_merge_time = base::Time::Now();
  base::RunLoop().RunUntilIdle();
  test_util_a_->ResetModel(true);

  // The newly added search engine should have been safely merged, with an
  // updated time.
  TemplateURL* added_turl = model()->GetTemplateURLForKeyword(kNewKeyword16);
  ASSERT_TRUE(added_turl);
  base::Time new_timestamp = added_turl->last_modified();
  EXPECT_GE(new_timestamp, pre_merge_time);
  std::string sync_guid = added_turl->sync_guid();

  // Bring down a copy of the prepopulate engine from Sync with the old values,
  // including the old timestamp and the same GUID. Ensure that it loses
  // conflict resolution against the local value, and an update is sent to the
  // server. The new timestamp should be preserved.
  syncer::SyncDataList initial_data;
  data_copy.SetKeyword(original_keyword);
  data_copy.sync_guid = sync_guid;
  std::unique_ptr<TemplateURL> sync_turl(new TemplateURL(data_copy));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(sync_turl->data()));

  ASSERT_EQ(prepop_turls.size() + starter_pack_turls.size(),
            model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());
  EXPECT_EQ(prepop_turls.size() + starter_pack_turls.size(),
            model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  ASSERT_EQ(added_turl, model()->GetTemplateURLForKeyword(kNewKeyword16));
  EXPECT_EQ(new_timestamp, added_turl->last_modified());
  syncer::SyncChange change = processor()->change_for_guid(sync_guid);
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, change.change_type());
  EXPECT_EQ(kNewKeyword,
            change.sync_data().GetSpecifics().search_engine().keyword());
  EXPECT_EQ(
      new_timestamp,
      base::Time::FromInternalValue(
          change.sync_data().GetSpecifics().search_engine().last_modified()));
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       SyncWithExtensionDefaultSearch) {
  // First start off with a few entries and make sure we can set an extension
  // default search provider.
  MergeAndExpectNotify(CreateInitialSyncData(), 1);
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID("guid2"));

  // Expect one change because of user default engine change.
  const size_t pending_changes = processor()->change_list_size();
  EXPECT_EQ(1U, pending_changes);
  ASSERT_TRUE(processor()->contains_guid("guid2"));
  EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE,
            processor()->change_for_guid("guid2").change_type());

  const size_t sync_engines_count =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES).size();
  EXPECT_EQ(3U, sync_engines_count);
  ASSERT_TRUE(model()->GetDefaultSearchProvider());

  // Change the default search provider to an extension one.
  std::unique_ptr<TemplateURLData> extension =
      GenerateDummyTemplateURLData("extensiondefault");
  auto ext_dse = std::make_unique<TemplateURL>(
      *extension, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, "ext", Time(),
      true);
  test_util_a_->AddExtensionControlledTURL(std::move(ext_dse));

  const TemplateURL* dsp_turl = model()->GetDefaultSearchProvider();
  EXPECT_TRUE(model()->IsExtensionControlledDefaultSearch());

  // Extension-related changes to the DSE should not be synced as search engine
  // changes.
  EXPECT_EQ(pending_changes, processor()->change_list_size());
  EXPECT_EQ(sync_engines_count,
            model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // Add a new entry from Sync. It should still sync in despite the default
  // being extension controlled.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"newkeyword", "http://new.com/{searchTerms}",
                            "newdefault")));
  ProcessAndExpectNotify(changes, 1);

  EXPECT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // Change kSyncedDefaultSearchProviderGUID to point to the new entry and
  // ensure that the DSP remains extension controlled.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, "newdefault");

  EXPECT_EQ(dsp_turl, model()->GetDefaultSearchProvider());
  EXPECT_TRUE(model()->IsExtensionControlledDefaultSearch());

  // Remove extension DSE. Ensure that the DSP changes to the expected pending
  // entry from Sync.
  const TemplateURL* expected_default =
      model()->GetTemplateURLForGUID("newdefault");
  test_util_a_->RemoveExtensionControlledTURL("ext");

  EXPECT_EQ(expected_default, model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       SyncedDefaultArrivesAfterStartup) {
  // Start with the default set to something in the model before we start
  // syncing.
  model()->Add(CreateTestTemplateURL(
      u"what", "http://thewhat.com/{searchTerms}", "initdefault"));
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID("initdefault"));

  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);

  // Set kSyncedDefaultSearchProviderGUID to something that is not yet in
  // the model but is expected in the initial sync. Ensure that this doesn't
  // change our default since we're not quite syncing yet.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, "guid2");

  EXPECT_EQ(default_search, model()->GetDefaultSearchProvider());

  // Now sync the initial data, which will include the search engine entry
  // destined to become the new default.
  syncer::SyncDataList initial_data = CreateInitialSyncData();
  // The default search provider should support replacement.
  std::unique_ptr<TemplateURL> turl(
      CreateTestTemplateURL(u"key2", "http://key2.com/{searchTerms}", "guid2",
                            base::Time::FromTimeT(90)));
  initial_data[1] =
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data());

  // When the default changes, a second notify is triggered.
  MergeAndExpectNotifyAtLeast(initial_data);

  // Ensure that the new default has been set.
  EXPECT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  ASSERT_NE(default_search, model()->GetDefaultSearchProvider());
  ASSERT_EQ("guid2", model()->GetDefaultSearchProvider()->sync_guid());
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       SyncedDefaultAlreadySetOnStartup) {
  // Start with the default set to something in the model before we start
  // syncing.
  const char kGUID[] = "initdefault";
  model()->Add(CreateTestTemplateURL(
      u"what", "http://thewhat.com/{searchTerms}", kGUID));
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID(kGUID));

  const TemplateURL* default_search = model()->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);

  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  // Set kSyncedDefaultSearchProviderGUID to the current default.
  SetDefaultSearchProviderGuidToPrefs(*prefs, kGUID);

  EXPECT_EQ(default_search, model()->GetDefaultSearchProvider());

  // Now sync the initial data.
  MergeAndExpectNotify(CreateInitialSyncData(), 1);

  // Ensure that the new entries were added and the default has not changed.
  EXPECT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  ASSERT_EQ(default_search, model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       ShouldLogLocalUntouchedAutogeneratedKeywordsDuringMerge) {
  // All the below keywords are untouched autogenerated keywords, given that
  // `safe_for_autoreplace` is true (implying that the keyword is autogenerated)
  // and `is_active` is `kUnspecified` (implying that the keyword is untouched).
  model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "guid1", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));

  // Prepopulated keyword.
  model()->Add(CreateTestTemplateURL(
      u"key2", "http://key2.com", "guid2", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/99999, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));

  // Starter pack keyword.
  model()->Add(CreateTestTemplateURL(
      u"key3", "http://key3.com", "guid3", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/1,
      TemplateURLData::ActiveStatus::kUnspecified));

  base::HistogramTester histogram_tester;

  // All the above keywords are untouched autogenerated keywords. All such
  // except prepopulated engines are ignored (see crbug.com/404407977).
  EXPECT_THAT(model()->GetAllSyncData(syncer::SEARCH_ENGINES),
              ElementsAre(Property(
                  &syncer::SyncData::GetSpecifics,
                  Property(&sync_pb::EntitySpecifics::search_engine,
                           Property(&sync_pb::SearchEngineSpecifics::sync_guid,
                                    "guid2")))));

  // Only one of the above keywords is a prepopulated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.LocalUntouchedAutogenerated."
                  "IsPrepopulatedEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
  // Only one of the above keywords is a starter pack keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.LocalUntouchedAutogenerated."
                  "IsStarterPackEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
}

// This test verifies the logging in the following cases:
// 1. Whether a keyword is an untouched autogenerated keyword is logged upon
// every add, update and delete.
// 2. Whether an untouched autogenerated keyword is a prepopulated keyword.
// 3. Whether an untouched autogenerated keyword is a starter pack keyword.
// This test first adds different types of keywords, then updates them and
// finally deletes them, verifying that the histograms are logged correctly in
// each of these cases.
TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       ShouldLogUntouchedAutogeneratedKeywordsWhenChanged) {
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, syncer::SyncDataList{}, PassProcessor()));

  base::HistogramTester histogram_tester;

  // Not an untouched keyword.
  TemplateURL* turl0 = model()->Add(CreateTestTemplateURL(
      u"key0", "http://key0.com", "guid0", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue));
  ASSERT_TRUE(processor()->contains_guid("guid0"));
  EXPECT_THAT(processor()->change_for_guid("guid0"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_ADD));

  // All the below keywords are untouched autogenerated keywords, given that
  // `safe_for_autoreplace` is true (implying that the keyword is autogenerated)
  // and `is_active` is `kUnspecified` (implying that the keyword is untouched).
  TemplateURL* turl1 = model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "guid1", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  EXPECT_FALSE(processor()->contains_guid("guid1"));

  // Starter pack keyword.
  TemplateURL* turl2 = model()->Add(CreateTestTemplateURL(
      u"key2", "http://key2.com", "guid2", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/1,
      TemplateURLData::ActiveStatus::kUnspecified));
  EXPECT_FALSE(processor()->contains_guid("guid2"));

  // Prepopulated keyword.
  TemplateURL* turl3 = model()->Add(CreateTestTemplateURL(
      u"key3", "http://key3.com", "guid3", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/99999, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  ASSERT_TRUE(processor()->contains_guid("guid3"));
  EXPECT_THAT(processor()->change_for_guid("guid3"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_ADD));

  // Only one of the above keywords is not an untouched autogenerated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedAdded"),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 3)));
  // Only one of the above keywords is a prepopulated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedAdded."
                  "IsPrepopulatedEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
  // Only one of the above keywords is a starter pack keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedAdded."
                  "IsStarterPackEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));

  // Update a non-untouched autogenerated keyword.
  ASSERT_EQ(turl0, model()->GetTemplateURLForGUID("guid0"));
  model()->UpdateTemplateURLVisitTime(turl0);
  EXPECT_THAT(processor()->change_for_guid("guid0"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_UPDATE));

  // Update the above untouched autogenerated keywords.
  ASSERT_EQ(turl1, model()->GetTemplateURLForGUID("guid1"));
  model()->UpdateTemplateURLVisitTime(turl1);
  EXPECT_FALSE(processor()->contains_guid("guid1"));

  ASSERT_EQ(turl2, model()->GetTemplateURLForGUID("guid2"));
  model()->UpdateTemplateURLVisitTime(turl2);
  EXPECT_FALSE(processor()->contains_guid("guid2"));

  ASSERT_EQ(turl3, model()->GetTemplateURLForGUID("guid3"));
  model()->UpdateTemplateURLVisitTime(turl3);
  EXPECT_THAT(processor()->change_for_guid("guid3"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_UPDATE));

  // Only one of the above keywords is not an untouched autogenerated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedUpdated"),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 3)));
  // Only one of the above keywords is a prepopulated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedUpdated."
                  "IsPrepopulatedEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
  // Only one of the above keywords is a starter pack keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedUpdated."
                  "IsStarterPackEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));

  // Delete the above keywords.
  model()->Remove(turl0);
  EXPECT_THAT(processor()->change_for_guid("guid0"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_DELETE));
  model()->Remove(turl1);
  EXPECT_FALSE(processor()->contains_guid("guid1"));
  model()->Remove(turl2);
  EXPECT_FALSE(processor()->contains_guid("guid2"));
  model()->Remove(turl3);
  EXPECT_THAT(processor()->change_for_guid("guid3"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_DELETE));

  // Only one of the above keywords is not an untouched autogenerated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedDeleted"),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 3)));
  // Only one of the above keywords is a prepopulated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedDeleted."
                  "IsPrepopulatedEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
  // Only one of the above keywords is a starter pack keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedDeleted."
                  "IsStarterPackEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
}

// Regression test for crbug.com/405298133.
TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       MergeIgnoresLocalUntouchedAutogeneratedKeywords) {
  // Untouched autogenerated keyword.
  const TemplateURL* turl1 = model()->Add(CreateTestTemplateURL(
      u"localkey1", "http://localkey1.com", "guid1", base::Time::FromTimeT(10),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  // Untouched autogenerated keyword.
  const TemplateURL* turl2 = model()->Add(CreateTestTemplateURL(
      u"localkey2", "http://localkey2.com", "guid2", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  // Not an untouched autogenerated keyword.
  const TemplateURL* turl3 = model()->Add(CreateTestTemplateURL(
      u"localkey3", "http://localkey3.com", "guid3", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));

  syncer::SyncDataList initial_data;
  // Not an untouched autogenerated keyword and more recent than `turl1`.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"accountkey1", "http://accountkey1.com", "guid1",
          base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));
  // Not an untouched autogenerated keyword but less recent than `turl2`.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"accountkey2", "http://accountkey2.com", "guid2",
          base::Time::FromTimeT(10),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));
  // Not an untouched autogenerated keyword but less recent than `turl3`.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"accountkey3", "http://accountkey3.com", "guid3",
          base::Time::FromTimeT(10),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));

  ASSERT_FALSE(model()
                   ->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                              initial_data, PassProcessor())
                   .has_value());

  // For `guid1`, the account keyword wins since it is more recent.
  ASSERT_EQ(turl1, model()->GetTemplateURLForGUID("guid1"));
  EXPECT_EQ(turl1->keyword(), u"accountkey1");
  EXPECT_FALSE(processor()->contains_guid("guid1"));
  // For `guid2`, the account keyword wins even though it is less recent.
  ASSERT_EQ(turl2, model()->GetTemplateURLForGUID("guid2"));
  EXPECT_EQ(turl2->keyword(), u"accountkey2");
  EXPECT_FALSE(processor()->contains_guid("guid2"));
  // For `guid3`, the local keyword wins since it is more recent. This is also
  // committed to the processor since it has not been filtered out as it's not
  // an untouched autogenerated keyword.
  ASSERT_EQ(turl3, model()->GetTemplateURLForGUID("guid3"));
  EXPECT_EQ(turl3->keyword(), u"localkey3");
  ASSERT_TRUE(processor()->contains_guid("guid3"));
  EXPECT_EQ(processor()->change_for_guid("guid3").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
  EXPECT_EQ(processor()
                ->change_for_guid("guid3")
                .sync_data()
                .GetSpecifics()
                .search_engine()
                .keyword(),
            "localkey3");
}

TEST_F(TemplateURLServiceSyncTestWithoutSeparateLocalAndAccountSearchEngines,
       SyncMergeDeletesDefault) {
  // If the value from Sync is a duplicate of the local default and is newer, it
  // should safely replace the local value and set as the new default.
  TemplateURL* default_turl = model()->Add(
      CreateTestTemplateURL(u"key1", "http://key1.com/{searchTerms}",
                            "whateverguid", base::Time::FromTimeT(10)));
  model()->SetUserSelectedDefaultSearchProvider(default_turl);

  syncer::SyncDataList initial_data = CreateInitialSyncData();
  // The guid1 entry should be a duplicate of the default.
  std::unique_ptr<TemplateURL> turl(
      CreateTestTemplateURL(u"key1", "http://key1.com/{searchTerms}", "guid1",
                            base::Time::FromTimeT(90)));
  initial_data[0] =
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data());
  MergeAndExpectNotify(initial_data, 1);

  EXPECT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("whateverguid"));
  EXPECT_EQ(model()->GetDefaultSearchProvider(),
            model()->GetTemplateURLForGUID("guid1"));
}

class TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines
    : public TemplateURLServiceSyncTest {
 public:
  TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines()
      : feature_list_(syncer::kSeparateLocalAndAccountSearchEngines) {}

  KeywordWebDataService* web_data_service() {
    return test_util_a_->web_data_service();
  }

  std::vector<TemplateURLData> GetKeywordsFromDatabase() {
    KeywordsConsumer consumer;
    test_util_a_->web_data_service()->GetKeywords(&consumer);
    return consumer.Get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       GetAllSyncDataPriorToSyncStart) {
  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL(u"key2", "http://key2.com"));
  model()->Add(CreateTestTemplateURL(u"key3", "http://key3.com"));
  syncer::SyncDataList all_sync_data =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES);

  EXPECT_TRUE(all_sync_data.empty());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       GetAllSyncData) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL(u"key2", "http://key2.com"));
  model()->Add(CreateTestTemplateURL(u"key3", "http://key3.com"));
  syncer::SyncDataList all_sync_data =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES);

  EXPECT_EQ(3U, all_sync_data.size());

  for (syncer::SyncDataList::const_iterator iter = all_sync_data.begin();
       iter != all_sync_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    const TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    std::unique_ptr<TemplateURL> deserialized(Deserialize(*iter));
    AssertEquals(*service_turl, *deserialized);
  }
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       GetAllSyncDataNoManagedEngines) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL(u"key2", "http://key2.com"));
  model()->Add(CreateTestTemplateURL(
      u"key3", "http://key3.com", std::string(), base::Time::FromTimeT(100),
      false, TemplateURLData::PolicyOrigin::kDefaultSearchProvider));
  syncer::SyncDataList all_sync_data =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES);

  EXPECT_EQ(2U, all_sync_data.size());

  for (syncer::SyncDataList::const_iterator iter = all_sync_data.begin();
       iter != all_sync_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    std::unique_ptr<TemplateURL> deserialized(Deserialize(*iter));
    ASSERT_FALSE(service_turl->CreatedByPolicy());
    AssertEquals(*service_turl, *deserialized);
  }
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       GetAllSyncDataWithOmniboxExtension) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL(u"key2", "http://key2.com"));
  std::string fake_id("blahblahblah");
  std::string fake_url = std::string(kOmniboxScheme) + "://" + fake_id;
  model()->RegisterExtensionControlledTURL(fake_id, "unittest", "key3",
                                           fake_url, Time(), false);
  syncer::SyncDataList all_sync_data =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES);

  EXPECT_EQ(2U, all_sync_data.size());

  for (syncer::SyncDataList::const_iterator iter = all_sync_data.begin();
       iter != all_sync_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    const TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    std::unique_ptr<TemplateURL> deserialized(Deserialize(*iter));
    AssertEquals(*service_turl, *deserialized);
  }
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       GetAllSyncDataWithSearchOverrideExtension) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL(u"key2", "http://key2.com"));

  // Change default search provider to an extension one.
  std::unique_ptr<TemplateURLData> extension =
      GenerateDummyTemplateURLData("extension");
  auto ext_dse = std::make_unique<TemplateURL>(
      *extension, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, "ext", Time(),
      true);
  test_util_a_->AddExtensionControlledTURL(std::move(ext_dse));

  const TemplateURL* ext_turl = model()->GetDefaultSearchProvider();
  EXPECT_TRUE(model()->IsExtensionControlledDefaultSearch());

  // Extension default search must not be synced across browsers.
  syncer::SyncDataList all_sync_data =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES);
  EXPECT_EQ(2U, all_sync_data.size());

  for (auto sync_data : all_sync_data) {
    std::string guid = GetGUID(sync_data);
    const TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    std::unique_ptr<TemplateURL> deserialized = Deserialize(sync_data);
    AssertEquals(*service_turl, *deserialized);
    EXPECT_NE(TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION,
              deserialized->type());
    EXPECT_NE(ext_turl->keyword(), deserialized->keyword());
    EXPECT_NE(ext_turl->short_name(), deserialized->short_name());
    EXPECT_NE(ext_turl->url(), deserialized->url());
  }
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       MergeIntoEmpty) {
  ASSERT_TRUE(model()->GetAllSyncData(syncer::SEARCH_ENGINES).empty());
  syncer::SyncDataList initial_data = CreateInitialSyncData();
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  EXPECT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  // We expect the model to have accepted all of the initial sync data. Search
  // through the model using the GUIDs to ensure that they're present.
  for (syncer::SyncDataList::const_iterator iter = initial_data.begin();
       iter != initial_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    EXPECT_TRUE(model()->GetTemplateURLForGUID(guid));
  }

  EXPECT_EQ(0U, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       MergeInAllNewData) {
  model()->Add(CreateTestTemplateURL(u"abc.com", "http://abc.com", "abc"));
  model()->Add(CreateTestTemplateURL(u"def.com", "http://def.com", "def"));
  model()->Add(CreateTestTemplateURL(u"xyz.com", "http://xyz.com", "xyz"));

  syncer::SyncDataList initial_data = CreateInitialSyncData();
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  // We expect the model to have accepted all of the initial sync data. Search
  // through the model using the GUIDs to ensure that they're present.
  for (syncer::SyncDataList::const_iterator iter = initial_data.begin();
       iter != initial_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    EXPECT_TRUE(model()->GetTemplateURLForGUID(guid));
  }
  // All the original TemplateURLs should also remain in the model.
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"abc.com"));
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"def.com"));
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"xyz.com"));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       MergeSyncIsTheSame) {
  // The local data is the same as the sync data merged in. i.e. - There have
  // been no changes since the last time we synced. Even the last_modified
  // timestamps are the same.
  syncer::SyncDataList initial_data = CreateInitialSyncData();
  for (syncer::SyncDataList::const_iterator iter = initial_data.begin();
       iter != initial_data.end(); ++iter) {
    model()->Add(std::make_unique<TemplateURL>(Deserialize(*iter)->data()));
  }

  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  for (syncer::SyncDataList::const_iterator iter = initial_data.begin();
       iter != initial_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    const TemplateURL* turl = model()->GetTemplateURLForGUID(guid);
    ASSERT_TRUE(turl);
    EXPECT_EQ(turl->GetLocalData(), turl->GetAccountData());
  }
  EXPECT_EQ(0U, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       MergeAddFromOlderSyncData) {
  // GUIDs all differ, so this is data to be added from Sync, but the timestamps
  // from Sync are older. Set up the local data so that one is a dupe, one has a
  // conflicting keyword, and the last has no conflicts (a clean ADD).
  // Duplicate keyword, same hostname
  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com", "localguid1",
                                     base::Time::FromTimeT(100)));

  // Duplicate keyword, different hostname
  model()->Add(CreateTestTemplateURL(u"key2", "http://expected.com",
                                     "localguid2", base::Time::FromTimeT(100)));

  // Add
  model()->Add(
      CreateTestTemplateURL(u"unique", "http://unique.com", "localguid3"));

  // The dupe and conflict results in merges, as local values are always merged
  // with sync values if there is a keyword conflict. The unique keyword should
  // be added.
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    CreateInitialSyncData(), PassProcessor());

  // The key1 duplicate keyword results in the local copy winning. Ensure that
  // Sync's copy was not added. The local copy should have received the sync
  // data's GUID.
  const TemplateURL* guid1 = model()->GetTemplateURLForGUID("guid1");
  ASSERT_TRUE(guid1);
  EXPECT_TRUE(guid1->GetLocalData());
  EXPECT_TRUE(guid1->GetAccountData());
  // The local sync_guid should no longer be found.
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid1"));

  // The key2 duplicate keyword results in a merge, with the values of the local
  // copy winning, so ensure it retains the original URL.
  const TemplateURL* guid2 = model()->GetTemplateURLForGUID("guid2");
  ASSERT_TRUE(guid2);
  EXPECT_TRUE(guid2->GetLocalData());
  EXPECT_TRUE(guid2->GetAccountData());
  EXPECT_EQ(u"key2", guid2->keyword());
  EXPECT_EQ("http://expected.com", guid2->url());
  // The local sync_guid should no longer be found.
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid2"));

  // The last TemplateURL should have had no conflicts and was just added. It
  // should not have replaced the third local TemplateURL.
  const TemplateURL* localguid3 = model()->GetTemplateURLForGUID("localguid3");
  ASSERT_TRUE(localguid3);
  EXPECT_FALSE(localguid3->GetAccountData());

  const TemplateURL* guid3 = model()->GetTemplateURLForGUID("guid3");
  ASSERT_TRUE(guid3);
  EXPECT_FALSE(guid3->GetLocalData());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       MergeUpdateFromSync) {
  // The local data is the same as the sync data merged in, but timestamps have
  // changed. Ensure the right fields are merged in.
  syncer::SyncDataList initial_data;
  TemplateURL* turl1 = model()->Add(CreateTestTemplateURL(
      u"abc.com", "http://abc.com", "abc", base::Time::FromTimeT(9000)));
  const TemplateURL* turl2 = model()->Add(CreateTestTemplateURL(
      u"xyz.com", "http://xyz.com", "xyz", base::Time::FromTimeT(9000)));

  std::unique_ptr<TemplateURL> turl1_newer = CreateTestTemplateURL(
      u"abc.com", "http://abc.ca", "abc", base::Time::FromTimeT(9999));
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      turl1_newer->data()));

  std::unique_ptr<TemplateURL> turl2_older = CreateTestTemplateURL(
      u"xyz.com", "http://xyz.ca", "xyz", base::Time::FromTimeT(8888));
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      turl2_older->data()));

  MergeAndExpectNotify(initial_data, 1);

  // Check that the first overrides the local abc TemplateURL.
  EXPECT_EQ(turl1, model()->GetTemplateURLForGUID("abc"));
  EXPECT_EQ("http://abc.ca", turl1->url());

  // Check that the second is overridden by the local xyz TemplateURL.
  EXPECT_EQ(turl2, model()->GetTemplateURLForGUID("xyz"));
  EXPECT_EQ("http://xyz.com", turl2->url());

  // No changes were sent to Sync.
  EXPECT_EQ(0U, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       MergeAddFromNewerSyncData) {
  // GUIDs all differ, so Sync may overtake some entries, but the timestamps
  // from Sync are newer. Set up the local data so that one is a dupe, one has a
  // conflicting keyword, and the last has no conflicts (a clean ADD).
  // Duplicate keyword, same hostname
  model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "localguid1", base::Time::FromTimeT(10),
      false, TemplateURLData::PolicyOrigin::kNoPolicy, 111));

  // Duplicate keyword, different hostname
  model()->Add(CreateTestTemplateURL(
      u"key2", "http://expected.com", "localguid2", base::Time::FromTimeT(10),
      false, TemplateURLData::PolicyOrigin::kNoPolicy, 112));

  // Add
  model()->Add(CreateTestTemplateURL(
      u"unique", "http://unique.com", "localguid3", base::Time::FromTimeT(10),
      false, TemplateURLData::PolicyOrigin::kNoPolicy, 113));

  // The duplicate keywords results in merges. The unique keyword be added to
  // the model.
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    CreateInitialSyncData(), PassProcessor());

  // The key1 duplicate keyword results in Sync's copy winning. Ensure that
  // Sync's copy overrid the local copy.
  const TemplateURL* guid1 = model()->GetTemplateURLForGUID("guid1");
  ASSERT_TRUE(guid1);
  EXPECT_TRUE(guid1->GetLocalData());
  EXPECT_TRUE(guid1->GetAccountData());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid1"));

  // The key2 duplicate keyword results in Sync's copy winning, so ensure it
  // retains the original keyword and is added.
  const TemplateURL* guid2_sync = model()->GetTemplateURLForGUID("guid2");
  ASSERT_TRUE(guid2_sync);
  EXPECT_TRUE(guid2_sync->GetLocalData());
  EXPECT_TRUE(guid2_sync->GetAccountData());
  EXPECT_EQ(u"key2", guid2_sync->keyword());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid2"));

  // The last TemplateURL should have had no conflicts and was just added. It
  // should not have replaced the third local TemplateURL.
  const TemplateURL* localguid3 = model()->GetTemplateURLForGUID("localguid3");
  ASSERT_TRUE(localguid3);
  EXPECT_FALSE(localguid3->GetAccountData());

  const TemplateURL* guid3 = model()->GetTemplateURLForGUID("guid3");
  ASSERT_TRUE(guid3);
  EXPECT_FALSE(guid3->GetLocalData());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       MergeIgnoresPolicyAndPlayAPIEngines) {
  // Add a policy-created engine.
  model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "localguid1", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false,
      /*created_by_policy=*/
      TemplateURLData::PolicyOrigin::kDefaultSearchProvider));

  {
    auto play_api_engine = CreateTestTemplateURL(
        u"key2", "http://key2.com", "localguid2", base::Time::FromTimeT(100));
    TemplateURLData data(play_api_engine->data());
    data.regulatory_origin = RegulatoryExtensionType::kAndroidEEA;
    play_api_engine = std::make_unique<TemplateURL>(data);
    model()->Add(std::move(play_api_engine));
  }

  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    CreateInitialSyncData(), PassProcessor());

  // The policy engine should be ignored when it comes to conflict resolution.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid1"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("localguid1"));

  // The Play API engine should be ignored when it comes to conflict resolution.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid2"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("localguid2"));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       MergeInSyncTemplateURL) {
  // An enumeration used to indicate which TemplateURL test value is expected
  // for a particular test result.
  enum ExpectedTemplateURL {
    LOCAL,
    SYNC,
    BOTH,
    NEITHER,
  };

  // Sets up and executes a MergeInSyncTemplateURL test given a number of
  // expected start and end states:
  //  * |conflict_winner| denotes which TemplateURL should win the
  //    conflict.
  //  * |synced_at_start| denotes which of the TemplateURLs should known
  //    to Sync.
  //  * |update_sent| denotes which TemplateURL should have an
  //    ACTION_UPDATE sent to the server after the merge.
  //  * |present_in_model| denotes which TemplateURL should be found in
  //    the model after the merge.
  //  * If |keywords_conflict| is true, the TemplateURLs are set up with
  //    the same keyword.
  struct TestCases {
    ExpectedTemplateURL conflict_winner;
    ExpectedTemplateURL synced_at_start;
    ExpectedTemplateURL update_sent;
    ExpectedTemplateURL present_in_model;
    bool keywords_conflict;
    size_t final_num_turls;
  };
  const auto test_cases = std::to_array<TestCases>({
      // Both are synced and the new sync entry is better: Local is left as-is,
      // and the Sync is added.
      {SYNC, BOTH, NEITHER, BOTH, true, 2},
      // Both are synced and the local entry is better: Sync is still added to
      // the model.
      {LOCAL, BOTH, NEITHER, BOTH, true, 2},
      // Local was not known to Sync and the new sync entry is better: Sync is
      // added. Local is removed. No updates.
      {SYNC, SYNC, NEITHER, SYNC, true, 1},
      // Local was not known to sync and the local entry is better: Local is
      // updated with sync GUID, Sync is not added. UPDATE sent for Sync.
      {LOCAL, SYNC, SYNC, SYNC, true, 1},
      // No conflicting keyword. Both should be added with their original
      // keywords, with no updates sent. Note that MergeDataAndStartSyncing is
      // responsible for creating the ACTION_ADD for the local TemplateURL.
      {NEITHER, SYNC, NEITHER, BOTH, false, 2},
  });

  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, {},
                                    std::move(sync_processor_));

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Case #" << i << std::endl);

    // Assert all the valid states of ExpectedTemplateURLs.
    ASSERT_FALSE(test_cases[i].conflict_winner == BOTH);
    ASSERT_FALSE(test_cases[i].synced_at_start == NEITHER);
    ASSERT_FALSE(test_cases[i].synced_at_start == LOCAL);
    ASSERT_FALSE(test_cases[i].update_sent == BOTH);
    ASSERT_FALSE(test_cases[i].present_in_model == NEITHER);

    const std::u16string local_keyword = u"localkeyword";
    const std::u16string sync_keyword =
        test_cases[i].keywords_conflict ? local_keyword : u"synckeyword";
    const std::string local_url = "www.localurl.com";
    const std::string sync_url = "www.syncurl.com";
    const base::Time local_last_modified = base::Time::FromTimeT(100);
    const base::Time sync_last_modified =
        base::Time::FromTimeT(test_cases[i].conflict_winner == SYNC ? 110 : 90);
    const std::string local_guid = "local_guid";
    const std::string sync_guid = "sync_guid";

    // Initialize expectations.
    std::u16string expected_local_keyword = local_keyword;
    std::u16string expected_sync_keyword = sync_keyword;

    // Create the data and run the actual test.
    TemplateURL* local_turl = model()->Add(CreateTestTemplateURL(
        local_keyword, local_url, local_guid, local_last_modified));
    std::unique_ptr<TemplateURL> sync_turl(CreateTestTemplateURL(
        sync_keyword, sync_url, sync_guid, sync_last_modified));

    SyncDataMap sync_data;
    if (test_cases[i].synced_at_start == SYNC ||
        test_cases[i].synced_at_start == BOTH) {
      sync_data[sync_turl->sync_guid()] =
          TemplateURLService::CreateSyncDataFromTemplateURLData(
              sync_turl->data());
    }
    if (test_cases[i].synced_at_start == BOTH) {
      sync_data[local_turl->sync_guid()] =
          TemplateURLService::CreateSyncDataFromTemplateURLData(
              local_turl->data());
    }
    SyncDataMap initial_data;
    initial_data[local_turl->sync_guid()] =
        TemplateURLService::CreateSyncDataFromTemplateURLData(
            local_turl->data());

    syncer::SyncChangeList change_list;
    test_util_a_->ResetObserverCount();
    model()->MergeInSyncTemplateURL(sync_turl.get(), sync_data, &change_list,
                                    &initial_data);
    EXPECT_EQ(test_cases[i].final_num_turls,
              model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
    EXPECT_EQ(1, test_util_a_->GetObserverCount());

    // Check for expected updates, if any.
    std::string expected_update_guid;
    if (test_cases[i].update_sent == LOCAL) {
      expected_update_guid = local_guid;
    } else if (test_cases[i].update_sent == SYNC) {
      expected_update_guid = sync_guid;
    }
    ASSERT_EQ(0U, change_list.size());

    // Check for TemplateURLs expected in the model. Note that this is checked
    // by GUID rather than the initial pointer, as a merge could occur (the
    // Sync TemplateURL overtakes the local one). Also remove the present
    // TemplateURL when done so the next test case starts with a clean slate.
    if (test_cases[i].present_in_model == LOCAL ||
        test_cases[i].present_in_model == BOTH) {
      ASSERT_TRUE(model()->GetTemplateURLForGUID(local_guid));
      EXPECT_EQ(expected_local_keyword, local_turl->keyword());
      EXPECT_EQ(local_url, local_turl->url());
      EXPECT_EQ(local_last_modified, local_turl->last_modified());
      model()->Remove(model()->GetTemplateURLForGUID(local_guid));
    }
    if (test_cases[i].present_in_model == SYNC ||
        test_cases[i].present_in_model == BOTH) {
      ASSERT_TRUE(model()->GetTemplateURLForGUID(sync_guid));
      EXPECT_EQ(expected_sync_keyword, sync_turl->keyword());
      EXPECT_EQ(sync_url, sync_turl->url());
      EXPECT_EQ(sync_last_modified, sync_turl->last_modified());
      model()->Remove(model()->GetTemplateURLForGUID(sync_guid));
    }
  }
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       MergeTwiceWithSameSyncData) {
  // Ensure that a second merge with the same data as the first updates the
  // effective data, since upon sync stop the account data should have been
  // removed.
  syncer::SyncDataList initial_data;
  initial_data.push_back(CreateInitialSyncData()[0]);

  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com", "guid1",
                                     base::Time::FromTimeT(10)));  // earlier

  std::optional<syncer::ModelError> error =
      MergeAndExpectNotify(initial_data, 1);
  ASSERT_FALSE(error.has_value());

  // Account data is added into the turl.
  TemplateURL* guid1_url = model()->GetTemplateURLForGUID("guid1");
  ASSERT_TRUE(guid1_url);
  std::unique_ptr<TemplateURL> updated_turl(
      new TemplateURL(guid1_url->GetLocalData(), guid1_url->GetAccountData()));
  EXPECT_EQ(Time::FromTimeT(90), updated_turl->last_modified());

  // Modify a single field of the initial data.
  std::unique_ptr<TemplateURL> temp_turl(Deserialize(initial_data[0]));
  TemplateURLData data(temp_turl->data());
  data.SetShortName(u"SomethingDifferent");
  temp_turl = std::make_unique<TemplateURL>(data);
  initial_data.clear();
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(temp_turl->data()));

  // Remerge the data again. This simulates shutting down and syncing again
  // at a different time, but the cloud data has not changed.
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  sync_processor_wrapper_ =
      std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
          sync_processor_.get());
  error = MergeAndExpectNotify(initial_data, 1);
  ASSERT_FALSE(error.has_value());

  // Check that the account data was applied again.
  const TemplateURL* reupdated_turl = model()->GetTemplateURLForGUID("guid1");
  ASSERT_TRUE(reupdated_turl);
  ASSERT_NE(updated_turl->data(), reupdated_turl->data());
  ASSERT_EQ(reupdated_turl->short_name(), u"SomethingDifferent");
  ASSERT_EQ(updated_turl->short_name(), u"unittest");
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       SyncMergeUpdatesDefault) {
  // If the value from Sync is a duplicate of the local default and is newer, it
  // should safely replace the local value and set as the new default.
  TemplateURL* default_turl = model()->Add(
      CreateTestTemplateURL(u"key1", "http://key1.com/{searchTerms}",
                            "whateverguid", base::Time::FromTimeT(10)));
  model()->SetUserSelectedDefaultSearchProvider(default_turl);
  ASSERT_EQ(model()->GetDefaultSearchProvider(), default_turl);

  syncer::SyncDataList initial_data = CreateInitialSyncData();
  // The guid1 entry should be a duplicate of the default.
  std::unique_ptr<TemplateURL> turl(
      CreateTestTemplateURL(u"key1", "http://key1.com/{searchTerms}", "guid1",
                            base::Time::FromTimeT(90)));
  initial_data[0] =
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data());
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    CreateInitialSyncData(), PassProcessor());

  ASSERT_EQ(model()->GetDefaultSearchProvider(), default_turl);
  EXPECT_FALSE(model()->GetTemplateURLForGUID("whateverguid"));
  EXPECT_EQ(model()->GetDefaultSearchProvider(),
            model()->GetTemplateURLForGUID("guid1"));
  EXPECT_TRUE(default_turl->GetLocalData());
  // Account data itself is not merged, only the guid of the local turl is
  // updated.
  EXPECT_FALSE(default_turl->GetAccountData());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       LocalDefaultWinsConflict) {
  // We expect that the local default always wins keyword conflict resolution.
  const std::u16string keyword(u"key1");
  const std::string url("http://whatever.com/{searchTerms}");
  TemplateURL* default_turl = model()->Add(CreateTestTemplateURL(
      keyword, url, "whateverguid", base::Time::FromTimeT(10)));
  model()->SetUserSelectedDefaultSearchProvider(default_turl);

  syncer::SyncDataList initial_data = CreateInitialSyncData();
  // The guid1 entry should be different from the default but conflict in the
  // keyword.
  std::unique_ptr<TemplateURL> turl =
      CreateTestTemplateURL(keyword, "http://key1.com/{searchTerms}", "guid1",
                            base::Time::FromTimeT(90));
  initial_data[0] =
      TemplateURLService::CreateSyncDataFromTemplateURLData(turl->data());
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    CreateInitialSyncData(), PassProcessor());

  // Since the local default was not yet synced, it should be merged with the
  // conflicting TemplateURL. However, its values should have been preserved
  // since it would have won conflict resolution due to being the default.
  const TemplateURL* winner = model()->GetTemplateURLForGUID("guid1");
  ASSERT_TRUE(winner);
  EXPECT_EQ(model()->GetDefaultSearchProvider(), winner);
  EXPECT_EQ(keyword, winner->keyword());
  EXPECT_EQ(url, winner->url());
  EXPECT_TRUE(winner->GetLocalData());
  // Account data is not merged and basically ignored.
  EXPECT_FALSE(winner->GetAccountData());

  // There is no loser, as the two were merged together. The local sync_guid
  // should no longer be found in the model.
  const TemplateURL* loser = model()->GetTemplateURLForGUID("whateverguid");
  ASSERT_FALSE(loser);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       PreSyncDeletes) {
  model()->pre_sync_deletes_.insert("guid1");
  model()->pre_sync_deletes_.insert("guid2");
  model()->pre_sync_deletes_.insert("aaa");
  model()->Add(CreateTestTemplateURL(u"whatever", "http://key1.com", "bbb"));
  MergeAndExpectNotify(CreateInitialSyncData(), 1);

  // Model shouldhave GUIDs {bbb, guid3} after initial merge.
  EXPECT_EQ(1U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_TRUE(model()->GetTemplateURLForGUID("bbb"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid3"));
  syncer::SyncChange change = processor()->change_for_guid("guid1");
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, change.change_type());
  change = processor()->change_for_guid("guid2");
  EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, change.change_type());
  // "aaa" should have been pruned out on account of not being from Sync.
  EXPECT_FALSE(processor()->contains_guid("aaa"));
  // The set of pre-sync deletes should be cleared so they're not reused if
  // MergeDataAndStartSyncing gets called again.
  EXPECT_TRUE(model()->pre_sync_deletes_.empty());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       PreSyncUpdates) {
  const char16_t kNewKeyword16[] = u"somethingnew";
  // Fetch the prepopulate search engines so we know what they are.
  std::vector<std::unique_ptr<TemplateURLData>> prepop_turls =
      TemplateURLPrepopulateData::ResolverFactory::GetForProfile(profile_a())
          ->GetPrepopulatedEngines();

  std::vector<std::unique_ptr<TemplateURLData>> starter_pack_turls =
      TemplateURLStarterPackData::GetStarterPackEngines();

  // We have to prematurely exit this test if for some reason this machine does
  // not have any prepopulate TemplateURLs.
  ASSERT_FALSE(prepop_turls.empty());

  // Create a copy of the first TemplateURL with a really old timestamp and a
  // new keyword. Add it to the model.
  TemplateURLData data_copy(*prepop_turls[0]);
  data_copy.last_modified = Time::FromTimeT(10);
  std::u16string original_keyword = data_copy.keyword();
  data_copy.SetKeyword(kNewKeyword16);
  // Set safe_for_autoreplace to false so our keyword survives.
  data_copy.safe_for_autoreplace = false;
  model()->Add(std::make_unique<TemplateURL>(data_copy));

  // Merge the prepopulate search engines.
  base::Time pre_merge_time = base::Time::Now();
  base::RunLoop().RunUntilIdle();
  test_util_a_->ResetModel(true);

  // The newly added search engine should have been safely merged, with an
  // updated time.
  TemplateURL* added_turl = model()->GetTemplateURLForKeyword(kNewKeyword16);
  ASSERT_TRUE(added_turl);
  base::Time new_timestamp = added_turl->last_modified();
  EXPECT_GE(new_timestamp, pre_merge_time);
  std::string sync_guid = added_turl->sync_guid();

  // Bring down a copy of the prepopulate engine from Sync with the old values,
  // including the old timestamp and the same GUID. Ensure that it loses
  // conflict resolution against the local value. The new timestamp should be
  // preserved.
  syncer::SyncDataList initial_data;
  data_copy.SetKeyword(original_keyword);
  data_copy.sync_guid = sync_guid;
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(data_copy));

  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  ASSERT_EQ(added_turl, model()->GetTemplateURLForKeyword(kNewKeyword16));
  EXPECT_EQ(new_timestamp, added_turl->last_modified());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       SyncWithExtensionDefaultSearch) {
  // First start off with a few entries and make sure we can set an extension
  // default search provider.
  MergeAndExpectNotify(CreateInitialSyncData(), 1);
  model()->SetUserSelectedDefaultSearchProvider(
      model()->GetTemplateURLForGUID("guid2"));

  const size_t sync_engines_count =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES).size();
  EXPECT_EQ(3U, sync_engines_count);
  ASSERT_TRUE(model()->GetDefaultSearchProvider());

  // Change the default search provider to an extension one.
  std::unique_ptr<TemplateURLData> extension =
      GenerateDummyTemplateURLData("extensiondefault");
  auto ext_dse = std::make_unique<TemplateURL>(
      *extension, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, "ext", Time(),
      true);
  test_util_a_->AddExtensionControlledTURL(std::move(ext_dse));

  const TemplateURL* dsp_turl = model()->GetDefaultSearchProvider();
  EXPECT_TRUE(model()->IsExtensionControlledDefaultSearch());

  // Extension-related changes to the DSE should not be synced as search engine
  // changes.
  EXPECT_EQ(0u, processor()->change_list_size());
  EXPECT_EQ(sync_engines_count,
            model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // Add a new entry from Sync. It should still sync in despite the default
  // being extension controlled.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"newkeyword", "http://new.com/{searchTerms}",
                            "newdefault")));
  ProcessAndExpectNotify(changes, 1);

  EXPECT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // Change kSyncedDefaultSearchProviderGUID to point to the new entry and
  // ensure that the DSP remains extension controlled.
  auto* prefs = profile_a()->GetTestingPrefService();
  ASSERT_TRUE(prefs);
  SetDefaultSearchProviderGuidToPrefs(*prefs, "newdefault");

  EXPECT_EQ(dsp_turl, model()->GetDefaultSearchProvider());
  EXPECT_TRUE(model()->IsExtensionControlledDefaultSearch());

  // Remove extension DSE. Ensure that the DSP changes to the expected pending
  // entry from Sync.
  const TemplateURL* expected_default =
      model()->GetTemplateURLForGUID("newdefault");
  test_util_a_->RemoveExtensionControlledTURL("ext");

  EXPECT_EQ(expected_default, model()->GetDefaultSearchProvider());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       StopSyncingRemovesAccountOnlyTemplateURLs) {
  // Add local and account template urls.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"localguid"));
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  ASSERT_THAT(model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                                initial_data, PassProcessor()),
              Eq(std::nullopt));
  ASSERT_TRUE(model()->GetTemplateURLForGUID("accountguid"));
  ASSERT_TRUE(model()->GetTemplateURLForGUID("localguid"));

  base::HistogramTester histogram_tester;
  model()->StopSyncing(syncer::SEARCH_ENGINES);

  // Only account template urls should get removed.
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_TRUE(model()->GetTemplateURLForGUID("localguid"));
  // Logged when removing the account only turl.
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.HasLocalDataDuringStopSyncing2", false, 1);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       StopSyncingRemovesAccountData) {
  // Add local and account template urls.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com", /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(10)));
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"guid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  ASSERT_THAT(model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                                initial_data, PassProcessor()),
              Eq(std::nullopt));

  // Account value wins as it has a more recent last_modified time.
  const TemplateURL* turl = model()->GetTemplateURLForGUID("guid");
  ASSERT_TRUE(turl);
  ASSERT_EQ(turl->keyword(), u"accountkey");
  ASSERT_EQ(turl->url(), "http://accounturl.com");

  base::HistogramTester histogram_tester;
  model()->StopSyncing(syncer::SEARCH_ENGINES);

  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.HasLocalDataDuringStopSyncing2", true, 1);
  // Only account data is removed.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid"));
  EXPECT_EQ(turl->keyword(), u"localkey");
  EXPECT_EQ(turl->url(), "http://localurl.com");
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       StopSyncingDoesNotRemoveLocalData) {
  // Add local and account template urls.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"abc", /*url=*/"http://abc.com", /*guid=*/"guid1",
      /*last_modified=*/base::Time::FromTimeT(100)));
  std::optional<syncer::ModelError> merge_error =
      model()->MergeDataAndStartSyncing(
          syncer::SEARCH_ENGINES,
          CreateInitialSyncData(/*last_modified=*/base::Time::FromTimeT(10)),
          PassProcessor());
  ASSERT_FALSE(merge_error.has_value());

  // Local value wins as it has a more recent last_modified time.
  const TemplateURL* turl = model()->GetTemplateURLForGUID("guid1");
  ASSERT_TRUE(turl);
  ASSERT_EQ(turl->keyword(), u"abc");
  ASSERT_EQ(turl->url(), "http://abc.com");

  model()->StopSyncing(syncer::SEARCH_ENGINES);

  // Account data is removed, but local value still persists.
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid1"));
  EXPECT_EQ(turl->keyword(), u"abc");
  EXPECT_EQ(turl->url(), "http://abc.com");
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       StopSyncingRemovesAccountValueOfPreexistingDefaultSearchProvider) {
  // Add local template url.
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(10)));
  // Set `turl` as the default search provider.
  model()->SetUserSelectedDefaultSearchProvider(turl);

  syncer::SyncDataList initial_data;
  // Add an account template url with the same GUID.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/turl->sync_guid(),
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  // Account keyword has more recent timestamp and thus wins.
  ASSERT_FALSE(model()->GetTemplateURLForKeyword(u"localkey"));
  ASSERT_EQ(turl, model()->GetTemplateURLForKeyword(u"accountkey"));
  ASSERT_EQ(turl, model()->GetDefaultSearchProvider());

  ASSERT_THAT(turl->GetLocalData(),
              Optional(Property(&TemplateURLData::keyword, u"localkey")));
  ASSERT_THAT(turl->GetAccountData(),
              Optional(Property(&TemplateURLData::keyword, u"accountkey")));

  base::HistogramTester histogram_tester;
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(turl, model()->GetDefaultSearchProvider());
  // The local value takes over.
  EXPECT_EQ(turl, model()->GetTemplateURLForKeyword(u"localkey"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"accountkey"));
  EXPECT_THAT(turl->GetLocalData(),
              Optional(Property(&TemplateURLData::keyword, u"localkey")));
  // The account value is removed.
  EXPECT_FALSE(turl->GetAccountData());
  // The histogram is not logged since a local value already existed.
  histogram_tester.ExpectTotalCount(
      "Sync.SearchEngine.AccountDefaultSearchEngineCopiedToLocal", 0);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       StopSyncingRemovesAccountValueOfNewlySetDefaultSearchProvider) {
  // Add local template url.
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(10)));

  // Add another local template url, which is the default search provider.
  TemplateURL* dse = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey2", /*url=*/"http://localurl2.com",
      /*guid=*/"guid2",
      /*last_modified=*/base::Time::FromTimeT(10)));
  model()->SetUserSelectedDefaultSearchProvider(dse);

  syncer::SyncDataList initial_data;
  // Add an account template url with the same GUID as the local non-default
  // one.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/turl->sync_guid(),
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  // Account keyword has more recent timestamp and thus wins over the
  // non-default local keyword.
  ASSERT_FALSE(model()->GetTemplateURLForKeyword(u"localkey"));
  ASSERT_EQ(turl, model()->GetTemplateURLForKeyword(u"accountkey"));
  ASSERT_NE(turl, model()->GetDefaultSearchProvider());

  // Set `turl` as the default search provider.
  model()->SetUserSelectedDefaultSearchProvider(turl);

  ASSERT_EQ(turl, model()->GetDefaultSearchProvider());
  ASSERT_THAT(turl->GetLocalData(),
              Optional(Property(&TemplateURLData::keyword, u"localkey")));
  ASSERT_THAT(turl->GetAccountData(),
              Optional(Property(&TemplateURLData::keyword, u"accountkey")));

  base::HistogramTester histogram_tester;
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  // The default search provider has not changed.
  EXPECT_EQ(turl, model()->GetDefaultSearchProvider());
  // The local value takes over.
  EXPECT_EQ(turl, model()->GetTemplateURLForKeyword(u"localkey"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"accountkey"));
  EXPECT_THAT(turl->GetLocalData(),
              Optional(Property(&TemplateURLData::keyword, u"localkey")));
  // The account value is removed.
  EXPECT_FALSE(turl->GetAccountData());
  // The histogram is not logged since a local value already existed.
  histogram_tester.ExpectTotalCount(
      "Sync.SearchEngine.AccountDefaultSearchEngineCopiedToLocal", 0);
}

// Regression test for crbug.com/401189582.
// Tests that an account-only default search provider is copied to local upon
// sync stop.
TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       StopSyncingCopiesAccountValueToLocalForAccountDefaultSearchProvider) {
  syncer::SyncDataList initial_data;
  // Add an account template url.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  TemplateURL* turl = model()->GetTemplateURLForGUID("accountguid");
  ASSERT_NE(turl, model()->GetDefaultSearchProvider());

  // Set `turl` as the default search provider.
  model()->SetUserSelectedDefaultSearchProvider(turl);

  ASSERT_EQ(turl, model()->GetDefaultSearchProvider());
  ASSERT_FALSE(turl->GetLocalData());
  ASSERT_THAT(turl->GetAccountData(),
              Optional(Property(&TemplateURLData::keyword, u"accountkey")));

  base::HistogramTester histogram_tester;
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(turl, model()->GetDefaultSearchProvider());
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.AccountDefaultSearchEngineCopiedToLocal", true, 1);
  // Since only the account value existed, it was copied to local to avoid any
  // unsafe behavior.
  EXPECT_EQ(turl, model()->GetTemplateURLForKeyword(u"accountkey"));
  EXPECT_THAT(turl->GetLocalData(),
              Optional(Property(&TemplateURLData::keyword, u"accountkey")));
  // The account data is moved to local.
  EXPECT_FALSE(turl->GetAccountData());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ProcessSyncUpdatesHandlesAdd) {
  MergeAndExpectNotify(syncer::SyncDataList{}, 0);

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"accountkey", "http://accounturl.com",
                            "accountguid")));
  ProcessAndExpectNotify(changes, 1);

  EXPECT_EQ(1U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_TRUE(model()->GetTemplateURLForGUID("accountguid"));

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid"));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ProcessSyncUpdatesHandlesAddUponConflict) {
  // Add local template url.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com", /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(10)));

  MergeAndExpectNotify(syncer::SyncDataList{}, 0);

  syncer::SyncChangeList changes;
  changes.push_back(
      CreateTestSyncChange(syncer::SyncChange::ACTION_ADD,
                           CreateTestTemplateURL(
                               /*keyword=*/u"accountkey",
                               /*url=*/"http://accounturl.com", /*guid=*/"guid",
                               /*last_modified=*/base::Time::FromTimeT(100))));
  ProcessAndExpectNotify(changes, 1);

  EXPECT_EQ(1U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  const TemplateURL* turl = model()->GetTemplateURLForGUID("guid");
  ASSERT_TRUE(turl);
  EXPECT_TRUE(turl->GetLocalData());
  EXPECT_TRUE(turl->GetAccountData());
  EXPECT_EQ(u"accountkey", turl->keyword());
  EXPECT_EQ("http://accounturl.com", turl->url());

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_EQ(u"localkey", model()->GetTemplateURLForGUID("guid")->keyword());
  EXPECT_EQ("http://localurl.com",
            model()->GetTemplateURLForGUID("guid")->url());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ProcessSyncUpdatesErrorsUponRemoveWhenNoAccountData) {
  MergeAndExpectNotify(syncer::SyncDataList{}, 0);

  // Add a template url.
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://url.com", /*guid=*/"guid"));

  syncer::SyncChangeList changes;
  // DELETE for a non-existent account turl.
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_DELETE,
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100))));
  std::optional<syncer::ModelError> error = ProcessAndExpectNotify(changes, 0);
  // ProcessSyncUpdates() returns an error.
  EXPECT_TRUE(error.has_value());

  EXPECT_EQ(1U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  EXPECT_TRUE(turl->GetLocalData());
  EXPECT_TRUE(turl->GetAccountData());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ProcessSyncUpdatesHandlesRemove) {
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  MergeAndExpectNotify(initial_data, 1);

  ASSERT_EQ(1U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  ASSERT_TRUE(model()->GetTemplateURLForGUID("accountguid"));

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_DELETE,
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100))));
  ProcessAndExpectNotify(changes, 1);

  EXPECT_EQ(0U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid"));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ProcessSyncUpdatesHandlesRemoveWhenConflict) {
  // Add local template url.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com", /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(10)));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"guid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  MergeAndExpectNotify(initial_data, 1);

  ASSERT_EQ(1U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  const TemplateURL* turl = model()->GetTemplateURLForGUID("guid");
  ASSERT_TRUE(turl);
  EXPECT_EQ(u"accountkey", turl->keyword());
  EXPECT_EQ("http://accounturl.com", turl->url());

  syncer::SyncChangeList changes;
  changes.push_back(
      CreateTestSyncChange(syncer::SyncChange::ACTION_DELETE,
                           CreateTestTemplateURL(
                               /*keyword=*/u"accountkey",
                               /*url=*/"http://accounturl.com", /*guid=*/"guid",
                               /*last_modified=*/base::Time::FromTimeT(100))));
  ProcessAndExpectNotify(changes, 1);

  EXPECT_EQ(0U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  ASSERT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_EQ(u"localkey", turl->keyword());
  EXPECT_EQ("http://localurl.com", turl->url());

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_EQ(u"localkey", model()->GetTemplateURLForGUID("guid")->keyword());
  EXPECT_EQ("http://localurl.com",
            model()->GetTemplateURLForGUID("guid")->url());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ProcessSyncUpdatesHandlesUpdate) {
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  MergeAndExpectNotify(initial_data, 1);

  const TemplateURL* turl = model()->GetTemplateURLForGUID("accountguid");
  ASSERT_TRUE(turl);
  ASSERT_FALSE(turl->GetLocalData());
  EXPECT_EQ(u"accountkey", turl->keyword());
  EXPECT_EQ("http://accounturl.com", turl->url());

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(u"newkey", "http://newurl.com", "accountguid")));
  ProcessAndExpectNotify(changes, 1);

  ASSERT_EQ(turl, model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_FALSE(turl->GetLocalData());
  EXPECT_EQ(u"newkey", turl->keyword());
  EXPECT_EQ("http://newurl.com", turl->url());

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid"));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ProcessSyncUpdatesHandlesUpdateWhenConflict) {
  // Add local template url.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com", /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(10)));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"guid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  MergeAndExpectNotify(initial_data, 1);

  const TemplateURL* turl = model()->GetTemplateURLForGUID("guid");
  ASSERT_TRUE(turl);
  EXPECT_EQ(u"accountkey", turl->keyword());
  EXPECT_EQ("http://accounturl.com", turl->url());

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(
          /*keyword=*/u"newkey", /*url=*/"http://newurl.com", /*guid=*/"guid",
          /*last_modified=*/base::Time::FromTimeT(100))));
  ProcessAndExpectNotify(changes, 1);

  ASSERT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  EXPECT_EQ(u"newkey", turl->keyword());
  EXPECT_EQ("http://newurl.com", turl->url());

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_EQ(u"localkey", model()->GetTemplateURLForGUID("guid")->keyword());
  EXPECT_EQ("http://localurl.com",
            model()->GetTemplateURLForGUID("guid")->url());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       GetAllSyncDataDoesNotCountLocalOnlySearchEngines) {
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key1", /*url=*/"http://url1.com", /*guid=*/"guid1"));
  EXPECT_EQ(0u, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, syncer::SyncDataList{}, PassProcessor()));

  EXPECT_EQ(0u, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key2", /*url=*/"http://url2.com", /*guid=*/"guid2"));
  EXPECT_EQ(1u, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       AddOnlyLocalValueIfNotSyncing) {
  const TemplateURL* turl1 = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key1", /*url=*/"http://url1.com", /*guid=*/"guid1"));

  ASSERT_TRUE(turl1);
  EXPECT_THAT(GetKeywordsFromDatabase(), Contains(turl1->data()));
  EXPECT_EQ(turl1, model()->GetTemplateURLForGUID("guid1"));
  EXPECT_FALSE(turl1->GetAccountData());

  std::unique_ptr<TemplateURL> local_turl = CreateTestTemplateURL(
      /*keyword=*/u"localkeyword", /*url=*/"http://localurl.com",
      /*guid=*/"guid2-local", /*last_modified=*/base::Time::FromTimeT(10));
  std::unique_ptr<TemplateURL> account_turl = CreateTestTemplateURL(
      /*keyword=*/u"accountkeyword", /*url=*/"http://accounturl.com",
      /*guid=*/"guid2-account", /*last_modified=*/base::Time::FromTimeT(100));
  const TemplateURL* turl2 = model()->Add(
      std::make_unique<TemplateURL>(local_turl->data(), account_turl->data()));
  ASSERT_TRUE(turl2);
  EXPECT_THAT(GetKeywordsFromDatabase(), Contains(turl2->data()));
  EXPECT_EQ(turl2, model()->GetTemplateURLForGUID("guid2-local"));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("guid2-account"));
  EXPECT_FALSE(turl2->GetAccountData());

  EXPECT_FALSE(model()->Add(std::make_unique<TemplateURL>(
      std::nullopt,
      CreateTestTemplateURL(
          /*keyword=*/u"key3", /*url=*/"http://url3.com", /*guid=*/"guid3")
          ->data())));
  EXPECT_THAT(GetKeywordsFromDatabase(),
              Not(Contains(Field(&TemplateURLData::sync_guid, "guid3"))));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("guid3"));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       AddOnlyAccountValueIfFromSync) {
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid")
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  const TemplateURL* turl = model()->GetTemplateURLForGUID("accountguid");
  ASSERT_TRUE(turl);
  EXPECT_FALSE(turl->GetLocalData());
  EXPECT_THAT(GetKeywordsFromDatabase(),
              Not(Contains(Field(&TemplateURLData::sync_guid, "accountguid"))));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       DualWriteUponAddingLocalOnlySearchEngine) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  base::HistogramTester histogram_tester;
  // Add a template url.
  const TemplateURLData data =
      CreateTestTemplateURL(
          /*keyword=*/u"abc", /*url=*/"http://abc.com", /*guid=*/"guid")
          ->data();
  model()->Add(std::make_unique<TemplateURL>(data, std::nullopt));

  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.AddedKeywordHasAccountData", false, 1);
  // Both local and account values should have been populated.
  const TemplateURL* turl = model()->GetTemplateURLForGUID("guid");
  ASSERT_TRUE(turl);
  EXPECT_EQ(turl->GetLocalData(), turl->GetAccountData());
  EXPECT_THAT(GetKeywordsFromDatabase(),
              Contains(Field(&TemplateURLData::sync_guid, "guid")));
  EXPECT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_ADD);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       DualWriteUponAddingAccountOnlySearchEngine) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  base::HistogramTester histogram_tester;
  // Add a template url.
  const TemplateURLData data =
      CreateTestTemplateURL(
          /*keyword=*/u"abc", /*url=*/"http://abc.com", /*guid=*/"guid")
          ->data();
  model()->Add(std::make_unique<TemplateURL>(std::nullopt, data));

  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.AddedKeywordHasAccountData", true, 1);
  // Both local and account values should have been populated.
  const TemplateURL* turl = model()->GetTemplateURLForGUID("guid");
  ASSERT_TRUE(turl);
  EXPECT_EQ(turl->GetLocalData(), turl->GetAccountData());
  EXPECT_THAT(GetKeywordsFromDatabase(),
              Contains(Field(&TemplateURLData::sync_guid, "guid")));
  EXPECT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_ADD);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       DualWriteUponAddingSearchEngineWithLocalAndAccountData) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  base::HistogramTester histogram_tester;
  // Add a template url.
  const TemplateURLData data =
      CreateTestTemplateURL(
          /*keyword=*/u"abc", /*url=*/"http://abc.com", /*guid=*/"guid")
          ->data();
  model()->Add(std::make_unique<TemplateURL>(data, data));

  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.AddedKeywordHasAccountData", true, 1);
  // Both local and account values should have been populated.
  const TemplateURL* turl = model()->GetTemplateURLForGUID("guid");
  ASSERT_TRUE(turl);
  EXPECT_EQ(turl->GetLocalData(), turl->GetAccountData());
  EXPECT_THAT(GetKeywordsFromDatabase(),
              Contains(Field(&TemplateURLData::sync_guid, "guid")));
  EXPECT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_ADD);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       RemoveLocalOnlySearchEngine) {
  // Add a local search engine.
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"localguid"));

  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());
  ASSERT_EQ(0u, processor()->change_list_size());

  ASSERT_THAT(GetKeywordsFromDatabase(), Contains(turl->data()));
  model()->Remove(turl);

  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"localkey"));
  // Nothing should be committed since there was no account data.
  EXPECT_EQ(0u, processor()->change_list_size());
  EXPECT_THAT(GetKeywordsFromDatabase(),
              Not(Contains(Field(&TemplateURLData::sync_guid, "localguid"))));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       RemoveAccountOnlySearchEngine) {
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid")
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());
  ASSERT_EQ(0u, processor()->change_list_size());
  ASSERT_THAT(GetKeywordsFromDatabase(),
              Not(Contains(Field(&TemplateURLData::sync_guid, "accountguid"))));

  const TemplateURL* turl = model()->GetTemplateURLForGUID("accountguid");
  model()->Remove(turl);

  ASSERT_THAT(GetKeywordsFromDatabase(),
              Not(Contains(Field(&TemplateURLData::sync_guid, "accountguid"))));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"accountkey"));
  // Deletion should be committed.
  EXPECT_EQ(1u, processor()->change_list_size());
  EXPECT_EQ(processor()->change_for_guid("accountguid").change_type(),
            syncer::SyncChange::ACTION_DELETE);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       RemoveSearchEngineWithLocalAndAccountData) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://url.com", /*guid=*/"guid"));
  ASSERT_EQ(turl->GetLocalData(), turl->GetAccountData());
  ASSERT_THAT(GetKeywordsFromDatabase(),
              Contains(Field(&TemplateURLData::sync_guid, "guid")));
  ASSERT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_ADD);

  model()->Remove(turl);
  ASSERT_THAT(GetKeywordsFromDatabase(),
              Not(Contains(Field(&TemplateURLData::sync_guid, "guid"))));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("guid"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"key"));
  // Deletion should be committed.
  EXPECT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_DELETE);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       RemoveSearchEngineWithDifferentLocalAndAccountData) {
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://localurl.com", /*guid=*/"localguid"));
  ASSERT_TRUE(model()->GetTemplateURLForGUID("localguid"));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"key", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid")
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());
  ASSERT_EQ(0u, processor()->change_list_size());
  ASSERT_THAT(
      GetKeywordsFromDatabase(),
      Contains(AllOf(Property(&TemplateURLData::keyword, u"key"),
                     Field(&TemplateURLData::sync_guid, "accountguid"))));
  ASSERT_THAT(
      GetKeywordsFromDatabase(),
      Not(Contains(AllOf(Field(&TemplateURLData::sync_guid, "localguid")))));
  ASSERT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  const TemplateURL* turl = model()->GetTemplateURLForGUID("accountguid");
  ASSERT_TRUE(turl);

  model()->Remove(turl);
  ASSERT_THAT(GetKeywordsFromDatabase(),
              Not(Contains(Property(&TemplateURLData::keyword, u"key"))));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"key"));
  // Deletion should be committed.
  EXPECT_EQ(1u, processor()->change_list_size());
  EXPECT_EQ(processor()->change_for_guid("accountguid").change_type(),
            syncer::SyncChange::ACTION_DELETE);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldAddIntoDatabaseUponUpdateIfNotExistingEarlier) {
  // Start syncing.
  syncer::SyncDataList initial_data;
  // Account-only search engine.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"key", /*url=*/"http://url.com",
          /*guid=*/"guid")
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  // Account data is not added to the database.
  ASSERT_THAT(GetKeywordsFromDatabase(),
              Not(Contains(Property(&TemplateURLData::keyword, u"key"))));

  TemplateURL* turl = model()->GetTemplateURLForKeyword(u"key");
  // Update the account-only search engine.
  model()->ResetTemplateURL(turl, u"newtitle", u"newkey", "http://newurl.com");

  // This should write the updated data to local and thus add to the database.
  EXPECT_EQ(turl, model()->GetTemplateURLForKeyword(u"newkey"));
  EXPECT_TRUE(turl->GetLocalData());
  EXPECT_THAT(
      GetKeywordsFromDatabase(),
      ElementsAre(AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                        Property(&TemplateURLData::keyword, u"newkey"))));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotSendUpdateToSyncIfAccountDataIsUnchanged) {
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(100)));

  // Start syncing.
  syncer::SyncDataList initial_data;
  // Local turl has the more recent last_modified time and thus is the active
  // value.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"guid", /*last_modified=*/base::Time::FromTimeT(10))
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());
  ASSERT_EQ(0u, processor()->change_list_size());

  const base::Time time_now = base::Time::Now();
  const base::Time null_time;
  ASSERT_NE(time_now, null_time);

  // Update last_visited time for `turl`. This should update the local value.
  model()->UpdateTemplateURLVisitTime(turl);
  EXPECT_NE(turl->GetLocalData(), turl->GetAccountData());
  // Local last_visited has been updated whereas the account last_visited stays
  // null.
  EXPECT_GE(turl->GetLocalData()->last_visited, time_now);
  EXPECT_EQ(turl->GetAccountData()->last_visited, null_time);

  // No change is committed since only the local data was updated.
  EXPECT_EQ(0u, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldDualWriteUponResetTemplateURLIfLocalOnly) {
  // Local only search engine.
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://url.com", /*guid=*/"guid"));

  // Start syncing.
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());
  ASSERT_EQ(0u, processor()->change_list_size());

  // Update the local-only search engine.
  model()->ResetTemplateURL(turl, u"newtitle", u"newkey", "http://newurl.com");

  // This should update and write the new value to both local and account.
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"key"));
  EXPECT_EQ(turl, model()->GetTemplateURLForKeyword(u"newkey"));
  EXPECT_EQ(turl->GetLocalData(), turl->GetAccountData());
  EXPECT_THAT(turl, Pointee(AllOf(Property(&TemplateURL::sync_guid, "guid"),
                                  Property(&TemplateURL::keyword, u"newkey"))));
  EXPECT_THAT(
      GetKeywordsFromDatabase(),
      ElementsAre(AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                        Property(&TemplateURLData::keyword, u"newkey"))));
  // Update should be committed.
  EXPECT_EQ(1u, processor()->change_list_size());
  EXPECT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldDualWriteUponResetTemplateURLIfAccountOnly) {
  // Start syncing.
  syncer::SyncDataList initial_data;
  // Account-only search engine.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"key", /*url=*/"http://url.com",
          /*guid=*/"guid")
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  // Account search engine is not added to the database.
  ASSERT_THAT(GetKeywordsFromDatabase(),
              Not(Contains(Property(&TemplateURLData::keyword, u"key"))));

  TemplateURL* turl = model()->GetTemplateURLForKeyword(u"key");
  model()->ResetTemplateURL(turl, u"newtitle", u"newkey", "http://newurl.com");

  // This should update and write the new value to both local and account.
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"key"));
  EXPECT_EQ(turl, model()->GetTemplateURLForKeyword(u"newkey"));
  EXPECT_EQ(turl->GetLocalData(), turl->GetAccountData());
  EXPECT_THAT(turl, Pointee(AllOf(Property(&TemplateURL::sync_guid, "guid"),
                                  Property(&TemplateURL::keyword, u"newkey"))));
  EXPECT_THAT(
      GetKeywordsFromDatabase(),
      ElementsAre(AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                        Property(&TemplateURLData::keyword, u"newkey"))));
  // Update should be committed.
  EXPECT_EQ(1u, processor()->change_list_size());
  EXPECT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldDualWriteUponResetTemplateURL) {
  // Start syncing.
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  // Add a search engine to local and account.
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://url.com", /*guid=*/"guid"));

  // Newly added search engine is dual written to both local and account.
  ASSERT_EQ(turl->GetLocalData(), turl->GetAccountData());
  ASSERT_THAT(GetKeywordsFromDatabase(),
              ElementsAre(AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                                Property(&TemplateURLData::keyword, u"key"))));
  ASSERT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_ADD);

  model()->ResetTemplateURL(turl, u"newtitle", u"newkey", "http://newurl.com");

  // This should update and write the new value to both local and account.
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"key"));
  EXPECT_EQ(turl, model()->GetTemplateURLForKeyword(u"newkey"));
  EXPECT_EQ(turl->GetLocalData(), turl->GetAccountData());
  EXPECT_THAT(turl, Pointee(AllOf(Property(&TemplateURL::sync_guid, "guid"),
                                  Property(&TemplateURL::keyword, u"newkey"))));
  EXPECT_THAT(
      GetKeywordsFromDatabase(),
      ElementsAre(AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                        Property(&TemplateURLData::keyword, u"newkey"))));
  // Update should be committed.
  EXPECT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldDualWriteUponSetIsActiveTemplateURL) {
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://url.com", /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kFalse));

  // Start syncing.
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  ASSERT_EQ(turl->is_active(), TemplateURLData::ActiveStatus::kFalse);

  model()->SetIsActiveTemplateURL(turl, /*is_active=*/true);
  // This should update and write the activated turl to both local and account.
  ASSERT_EQ(turl->is_active(), TemplateURLData::ActiveStatus::kTrue);
  EXPECT_EQ(turl->GetLocalData(), turl->GetAccountData());
  EXPECT_THAT(GetKeywordsFromDatabase(),
              ElementsAre(AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                                Field(&TemplateURLData::is_active,
                                      TemplateURLData::ActiveStatus::kTrue))));
  // Update should be committed.
  EXPECT_EQ(1u, processor()->change_list_size());
  EXPECT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotDualWriteUponUpdateProviderFavicons) {
  // Local-only search engine.
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://url.com", /*guid=*/"guid"));

  // Start syncing.
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  model()->UpdateProviderFavicons(
      GURL("https://enterprise_search.com/q=searchTerm"),
      GURL("https://enterprise_search.com/newfavicon.ico"));

  // This should not write the new value to account.
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_EQ(0u, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotDualWriteUponProcessSyncUpdateChanges) {
  // Add local template url.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com", /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(10)));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"guid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  // Account data is winning.
  const TemplateURL* turl = model()->GetTemplateURLForGUID("guid");
  ASSERT_TRUE(turl);
  EXPECT_EQ(u"accountkey", turl->keyword());
  EXPECT_EQ("http://accounturl.com", turl->url());

  // Incoming incremental update.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(
          /*keyword=*/u"newkey", /*url=*/"http://newurl.com", /*guid=*/"guid",
          /*last_modified=*/base::Time::FromTimeT(100))));
  ASSERT_FALSE(model()->ProcessSyncChanges(FROM_HERE, changes));

  // This should not write the updated account value to local.
  ASSERT_THAT(turl,
              Pointee(AllOf(Property(&TemplateURL::sync_guid, "guid"),
                            Property(&TemplateURL::keyword, u"newkey"),
                            Property(&TemplateURL::url, "http://newurl.com"))));
  EXPECT_NE(turl->GetAccountData(), turl->GetLocalData());
  EXPECT_THAT(*turl->GetLocalData(),
              AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                    Property(&TemplateURLData::keyword, u"localkey"),
                    Property(&TemplateURLData::url, "http://localurl.com")));
  EXPECT_THAT(GetKeywordsFromDatabase(),
              ElementsAre(AllOf(
                  Field(&TemplateURLData::sync_guid, "guid"),
                  Property(&TemplateURLData::keyword, u"localkey"),
                  Property(&TemplateURLData::url, "http://localurl.com"))));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotDualWriteUponStopSyncingWithLocalAndAccountValue) {
  // Add local template url.
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com", /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(10)));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"guid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  ASSERT_FALSE(model()->GetTemplateURLForKeyword(u"localkey"));
  ASSERT_EQ(turl, model()->GetTemplateURLForKeyword(u"accountkey"));
  ASSERT_NE(turl->GetAccountData(), turl->GetLocalData());

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  // This should not write the local and the account value to the other store.
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_EQ(turl, model()->GetTemplateURLForKeyword(u"localkey"));
  EXPECT_THAT(
      turl, Pointee(AllOf(Property(&TemplateURL::sync_guid, "guid"),
                          Property(&TemplateURL::keyword, u"localkey"),
                          Property(&TemplateURL::url, "http://localurl.com"))));
  EXPECT_THAT(GetKeywordsFromDatabase(),
              ElementsAre(AllOf(
                  Field(&TemplateURLData::sync_guid, "guid"),
                  Property(&TemplateURLData::keyword, u"localkey"),
                  Property(&TemplateURLData::url, "http://localurl.com"))));
  EXPECT_EQ(0u, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotDualWriteUponUpdateTemplateURLVisitTime) {
  TemplateURL* turl1 = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey1", /*url=*/"http://localurl1.com",
      /*guid=*/"guid1",
      /*last_modified=*/base::Time::FromTimeT(100)));
  TemplateURL* turl2 = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey2", /*url=*/"http://localurl2.com",
      /*guid=*/"guid2", /*last_modified=*/base::Time::FromTimeT(10)));

  // Start syncing.
  syncer::SyncDataList initial_data;
  // Local turl1 has the more recent last_modified time and thus is the active
  // value.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey1", /*url=*/"http://accounturl1.com",
          /*guid=*/"guid1", /*last_modified=*/base::Time::FromTimeT(10))
          ->data()));
  // Account turl2 has the more recent last_modified time and thus is the active
  // value.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey2", /*url=*/"http://accounturl1.com",
          /*guid=*/"guid2", /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());
  ASSERT_EQ(0u, processor()->change_list_size());

  const base::Time time_now = base::Time::Now();
  const base::Time null_time;
  ASSERT_NE(time_now, null_time);

  // Update last_visited time for `turl1`. This should update the local value.
  model()->UpdateTemplateURLVisitTime(turl1);
  ASSERT_TRUE(turl1->GetLocalData());
  ASSERT_TRUE(turl1->GetAccountData());
  EXPECT_NE(turl1->GetLocalData(), turl1->GetAccountData());
  // Local last_visited has been updated whereas the account last_visited stays
  // null.
  EXPECT_GE(turl1->GetLocalData()->last_visited, time_now);
  EXPECT_EQ(turl1->GetAccountData()->last_visited, null_time);
  // No change is committed since only the local data was updated.
  EXPECT_EQ(0u, processor()->change_list_size());
  EXPECT_THAT(*turl1->GetAccountData(),
              AllOf(Field(&TemplateURLData::sync_guid, "guid1"),
                    Property(&TemplateURLData::keyword, u"accountkey1")));
  EXPECT_THAT(turl1,
              Pointee(AllOf(Property(&TemplateURL::sync_guid, "guid1"),
                            Property(&TemplateURL::keyword, u"localkey1"))));
  EXPECT_THAT(GetKeywordsFromDatabase(),
              Contains(AllOf(Field(&TemplateURLData::sync_guid, "guid1"),
                             Property(&TemplateURLData::keyword, u"localkey1"),
                             Field(&TemplateURLData::last_visited,
                                   turl1->last_visited()))));

  // Update last_visited time for `turl2`. This should update only the account
  // value.
  model()->UpdateTemplateURLVisitTime(turl2);
  ASSERT_TRUE(turl2->GetLocalData());
  ASSERT_TRUE(turl2->GetAccountData());
  EXPECT_NE(turl2->GetLocalData(), turl2->GetAccountData());
  // Account last_visited is updated whereas the local last_visited stays null.
  EXPECT_EQ(turl2->GetLocalData()->last_visited, null_time);
  EXPECT_GE(turl2->GetAccountData()->last_visited, time_now);
  // Change is committed since the account data was updated.
  EXPECT_EQ(1u, processor()->change_list_size());
  EXPECT_THAT(turl2,
              Pointee(AllOf(Property(&TemplateURL::sync_guid, "guid2"),
                            Property(&TemplateURL::keyword, u"accountkey2"))));
  EXPECT_THAT(*turl2->GetLocalData(),
              AllOf(Field(&TemplateURLData::sync_guid, "guid2"),
                    Property(&TemplateURLData::keyword, u"localkey2")));
  EXPECT_THAT(
      GetKeywordsFromDatabase(),
      Contains(AllOf(Field(&TemplateURLData::sync_guid, "guid2"),
                     Property(&TemplateURLData::keyword, u"localkey2"),
                     Field(&TemplateURLData::last_visited, null_time))));
}

TEST_F(
    TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
    ShouldNotDualWriteUponUpdateTemplateURLVisitTimeForLocalOnlyTemplateURL) {
  TemplateURL* turl1 = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey1", /*url=*/"http://localurl1.com",
      /*guid=*/"guid1",
      /*last_modified=*/base::Time::FromTimeT(100)));

  // Start syncing.
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());
  ASSERT_EQ(0u, processor()->change_list_size());

  const base::Time time_now = base::Time::Now();
  ASSERT_FALSE(time_now.is_null());

  // Update last_visited time for `turl1`. This should update the account value.
  model()->UpdateTemplateURLVisitTime(turl1);

  // No account data is created.
  EXPECT_FALSE(turl1->GetAccountData());
  EXPECT_GE(turl1->GetLocalData()->last_visited, time_now);
  // No change is committed since only the local data was updated.
  EXPECT_EQ(0u, processor()->change_list_size());
  EXPECT_THAT(turl1,
              Pointee(AllOf(Property(&TemplateURL::sync_guid, "guid1"),
                            Property(&TemplateURL::keyword, u"localkey1"))));
  EXPECT_THAT(GetKeywordsFromDatabase(),
              Contains(AllOf(Field(&TemplateURLData::sync_guid, "guid1"),
                             Property(&TemplateURLData::keyword, u"localkey1"),
                             Field(&TemplateURLData::last_visited,
                                   turl1->last_visited()))));
}

TEST_F(
    TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
    ShouldNotDualWriteUponUpdateTemplateURLVisitTimeForAccountOnlyTemplateURL) {
  // Start syncing.
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey1", /*url=*/"http://accounturl1.com",
          /*guid=*/"guid1", /*last_modified=*/base::Time::FromTimeT(10))
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());
  ASSERT_EQ(0u, processor()->change_list_size());

  const base::Time time_now = base::Time::Now();
  ASSERT_FALSE(time_now.is_null());

  TemplateURL* turl1 = model()->GetTemplateURLForGUID("guid1");
  ASSERT_TRUE(turl1);
  // Update last_visited time for `turl1`. This should update the local value.
  model()->UpdateTemplateURLVisitTime(turl1);

  // No local data is created.
  EXPECT_FALSE(turl1->GetLocalData());
  EXPECT_GE(turl1->GetAccountData()->last_visited, time_now);
  // Change is committed since the account data was updated.
  EXPECT_EQ(1u, processor()->change_list_size());
  EXPECT_THAT(turl1,
              Pointee(AllOf(Property(&TemplateURL::sync_guid, "guid1"),
                            Property(&TemplateURL::keyword, u"accountkey1"))));
  // Account search engine is not added to the database.
  EXPECT_THAT(GetKeywordsFromDatabase(), IsEmpty());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotDualWriteUponSetUserSelectedDefaultSearchProvider) {
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://url.com", /*guid=*/"guid"));
  ASSERT_NE(model()->GetDefaultSearchProvider(), turl);

  // Start syncing.
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  model()->SetUserSelectedDefaultSearchProvider(turl);
  // Default search engines are not taken care of by sync anymore.
  ASSERT_EQ(model()->GetDefaultSearchProvider(), turl);
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_EQ(0u, processor()->change_list_size());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,

       ShouldNotAddToDatabaseUponInitialMerge) {
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"key", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid")
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  EXPECT_EQ(0u, processor()->change_list_size());
  // Account search engine is not added to the database.
  EXPECT_THAT(GetKeywordsFromDatabase(), IsEmpty());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldOnlyUpdateSyncGuidUponInitialMergeIfConflict) {
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://localurl.com", /*guid=*/"localguid"));
  ASSERT_TRUE(turl);

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"key", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid")
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());
  ASSERT_EQ(0u, processor()->change_list_size());

  EXPECT_THAT(turl, Pointee(AllOf(
                        Property(&TemplateURL::sync_guid, "accountguid"),
                        Property(&TemplateURL::url, "http://accounturl.com"))));
  // Only the sync guid is updated, no other fields.
  EXPECT_THAT(GetKeywordsFromDatabase(),
              ElementsAre(AllOf(
                  Field(&TemplateURLData::sync_guid, "accountguid"),
                  Property(&TemplateURLData::url, "http://localurl.com"))));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotUpdateDatabaseEntryIfLocalHasSameGuid) {
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com", /*guid=*/"guid"));
  ASSERT_TRUE(turl);

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"guid")
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());
  ASSERT_EQ(0u, processor()->change_list_size());

  EXPECT_TRUE(turl->GetLocalData());
  EXPECT_TRUE(turl->GetAccountData());
  EXPECT_THAT(turl, Pointee(AllOf(
                        Property(&TemplateURL::sync_guid, "guid"),
                        Property(&TemplateURL::keyword, u"accountkey"),
                        Property(&TemplateURL::url, "http://accounturl.com"))));
  // Local data is unchanged.
  EXPECT_THAT(GetKeywordsFromDatabase(),
              ElementsAre(AllOf(
                  Field(&TemplateURLData::sync_guid, "guid"),
                  Property(&TemplateURLData::keyword, u"localkey"),
                  Property(&TemplateURLData::url, "http://localurl.com"))));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotAddToDatabaseUponIncrementalAdd) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey",
          /*url=*/"http://accounturl.com", /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100))));
  model()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_EQ(0u, processor()->change_list_size());
  EXPECT_THAT(
      model()->GetTemplateURLForGUID("accountguid"),
      Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey"),
                    Property(&TemplateURL::url, "http://accounturl.com"))));
  // Account search engine is not added to the database.
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid")->GetLocalData());
  EXPECT_THAT(GetKeywordsFromDatabase(), IsEmpty());
}

TEST_F(
    TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
    ShouldNotUpdateDatabaseUponIncrementalAddIfConflictWithPreexistingAccountSearchEngine) {
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey1", /*url=*/"http://accounturl1.com",
          /*guid=*/"accountguid")
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey2",
          /*url=*/"http://accounturl2.com", /*guid=*/"accountguid")));
  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(0u, processor()->change_list_size());
  const TemplateURL* turl = model()->GetTemplateURLForGUID("accountguid");
  EXPECT_FALSE(turl->GetLocalData());
  EXPECT_THAT(
      turl,
      Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey2"),
                    Property(&TemplateURL::url, "http://accounturl2.com"))));
  // Account search engine is not added to the database.
  EXPECT_THAT(GetKeywordsFromDatabase(), IsEmpty());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotUpdateDatabaseEntryIfLocalHasSameGuidUponIncrementalAdd) {
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com", /*guid=*/"guid"));
  ASSERT_TRUE(turl);

  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey",
          /*url=*/"http://accounturl.com", /*guid=*/"guid")));
  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(0u, processor()->change_list_size());
  // Local and account data are merged since the sync guid is the same.
  EXPECT_TRUE(turl->GetLocalData());
  EXPECT_TRUE(turl->GetAccountData());
  EXPECT_THAT(turl,
              Pointee(AllOf(Property(&TemplateURL::sync_guid, "guid"),
                            Property(&TemplateURL::keyword, u"accountkey"))));
  // Database entry is not updated.
  EXPECT_THAT(
      GetKeywordsFromDatabase(),
      ElementsAre(AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                        Property(&TemplateURLData::keyword, u"localkey"))));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotConflictIfSyncGuidIsDifferentUponIncrementalAdd) {
  const TemplateURL* local = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://localurl.com", /*guid=*/"localguid"));
  ASSERT_TRUE(local);

  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(
          /*keyword=*/u"key",
          /*url=*/"http://accounturl.com", /*guid=*/"accountguid")));
  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(0u, processor()->change_list_size());
  // Local and account data are not merged since the sync guid is different,
  // even though the keyword is the same.
  EXPECT_TRUE(local->GetLocalData());
  EXPECT_FALSE(local->GetAccountData());
  EXPECT_THAT(local, Pointee(AllOf(
                         Property(&TemplateURL::sync_guid, "localguid"),
                         Property(&TemplateURL::keyword, u"key"),
                         Property(&TemplateURL::url, "http://localurl.com"))));
  EXPECT_THAT(GetKeywordsFromDatabase(),
              ElementsAre(AllOf(Field(&TemplateURLData::sync_guid, "localguid"),
                                Property(&TemplateURLData::keyword, u"key"))));

  const TemplateURL* account = model()->GetTemplateURLForGUID("accountguid");
  EXPECT_FALSE(account->GetLocalData());
  EXPECT_TRUE(account->GetAccountData());
  EXPECT_THAT(
      account,
      Pointee(AllOf(Property(&TemplateURL::sync_guid, "accountguid"),
                    Property(&TemplateURL::keyword, u"key"),
                    Property(&TemplateURL::url, "http://accounturl.com"))));
  EXPECT_THAT(GetKeywordsFromDatabase(),
              Not(ElementsAre(
                  AllOf(Field(&TemplateURLData::sync_guid, "accountguid")))));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotAddToDatabaseUponIncrementalDeletion) {
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey1", /*url=*/"http://accounturl1.com",
          /*guid=*/"accountguid")
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());
  ASSERT_THAT(GetKeywordsFromDatabase(), IsEmpty());

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_DELETE,
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey2",
          /*url=*/"http://accounturl2.com", /*guid=*/"accountguid")));
  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(0u, processor()->change_list_size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_THAT(GetKeywordsFromDatabase(), IsEmpty());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotUpdateDatabaseUponIncrementalDeletionIfNonExistentAccountData) {
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://url.com", /*guid=*/"guid"));
  ASSERT_TRUE(turl);

  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  syncer::SyncChangeList changes;
  changes.push_back(
      CreateTestSyncChange(syncer::SyncChange::ACTION_DELETE,
                           CreateTestTemplateURL(
                               /*keyword=*/u"key",
                               /*url=*/"http://url.com", /*guid=*/"guid")));
  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(0u, processor()->change_list_size());
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  EXPECT_THAT(GetKeywordsFromDatabase(),
              ElementsAre(AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                                Property(&TemplateURLData::keyword, u"key"))));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotRemoveLocalUponIncrementalDeletion) {
  // Add a local-only template url.
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://url.com", /*guid=*/"guid"));

  // Start syncing.
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  syncer::SyncChangeList changes;
  changes.push_back(
      CreateTestSyncChange(syncer::SyncChange::ACTION_DELETE,
                           CreateTestTemplateURL(
                               /*keyword=*/u"key",
                               /*url=*/"http://url.com", /*guid=*/"guid")));
  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(0u, processor()->change_list_size());
  // Local search engine is not deleted.
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  EXPECT_TRUE(turl->GetLocalData());
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_THAT(GetKeywordsFromDatabase(),
              ElementsAre(AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                                Property(&TemplateURLData::keyword, u"key"))));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotAddToDatabaseUponIncrementalUpdateForNonExistentSearchEngine) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey",
          /*url=*/"http://accounturl.com", /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100))));
  model()->ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_EQ(0u, processor()->change_list_size());
  const TemplateURL* turl = model()->GetTemplateURLForGUID("accountguid");
  EXPECT_FALSE(turl->GetLocalData());
  EXPECT_THAT(
      model()->GetTemplateURLForGUID("accountguid"),
      Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey"),
                    Property(&TemplateURL::url, "http://accounturl.com"))));
  // Account search engine is not added to the database.
  EXPECT_THAT(GetKeywordsFromDatabase(), IsEmpty());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotAddToDatabaseUponIncrementalUpdate) {
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey1", /*url=*/"http://accounturl1.com",
          /*guid=*/"accountguid")
          ->data()));
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey2",
          /*url=*/"http://accounturl2.com", /*guid=*/"accountguid")));
  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(0u, processor()->change_list_size());
  EXPECT_THAT(
      model()->GetTemplateURLForGUID("accountguid"),
      Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey2"),
                    Property(&TemplateURL::url, "http://accounturl2.com"))));
  // Account search engine is not added to the database.
  EXPECT_THAT(GetKeywordsFromDatabase(), IsEmpty());
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotUpdateDatabaseUponIncrementalUpdateIfLocalAndAccountExist) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://url.com", /*guid=*/"guid"));
  ASSERT_TRUE(turl);

  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_UPDATE,
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey",
          /*url=*/"http://accounturl.com", /*guid=*/"guid")));
  model()->ProcessSyncChanges(FROM_HERE, changes);

  EXPECT_EQ(0u, processor()->change_list_size());
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  EXPECT_TRUE(turl->GetLocalData());
  EXPECT_TRUE(turl->GetAccountData());
  EXPECT_THAT(turl, Pointee(AllOf(
                        Property(&TemplateURL::keyword, u"accountkey"),
                        Property(&TemplateURL::url, "http://accounturl.com"))));
  // Local data is unchanged.
  EXPECT_THAT(*turl->GetLocalData(),
              AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                    Property(&TemplateURLData::keyword, u"key")));
  EXPECT_THAT(GetKeywordsFromDatabase(),
              ElementsAre(AllOf(Field(&TemplateURLData::sync_guid, "guid"),
                                Property(&TemplateURLData::keyword, u"key"))));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       MergeIgnoresUntouchedAutogeneratedKeywords) {
  syncer::SyncDataList initial_data;
  // `safe_for_autoreplace` is true and `is_active` is `kUnspecified`. This
  // represents an autogenerated keyword which the user has not touched.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"key", "http://url.com", "guid", base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/true,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));

  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  // This search engine should be ignored.
  EXPECT_FALSE(model()->GetTemplateURLForGUID("guid"));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       AddingUntouchedAutogeneratedKeywordsSendsNoUpdate) {
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, syncer::SyncDataList{}, PassProcessor()));

  // `safe_for_autoreplace` is true and `is_active` is `kUnspecified`. This
  // represents an autogenerated keyword which the user has not touched.
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      u"key", "http://url.com", "guid", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));

  ASSERT_THAT(
      turl,
      Pointee(AllOf(Property(&TemplateURL::safe_for_autoreplace, true),
                    Property(&TemplateURL::is_active,
                             TemplateURLData::ActiveStatus::kUnspecified))));
  // No account data should be created.
  EXPECT_FALSE(turl->GetAccountData());
  // Nothing is committed to the server.
  EXPECT_EQ(processor()->change_list_size(), 0u);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       UpdatingUntouchedAutogeneratedKeywordsSendsUpdate) {
  // `safe_for_autoreplace` is true and `is_active` is `kUnspecified`. This
  // represents an autogenerated keyword which the user has not touched.
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      u"key", "http://url.com", "guid", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));

  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, syncer::SyncDataList{}, PassProcessor()));

  EXPECT_EQ(turl, model()->GetTemplateURLForKeyword(u"key"));
  // No account data is created.
  EXPECT_FALSE(turl->GetAccountData());
  // Nothing is committed to the server.
  EXPECT_EQ(0U, processor()->change_list_size());

  // Change a keyword.
  model()->ResetTemplateURL(turl, turl->short_name(), u"newkey", turl->url());

  // `safe_for_autoreplace` changes to false and the keyword is marked active,
  // since the keyword was manually updated.
  EXPECT_THAT(turl,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"newkey"),
                            Property(&TemplateURL::safe_for_autoreplace, false),
                            Property(&TemplateURL::is_active,
                                     TemplateURLData::ActiveStatus::kTrue))));
  // Both local and account are created.
  EXPECT_EQ(turl->GetAccountData(), turl->GetLocalData());

  // The change is committed to the server.
  ASSERT_TRUE(processor()->contains_guid("guid"));
  EXPECT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
}

class TemplateURLServiceSyncMergeTest : public TemplateURLServiceSyncTest {
 public:
  void ShouldOverrideLocalWithSameGuidIfBetter();
  void ShouldNotOverrideLocalWithSameGuidIfNotBetter();
  void ShouldOverrideDuplicateLocalIfBetter();
  void ShouldNotOverrideDuplicateLocalIfNotBetter();
  void ShouldNotOverrideDuplicateLocalDefaultSearchProvider();
  void ShouldUpdateConflictingDefaultSearchEngineIfBetter();
  void ShouldNotUpdateConflictingDefaultSearchEngineIfNotBetter();
  void ShouldUpdateConflictingStarterPackSearchEngineIfBetter();
  void ShouldNotUpdateConflictingStarterPackSearchEngineIfNotBetter();
  void ShouldUpdateConflictingPrepopulatedSearchEngineIfBetter();
  void ShouldNotUpdateConflictingPrepopulatedSearchEngineIfNotBetter();
};

class
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled
    : public TemplateURLServiceSyncMergeTest {
 public:
  TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled() {
    feature_list_.InitAndDisableFeature(
        syncer::kSeparateLocalAndAccountSearchEngines);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled
    : public TemplateURLServiceSyncMergeTest {
 public:
  TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled()
      : feature_list_(syncer::kSeparateLocalAndAccountSearchEngines) {}

 private:
  base::test::ScopedFeatureList feature_list_;
};

void TemplateURLServiceSyncMergeTest::
    ShouldOverrideLocalWithSameGuidIfBetter() {
  // Add local template url.
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com", /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(10)));
  ASSERT_EQ(turl, model()->GetTemplateURLForKeyword(u"localkey"));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"guid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  // Account keyword has more recent timestamp and thus wins.
  ASSERT_FALSE(model()->GetTemplateURLForKeyword(u"localkey"));
  ASSERT_EQ(turl, model()->GetTemplateURLForKeyword(u"accountkey"));

  // Nothing is committed to the server.
  ASSERT_EQ(0u, processor()->change_list_size());
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled,
    ShouldOverrideLocalWithSameGuidIfBetter) {
  ASSERT_NO_FATAL_FAILURE(ShouldOverrideLocalWithSameGuidIfBetter());

  // Stopping sync should leave the sync value.
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"localkey"));
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"accountkey"));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldOverrideLocalWithSameGuidIfBetter) {
  ASSERT_NO_FATAL_FAILURE(ShouldOverrideLocalWithSameGuidIfBetter());

  // Account keyword should not replace but only override the local keyword.
  const TemplateURL* turl = model()->GetTemplateURLForKeyword(u"accountkey");
  EXPECT_THAT(turl->GetLocalData(),
              Optional(Property(&TemplateURLData::keyword, u"localkey")));

  // Stopping sync should remove the sync value.
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(turl, model()->GetTemplateURLForKeyword(u"localkey"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"accountkey"));
}

void TemplateURLServiceSyncMergeTest::
    ShouldNotOverrideLocalWithSameGuidIfNotBetter() {
  // Add local template url.
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com", /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(100)));
  ASSERT_EQ(turl, model()->GetTemplateURLForKeyword(u"localkey"));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"guid",
          /*last_modified=*/base::Time::FromTimeT(10))
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  // Local keyword has a more recent timestamp and thus wins.
  ASSERT_EQ(turl, model()->GetTemplateURLForKeyword(u"localkey"));
  ASSERT_FALSE(model()->GetTemplateURLForKeyword(u"accountkey"));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled,
    ShouldNotOverrideLocalWithSameGuidIfNotBetter) {
  ASSERT_NO_FATAL_FAILURE(ShouldNotOverrideLocalWithSameGuidIfNotBetter());

  // Local keyword is committed to the server.
  ASSERT_TRUE(processor()->contains_guid("guid"));
  EXPECT_EQ(processor()->change_for_guid("guid").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
  EXPECT_EQ(processor()
                ->change_for_guid("guid")
                .sync_data()
                .GetSpecifics()
                .search_engine()
                .keyword(),
            "localkey");

  // Stopping sync should not affect the value.
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_TRUE(model()->GetTemplateURLForKeyword(u"localkey"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"accountkey"));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldNotOverrideLocalWithSameGuidIfNotBetter) {
  ASSERT_NO_FATAL_FAILURE(ShouldNotOverrideLocalWithSameGuidIfNotBetter());

  EXPECT_EQ(processor()->change_list_size(), 0u);

  // Account keyword is not ignored but is only overridden by the local keyword.
  const TemplateURL* turl = model()->GetTemplateURLForKeyword(u"localkey");
  EXPECT_THAT(turl->GetAccountData(),
              Optional(Property(&TemplateURLData::keyword, u"accountkey")));

  // Stopping sync should remove the sync value.
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(turl, model()->GetTemplateURLForKeyword(u"localkey"));
  EXPECT_FALSE(model()->GetTemplateURLForKeyword(u"accountkey"));
}

void TemplateURLServiceSyncMergeTest::ShouldOverrideDuplicateLocalIfBetter() {
  // Add local template url.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://localurl.com", /*guid=*/"localguid",
      /*last_modified=*/base::Time::FromTimeT(10)));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"key", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  // Nothing is committed to the server.
  ASSERT_EQ(0u, processor()->change_list_size());
  // Account keyword wins.
  const TemplateURL* turl = model()->GetTemplateURLForGUID("accountguid");
  ASSERT_THAT(turl,
              Pointee(Property(&TemplateURL::url, "http://accounturl.com")));
  ASSERT_FALSE(model()->GetTemplateURLForGUID("localguid"));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled,
    ShouldOverrideDuplicateLocalIfBetter) {
  ASSERT_NO_FATAL_FAILURE(ShouldOverrideDuplicateLocalIfBetter());

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  // Local keyword has been removed and the account keyword stays.
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  EXPECT_THAT(model()->GetTemplateURLForGUID("accountguid"),
              Pointee(Property(&TemplateURL::url, "http://accounturl.com")));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldOverrideDuplicateLocalIfBetter) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(ShouldOverrideDuplicateLocalIfBetter());

  // Sync guid of the local keyword should be updated.
  const TemplateURL* turl = model()->GetTemplateURLForKeyword(u"key");
  EXPECT_THAT(
      turl->GetLocalData(),
      Optional(AllOf(Field(&TemplateURLData::sync_guid, "accountguid"),
                     Property(&TemplateURLData::url, "http://localurl.com"))));
  EXPECT_THAT(turl->GetAccountData(),
              Optional(AllOf(
                  Field(&TemplateURLData::sync_guid, "accountguid"),
                  Property(&TemplateURLData::url, "http://accounturl.com"))));
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.DuplicateIsDefaultSearchProvider", false, 1);

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  // Account keyword is removed, but the local keyword stays behind, with
  // updated sync guid.
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_THAT(turl,
              Pointee(Property(&TemplateURL::url, "http://localurl.com")));
}

void TemplateURLServiceSyncMergeTest::
    ShouldNotOverrideDuplicateLocalIfNotBetter() {
  // Add local template url.
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://localurl.com", /*guid=*/"localguid",
      /*last_modified=*/base::Time::FromTimeT(100)));
  ASSERT_EQ(turl, model()->GetTemplateURLForKeyword(u"key"));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"key", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(10))
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  ASSERT_FALSE(processor()->contains_guid("localguid"));
  // Sync guid of local turl is updated.
  ASSERT_EQ(turl, model()->GetTemplateURLForGUID("accountguid"));
  ASSERT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  ASSERT_EQ(turl->url(), "http://localurl.com");
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled,
    ShouldNotOverrideDuplicateLocalIfNotBetter) {
  ASSERT_NO_FATAL_FAILURE(ShouldNotOverrideDuplicateLocalIfNotBetter());

  // Local keyword is committed to the server.
  ASSERT_TRUE(processor()->contains_guid("accountguid"));
  EXPECT_EQ(processor()->change_for_guid("accountguid").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
  EXPECT_EQ(processor()
                ->change_for_guid("accountguid")
                .sync_data()
                .GetSpecifics()
                .search_engine()
                .url(),
            "http://localurl.com");

  // Stopping sync should not change anything.
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  EXPECT_THAT(model()->GetTemplateURLForGUID("accountguid"),
              Pointee(Property(&TemplateURL::url, "http://localurl.com")));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldNotOverrideDuplicateLocalIfNotBetter) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(ShouldNotOverrideDuplicateLocalIfNotBetter());

  // Nothing is committed to the server.
  EXPECT_FALSE(processor()->contains_guid("accountguid"));

  // Sync guid of the local keyword should be updated.
  const TemplateURL* turl = model()->GetTemplateURLForKeyword(u"key");
  EXPECT_THAT(
      turl->GetLocalData(),
      Optional(AllOf(Field(&TemplateURLData::sync_guid, "accountguid"),
                     Property(&TemplateURLData::url, "http://localurl.com"))));
  EXPECT_THAT(turl->GetAccountData(),
              Optional(AllOf(
                  Field(&TemplateURLData::sync_guid, "accountguid"),
                  Property(&TemplateURLData::url, "http://accounturl.com"))));
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.DuplicateIsDefaultSearchProvider", false, 1);

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  // Account keyword is removed, but the local keyword stays behind, with
  // updated sync guid.
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_THAT(turl,
              Pointee(Property(&TemplateURL::url, "http://localurl.com")));
}

void TemplateURLServiceSyncMergeTest::
    ShouldNotOverrideDuplicateLocalDefaultSearchProvider() {
  // Add local template url.
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"key", /*url=*/"http://localurl.com", /*guid=*/"localguid",
      /*last_modified=*/base::Time::FromTimeT(10)));
  model()->SetUserSelectedDefaultSearchProvider(turl);

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"key", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100))
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  ASSERT_FALSE(processor()->contains_guid("localguid"));
  // Sync guid of local turl is updated.
  ASSERT_EQ(turl, model()->GetDefaultSearchProvider());
  ASSERT_EQ(turl->sync_guid(), "accountguid");
  ASSERT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  ASSERT_EQ(turl->url(), "http://localurl.com");
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled,
    ShouldNotOverrideDuplicateLocalDefaultSearchProvider) {
  ASSERT_NO_FATAL_FAILURE(
      ShouldNotOverrideDuplicateLocalDefaultSearchProvider());

  // Local keyword is committed to the server.
  ASSERT_TRUE(processor()->contains_guid("accountguid"));
  EXPECT_EQ(processor()->change_for_guid("accountguid").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
  EXPECT_EQ(processor()
                ->change_for_guid("accountguid")
                .sync_data()
                .GetSpecifics()
                .search_engine()
                .url(),
            "http://localurl.com");

  // Stopping sync should not change anything.
  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  const TemplateURL* turl = model()->GetTemplateURLForGUID("accountguid");
  EXPECT_THAT(turl,
              Pointee(Property(&TemplateURL::url, "http://localurl.com")));
  EXPECT_EQ(turl, model()->GetDefaultSearchProvider());
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldNotOverrideDuplicateLocalDefaultSearchProvider) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      ShouldNotOverrideDuplicateLocalDefaultSearchProvider());

  // Nothing is committed to the server.
  EXPECT_EQ(processor()->change_list_size(), 0u);

  // Sync guid of the local keyword should be updated.
  const TemplateURL* turl = model()->GetTemplateURLForKeyword(u"key");
  EXPECT_THAT(
      turl->GetLocalData(),
      Optional(AllOf(Field(&TemplateURLData::sync_guid, "accountguid"),
                     Property(&TemplateURLData::url, "http://localurl.com"))));
  // Account data is ignored.
  EXPECT_FALSE(turl->GetAccountData());
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.DuplicateIsDefaultSearchProvider", true, 1);

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  // The local keyword stays behind with updated sync guid.
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_EQ(turl, model()->GetDefaultSearchProvider());
  EXPECT_THAT(turl,
              Pointee(Property(&TemplateURL::url, "http://localurl.com")));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
}

void TemplateURLServiceSyncMergeTest::
    ShouldUpdateConflictingDefaultSearchEngineIfBetter() {
  // Add local template url.
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"localguid",
      /*last_modified=*/base::Time::FromTimeT(10),
      /*safe_for_autoreplace=*/false,
      /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/99999, /*starter_pack_id=*/0));
  model()->SetUserSelectedDefaultSearchProvider(turl);

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/false,
          /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
          /*prepopulate_id=*/99999, /*starter_pack_id=*/0)
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  ASSERT_EQ(turl, model()->GetDefaultSearchProvider());
  ASSERT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  ASSERT_EQ(turl, model()->GetTemplateURLForGUID("accountguid"));
  ASSERT_THAT(turl,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey"),
                            Property(&TemplateURL::sync_guid, "accountguid"))));
  ASSERT_EQ(0u, processor()->change_list_size());
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled,
    ShouldUpdateConflictingDefaultSearchEngineIfBetter) {
  ASSERT_NO_FATAL_FAILURE(ShouldUpdateConflictingDefaultSearchEngineIfBetter());

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  const TemplateURL* turl = model()->GetDefaultSearchProvider();
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_THAT(turl,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey"),
                            Property(&TemplateURL::sync_guid, "accountguid"))));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldUpdateConflictingDefaultSearchEngineIfBetter) {
  ASSERT_NO_FATAL_FAILURE(ShouldUpdateConflictingDefaultSearchEngineIfBetter());

  const TemplateURL* turl = model()->GetTemplateURLForKeyword(u"accountkey");
  EXPECT_THAT(
      turl->GetLocalData(),
      Optional(AllOf(Property(&TemplateURLData::keyword, u"localkey"),
                     Field(&TemplateURLData::sync_guid, "accountguid"))));
  EXPECT_THAT(
      turl->GetAccountData(),
      Optional(AllOf(Property(&TemplateURLData::keyword, u"accountkey"),
                     Field(&TemplateURLData::sync_guid, "accountguid"))));

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(turl, model()->GetDefaultSearchProvider());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_THAT(turl,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"localkey"),
                            Property(&TemplateURL::sync_guid, "accountguid"))));
}

void TemplateURLServiceSyncMergeTest::
    ShouldNotUpdateConflictingDefaultSearchEngineIfNotBetter() {
  // Add local template url.
  TemplateURL* local = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"localguid",
      /*last_modified=*/base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false,
      /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/99999, /*starter_pack_id=*/0));
  model()->SetUserSelectedDefaultSearchProvider(local);

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(10),
          /*safe_for_autoreplace=*/false,
          /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
          /*prepopulate_id=*/99999, /*starter_pack_id=*/0)
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));
  ASSERT_FALSE(processor()->contains_guid("accountguid"));

  ASSERT_EQ(local, model()->GetDefaultSearchProvider());
  ASSERT_THAT(local,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"localkey"),
                            Property(&TemplateURL::sync_guid, "localguid"))));

  const TemplateURL* account = model()->GetTemplateURLForGUID("accountguid");
  ASSERT_NE(local, account);
  ASSERT_THAT(account, Pointee(Property(&TemplateURL::keyword, u"accountkey")));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled,
    ShouldNotUpdateConflictingDefaultSearchEngineIfNotBetter) {
  ASSERT_NO_FATAL_FAILURE(
      ShouldNotUpdateConflictingDefaultSearchEngineIfNotBetter());

  // Local turl is committed to the server as-is.
  ASSERT_TRUE(processor()->contains_guid("localguid"));
  EXPECT_EQ(processor()->change_for_guid("localguid").change_type(),
            syncer::SyncChange::ACTION_ADD);
  EXPECT_EQ(processor()
                ->change_for_guid("localguid")
                .sync_data()
                .GetSpecifics()
                .search_engine()
                .url(),
            "http://localurl.com");

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  const TemplateURL* local = model()->GetTemplateURLForGUID("localguid");
  EXPECT_THAT(local,
              AllOf(Pointee(Property(&TemplateURL::keyword, u"localkey")),
                    Eq(model()->GetDefaultSearchProvider())));
  EXPECT_THAT(model()->GetTemplateURLForGUID("accountguid"),
              Pointee(Property(&TemplateURL::keyword, u"accountkey")));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldNotUpdateConflictingDefaultSearchEngineIfNotBetter) {
  ASSERT_NO_FATAL_FAILURE(
      ShouldNotUpdateConflictingDefaultSearchEngineIfNotBetter());

  // Nothing is committed to the server.
  EXPECT_EQ(processor()->change_list_size(), 0u);

  const TemplateURL* local = model()->GetTemplateURLForGUID("localguid");
  EXPECT_FALSE(local->GetAccountData());
  EXPECT_THAT(local,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"localkey"),
                            Property(&TemplateURL::sync_guid, "localguid"))));

  const TemplateURL* account = model()->GetTemplateURLForGUID("accountguid");
  EXPECT_FALSE(account->GetLocalData());
  EXPECT_THAT(account,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey"),
                            Property(&TemplateURL::sync_guid, "accountguid"))));

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(local, model()->GetTemplateURLForGUID("localguid"));
  EXPECT_THAT(local,
              AllOf(Pointee(Property(&TemplateURL::keyword, u"localkey")),
                    Eq(model()->GetDefaultSearchProvider())));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid"));
}

void TemplateURLServiceSyncMergeTest::
    ShouldUpdateConflictingStarterPackSearchEngineIfBetter() {
  // Add local template url.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"localguid",
      /*last_modified=*/base::Time::FromTimeT(10),
      /*safe_for_autoreplace=*/false,
      /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/1));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/false,
          /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
          /*prepopulate_id=*/0, /*starter_pack_id=*/1)
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  ASSERT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  const TemplateURL* turl =
      model()->FindStarterPackTemplateURL(/*starter_pack_id=*/1);
  ASSERT_THAT(turl,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey"),
                            Property(&TemplateURL::sync_guid, "accountguid"))));
  ASSERT_EQ(turl, model()->GetTemplateURLForGUID("accountguid"));
  ASSERT_EQ(0u, processor()->change_list_size());
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled,
    ShouldUpdateConflictingStarterPackSearchEngineIfBetter) {
  ASSERT_NO_FATAL_FAILURE(
      ShouldUpdateConflictingStarterPackSearchEngineIfBetter());

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  const TemplateURL* turl =
      model()->FindStarterPackTemplateURL(/*starter_pack_id=*/1);
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_THAT(turl,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey"),
                            Property(&TemplateURL::sync_guid, "accountguid"))));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldUpdateConflictingStarterPackSearchEngineIfBetter) {
  ASSERT_NO_FATAL_FAILURE(
      ShouldUpdateConflictingStarterPackSearchEngineIfBetter());

  const TemplateURL* turl = model()->GetTemplateURLForKeyword(u"accountkey");
  EXPECT_THAT(
      turl->GetLocalData(),
      Optional(AllOf(Property(&TemplateURLData::keyword, u"localkey"),
                     Field(&TemplateURLData::sync_guid, "accountguid"))));
  EXPECT_THAT(
      turl->GetAccountData(),
      Optional(AllOf(Property(&TemplateURLData::keyword, u"accountkey"),
                     Field(&TemplateURLData::sync_guid, "accountguid"))));

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(turl, model()->FindStarterPackTemplateURL(/*starter_pack_id=*/1));
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_THAT(turl,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"localkey"),
                            Property(&TemplateURL::sync_guid, "accountguid"))));
}

void TemplateURLServiceSyncMergeTest::
    ShouldNotUpdateConflictingStarterPackSearchEngineIfNotBetter() {
  // Add local template url.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"localguid",
      /*last_modified=*/base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false,
      /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/1));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(10),
          /*safe_for_autoreplace=*/false,
          /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
          /*prepopulate_id=*/0, /*starter_pack_id=*/1)
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  ASSERT_FALSE(processor()->contains_guid("accountguid"));
  const TemplateURL* local = model()->GetTemplateURLForGUID("localguid");
  ASSERT_THAT(local, Pointee(Property(&TemplateURL::keyword, u"localkey")));
  ASSERT_EQ(local, model()->FindStarterPackTemplateURL(/*starter_pack_id=*/1));
  const TemplateURL* account = model()->GetTemplateURLForGUID("accountguid");
  ASSERT_THAT(account, Pointee(Property(&TemplateURL::keyword, u"accountkey")));
  ASSERT_NE(local, account);
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled,
    ShouldNotUpdateConflictingStarterPackSearchEngineIfNotBetter) {
  ASSERT_NO_FATAL_FAILURE(
      ShouldNotUpdateConflictingStarterPackSearchEngineIfNotBetter());

  // Local turl is committed to the server as-is.
  ASSERT_TRUE(processor()->contains_guid("localguid"));
  EXPECT_EQ(processor()->change_for_guid("localguid").change_type(),
            syncer::SyncChange::ACTION_ADD);
  EXPECT_EQ(processor()
                ->change_for_guid("localguid")
                .sync_data()
                .GetSpecifics()
                .search_engine()
                .url(),
            "http://localurl.com");

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  const TemplateURL* local = model()->GetTemplateURLForGUID("localguid");
  EXPECT_THAT(
      local,
      AllOf(Pointee(Property(&TemplateURL::keyword, u"localkey")),
            Eq(model()->FindStarterPackTemplateURL(/*starter_pack_id=*/1))));
  EXPECT_THAT(model()->GetTemplateURLForGUID("accountguid"),
              Pointee(Property(&TemplateURL::keyword, u"accountkey")));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldNotUpdateConflictingStarterPackSearchEngineIfNotBetter) {
  ASSERT_NO_FATAL_FAILURE(
      ShouldNotUpdateConflictingStarterPackSearchEngineIfNotBetter());

  // Nothing is committed to the server.
  EXPECT_EQ(processor()->change_list_size(), 0u);

  const TemplateURL* local = model()->GetTemplateURLForGUID("localguid");
  EXPECT_FALSE(local->GetAccountData());
  EXPECT_THAT(local,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"localkey"),
                            Property(&TemplateURL::sync_guid, "localguid"))));

  const TemplateURL* account = model()->GetTemplateURLForGUID("accountguid");
  EXPECT_FALSE(account->GetLocalData());
  EXPECT_THAT(account,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey"),
                            Property(&TemplateURL::sync_guid, "accountguid"))));

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(local, model()->GetTemplateURLForGUID("localguid"));
  EXPECT_THAT(
      local,
      AllOf(Pointee(Property(&TemplateURL::keyword, u"localkey")),
            Eq(model()->FindStarterPackTemplateURL(/*starter_pack_id=*/1))));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid"));
}

void TemplateURLServiceSyncMergeTest::
    ShouldUpdateConflictingPrepopulatedSearchEngineIfBetter() {
  // Add local template url.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"localguid",
      /*last_modified=*/base::Time::FromTimeT(10),
      /*safe_for_autoreplace=*/false,
      /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/99999, /*starter_pack_id=*/0));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/false,
          /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
          /*prepopulate_id=*/99999, /*starter_pack_id=*/0)
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  ASSERT_EQ(processor()->change_list_size(), 0u);
  ASSERT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  const TemplateURL* turl = model()->GetTemplateURLForGUID("accountguid");
  ASSERT_THAT(turl, Pointee(Property(&TemplateURL::keyword, u"accountkey")));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled,
    ShouldUpdateConflictingPrepopulatedSearchEngineIfBetter) {
  ASSERT_NO_FATAL_FAILURE(
      ShouldUpdateConflictingPrepopulatedSearchEngineIfBetter());

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_THAT(model()->GetTemplateURLForGUID("accountguid"),
              Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey"),
                            Property(&TemplateURL::sync_guid, "accountguid"))));
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldUpdateConflictingPrepopulatedSearchEngineIfBetter) {
  ASSERT_NO_FATAL_FAILURE(
      ShouldUpdateConflictingPrepopulatedSearchEngineIfBetter());

  const TemplateURL* turl = model()->GetTemplateURLForKeyword(u"accountkey");
  EXPECT_THAT(
      turl->GetLocalData(),
      Optional(AllOf(Property(&TemplateURLData::keyword, u"localkey"),
                     Field(&TemplateURLData::sync_guid, "accountguid"))));
  EXPECT_THAT(
      turl->GetAccountData(),
      Optional(AllOf(Property(&TemplateURLData::keyword, u"accountkey"),
                     Field(&TemplateURLData::sync_guid, "accountguid"))));

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_FALSE(model()->GetTemplateURLForGUID("localguid"));
  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("accountguid"));
  EXPECT_FALSE(turl->GetAccountData());
  EXPECT_THAT(turl,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"localkey"),
                            Property(&TemplateURL::sync_guid, "accountguid"))));
}

void TemplateURLServiceSyncMergeTest::
    ShouldNotUpdateConflictingPrepopulatedSearchEngineIfNotBetter() {
  // Add local template url.
  model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"localguid",
      /*last_modified=*/base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false,
      /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/99999, /*starter_pack_id=*/0));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"accountguid",
          /*last_modified=*/base::Time::FromTimeT(10),
          /*safe_for_autoreplace=*/false,
          /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
          /*prepopulate_id=*/99999, /*starter_pack_id=*/0)
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  ASSERT_FALSE(processor()->contains_guid("accountguid"));
  const TemplateURL* local = model()->GetTemplateURLForGUID("localguid");
  ASSERT_THAT(local, Pointee(Property(&TemplateURL::keyword, u"localkey")));
  const TemplateURL* account = model()->GetTemplateURLForGUID("accountguid");
  ASSERT_THAT(account, Pointee(Property(&TemplateURL::keyword, u"accountkey")));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesDisabled,
    ShouldNotUpdateConflictingPrepopulatedSearchEngineIfNotBetter) {
  ASSERT_NO_FATAL_FAILURE(
      ShouldNotUpdateConflictingPrepopulatedSearchEngineIfNotBetter());

  // Local turl is committed to the server as-is.
  ASSERT_TRUE(processor()->contains_guid("localguid"));
  EXPECT_EQ(processor()->change_for_guid("localguid").change_type(),
            syncer::SyncChange::ACTION_ADD);
  EXPECT_EQ(processor()
                ->change_for_guid("localguid")
                .sync_data()
                .GetSpecifics()
                .search_engine()
                .url(),
            "http://localurl.com");

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  const TemplateURL* local = model()->GetTemplateURLForGUID("localguid");
  EXPECT_EQ(local->keyword(), u"localkey");
  EXPECT_THAT(model()->GetTemplateURLForGUID("accountguid"),
              Pointee(Property(&TemplateURL::keyword, u"accountkey")));
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldNotUpdateConflictingPrepopulatedSearchEngineIfNotBetter) {
  ASSERT_NO_FATAL_FAILURE(
      ShouldNotUpdateConflictingPrepopulatedSearchEngineIfNotBetter());

  // Nothing is committed to the server.
  ASSERT_EQ(0u, processor()->change_list_size());

  const TemplateURL* local = model()->GetTemplateURLForGUID("localguid");
  EXPECT_FALSE(local->GetAccountData());
  EXPECT_THAT(local,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"localkey"),
                            Property(&TemplateURL::sync_guid, "localguid"))));

  const TemplateURL* account = model()->GetTemplateURLForGUID("accountguid");
  EXPECT_FALSE(account->GetLocalData());
  EXPECT_THAT(account,
              Pointee(AllOf(Property(&TemplateURL::keyword, u"accountkey"),
                            Property(&TemplateURL::sync_guid, "accountguid"))));

  model()->StopSyncing(syncer::SEARCH_ENGINES);
  EXPECT_EQ(local, model()->GetTemplateURLForGUID("localguid"));
  EXPECT_EQ(local->keyword(), u"localkey");
  EXPECT_FALSE(model()->GetTemplateURLForGUID("accountguid"));
}

// Regression test for crbug.com/405036427.
TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldNotCrashForUntouchedAutogeneratedSearchEnginesContainingAccountDataUponGetAllSyncData) {
  const TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true,
      /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      /*is_active=*/TemplateURLData::ActiveStatus::kUnspecified));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"guid",
          /*last_modified=*/base::Time::FromTimeT(10),
          /*safe_for_autoreplace=*/true,
          /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
          /*prepopulate_id=*/0, /*starter_pack_id=*/0,
          /*is_active=*/TemplateURLData::ActiveStatus::kTrue)
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  EXPECT_THAT(
      turl,
      Pointee(AllOf(Property(&TemplateURL::keyword, u"localkey"),
                    Property(&TemplateURL::is_active,
                             TemplateURLData::ActiveStatus::kUnspecified))));
  EXPECT_THAT(
      turl->GetLocalData(),
      Optional(AllOf(Property(&TemplateURLData::keyword, u"localkey"),
                     Field(&TemplateURLData::is_active,
                           TemplateURLData::ActiveStatus::kUnspecified))));
  EXPECT_THAT(turl->GetAccountData(),
              Optional(AllOf(Property(&TemplateURLData::keyword, u"accountkey"),
                             Field(&TemplateURLData::is_active,
                                   TemplateURLData::ActiveStatus::kTrue))));
  // Should not crash.
  EXPECT_EQ(1U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());

  // Should not crash.
  model()->Remove(turl);
  EXPECT_EQ(0U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
}

TEST_F(
    TemplateURLServiceSyncMergeTestWithSeparateLocalAndAccountSearchEnginesEnabled,
    ShouldOnlyCommitAccountDataUponProcessSyncChanges) {
  TemplateURL* turl = model()->Add(CreateTestTemplateURL(
      /*keyword=*/u"localkey", /*url=*/"http://localurl.com",
      /*guid=*/"guid",
      /*last_modified=*/base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true,
      /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      /*is_active=*/TemplateURLData::ActiveStatus::kUnspecified));

  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          /*keyword=*/u"accountkey", /*url=*/"http://accounturl.com",
          /*guid=*/"guid",
          /*last_modified=*/base::Time::FromTimeT(10),
          /*safe_for_autoreplace=*/true,
          /*policy_origin=*/TemplateURLData::PolicyOrigin::kNoPolicy,
          /*prepopulate_id=*/0, /*starter_pack_id=*/0,
          /*is_active=*/TemplateURLData::ActiveStatus::kTrue)
          ->data()));
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, initial_data, PassProcessor()));

  EXPECT_EQ(turl, model()->GetTemplateURLForGUID("guid"));
  EXPECT_THAT(
      turl,
      Pointee(AllOf(Property(&TemplateURL::keyword, u"localkey"),
                    Property(&TemplateURL::is_active,
                             TemplateURLData::ActiveStatus::kUnspecified))));
  EXPECT_THAT(
      turl->GetLocalData(),
      Optional(AllOf(Property(&TemplateURLData::keyword, u"localkey"),
                     Field(&TemplateURLData::is_active,
                           TemplateURLData::ActiveStatus::kUnspecified))));
  EXPECT_THAT(turl->GetAccountData(),
              Optional(AllOf(Property(&TemplateURLData::keyword, u"accountkey"),
                             Field(&TemplateURLData::is_active,
                                   TemplateURLData::ActiveStatus::kTrue))));

  model()->Remove(turl);

  ASSERT_EQ(1u, processor()->change_list_size());
  // Should commit the account data.
  EXPECT_EQ(processor()
                ->change_for_guid("guid")
                .sync_data()
                .GetSpecifics()
                .search_engine()
                .keyword(),
            "accountkey");
}

// This test verifies the logging in the following cases:
// 1. Whether a keyword is an untouched autogenerated keyword is logged upon
// every add, update and delete.
// 2. Whether an untouched autogenerated keyword is a prepopulated keyword.
// 3. Whether an untouched autogenerated keyword is a starter pack keyword.
// This test first adds different types of keywords, then updates them and
// finally deletes them, verifying that the histograms are logged correctly in
// each of these cases.
TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldLogUntouchedAutogeneratedKeywordsWhenChanged) {
  ASSERT_FALSE(model()->MergeDataAndStartSyncing(
      syncer::SEARCH_ENGINES, syncer::SyncDataList{}, PassProcessor()));

  base::HistogramTester histogram_tester;

  // Not an untouched keyword.
  TemplateURL* turl0 = model()->Add(CreateTestTemplateURL(
      u"key0", "http://key0.com", "guid0", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kTrue));
  EXPECT_EQ(turl0->GetLocalData(), turl0->GetAccountData());
  ASSERT_TRUE(processor()->contains_guid("guid0"));
  EXPECT_THAT(processor()->change_for_guid("guid0"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_ADD));

  // All the below keywords are untouched autogenerated keywords, given that
  // `safe_for_autoreplace` is true (implying that the keyword is autogenerated)
  // and `is_active` is `kUnspecified` (implying that the keyword is untouched).
  TemplateURL* turl1 = model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "guid1", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  EXPECT_FALSE(turl1->GetAccountData());
  EXPECT_FALSE(processor()->contains_guid("guid1"));

  // Starter pack keyword.
  TemplateURL* turl2 = model()->Add(CreateTestTemplateURL(
      u"key2", "http://key2.com", "guid2", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/1,
      TemplateURLData::ActiveStatus::kUnspecified));
  EXPECT_FALSE(turl2->GetAccountData());
  EXPECT_FALSE(processor()->contains_guid("guid2"));

  // Prepopulated keyword.
  TemplateURL* turl3 = model()->Add(CreateTestTemplateURL(
      u"key3", "http://key3.com", "guid3", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/99999, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  EXPECT_EQ(turl3->GetLocalData(), turl3->GetAccountData());
  ASSERT_TRUE(processor()->contains_guid("guid3"));
  EXPECT_THAT(processor()->change_for_guid("guid3"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_ADD));

  // Only one of the above keywords is not an untouched autogenerated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedAdded"),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 3)));
  // Only one of the above keywords is a prepopulated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedAdded."
                  "IsPrepopulatedEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));
  // Only one of the above keywords is a starter pack keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedAdded."
                  "IsStarterPackEntry"),
              base::BucketsAre(base::Bucket(false, 2), base::Bucket(true, 1)));

  // Update a non-untouched autogenerated keyword.
  ASSERT_EQ(turl0, model()->GetTemplateURLForGUID("guid0"));
  model()->UpdateTemplateURLVisitTime(turl0);
  EXPECT_THAT(processor()->change_for_guid("guid0"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_UPDATE));

  // Update the above untouched autogenerated keywords.
  // `turl1` does not log the histograms because it has no account data.
  ASSERT_EQ(turl1, model()->GetTemplateURLForGUID("guid1"));
  model()->UpdateTemplateURLVisitTime(turl1);
  EXPECT_FALSE(turl1->GetAccountData());
  EXPECT_FALSE(processor()->contains_guid("guid1"));

  // `turl2` does not log the histograms because it has no account data.
  ASSERT_EQ(turl2, model()->GetTemplateURLForGUID("guid2"));
  model()->UpdateTemplateURLVisitTime(turl2);
  EXPECT_FALSE(turl2->GetAccountData());
  EXPECT_FALSE(processor()->contains_guid("guid2"));

  ASSERT_EQ(turl3, model()->GetTemplateURLForGUID("guid3"));
  model()->UpdateTemplateURLVisitTime(turl3);
  EXPECT_THAT(processor()->change_for_guid("guid3"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_UPDATE));

  // Note that `turl1` and `turl2` are not logged because they have no account
  // data.
  // Only one of the above keywords is not an untouched autogenerated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedUpdated"),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  // Only one of the above keywords is a prepopulated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedUpdated."
                  "IsPrepopulatedEntry"),
              base::BucketsAre(base::Bucket(false, 0), base::Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedUpdated."
                  "IsStarterPackEntry"),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 0)));

  // Delete the above keywords.
  model()->Remove(turl0);
  EXPECT_THAT(processor()->change_for_guid("guid0"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_DELETE));
  model()->Remove(turl1);
  EXPECT_FALSE(processor()->contains_guid("guid1"));
  model()->Remove(turl2);
  EXPECT_FALSE(processor()->contains_guid("guid2"));
  model()->Remove(turl3);
  EXPECT_THAT(processor()->change_for_guid("guid3"),
              Property(&syncer::SyncChange::change_type,
                       syncer::SyncChange::ACTION_DELETE));

  // Note that `turl1` and `turl2` are not logged because they have no account
  // data.
  // Only one of the above keywords is not an untouched autogenerated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedDeleted"),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
  // Only one of the above keywords is a prepopulated keyword.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedDeleted."
                  "IsPrepopulatedEntry"),
              base::BucketsAre(base::Bucket(false, 0), base::Bucket(true, 1)));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Sync.SearchEngine.UntouchedAutogeneratedDeleted."
                  "IsStarterPackEntry"),
              base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 0)));
}

// Regression test for crbug.com/405298133.
TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       MergeIgnoresLocalUntouchedAutogeneratedKeywords) {
  // Untouched autogenerated keyword.
  const TemplateURL* turl1 = model()->Add(CreateTestTemplateURL(
      u"localkey1", "http://localkey1.com", "guid1", base::Time::FromTimeT(10),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  // Untouched autogenerated keyword.
  const TemplateURL* turl2 = model()->Add(CreateTestTemplateURL(
      u"localkey2", "http://localkey2.com", "guid2", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/true, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));
  // Not an untouched autogenerated keyword.
  const TemplateURL* turl3 = model()->Add(CreateTestTemplateURL(
      u"localkey3", "http://localkey3.com", "guid3", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false, TemplateURLData::PolicyOrigin::kNoPolicy,
      /*prepopulate_id=*/0, /*starter_pack_id=*/0,
      TemplateURLData::ActiveStatus::kUnspecified));

  syncer::SyncDataList initial_data;
  // Not an untouched autogenerated keyword and more recent than `turl1`.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"accountkey1", "http://accountkey1.com", "guid1",
          base::Time::FromTimeT(100),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));
  // Not an untouched autogenerated keyword but less recent than `turl2`.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"accountkey2", "http://accountkey2.com", "guid2",
          base::Time::FromTimeT(10),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));
  // Not an untouched autogenerated keyword but less recent than `turl3`.
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(
          u"accountkey3", "http://accountkey3.com", "guid3",
          base::Time::FromTimeT(10),
          /*safe_for_autoreplace=*/false,
          TemplateURLData::PolicyOrigin::kNoPolicy, /*prepopulate_id=*/0,
          /*starter_pack_id=*/0, TemplateURLData::ActiveStatus::kUnspecified)
          ->data()));

  ASSERT_FALSE(model()
                   ->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                              initial_data, PassProcessor())
                   .has_value());

  // For `guid1`, the account keyword wins since it is more recent.
  ASSERT_EQ(turl1, model()->GetTemplateURLForGUID("guid1"));
  EXPECT_EQ(turl1->keyword(), u"accountkey1");
  EXPECT_FALSE(processor()->contains_guid("guid1"));
  // For `guid2`, the account keyword wins even though it is less recent.
  ASSERT_EQ(turl2, model()->GetTemplateURLForGUID("guid2"));
  EXPECT_EQ(turl2->keyword(), u"accountkey2");
  EXPECT_FALSE(processor()->contains_guid("guid2"));
  // For `guid3`, the local keyword wins since it is more recent. This is also
  // committed to the processor since it has not been filtered out as it's not
  // an untouched autogenerated keyword.
  ASSERT_EQ(turl3, model()->GetTemplateURLForGUID("guid3"));
  EXPECT_EQ(turl3->keyword(), u"localkey3");
  EXPECT_FALSE(processor()->contains_guid("guid3"));
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldNotRemoveAccountDataUponBrowserShutdown) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  TemplateURLData data1 =
      CreateTestTemplateURL(u"key1", "http://key1.com", "guid1")->data();
  // Add a local-and-account search engine.
  const TemplateURL* turl1 =
      model()->Add(std::make_unique<TemplateURL>(data1, data1));
  ASSERT_TRUE(turl1->GetAccountData());
  ASSERT_TRUE(turl1->GetLocalData());

  base::HistogramTester histogram_tester;
  model()->OnBrowserShutdown(syncer::SEARCH_ENGINES);

  ASSERT_EQ(turl1, model()->GetTemplateURLForGUID("guid1"));
  EXPECT_TRUE(turl1->GetAccountData());
  histogram_tester.ExpectTotalCount(
      "Sync.SearchEngine.HasLocalDataDuringStopSyncing2", 0);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldRemoveAccountDataUponStopSyncing) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  TemplateURLData data1 =
      CreateTestTemplateURL(u"key1", "http://key1.com", "guid1")->data();
  // Add a local-and-account search engine.
  const TemplateURL* turl1 =
      model()->Add(std::make_unique<TemplateURL>(data1, data1));

  ASSERT_TRUE(turl1->GetAccountData());
  ASSERT_TRUE(turl1->GetLocalData());

  base::HistogramTester histogram_tester;
  model()->StopSyncing(syncer::SEARCH_ENGINES);

  ASSERT_EQ(turl1, model()->GetTemplateURLForGUID("guid1"));
  EXPECT_FALSE(turl1->GetAccountData());
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.HasLocalDataDuringStopSyncing2", true, 1);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldLogCommittedChangesUponSyncStart) {
  syncer::SyncDataList initial_data;
  // This should lead to a deletion commit (because of empty url).
  syncer::SyncData sync_data1 =
      TemplateURLService::CreateSyncDataFromTemplateURLData(
          CreateTestTemplateURL(u"key1", /*url=*/"https://key1.com", "guid1")
              ->data());
  const_cast<sync_pb::EntitySpecifics&>(sync_data1.GetSpecifics())
      .mutable_search_engine()
      ->set_url("");
  initial_data.push_back(sync_data1);

  // This should lead to an update commit.
  TemplateURLData data2 =
      CreateTestTemplateURL(u"key2", "http://key2.com", "guid2")->data();
  data2.input_encodings = {"UTF-8", "UTF-16", "UTF-16", "UTF-8"};
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURLData(data2));

  base::HistogramTester histogram_tester;
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  ASSERT_EQ(processor()->change_list_size(), 2u);
  ASSERT_TRUE(processor()->contains_guid("guid1"));
  ASSERT_EQ(processor()->change_for_guid("guid1").change_type(),
            syncer::SyncChange::ACTION_DELETE);
  ASSERT_FALSE(model()->GetTemplateURLForGUID("guid1"));
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.ChangesCommittedUponSyncStart_Deleted", /*sample=*/1,
      /*expected_bucket_count=*/1);
  ASSERT_TRUE(processor()->contains_guid("guid2"));
  ASSERT_EQ(processor()->change_for_guid("guid2").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
  ASSERT_THAT(model()->GetTemplateURLForGUID("guid2"),
              Pointee(Property(&TemplateURL::input_encodings,
                               std::vector<std::string>{"UTF-8", "UTF-16"})));
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.ChangesCommittedUponSyncStart_Updated", /*sample=*/1,
      /*expected_bucket_count=*/1);

  // No adds are committed upon sync start.
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.ChangesCommittedUponSyncStart_Added", /*sample=*/0,
      /*expected_bucket_count=*/1);
}

TEST_F(TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines,
       ShouldLogCommittedChangesUponIncrementalUpdate) {
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES,
                                    syncer::SyncDataList{}, PassProcessor());

  syncer::SyncChangeList changes;
  // This should lead to an deletion commit (because of empty url).
  syncer::SyncData sync_data1 =
      TemplateURLService::CreateSyncDataFromTemplateURLData(
          CreateTestTemplateURL(u"key1", /*url=*/"https://key1.com", "guid1")
              ->data());
  const_cast<sync_pb::EntitySpecifics&>(sync_data1.GetSpecifics())
      .mutable_search_engine()
      ->set_url("");
  changes.emplace_back(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                       sync_data1);
  // This should lead to an update commit.
  TemplateURLData data2 =
      CreateTestTemplateURL(u"key2", "http://key2.com", "guid2")->data();
  data2.input_encodings = {"UTF-8", "UTF-16", "UTF-16", "UTF-8"};
  changes.push_back(CreateTestSyncChange(syncer::SyncChange::ACTION_UPDATE,
                                         std::make_unique<TemplateURL>(data2)));

  base::HistogramTester histogram_tester;
  model()->ProcessSyncChanges(FROM_HERE, changes);

  ASSERT_EQ(processor()->change_list_size(), 2u);
  ASSERT_TRUE(processor()->contains_guid("guid1"));
  ASSERT_EQ(processor()->change_for_guid("guid1").change_type(),
            syncer::SyncChange::ACTION_DELETE);
  ASSERT_FALSE(model()->GetTemplateURLForGUID("guid1"));
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.ChangesCommittedUponIncrementalUpdate_Deleted",
      /*sample=*/1,
      /*expected_bucket_count=*/1);
  ASSERT_TRUE(processor()->contains_guid("guid2"));
  ASSERT_EQ(processor()->change_for_guid("guid2").change_type(),
            syncer::SyncChange::ACTION_UPDATE);
  ASSERT_THAT(model()->GetTemplateURLForGUID("guid2"),
              Pointee(Property(&TemplateURL::input_encodings,
                               std::vector<std::string>{"UTF-8", "UTF-16"})));
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.ChangesCommittedUponIncrementalUpdate_Updated",
      /*sample=*/1,
      /*expected_bucket_count=*/1);

  // No adds are committed upon sync start.
  histogram_tester.ExpectUniqueSample(
      "Sync.SearchEngine.ChangesCommittedUponIncrementalUpdate_Added",
      /*sample=*/0,
      /*expected_bucket_count=*/1);
}

class TemplateURLServiceSyncTestWithAvoidFaviconOnlyCommits
    : public TemplateURLServiceSyncTestWithSeparateLocalAndAccountSearchEngines {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kSearchEngineAvoidFaviconOnlyCommits};
};

TEST_F(TemplateURLServiceSyncTestWithAvoidFaviconOnlyCommits,
       ShouldNotCommitFaviconOnlyChanges) {
  // Add a local-only search engine.
  TemplateURL* local_turl = model()->Add(
      CreateTestTemplateURL(u"localkey", "http://localkey.com", "localguid",
                            base::Time::FromTimeT(100)));

  // Add an account-only search engine.
  syncer::SyncDataList initial_data;
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURLData(
      CreateTestTemplateURL(u"keyword", "http://keyword.com", "guid",
                            base::Time::FromTimeT(100))
          ->data()));
  // Start syncing.
  model()->MergeDataAndStartSyncing(syncer::SEARCH_ENGINES, initial_data,
                                    PassProcessor());

  base::HistogramTester histogram_tester;

  // Local-only search engines: no affect since they have no account data.
  ASSERT_EQ(local_turl, model()->GetTemplateURLForGUID("localguid"));
  ASSERT_EQ(local_turl->GetAccountData(), std::nullopt);
  TemplateURLData data = local_turl->data();
  // Update the favicon URL.
  data.favicon_url = GURL("http://localfavicon.com");
  model()->UpdateData(local_turl, data);
  ASSERT_EQ(local_turl->favicon_url(), GURL("http://localfavicon.com"));
  ASSERT_EQ(local_turl->GetAccountData(), std::nullopt);
  ASSERT_EQ(0u, processor()->change_list_size());
  histogram_tester.ExpectTotalCount("Sync.SearchEngine.FaviconOnlyUpdate", 0);
  // Update any other field.
  data.SetKeyword(u"newkeyword");
  model()->UpdateData(local_turl, data);
  ASSERT_EQ(local_turl->keyword(), u"newkeyword");
  EXPECT_EQ(0u, processor()->change_list_size());
  histogram_tester.ExpectTotalCount("Sync.SearchEngine.FaviconOnlyUpdate", 0);

  // Search engines with account data.
  TemplateURL* account_turl = model()->GetTemplateURLForGUID("guid");
  ASSERT_NE(account_turl, nullptr);
  ASSERT_EQ(account_turl->GetLocalData(), std::nullopt);
  // Update the favicon URL.
  data = account_turl->data();
  data.favicon_url = GURL("http://favicon.com");
  model()->UpdateData(account_turl, data);
  ASSERT_EQ(account_turl->favicon_url(), GURL("http://favicon.com"));
  EXPECT_EQ(0u, processor()->change_list_size());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Sync.SearchEngine.FaviconOnlyUpdate"),
      base::BucketsAre(base::Bucket(true, 1)));

  // Update any other field.
  data.SetKeyword(u"newkeyword");
  model()->UpdateData(account_turl, data);
  ASSERT_EQ(account_turl->keyword(), u"newkeyword");
  EXPECT_EQ(1u, processor()->change_list_size());
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Sync.SearchEngine.FaviconOnlyUpdate"),
      base::BucketsAre(base::Bucket(false, 1), base::Bucket(true, 1)));
}
