// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor_wrapper_for_test.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/protocol/sync.pb.h"
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

namespace {

// Get all sync data for the custom dictionary without limiting to maximum
// number of syncable words.
syncer::SyncDataList GetAllSyncDataNoLimit(
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

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
};

// An implementation of SyncErrorFactory that does not upload the error message
// and updates an outside error counter. This lets us know the number of error
// messages in an instance of this class after that instance is deleted.
class SyncErrorFactoryStub : public syncer::SyncErrorFactory {
 public:
  explicit SyncErrorFactoryStub(int* error_counter)
      : error_counter_(error_counter) {}
  ~SyncErrorFactoryStub() override {}

  // Overridden from syncer::SyncErrorFactory:
  syncer::SyncError CreateAndUploadError(const base::Location& location,
                                         const std::string& message) override {
    (*error_counter_)++;
    return syncer::SyncError(location,
                             syncer::SyncError::DATATYPE_ERROR,
                             message,
                             syncer::DICTIONARY);
  }

 private:
  int* error_counter_;
  DISALLOW_COPY_AND_ASSIGN(SyncErrorFactoryStub);
};

// Counts the number of notifications for dictionary load and change.
class DictionaryObserverCounter : public SpellcheckCustomDictionary::Observer {
 public:
  DictionaryObserverCounter() : loads_(0), changes_(0) {}
  virtual ~DictionaryObserverCounter() {}

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
  DISALLOW_COPY_AND_ASSIGN(DictionaryObserverCounter);
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
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile2, base::BindRepeating(&BuildSpellcheckService)));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

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
  base::WriteFile(path, content.c_str(), content.length());
  EXPECT_TRUE(LoadDictionaryFile(path)->words.empty());
}

// Legacy dictionary with two words should be converted to new format dictionary
// with two words.
TEST_F(SpellcheckCustomDictionaryTest,
       LegacyDictionaryWithTwoWordsShouldBeConverted) {
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);

  std::string content = "foo\nbar\nfoo\n";
  base::WriteFile(path, content.c_str(), content.length());
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
  base::WriteFile(path, content.c_str(), content.length());
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
  base::WriteFile(path, content.c_str(), content.length());
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
  base::WriteFile(path, content.c_str(), content.length());
  EXPECT_EQ(expected, LoadDictionaryFile(path)->words);
}

TEST_F(SpellcheckCustomDictionaryTest,
       GetAllSyncDataAccuratelyReflectsDictionaryState) {
  SpellcheckCustomDictionary* dictionary =
      SpellcheckServiceFactory::GetForContext(
          &profile_)->GetCustomDictionary();

  syncer::SyncDataList data = dictionary->GetAllSyncData(syncer::DICTIONARY);
  EXPECT_TRUE(data.empty());

  EXPECT_TRUE(dictionary->AddWord("bar"));
  EXPECT_TRUE(dictionary->AddWord("foo"));

  data = dictionary->GetAllSyncData(syncer::DICTIONARY);
  EXPECT_EQ(2UL, data.size());
  std::vector<std::string> words;
  words.push_back("bar");
  words.push_back("foo");
  for (size_t i = 0; i < data.size(); i++) {
    EXPECT_TRUE(data[i].GetSpecifics().has_dictionary());
    EXPECT_EQ(syncer::DICTIONARY, data[i].GetDataType());
    EXPECT_EQ(words[i], syncer::SyncDataLocal(data[i]).GetTag());
    EXPECT_EQ(words[i], data[i].GetSpecifics().dictionary().word());
  }

  EXPECT_TRUE(dictionary->RemoveWord("bar"));
  EXPECT_TRUE(dictionary->RemoveWord("foo"));

  data = dictionary->GetAllSyncData(syncer::DICTIONARY);
  EXPECT_TRUE(data.empty());
}

TEST_F(SpellcheckCustomDictionaryTest, GetAllSyncDataHasLimit) {
  SpellcheckCustomDictionary* dictionary =
      SpellcheckServiceFactory::GetForContext(
          &profile_)->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords - 1; i++) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*dictionary, change);
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords - 1,
            dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords - 1,
            dictionary->GetAllSyncData(syncer::DICTIONARY).size());

  dictionary->AddWord("baz");
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            dictionary->GetAllSyncData(syncer::DICTIONARY).size());

  dictionary->AddWord("bar");
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            dictionary->GetAllSyncData(syncer::DICTIONARY).size());

  dictionary->AddWord("snafoo");
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 2,
            dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            dictionary->GetAllSyncData(syncer::DICTIONARY).size());
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

  EXPECT_FALSE(dictionary->ProcessSyncChanges(FROM_HERE, changes).IsSet());

  const std::set<std::string>& words = dictionary->GetWords();
  EXPECT_EQ(2UL, words.size());
  EXPECT_EQ(0UL, words.count("bar"));
  EXPECT_EQ(1UL, words.count("foo"));
  EXPECT_EQ(1UL, words.count("baz"));
}

