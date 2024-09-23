// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "components/search_engines/util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/search_engine_specifics.pb.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using testing::IsNull;
using testing::NotNull;

namespace {

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

TestChangeProcessor::~TestChangeProcessor() {
}

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
  ~TestTemplateURLServiceClient() override {}

  void Shutdown() override {}
  void SetOwner(TemplateURLService* owner) override {}
  void DeleteAllSearchTermsForKeyword(TemplateURLID id) override {}
  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURLID id,
                                   const std::u16string& term) override {}
  void AddKeywordGeneratedVisit(const GURL& url) override {}
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
  syncer::SyncDataList CreateInitialSyncData() const;

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
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl));
}

syncer::SyncDataList TemplateURLServiceSyncTest::CreateInitialSyncData() const {
  syncer::SyncDataList list;

  std::unique_ptr<TemplateURL> turl = CreateTestTemplateURL(
      u"key1", "http://key1.com", "guid1", base::Time::FromTimeT(90));
  list.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl.get()));
  turl = CreateTestTemplateURL(u"key2", "http://key2.com", "guid2",
                               base::Time::FromTimeT(90));
  list.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl.get()));
  turl = CreateTestTemplateURL(u"key3", "http://key3.com", "guid3",
                               base::Time::FromTimeT(90));
  list.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl.get()));

  return list;
}

std::unique_ptr<TemplateURL> TemplateURLServiceSyncTest::Deserialize(
    const syncer::SyncData& sync_data) {
  syncer::SyncChangeList dummy;
  TestTemplateURLServiceClient client;
  return TemplateURLService::CreateTemplateURLFromTemplateURLAndSyncData(
      &client, /*prefs=*/nullptr, /*search_engine_choice_service=*/nullptr,
      SearchTermsData(), /*existing_turl=*/nullptr, sync_data, &dummy);
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
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl);
  // Convert the specifics back to a TemplateURL.
  std::unique_ptr<TemplateURL> deserialized(Deserialize(sync_data));
  EXPECT_TRUE(deserialized.get());
  // Ensure that the original and the deserialized TURLs are equal in values.
  AssertEquals(*turl, *deserialized);
}

