// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"

#include <stddef.h>

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/features.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/dictionary_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::HistogramBase;
using base::HistogramSamples;
using base::StatisticsRecorder;
using testing::UnorderedElementsAre;

namespace {

// Create a sync data list using all words in the dictionary, irrespective of if
// they are synced or not.
syncer::SyncDataList CreateSyncDataListFromDictionary(
    const SpellcheckCustomDictionary* dictionary) {
  syncer::SyncDataList data;
  for (const std::string& word : dictionary->GetWords()) {
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    data.push_back(syncer::SyncData::CreateLocalData(word, word, specifics));
  }
  return data;
}

}  // namespace

static std::unique_ptr<KeyedService> BuildSpellcheckService(
    content::BrowserContext* profile) {
  return std::make_unique<SpellcheckService>(static_cast<Profile*>(profile));
}

class SpellcheckCustomDictionaryTest : public testing::Test {
 public:
  SpellcheckCustomDictionaryTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

 protected:
  void SetUp() override {
    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_, base::BindRepeating(&BuildSpellcheckService));
  }

  void TearDown() override {
    // Allow tasks with pending I/O (e.g.,
    // SpellcheckCustomDictionary::LoadDictionaryFile) to complete before
    // destroying the testing profiles.
    task_environment_.RunUntilIdle();
  }

  // A wrapper around SpellcheckCustomDictionary::LoadDictionaryFile private
  // function to avoid a large number of FRIEND_TEST declarations in
  // SpellcheckCustomDictionary.
  std::unique_ptr<SpellcheckCustomDictionary::LoadFileResult>
  LoadDictionaryFile(const base::FilePath& path) {
    return SpellcheckCustomDictionary::LoadDictionaryFile(path);
  }

  // A wrapper around SpellcheckCustomDictionary::UpdateDictionaryFile private
  // function to avoid a large number of FRIEND_TEST declarations in
  // SpellcheckCustomDictionary.
  void UpdateDictionaryFile(
      std::unique_ptr<SpellcheckCustomDictionary::Change> dictionary_change,
      const base::FilePath& path) {
    SpellcheckCustomDictionary::UpdateDictionaryFile(
        std::move(dictionary_change), path);
  }

  // A wrapper around SpellcheckCustomDictionary::OnLoaded private method to
  // avoid a large number of FRIEND_TEST declarations in
  // SpellcheckCustomDictionary.
  void OnLoaded(SpellcheckCustomDictionary& dictionary,
                std::unique_ptr<std::set<std::string>> words) {
    std::unique_ptr<SpellcheckCustomDictionary::LoadFileResult> result(
        new SpellcheckCustomDictionary::LoadFileResult);
    result->is_valid_file = true;
    result->words = *words;
    dictionary.OnLoaded(std::move(result));
  }

  // A wrapper around SpellcheckCustomDictionary::Apply private method to avoid
  // a large number of FRIEND_TEST declarations in SpellcheckCustomDictionary.
  void Apply(
      SpellcheckCustomDictionary& dictionary,
      const SpellcheckCustomDictionary::Change& change) {
    return dictionary.Apply(change);
  }

  // Returns the custom dictionary for an extra profile, created on demand.
  SpellcheckCustomDictionary* MakeExtraProfileDictionary() {
    extra_profiles_.emplace_back();
    auto* const extra_profile = &extra_profiles_.back();
    return static_cast<SpellcheckService*>(
               SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
                   extra_profile, base::BindRepeating(&BuildSpellcheckService)))
        ->GetCustomDictionary();
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  std::list<TestingProfile> extra_profiles_;
};

// Counts the number of notifications for dictionary load and change.
class DictionaryObserverCounter : public SpellcheckCustomDictionary::Observer {
 public:
  DictionaryObserverCounter() : loads_(0), changes_(0) {}

  DictionaryObserverCounter(const DictionaryObserverCounter&) = delete;
  DictionaryObserverCounter& operator=(const DictionaryObserverCounter&) =
      delete;

  virtual ~DictionaryObserverCounter() = default;

  int loads() const { return loads_; }
  int changes() const { return changes_; }