TEST_F(SpellcheckCustomDictionaryTest, MergeDataAndStartSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile2, base::BindRepeating(&BuildSpellcheckService)));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

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

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)),
                       std::unique_ptr<syncer::SyncErrorFactory>(
                           new SyncErrorFactoryStub(&error_counter)))
                   .error()
                   .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  std::set<std::string> words = custom_dictionary->GetWords();
  std::set<std::string> words2 = custom_dictionary2->GetWords();
  EXPECT_EQ(words.size(), words2.size());
  EXPECT_EQ(words, words2);
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

  TestingProfile profile2;
  SpellcheckCustomDictionary* custom_dictionary2 =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile2, base::BindRepeating(&BuildSpellcheckService)))
          ->GetCustomDictionary();

  std::unique_ptr<SpellcheckCustomDictionary::Change> change(
      new SpellcheckCustomDictionary::Change);
  change->AddWord("foo");
  Apply(*custom_dictionary2, *change);

  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);
  UpdateDictionaryFile(std::move(change), path);
  EXPECT_TRUE(custom_dictionary->GetWords().empty());

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)),
                       std::unique_ptr<syncer::SyncErrorFactory>(
                           new SyncErrorFactoryStub(&error_counter)))
                   .error()
                   .IsSet());
  EXPECT_EQ(0, error_counter);
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

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigBeforeSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile2, base::BindRepeating(&BuildSpellcheckService)));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords + 1; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)),
                       std::unique_ptr<syncer::SyncErrorFactory>(
                           new SyncErrorFactoryStub(&error_counter)))
                   .error()
                   .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigAndServerFull) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile2, base::BindRepeating(&BuildSpellcheckService)));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

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

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)),
                       std::unique_ptr<syncer::SyncErrorFactory>(
                           new SyncErrorFactoryStub(&error_counter)))
                   .error()
                   .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords * 2 + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());
}

TEST_F(SpellcheckCustomDictionaryTest, ServerTooBig) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile2, base::BindRepeating(&BuildSpellcheckService)));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

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

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       GetAllSyncDataNoLimit(custom_dictionary2),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)),
                       std::unique_ptr<syncer::SyncErrorFactory>(
                           new SyncErrorFactoryStub(&error_counter)))
                   .error()
                   .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords * 2 + 2,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigToStartSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile2, base::BindRepeating(&BuildSpellcheckService)));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords - 1; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);

  custom_dictionary2->AddWord("bar");
  custom_dictionary2->AddWord("baz");

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)),
                       std::unique_ptr<syncer::SyncErrorFactory>(
                           new SyncErrorFactoryStub(&error_counter)))
                   .error()
                   .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());
}

TEST_F(SpellcheckCustomDictionaryTest, DictionaryTooBigToContiueSyncing) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile2, base::BindRepeating(&BuildSpellcheckService)));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords - 1; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)),
                       std::unique_ptr<syncer::SyncErrorFactory>(
                           new SyncErrorFactoryStub(&error_counter)))
                   .error()
                   .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  custom_dictionary->AddWord("bar");
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  custom_dictionary->AddWord("baz");
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());
}

TEST_F(SpellcheckCustomDictionaryTest, LoadAfterSyncStart) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile2, base::BindRepeating(&BuildSpellcheckService)));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  custom_dictionary->AddWord("foo");

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)),
                       std::unique_ptr<syncer::SyncErrorFactory>(
                           new SyncErrorFactoryStub(&error_counter)))
                   .error()
                   .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  std::unique_ptr<std::set<std::string>> custom_words(
      new std::set<std::string>);
  custom_words->insert("bar");
  OnLoaded(*custom_dictionary, std::move(custom_words));
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(2UL, custom_dictionary->GetWords().size());
  EXPECT_EQ(2UL, custom_dictionary2->GetWords().size());

  EXPECT_EQ(2UL, custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(2UL, custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());
}