TEST_F(TemplateURLServiceSyncTest, GetAllSyncDataBasic) {
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

TEST_F(TemplateURLServiceSyncTest, GetAllSyncDataWithOmniboxExtension) {
  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL(u"key2", "http://key2.com"));
  std::string fake_id("blahblahblah");
  std::string fake_url = std::string(kOmniboxScheme) + "://" + fake_id;
  model()->RegisterOmniboxKeyword(fake_id, "unittest", "key3", fake_url,
                                  Time());
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

TEST_F(TemplateURLServiceSyncTest, GetAllSyncDataWithSearchOverrideExtension) {
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

TEST_F(TemplateURLServiceSyncTest, GetAllSyncDataNoManagedEngines) {
  model()->Add(CreateTestTemplateURL(u"key1", "http://key1.com"));
  model()->Add(CreateTestTemplateURL(u"key2", "http://key2.com"));
  model()->Add(CreateTestTemplateURL(
      u"key3", "http://key3.com", std::string(), base::Time::FromTimeT(100),
      false, TemplateURLData::CreatedByPolicy::kDefaultSearchProvider));
  syncer::SyncDataList all_sync_data =
      model()->GetAllSyncData(syncer::SEARCH_ENGINES);

  EXPECT_EQ(2U, all_sync_data.size());

  for (syncer::SyncDataList::const_iterator iter = all_sync_data.begin();
      iter != all_sync_data.end(); ++iter) {
    std::string guid = GetGUID(*iter);
    TemplateURL* service_turl = model()->GetTemplateURLForGUID(guid);
    std::unique_ptr<TemplateURL> deserialized(Deserialize(*iter));
    ASSERT_EQ(service_turl->created_by_policy(),
              TemplateURLData::CreatedByPolicy::kNoPolicy);
    AssertEquals(*service_turl, *deserialized);
  }
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

TEST_F(TemplateURLServiceSyncTest, MergeInAllNewData) {
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

TEST_F(TemplateURLServiceSyncTest, MergeSyncIsTheSame) {
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

TEST_F(TemplateURLServiceSyncTest, MergeUpdateFromSync) {
  // The local data is the same as the sync data merged in, but timestamps have
  // changed. Ensure the right fields are merged in.
  syncer::SyncDataList initial_data;
  TemplateURL* turl1 = model()->Add(CreateTestTemplateURL(
      u"abc.com", "http://abc.com", "abc", base::Time::FromTimeT(9000)));
  model()->Add(CreateTestTemplateURL(u"xyz.com", "http://xyz.com", "xyz",
                                     base::Time::FromTimeT(9000)));

  std::unique_ptr<TemplateURL> turl1_newer = CreateTestTemplateURL(
      u"abc.com", "http://abc.ca", "abc", base::Time::FromTimeT(9999));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl1_newer));

  std::unique_ptr<TemplateURL> turl2_older = CreateTestTemplateURL(
      u"xyz.com", "http://xyz.ca", "xyz", base::Time::FromTimeT(8888));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl2_older));

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

TEST_F(TemplateURLServiceSyncTest, MergeAddFromOlderSyncData) {
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

TEST_F(TemplateURLServiceSyncTest, MergeAddFromNewerSyncData) {
  // GUIDs all differ, so Sync may overtake some entries, but the timestamps
  // from Sync are newer. Set up the local data so that one is a dupe, one has a
  // conflicting keyword, and the last has no conflicts (a clean ADD).
  // Duplicate keyword, same hostname
  model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "localguid1", base::Time::FromTimeT(10),
      false, TemplateURLData::CreatedByPolicy::kNoPolicy, 111));

  // Duplicate keyword, different hostname
  model()->Add(CreateTestTemplateURL(
      u"key2", "http://expected.com", "localguid2", base::Time::FromTimeT(10),
      false, TemplateURLData::CreatedByPolicy::kNoPolicy, 112));

  // Add
  model()->Add(CreateTestTemplateURL(
      u"unique", "http://unique.com", "localguid3", base::Time::FromTimeT(10),
      false, TemplateURLData::CreatedByPolicy::kNoPolicy, 113));

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

TEST_F(TemplateURLServiceSyncTest, MergeIgnoresPolicyAndPlayAPIEngines) {
  // Add a policy-created engine.
  model()->Add(CreateTestTemplateURL(
      u"key1", "http://key1.com", "localguid1", base::Time::FromTimeT(100),
      /*safe_for_autoreplace=*/false,
      /*created_by_policy=*/
      TemplateURLData::CreatedByPolicy::kDefaultSearchProvider));

  {
    auto play_api_engine = CreateTestTemplateURL(
        u"key2", "http://key2.com", "localguid2", base::Time::FromTimeT(100));
    TemplateURLData data(play_api_engine->data());
    data.created_from_play_api = true;
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
  model()->RegisterOmniboxKeyword("extension1", "unittest", "keyword1",
                                  "http://extension1", Time());
  TemplateURL* extension1 = model()->GetTemplateURLForKeyword(u"keyword1");
  ASSERT_TRUE(extension1);
  EXPECT_EQ(0U, processor()->change_list_size());

  model()->RegisterOmniboxKeyword("extension2", "unittest", "keyword2",
                                  "http://extension2", Time());
  TemplateURL* extension2 = model()->GetTemplateURLForKeyword(u"keyword2");
  ASSERT_TRUE(extension2);
  EXPECT_EQ(0U, processor()->change_list_size());

  // Create some sync changes that will conflict with the extension keywords.
  syncer::SyncChangeList changes;
  changes.push_back(CreateTestSyncChange(
      syncer::SyncChange::ACTION_ADD,
      CreateTestTemplateURL(u"keyword1", "http://aaa.com", std::string(),
                            base::Time::FromTimeT(100), true,
                            TemplateURLData::CreatedByPolicy::kNoPolicy, 0)));
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
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl));

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
  EXPECT_TRUE(model()->GetTemplateURLForGUID("guid2"));
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

TEST_F(TemplateURLServiceSyncTest, MergeTwiceWithSameSyncData) {
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
      TemplateURLService::CreateSyncDataFromTemplateURL(*temp_turl));

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

TEST_F(TemplateURLServiceSyncTest, SyncedDefaultGUIDArrivesFirst) {
  syncer::SyncDataList initial_data = CreateInitialSyncData();
  // The default search provider should support replacement.
  std::unique_ptr<TemplateURL> turl(
      CreateTestTemplateURL(u"key2", "http://key2.com/{searchTerms}", "guid2",
                            base::Time::FromTimeT(90)));
  initial_data[1] = TemplateURLService::CreateSyncDataFromTemplateURL(*turl);
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
  data.created_by_policy = TemplateURLData::CreatedByPolicy::kNoPolicy;
  data.prepopulate_id = 999999;
  data.sync_guid = "guid2";
  std::unique_ptr<TemplateURL> turl2(new TemplateURL(data));
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURL(
      *turl1));
  initial_data.push_back(TemplateURLService::CreateSyncDataFromTemplateURL(
      *turl2));
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
  data.created_by_policy = TemplateURLData::CreatedByPolicy::kNoPolicy;
  data.prepopulate_id = 999999;
  data.sync_guid = "guid2";
  std::unique_ptr<TemplateURL> turl2(new TemplateURL(data));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl1));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl2));
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
  initial_data[1] = TemplateURLService::CreateSyncDataFromTemplateURL(*turl);

  // When the default changes, a second notify is triggered.
  MergeAndExpectNotifyAtLeast(initial_data);

  // Ensure that the new default has been set.
  EXPECT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
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
  EXPECT_EQ(4U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  ASSERT_EQ(default_search, model()->GetDefaultSearchProvider());
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

TEST_F(TemplateURLServiceSyncTest, SyncWithExtensionDefaultSearch) {
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

TEST_F(TemplateURLServiceSyncTest, SyncMergeDeletesDefault) {
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
  initial_data[0] = TemplateURLService::CreateSyncDataFromTemplateURL(*turl);
  MergeAndExpectNotify(initial_data, 1);

  EXPECT_EQ(3U, model()->GetAllSyncData(syncer::SEARCH_ENGINES).size());
  EXPECT_FALSE(model()->GetTemplateURLForGUID("whateverguid"));
  EXPECT_EQ(model()->GetDefaultSearchProvider(),
            model()->GetTemplateURLForGUID("guid1"));
}

TEST_F(TemplateURLServiceSyncTest, LocalDefaultWinsConflict) {
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
  initial_data[0] = TemplateURLService::CreateSyncDataFromTemplateURL(*turl);
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

TEST_F(TemplateURLServiceSyncTest, PreSyncDeletes) {
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

TEST_F(TemplateURLServiceSyncTest, PreSyncUpdates) {
  const char kNewKeyword[] = "somethingnew";
  const char16_t kNewKeyword16[] = u"somethingnew";
  // Fetch the prepopulate search engines so we know what they are.
  std::vector<std::unique_ptr<TemplateURLData>> prepop_turls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          profile_a()->GetTestingPrefService(),
          test_util_a_->search_engine_choice_service());

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
      TemplateURLService::CreateSyncDataFromTemplateURL(*sync_turl));

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
  EXPECT_EQ(new_timestamp, base::Time::FromInternalValue(
      change.sync_data().GetSpecifics().search_engine().last_modified()));
}

TEST_F(TemplateURLServiceSyncTest, SyncBaseURLs) {
  // Verify that bringing in a remote TemplateURL that uses Google base URLs
  // causes it to get a local keyword that matches the local base URL.
  syncer::SyncDataList initial_data;
  std::unique_ptr<TemplateURL> turl(CreateTestTemplateURL(
      u"google.co.uk", "{google:baseURL}search?q={searchTerms}", "guid"));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl));
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

TEST_F(TemplateURLServiceSyncTest, MergeInSyncTemplateURL) {
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
  const struct {
    ExpectedTemplateURL conflict_winner;
    ExpectedTemplateURL synced_at_start;
    ExpectedTemplateURL update_sent;
    ExpectedTemplateURL present_in_model;
    bool keywords_conflict;
    size_t final_num_turls;
  } test_cases[] = {
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
  };

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
          TemplateURLService::CreateSyncDataFromTemplateURL(*sync_turl);
    }
    if (test_cases[i].synced_at_start == BOTH) {
      sync_data[local_turl->sync_guid()] =
          TemplateURLService::CreateSyncDataFromTemplateURL(*local_turl);
    }
    SyncDataMap initial_data;
    initial_data[local_turl->sync_guid()] =
        TemplateURLService::CreateSyncDataFromTemplateURL(*local_turl);

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
    if (test_cases[i].update_sent == LOCAL)
      expected_update_guid = local_guid;
    else if (test_cases[i].update_sent == SYNC)
      expected_update_guid = sync_guid;
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
  }  // for
}

