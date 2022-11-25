// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/dictionary_helper.h"

#include <set>

#include "base/format_macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/sync/test/integration/dictionary_load_observer.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class DictionarySyncIntegrationTestHelper {
 public:
  DictionarySyncIntegrationTestHelper(
      const DictionarySyncIntegrationTestHelper&) = delete;
  DictionarySyncIntegrationTestHelper& operator=(
      const DictionarySyncIntegrationTestHelper&) = delete;

  // Same as SpellcheckCustomDictionary::AddWord/RemoveWord, except does not
  // write to disk.
  static bool ApplyChange(SpellcheckCustomDictionary* dictionary,
                          SpellcheckCustomDictionary::Change* change) {
    int result = change->Sanitize(dictionary->GetWords());
    dictionary->Apply(*change);
    dictionary->Notify(*change);
    dictionary->Sync(*change);
    return !result;
  }
};

namespace dictionary_helper {
namespace {

SpellcheckCustomDictionary* GetDictionary(int index) {
  return SpellcheckServiceFactory::GetForContext(
             sync_datatype_helper::test()->GetProfile(index))
      ->GetCustomDictionary();
}

void LoadDictionary(SpellcheckCustomDictionary* dictionary) {
  if (dictionary->IsLoaded()) {
    return;
  }
  base::RunLoop run_loop;
  DictionaryLoadObserver observer(
      content::GetDeferredQuitTaskForRunLoop(&run_loop));
  dictionary->AddObserver(&observer);
  dictionary->Load();
  run_loop.Run();
  dictionary->RemoveObserver(&observer);
  ASSERT_TRUE(dictionary->IsLoaded());
}

}  // namespace

const std::set<std::string>& GetDictionaryWords(int profile_index) {
  return GetDictionary(profile_index)->GetWords();
}

void LoadDictionaries() {
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    LoadDictionary(GetDictionary(i));
  }
}

size_t GetDictionarySize(int index) {
  return GetDictionary(index)->GetWords().size();
}

bool AddWord(int index, const std::string& word) {
  SpellcheckCustomDictionary::Change dictionary_change;
  dictionary_change.AddWord(word);
  bool result = DictionarySyncIntegrationTestHelper::ApplyChange(
      GetDictionary(index), &dictionary_change);
  return result;
}

bool AddWords(int index, int n, const std::string& prefix) {
  bool result = true;
  for (int i = 0; i < n; ++i) {
    result &= AddWord(index, prefix + base::NumberToString(i));
  }
  return result;
}

bool RemoveWord(int index, const std::string& word) {
  SpellcheckCustomDictionary::Change dictionary_change;
  dictionary_change.RemoveWord(word);
  bool result = DictionarySyncIntegrationTestHelper::ApplyChange(
      GetDictionary(index), &dictionary_change);
  return result;
}

DictionaryChecker::DictionaryChecker(
    const std::vector<std::string>& expected_words)
    : MultiClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncServices()),
      expected_words_(expected_words.begin(), expected_words.end()) {}

DictionaryChecker::~DictionaryChecker() = default;

bool DictionaryChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for matching dictionaries";
  for (int i = 0; i < sync_datatype_helper::test()->num_clients(); ++i) {
    if (GetDictionaryWords(/*profile_index=*/i) != expected_words_) {
      return false;
    }
  }
  return true;
}

NumDictionaryEntriesChecker::NumDictionaryEntriesChecker(int index,
                                                         size_t num_words)
    : SingleClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncService(index)),
      index_(index),
      num_words_(num_words) {}

bool NumDictionaryEntriesChecker::IsExitConditionSatisfied(std::ostream* os) {
  size_t actual_size = GetDictionarySize(index_);
  *os << "Waiting for client " << index_ << ": " << actual_size << " / "
      << num_words_ << " words downloaded";
  return actual_size == num_words_;
}

}  // namespace dictionary_helper