  // Overridden from SpellcheckCustomDictionary::Observer:
  void OnCustomDictionaryLoaded() override { loads_++; }
  void OnCustomDictionaryChanged(
      const SpellcheckCustomDictionary::Change& change) override {
    changes_++;
  }

 private:
  int loads_;
  int changes_;
};

TEST_F(SpellcheckCustomDictionaryTest, SaveAndLoad) {
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);

  // The custom word list should be empty now.
  EXPECT_TRUE(LoadDictionaryFile(path)->words.empty());

  std::unique_ptr<SpellcheckCustomDictionary::Change> change(
      new SpellcheckCustomDictionary::Change);
  change->AddWord("bar");
  change->AddWord("foo");

  UpdateDictionaryFile(std::move(change), path);
  std::set<std::string> expected;
  expected.insert("bar");
  expected.insert("foo");

  // The custom word list should include written words.
  EXPECT_EQ(expected, LoadDictionaryFile(path)->words);

  std::unique_ptr<SpellcheckCustomDictionary::Change> change2(
      new SpellcheckCustomDictionary::Change);
  change2->RemoveWord("bar");
  change2->RemoveWord("foo");
  UpdateDictionaryFile(std::move(change2), path);
  EXPECT_TRUE(LoadDictionaryFile(path)->words.empty());
}

TEST_F(SpellcheckCustomDictionaryTest, MultiProfile) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  std::set<std::string> expected1;
  std::set<std::string> expected2;

  custom_dictionary->AddWord("foo");
  custom_dictionary->AddWord("bar");
  expected1.insert("foo");
  expected1.insert("bar");

  custom_dictionary2->AddWord("hoge");
  custom_dictionary2->AddWord("fuga");
  expected2.insert("hoge");
  expected2.insert("fuga");

  std::set<std::string> actual1 = custom_dictionary->GetWords();
  EXPECT_EQ(actual1, expected1);

  std::set<std::string> actual2 = custom_dictionary2->GetWords();
  EXPECT_EQ(actual2, expected2);
}

// Legacy empty dictionary should be converted to new format empty dictionary.
TEST_F(SpellcheckCustomDictionaryTest, LegacyEmptyDictionaryShouldBeConverted) {
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content;
  base::WriteFile(path, content);
  EXPECT_TRUE(LoadDictionaryFile(path)->words.empty());
}

// Legacy dictionary with two words should be converted to new format dictionary
// with two words.
TEST_F(SpellcheckCustomDictionaryTest,
       LegacyDictionaryWithTwoWordsShouldBeConverted) {
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content = "foo\nbar\nfoo\n";
  base::WriteFile(path, content);
  std::set<std::string> expected;
  expected.insert("bar");
  expected.insert("foo");
  EXPECT_EQ(expected, LoadDictionaryFile(path)->words);
}

// Illegal words should be removed. Leading and trailing whitespace should be
// trimmed.
TEST_F(SpellcheckCustomDictionaryTest,
       IllegalWordsShouldBeRemovedFromDictionary) {
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content = "foo\n foo bar \n\n \nbar\n"
      "01234567890123456789012345678901234567890123456789"
      "01234567890123456789012345678901234567890123456789";
  base::WriteFile(path, content);
  std::set<std::string> expected;
  expected.insert("bar");
  expected.insert("foo");
  expected.insert("foo bar");
  EXPECT_EQ(expected, LoadDictionaryFile(path)->words);
}

// Write to dictionary should backup previous version and write the word to the
// end of the dictionary. If the dictionary file is corrupted on disk, the
// previous version should be reloaded.
TEST_F(SpellcheckCustomDictionaryTest, CorruptedWriteShouldBeRecovered) {
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content = "foo\nbar";
  base::WriteFile(path, content);
  std::set<std::string> expected;
  expected.insert("bar");
  expected.insert("foo");
  EXPECT_EQ(expected, LoadDictionaryFile(path)->words);

  std::unique_ptr<SpellcheckCustomDictionary::Change> change(
      new SpellcheckCustomDictionary::Change);
  change->AddWord("baz");
  UpdateDictionaryFile(std::move(change), path);
  content.clear();
  base::ReadFileToString(path, &content);
  content.append("corruption");
  base::WriteFile(path, content);
  EXPECT_EQ(expected, LoadDictionaryFile(path)->words);
}