TEST_F(SpellcheckCustomDictionaryTest, LoadAfterSyncStartTooBigToSync) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile2, base::BindRepeating(&BuildSpellcheckService)));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  custom_dictionary->AddWord("foo");

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)),
                       std::unique_ptr<syncer::SyncErrorFactory>(
                           new SyncErrorFactoryStub(&error_counter)))
                   .error()
                   .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  std::unique_ptr<std::set<std::string>> custom_words(
      new std::set<std::string>);
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords; ++i) {
    custom_words->insert(custom_words->end(), "foo" + base::NumberToString(i));
  }
  OnLoaded(*custom_dictionary, std::move(custom_words));
  EXPECT_EQ(0, error_counter);
  EXPECT_FALSE(custom_dictionary->IsSyncing());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords + 1,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());
}

TEST_F(SpellcheckCustomDictionaryTest, LoadDuplicatesAfterSync) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile2, base::BindRepeating(&BuildSpellcheckService)));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

  SpellcheckCustomDictionary::Change change;
  for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords / 2; ++i) {
    change.AddWord("foo" + base::NumberToString(i));
  }
  Apply(*custom_dictionary, change);

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)),
                       std::unique_ptr<syncer::SyncErrorFactory>(
                           new SyncErrorFactoryStub(&error_counter)))
                   .error()
                   .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  OnLoaded(*custom_dictionary,
           std::make_unique<std::set<std::string>>(change.to_add()));
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords / 2,
            custom_dictionary->GetWords().size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords / 2,
            custom_dictionary2->GetWords().size());

  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords / 2,
            custom_dictionary->GetAllSyncData(syncer::DICTIONARY).size());
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords / 2,
            custom_dictionary2->GetAllSyncData(syncer::DICTIONARY).size());
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

TEST_F(SpellcheckCustomDictionaryTest, DictionarySyncNotification) {
  SpellcheckService* spellcheck_service =
      SpellcheckServiceFactory::GetForContext(&profile_);
  SpellcheckCustomDictionary* custom_dictionary =
      spellcheck_service->GetCustomDictionary();
  TestingProfile profile2;
  SpellcheckService* spellcheck_service2 = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile2, base::BindRepeating(&BuildSpellcheckService)));
  SpellcheckCustomDictionary* custom_dictionary2 =
      spellcheck_service2->GetCustomDictionary();

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

  int error_counter = 0;
  EXPECT_FALSE(custom_dictionary
                   ->MergeDataAndStartSyncing(
                       syncer::DICTIONARY,
                       custom_dictionary2->GetAllSyncData(syncer::DICTIONARY),
                       std::unique_ptr<syncer::SyncChangeProcessor>(
                           new syncer::SyncChangeProcessorWrapperForTest(
                               custom_dictionary2)),
                       std::unique_ptr<syncer::SyncErrorFactory>(
                           new SyncErrorFactoryStub(&error_counter)))
                   .error()
                   .IsSet());
  EXPECT_EQ(0, error_counter);
  EXPECT_TRUE(custom_dictionary->IsSyncing());

  EXPECT_EQ(1, observer.changes());
  EXPECT_EQ(1, observer2.changes());

  custom_dictionary->RemoveObserver(&observer);
  custom_dictionary2->RemoveObserver(&observer2);
}

