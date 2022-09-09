// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_DICTIONARY_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_DICTIONARY_HELPER_H_

#include <stddef.h>

#include <set>
#include <string>

#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

namespace dictionary_helper {

// Returns set of words stored in dictionary for given |profile_index|.
const std::set<std::string>& GetDictionaryWords(int profile_index);

// Synchronously loads the dictionaries across all profiles. Returns only after
// the dictionaries have finished to load.
void LoadDictionaries();

// Used to check the size of the dictionary within a particular sync profile.
size_t GetDictionarySize(int index);

// Adds |word| to the dictionary for profile with index |index|. Returns true
// if |word| is valid and not a duplicate. Otherwise returns false.
bool AddWord(int index, const std::string& word);

// Add |n| words with the given |prefix| to the specified client |index|. Return
// value is true iff all words are not duplicates and valid.
bool AddWords(int index, int n, const std::string& prefix);

// Removes |word| from the dictionary for profile with index |index|.
// Returns true if |word| was found. Otherwise returns false.
bool RemoveWord(int index, const std::string& word);

// Checker to block until all clients have expected dictionaries.
class DictionaryChecker : public MultiClientStatusChangeChecker {
 public:
  explicit DictionaryChecker(const std::vector<std::string>& expected_words);
  ~DictionaryChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  std::set<std::string> expected_words_;
};

// Checker to block until the number of dictionary entries to equal to an
// expected count.
class NumDictionaryEntriesChecker : public SingleClientStatusChangeChecker {
 public:
  NumDictionaryEntriesChecker(int index, size_t num_words);
  ~NumDictionaryEntriesChecker() override = default;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  int index_;
  size_t num_words_;
};

}  // namespace dictionary_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_DICTIONARY_HELPER_H_