TEST_F(SpellcheckCustomDictionaryTest, ProcessSyncChanges) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* dictionary =
      spellcheck_service->GetCustomDictionary();

  dictionary->AddWord("foo");
  dictionary->AddWord("bar");

  syncer::SyncChangeList changes;
  {
    // Add existing word.
    std::string word = "foo";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }
  {
    // Add invalid word. This word is too long.
    std::string word = "01234567890123456789012345678901234567890123456789"
        "01234567890123456789012345678901234567890123456789";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }
  {
    // Add valid word.
    std::string word = "baz";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }
  {
    // Remove missing word.
    std::string word = "snafoo";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }
  {
    // Remove existing word.
    std::string word = "bar";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.push_back(syncer::SyncChange(
        FROM_HERE,
        syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateLocalData(word, word, specifics)));
  }

  EXPECT_FALSE(dictionary->ProcessSyncChanges(FROM_HERE, changes).has_value());

  const std::set<std::string>& words = dictionary->GetWords();
  EXPECT_EQ(2UL, words.size());
  EXPECT_EQ(0UL, words.count("bar"));
  EXPECT_EQ(1UL, words.count("foo"));
  EXPECT_EQ(1UL, words.count("baz"));
}

TEST_F(SpellcheckCustomDictionaryTest, SyncBeforeLoadDoesNotDuplicateWords) {
  // Test triggers network requests since it indirectly instantiates
  // SpellcheckHunspellDictionary's.
  // Install a mock server to avoid sending random network request to
  // localhost.
  net::EmbeddedTestServer embedded_test_server;
  embedded_test_server.RegisterRequestHandler(
      base::BindRepeating([](const net::test_server::HttpRequest& request) {
        // Mock implementation to hang the request.
        std::unique_ptr<net::test_server::HttpResponse> response;
        return response;
      }));
  net::test_server::RegisterDefaultHandlers(&embedded_test_server);
  ASSERT_TRUE(embedded_test_server.Start());

  // Forcibly set a hanging URL.
  GURL url = embedded_test_server.GetURL("/hang");
  SpellcheckHunspellDictionary::SetDownloadURLForTesting(url);

  SpellcheckCustomDictionary* custom_dictionary =
      SpellcheckServiceFactory::GetForContext(&profile_)->GetCustomDictionary();

  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  std::unique_ptr<SpellcheckCustomDictionary::Change> change(
      new SpellcheckCustomDictionary::Change);
  change->AddWord("foo");
  Apply(*custom_dictionary2, *change);

  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);
  UpdateDictionaryFile(std::move(change), path);
  EXPECT_TRUE(custom_dictionary->GetWords().empty());

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  std::set<std::string> expected_words_in_memory;
  expected_words_in_memory.insert("foo");
  EXPECT_EQ(expected_words_in_memory, custom_dictionary->GetWords());

  // Finish all writes to disk.
  content::RunAllTasksUntilIdle();

  std::string actual_contents_on_disk;
  base::ReadFileToString(path, &actual_contents_on_disk);
  static const char kExpectedContentsPrefix[] = "foo\nchecksum_v1 = ";
  EXPECT_EQ(
      kExpectedContentsPrefix,
      actual_contents_on_disk.substr(0, sizeof kExpectedContentsPrefix - 1));
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigAndServerFull) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  SpellcheckCustomDictionary::Change change;
  SpellcheckCustomDictionary::Change change2;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
    change2.AddWord("bar" + base::NumberToString(i));
  }
  change.AddWord("foo");
  Apply(*custom_dictionary, change);
  Apply(*custom_dictionary2, change2);

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetWords().size());

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_EQ(custom_dictionary->IsSyncing(),
            base::FeatureList::IsEnabled(
                syncer::kSpellcheckSeparateLocalAndAccountDictionaries));

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords * 2 + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetWords().size());
}