TEST_F(TemplateURLServiceSyncTest, MergePrepopulatedEngine) {
  std::unique_ptr<TemplateURLData> default_turl(
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          /*prefs=*/nullptr,
          /*search_engine_choice_service=*/nullptr));

  // Merge with an initial list containing a prepopulated engine with a wrong
  // URL.
  syncer::SyncDataList list;
  std::unique_ptr<TemplateURL> sync_turl = CopyTemplateURL(
      default_turl.get(), "http://wrong.url.com?q={searchTerms}", "default");
  list.push_back(TemplateURLService::CreateSyncDataFromTemplateURL(*sync_turl));
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
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          /*prefs=*/nullptr,
          /*search_engine_choice_service=*/nullptr));
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
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          /*prefs=*/nullptr,
          /*search_engine_choice_service=*/nullptr));

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
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          /*prefs=*/nullptr,
          /*search_engine_choice_service=*/nullptr));

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
  list.push_back(TemplateURLService::CreateSyncDataFromTemplateURL(*sync_turl));
  MergeAndExpectNotify(list, 1);

  const TemplateURL* result_turl = model()->GetTemplateURLForGUID("default");
  EXPECT_TRUE(result_turl);
  EXPECT_EQ(u"new_kw", result_turl->keyword());
  EXPECT_EQ(u"my name", result_turl->short_name());
  EXPECT_EQ(default_turl->url(), result_turl->url());
}