// The server has maximum number of words and the client has maximum number of
// different words before association time. No new words should be pushed to the
// sync server upon association. The client should accept words from the sync
// server, however.
TEST_F(SpellcheckCustomDictionaryTest, DictionarySyncLimit) {
  TestingProfile server_profile;
  SpellcheckService* server_spellcheck_service =
      static_cast<SpellcheckService*>(
          SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              &server_profile, base::BindRepeating(&BuildSpellcheckService)));

  // Here, |server_custom_dictionary| plays the role of the sync server.
  SpellcheckCustomDictionary* server_custom_dictionary =
      server_spellcheck_service->GetCustomDictionary();

  // Upload the maximum number of words to the sync server.
  {
    SpellcheckService* spellcheck_service =
        SpellcheckServiceFactory::GetForContext(&profile_);
    SpellcheckCustomDictionary* custom_dictionary =
        spellcheck_service->GetCustomDictionary();

    SpellcheckCustomDictionary::Change change;
    for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords; ++i) {
      change.AddWord("foo" + base::NumberToString(i));
    }
    Apply(*custom_dictionary, change);

    int error_counter = 0;
    EXPECT_FALSE(
        custom_dictionary
            ->MergeDataAndStartSyncing(
                syncer::DICTIONARY,
                server_custom_dictionary->GetAllSyncData(syncer::DICTIONARY),
                std::unique_ptr<syncer::SyncChangeProcessor>(
                    new syncer::SyncChangeProcessorWrapperForTest(
                        server_custom_dictionary)),
                std::unique_ptr<syncer::SyncErrorFactory>(
                    new SyncErrorFactoryStub(&error_counter)))
            .error()
            .IsSet());
    EXPECT_EQ(0, error_counter);
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
    TestingProfile client_profile;
    SpellcheckService* client_spellcheck_service =
        static_cast<SpellcheckService*>(
            SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
                &client_profile, base::BindRepeating(&BuildSpellcheckService)));

    // Here, |client_custom_dictionary| plays the role of the client.
    SpellcheckCustomDictionary* client_custom_dictionary =
        client_spellcheck_service->GetCustomDictionary();

    // Add the maximum number of words to the client. These words are all
    // different from those on the server.
    SpellcheckCustomDictionary::Change change;
    for (size_t i = 0; i < spellcheck::kMaxSyncableDictionaryWords; ++i) {
      change.AddWord("bar" + base::NumberToString(i));
    }
    Apply(*client_custom_dictionary, change);

    // Associate the server and the client.
    int error_counter = 0;
    EXPECT_FALSE(
        client_custom_dictionary
            ->MergeDataAndStartSyncing(
                syncer::DICTIONARY,
                server_custom_dictionary->GetAllSyncData(syncer::DICTIONARY),
                std::unique_ptr<syncer::SyncChangeProcessor>(
                    new syncer::SyncChangeProcessorWrapperForTest(
                        server_custom_dictionary)),
                std::unique_ptr<syncer::SyncErrorFactory>(
                    new SyncErrorFactoryStub(&error_counter)))
            .error()
            .IsSet());
    EXPECT_EQ(0, error_counter);
    EXPECT_FALSE(client_custom_dictionary->IsSyncing());
    EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords * 2,
              client_custom_dictionary->GetWords().size());
  }

  // The sync server should not receive more words, because it has the maximum
  // number of words already.
  EXPECT_EQ(spellcheck::kMaxSyncableDictionaryWords,
            server_custom_dictionary->GetWords().size());
}

#if defined(OS_WIN)
// Failing consistently on Win7+. See crbug.com/230534.
#define MAYBE_RecordSizeStatsCorrectly DISABLED_RecordSizeStatsCorrectly
#else
#define MAYBE_RecordSizeStatsCorrectly RecordSizeStatsCorrectly
#endif

TEST_F(SpellcheckCustomDictionaryTest, MAYBE_RecordSizeStatsCorrectly) {
  // Record a baseline.
  SpellCheckHostMetrics::RecordCustomWordCountStats(123);

  HistogramBase* histogram =
      StatisticsRecorder::FindHistogram("SpellCheck.CustomWords");
  ASSERT_TRUE(histogram != NULL);
  std::unique_ptr<HistogramSamples> baseline = histogram->SnapshotSamples();

  // Load the dictionary which should be empty.
  base::FilePath path =
      profile_.GetPath().Append(chrome::kCustomDictionaryFileName);
  EXPECT_TRUE(LoadDictionaryFile(path)->words.empty());

  // We expect there to be an entry with 0.
  histogram =
      StatisticsRecorder::FindHistogram("SpellCheck.CustomWords");
  ASSERT_TRUE(histogram != NULL);
  std::unique_ptr<HistogramSamples> samples = histogram->SnapshotSamples();

  samples->Subtract(*baseline);
  EXPECT_EQ(0,samples->sum());

  std::unique_ptr<SpellcheckCustomDictionary::Change> change(
      new SpellcheckCustomDictionary::Change);
  change->AddWord("bar");
  change->AddWord("foo");
  UpdateDictionaryFile(std::move(change), path);

  // Load the dictionary again and it should have 2 entries.
  EXPECT_EQ(2u, LoadDictionaryFile(path)->words.size());

  histogram =
      StatisticsRecorder::FindHistogram("SpellCheck.CustomWords");
  ASSERT_TRUE(histogram != NULL);
  std::unique_ptr<HistogramSamples> samples2 = histogram->SnapshotSamples();

  samples2->Subtract(*baseline);
  EXPECT_EQ(2,samples2->sum());
}

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