TEST_F(SpellcheckCustomDictionaryTest, ServerTooBig) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  SpellcheckCustomDictionary::Change change;
  SpellcheckCustomDictionary::Change change2;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords + 1; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
    change2.AddWord("bar" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);
  Apply(*custom_dictionary2, change2);

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary2->GetWords().size());

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_EQ(custom_dictionary->IsSyncing(),
            base::FeatureList::IsEnabled(
                syncer::kSpellcheckSeparateLocalAndAccountDictionaries));

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords * 2 + 2,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary2->GetWords().size());
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigToContiueSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords - 1; ++i) {
    custom_dictionary->AddWord("foo" + base::NumberToString(i));
  }
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  custom_dictionary->AddWord("bar");
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  custom_dictionary->AddWord("baz");
  EXPECT_EQ(custom_dictionary->IsSyncing(),
            base::FeatureList::IsEnabled(
                syncer::kSpellcheckSeparateLocalAndAccountDictionaries));

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetWords().size());
}

TEST_F(SpellcheckCustomDictionaryTest, LoadAfterSyncStart) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  custom_dictionary->AddWord("foo");

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  std::unique_ptr<std::set<std::string>> custom_words(
      new std::set<std::string>);
  custom_words->insert("bar");
  OnLoaded(*custom_dictionary, std::move(custom_words));
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(2UL, custom_dictionary->GetWords().size());
  EXPECT_EQ(2UL, custom_dictionary2->GetWords().size());
}

TEST_F(SpellcheckCustomDictionaryTest, LoadAfterSyncStartTooBigToSync) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  custom_dictionary->AddWord("foo");

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  std::unique_ptr<std::set<std::string>> custom_words(
      new std::set<std::string>);
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords; ++i) {
    custom_words->insert(custom_words->end(), "foo" + base::NumberToString(i));
  }
  OnLoaded(*custom_dictionary, std::move(custom_words));
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetWords().size());
}

TEST_F(SpellcheckCustomDictionaryTest, LoadDuplicatesAfterSync) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords / 2; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  OnLoaded(*custom_dictionary,
           std::make_unique<std::set<std::string>>(change.to_add()));
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords / 2,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords / 2,
            custom_dictionary2->GetWords().size());
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryLoadNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();

  DictionaryObserverCounter observer;
  custom_dictionary->AddObserver(&observer);

  std::unique_ptr<std::set<std::string>> custom_words(
      new std::set<std::string>);
  custom_words->insert("foo");
  custom_words->insert("bar");
  OnLoaded(*custom_dictionary, std::move(custom_words));

  EXPECT_GE(observer.loads(), 1);
  EXPECT_LE(observer.loads(), 2);
  EXPECT_EQ(0, observer.changes());

  custom_dictionary->RemoveObserver(&observer);
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryAddWordNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();

  OnLoaded(*custom_dictionary, base::WrapUnique(new std::set<std::string>));

  DictionaryObserverCounter observer;
  custom_dictionary->AddObserver(&observer);

  EXPECT_TRUE(custom_dictionary->AddWord("foo"));
  EXPECT_TRUE(custom_dictionary->AddWord("bar"));
  EXPECT_FALSE(custom_dictionary->AddWord("bar"));

  EXPECT_EQ(2, observer.changes());

  custom_dictionary->RemoveObserver(&observer);
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryRemoveWordNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();

  OnLoaded(*custom_dictionary, base::WrapUnique(new std::set<std::string>));

  EXPECT_TRUE(custom_dictionary->AddWord("foo"));
  EXPECT_TRUE(custom_dictionary->AddWord("bar"));

  DictionaryObserverCounter observer;
  custom_dictionary->AddObserver(&observer);

  EXPECT_TRUE(custom_dictionary->RemoveWord("foo"));
  EXPECT_TRUE(custom_dictionary->RemoveWord("bar"));
  EXPECT_FALSE(custom_dictionary->RemoveWord("baz"));

  EXPECT_EQ(2, observer.changes());

  custom_dictionary->RemoveObserver(&observer);
}