TEST_F(TemplateURLServiceSyncTest, MergeConflictingPrepopulatedEngine) {
  std::unique_ptr<TemplateURLData> default_turl(
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          /*prefs=*/nullptr,
          /*search_engine_choice_service=*/nullptr));

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
  list.push_back(TemplateURLService::CreateSyncDataFromTemplateURL(*sync_turl));
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
      *TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          /*prefs=*/nullptr,
          /*search_engine_choice_service=*/nullptr);

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

  syncer::SyncDataList list{TemplateURLService::CreateSyncDataFromTemplateURL(
      TemplateURL(changed_data))};
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
      *TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          /*prefs=*/nullptr,
          /*search_engine_choice_service=*/nullptr);

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
  syncer::SyncDataList list{TemplateURLService::CreateSyncDataFromTemplateURL(
                                TemplateURL(changed_data)),
                            TemplateURLService::CreateSyncDataFromTemplateURL(
                                TemplateURL(added_data))};
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
      *TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          /*prefs=*/nullptr,
          /*search_engine_choice_service=*/nullptr);

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
  syncer::SyncDataList list{TemplateURLService::CreateSyncDataFromTemplateURL(
                                TemplateURL(added_data)),
                            TemplateURLService::CreateSyncDataFromTemplateURL(
                                TemplateURL(changed_data))};
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
      *TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          /*prefs=*/nullptr,
          /*search_engine_choice_service=*/nullptr);

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
  syncer::SyncDataList list{TemplateURLService::CreateSyncDataFromTemplateURL(
                                TemplateURL(changed_data)),
                            TemplateURLService::CreateSyncDataFromTemplateURL(
                                TemplateURL(added_data))};
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
      *TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          /*prefs=*/nullptr,
          /*search_engine_choice_service=*/nullptr);

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
  syncer::SyncDataList list{TemplateURLService::CreateSyncDataFromTemplateURL(
                                TemplateURL(added_data)),
                            TemplateURLService::CreateSyncDataFromTemplateURL(
                                TemplateURL(changed_data))};
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
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          /*prefs=*/nullptr,
          /*search_engine_choice_service=*/nullptr));

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
  list.push_back(TemplateURLService::CreateSyncDataFromTemplateURL(*sync_turl));
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
  model()->RegisterOmniboxKeyword("extension1", "unittest", "keyword1",
                                  "http://extension1", Time());

  // Try to merge in a turl with preopulate_id also set to 0. This should work.
  syncer::SyncDataList initial_data;
  std::unique_ptr<TemplateURL> turl(
      CreateTestTemplateURL(u"what", "http://thewhat.com/{searchTerms}",
                            "normal_guid", base::Time::FromTimeT(10), true,
                            TemplateURLData::CreatedByPolicy::kNoPolicy, 0));
  initial_data.push_back(
      TemplateURLService::CreateSyncDataFromTemplateURL(*turl));

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
      TemplateURLService::CreateSyncDataFromTemplateURL(TemplateURL(data)),
      TemplateURLService::CreateSyncDataFromTemplateURL(
          TemplateURL(invalid_data))};
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