// The server has maximum number of words and the client has maximum number of
// different words before association time. No new words should be pushed to the
// sync server upon association. The client should accept words from the sync
// server, however.
// TODO(crbug.com/460064444): Maybe re-enable this test on Desktop Android
// builds. This flow is never exercised on Android because Dictionary is not
// synced on Android, but maybe this test failure hints at a real bug.
#if !BUILDFLAG(IS_DESKTOP_ANDROID)
TEST_F(SpellcheckCustomDictionaryTest, DictionarySyncLimit) {
  // Here, |server_custom_dictionary| plays the role of the sync server.
  SpellcheckCustomDictionary* server_custom_dictionary =
      MakeExtraProfileDictionary();

  // Upload the maximum number of words to the sync server.
  {
    SpellcheckService* spellcheck_service =
        SpellcheckServiceFactory::GetForContext(&profile_);
    SpellcheckCustomDictionary* custom_dictionary =
        spellcheck_service->GetCustomDictionary();

    ASSERT_FALSE(
        custom_dictionary
            ->MergeDataAndStartSyncing(
                syncer::DICTIONARY,
                CreateSyncDataListFromDictionary(server_custom_dictionary),
                std::unique_ptr<syncer::SyncChangeProcessor>(
                    new syncer::SyncChangeProcessorWrapperForTest(
                        server_custom_dictionary)))
            .has_value());

    for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords; ++i) {
      custom_dictionary->AddWord("foo" + base::NumberToString(i));
    }

    EXPECT_TRUE(custom_dictionary->IsSyncing());
    EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
              custom_dictionary->GetWords().size());
  }

  // The sync server now has the maximum number of words.
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            server_custom_dictionary->GetWords().size());

  // Associate the sync server with a client that also has the maximum number of
  // words, but all of these words are different from the ones on the sync
  // server.
  {
    // Here, |client_custom_dictionary| plays the role of the client.
    SpellcheckCustomDictionary* client_custom_dictionary =
        MakeExtraProfileDictionary();

    // Associate the server and the client.
    ASSERT_FALSE(
        client_custom_dictionary
            ->MergeDataAndStartSyncing(
                syncer::DICTIONARY,
                CreateSyncDataListFromDictionary(server_custom_dictionary),
                std::unique_ptr<syncer::SyncChangeProcessor>(
                    new syncer::SyncChangeProcessorWrapperForTest(
                        server_custom_dictionary)))
            .has_value());
    ASSERT_TRUE(client_custom_dictionary->IsSyncing());

    // Add the maximum number of words to the client. These words are all
    // different from those on the server.
    for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords; ++i) {
      client_custom_dictionary->AddWord("bar" + base::NumberToString(i));
    }
    EXPECT_EQ(client_custom_dictionary->IsSyncing(),
              base::FeatureList::IsEnabled(
                  syncer::kSpellcheckSeparateLocalAndAccountDictionaries));
    EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords * 2,
              client_custom_dictionary->GetWords().size());
  }

  // The sync server should not receive more words, because it has the maximum
  // number of words already.
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            server_custom_dictionary->GetWords().size());
}
#endif  // !BUILDFLAG(IS_DESKTOP_ANDROID)

TEST_F(SpellcheckCustomDictionaryTest, HasWord) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  OnLoaded(*custom_dictionary, base::WrapUnique(new std::set<std::string>));
  EXPECT_FALSE(custom_dictionary->HasWord("foo"));
  EXPECT_FALSE(custom_dictionary->HasWord("bar"));
  custom_dictionary->AddWord("foo");
  EXPECT_TRUE(custom_dictionary->HasWord("foo"));
  EXPECT_FALSE(custom_dictionary->HasWord("bar"));
}

class SpellcheckCustomDictionaryTestWithoutSeparateLocalAndAccountDictionaries
    : public SpellcheckCustomDictionaryTest {
 protected:
  SpellcheckCustomDictionaryTestWithoutSeparateLocalAndAccountDictionaries() {
    scoped_feature_list_.InitAndDisableFeature(
        syncer::kSpellcheckSeparateLocalAndAccountDictionaries);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SpellcheckCustomDictionaryTestWithoutSeparateLocalAndAccountDictionaries,
       MergeDataAndStartSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords / 2; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);

  SpellcheckCustomDictionary::Change change2;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords / 2; ++i) {
    change2.AddWord("bar" + base::NumberToString(i));
  }
  Apply(*custom_dictionary2, change2);

  std::set<std::string> merged_words = base::STLSetUnion<std::set<std::string>>(
      custom_dictionary->GetWords(), custom_dictionary2->GetWords());

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(custom_dictionary->GetWords(), merged_words);
  EXPECT_EQ(custom_dictionary2->GetWords(), merged_words);
}

TEST_F(SpellcheckCustomDictionaryTestWithoutSeparateLocalAndAccountDictionaries,
       StopSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords / 2; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);

  SpellcheckCustomDictionary::Change change2;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords / 2; ++i) {
    change2.AddWord("bar" + base::NumberToString(i));
  }
  Apply(*custom_dictionary2, change2);

  std::set<std::string> merged_words = base::STLSetUnion<std::set<std::string>>(
      custom_dictionary->GetWords(), custom_dictionary2->GetWords());

  ASSERT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());

  ASSERT_EQ(custom_dictionary->GetWords(), merged_words);
  ASSERT_EQ(custom_dictionary2->GetWords(), merged_words);

  custom_dictionary->StopSyncing(syncer::DICTIONARY);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  // The sync data was merged into the local dictionary and stays back even
  // after StopSyncing.
  EXPECT_EQ(custom_dictionary->GetWords(), merged_words);
  EXPECT_EQ(custom_dictionary2->GetWords(), merged_words);
}

TEST_F(SpellcheckCustomDictionaryTestWithoutSeparateLocalAndAccountDictionaries,
       DictionarySyncNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  OnLoaded(*custom_dictionary, base::WrapUnique(new std::set<std::string>));
  OnLoaded(*custom_dictionary2, base::WrapUnique(new std::set<std::string>));

  custom_dictionary->AddWord("foo");
  custom_dictionary->AddWord("bar");
  custom_dictionary2->AddWord("foo");
  custom_dictionary2->AddWord("baz");

  DictionaryObserverCounter observer;
  custom_dictionary->AddObserver(&observer);

  DictionaryObserverCounter observer2;
  custom_dictionary2->AddObserver(&observer2);

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(1, observer.changes());
  EXPECT_EQ(1, observer2.changes());

  custom_dictionary->RemoveObserver(&observer);
  custom_dictionary2->RemoveObserver(&observer2);
}

TEST_F(SpellcheckCustomDictionaryTestWithoutSeparateLocalAndAccountDictionaries,
       DictionaryTooBigBeforeSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords + 1; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetWords().size());
}

TEST_F(SpellcheckCustomDictionaryTestWithoutSeparateLocalAndAccountDictionaries,
       DictionaryTooBigToStartSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords - 1; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);

  custom_dictionary2->AddWord("bar");
  custom_dictionary2->AddWord("baz");

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetWords().size());
}

class SpellcheckCustomDictionaryTestWithSeparateLocalAndAccountDictionaries
    : public SpellcheckCustomDictionaryTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      syncer::kSpellcheckSeparateLocalAndAccountDictionaries};
};

TEST_F(SpellcheckCustomDictionaryTestWithSeparateLocalAndAccountDictionaries,
       MergeDataAndStartSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords / 2; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);

  SpellcheckCustomDictionary::Change change2;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords / 2; ++i) {
    change2.AddWord("bar" + base::NumberToString(i));
  }
  Apply(*custom_dictionary2, change2);

  std::set<std::string> local_words = custom_dictionary->GetWords();
  std::set<std::string> account_words = custom_dictionary2->GetWords();
  std::set<std::string> merged_words = base::STLSetUnion<std::set<std::string>>(
      custom_dictionary->GetWords(), custom_dictionary2->GetWords());

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  // `words` should now also contain all the words from `words2`, but not vice
  // versa since local data is not committed upon MergeDataAndStartSyncing.
  EXPECT_EQ(custom_dictionary->GetWords(), merged_words);
  EXPECT_EQ(custom_dictionary2->GetWords(), account_words);
}

TEST_F(SpellcheckCustomDictionaryTestWithSeparateLocalAndAccountDictionaries,
       StopSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords / 2; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);

  SpellcheckCustomDictionary::Change change2;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords / 2; ++i) {
    change2.AddWord("bar" + base::NumberToString(i));
  }
  Apply(*custom_dictionary2, change2);

  std::set<std::string> local_words = custom_dictionary->GetWords();
  std::set<std::string> account_words = custom_dictionary2->GetWords();
  std::set<std::string> merged_words = base::STLSetUnion<std::set<std::string>>(
      custom_dictionary->GetWords(), custom_dictionary2->GetWords());

  ASSERT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());

  ASSERT_EQ(custom_dictionary->GetWords(), merged_words);
  ASSERT_EQ(custom_dictionary2->GetWords(), account_words);

  custom_dictionary->StopSyncing(syncer::DICTIONARY);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  // The account data was kept separate, so after StopSyncing, the local
  // dictionary should contain only the local words.
  EXPECT_EQ(custom_dictionary->GetWords(), local_words);
  EXPECT_EQ(custom_dictionary2->GetWords(), account_words);
}

TEST_F(SpellcheckCustomDictionaryTestWithSeparateLocalAndAccountDictionaries,
       DictionarySyncNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* custom_dictionary2 = MakeExtraProfileDictionary();

  OnLoaded(*custom_dictionary, base::WrapUnique(new std::set<std::string>));
  OnLoaded(*custom_dictionary2, base::WrapUnique(new std::set<std::string>));

  custom_dictionary->AddWord("foo");
  custom_dictionary->AddWord("bar");
  custom_dictionary2->AddWord("foo");
  custom_dictionary2->AddWord("baz");

  DictionaryObserverCounter observer;
  custom_dictionary->AddObserver(&observer);

  DictionaryObserverCounter observer2;
  custom_dictionary2->AddObserver(&observer2);

  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)))
                   .has_value());
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(1, observer.changes());
  // No changes are committed back upon MergeDataAndStartSyncing.
  EXPECT_EQ(0, observer2.changes());

  custom_dictionary->RemoveObserver(&observer);
  custom_dictionary2->RemoveObserver(&observer2);
}

TEST_F(SpellcheckCustomDictionaryTestWithSeparateLocalAndAccountDictionaries,
       ProcessSyncChangesOnlyAffectsAccountDictionary) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* dictionary =
      spellcheck_service->GetCustomDictionary();
  SpellcheckCustomDictionary* server_dictionary = MakeExtraProfileDictionary();

  dictionary->AddWord("foo");
  dictionary->AddWord("bar");
  server_dictionary->AddWord("foo");
  server_dictionary->AddWord("baz");

  ASSERT_FALSE(dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       CreateSyncDataListFromDictionary(server_dictionary),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               server_dictionary)))
                   .has_value());
  ASSERT_TRUE(dictionary->IsSyncing());
  ASSERT_THAT(dictionary->GetWords(),
              UnorderedElementsAre("foo", "bar", "baz"));

  syncer::SyncChangeList changes;
  {
    // Add new word.
    // This should add the word to the account dictionary.
    std::string word = "baz2";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.emplace_back(
        FROM_HERE, syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateLocalData(word, word, specifics));
  }
  {
    // Remove common word.
    // This should only remove the word from the account dictionary but not
    // affect the local dictionary.
    std::string word = "foo";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.emplace_back(
        FROM_HERE, syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateLocalData(word, word, specifics));
  }
  {
    // Remove local-only word.
    // This should be a no-op.
    std::string word = "bar";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.emplace_back(
        FROM_HERE, syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateLocalData(word, word, specifics));
  }
  {
    // Remove account-only word.
    std::string word = "baz";
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_dictionary()->set_word(word);
    changes.emplace_back(
        FROM_HERE, syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateLocalData(word, word, specifics));
  }

  EXPECT_FALSE(dictionary->ProcessSyncChanges(FROM_HERE, changes).has_value());

  // The local dictionary should be unaffected by the sync changes. The account
  // dictionary should only contain the new word now.
  EXPECT_THAT(dictionary->GetWords(),
              UnorderedElementsAre("foo", "bar", "baz2"));

  dictionary->StopSyncing(syncer::DICTIONARY);
  ASSERT_FALSE(dictionary->IsSyncing());
  // The local dictionary should be unaffected by the sync changes.
  EXPECT_THAT(dictionary->GetWords(), UnorderedElementsAre("foo", "bar"));
}
