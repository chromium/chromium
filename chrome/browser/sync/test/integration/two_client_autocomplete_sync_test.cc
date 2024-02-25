// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::AutocompleteKey;
using autofill_helper::AddKeys;
using autofill_helper::GetAllKeys;
using autofill_helper::RemoveKey;

class TwoClientAutocompleteSyncTest : public SyncTest {
 public:
  TwoClientAutocompleteSyncTest() : SyncTest(TWO_CLIENT) {}

  TwoClientAutocompleteSyncTest(const TwoClientAutocompleteSyncTest&) = delete;
  TwoClientAutocompleteSyncTest& operator=(
      const TwoClientAutocompleteSyncTest&) = delete;

  ~TwoClientAutocompleteSyncTest() override = default;

  bool TestUsesSelfNotifications() override { return false; }
};

IN_PROC_BROWSER_TEST_F(TwoClientAutocompleteSyncTest, WebDataServiceSanity) {
  ASSERT_TRUE(SetupSync());

  // Client0 adds a key.
  AddKeys(0, {AutocompleteKey("name0", "value0")});
  EXPECT_TRUE(AutocompleteKeysChecker(0, 1).Wait());
  EXPECT_EQ(1U, GetAllKeys(0).size());

  // Client1 adds a key.
  AddKeys(1, {AutocompleteKey("name1", "value1-0")});
  EXPECT_TRUE(AutocompleteKeysChecker(0, 1).Wait());
  EXPECT_EQ(2U, GetAllKeys(0).size());

  // Client0 adds a key with the same name.
  AddKeys(0, {AutocompleteKey("name1", "value1-1")});
  EXPECT_TRUE(AutocompleteKeysChecker(0, 1).Wait());
  EXPECT_EQ(3U, GetAllKeys(0).size());

  // Client1 removes a key.
  RemoveKey(1, AutocompleteKey("name1", "value1-0"));
  EXPECT_TRUE(AutocompleteKeysChecker(0, 1).Wait());
  EXPECT_EQ(2U, GetAllKeys(0).size());

  // Client0 removes the rest.
  RemoveKey(0, AutocompleteKey("name0", "value0"));
  RemoveKey(0, AutocompleteKey("name1", "value1-1"));
  EXPECT_TRUE(AutocompleteKeysChecker(0, 1).Wait());
  EXPECT_EQ(0U, GetAllKeys(0).size());
}

IN_PROC_BROWSER_TEST_F(TwoClientAutocompleteSyncTest, AddUnicodeProfile) {
  ASSERT_TRUE(SetupClients());

  std::set<AutocompleteKey> keys;
  keys.insert(AutocompleteKey(u"Sigur R\u00F3s", u"\u00C1g\u00E6tis byrjun"));
  AddKeys(0, keys);
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutocompleteKeysChecker(0, 1).Wait());
}

IN_PROC_BROWSER_TEST_F(TwoClientAutocompleteSyncTest,
                       AddDuplicateNamesToSameProfile) {
  ASSERT_TRUE(SetupClients());

  std::set<AutocompleteKey> keys;
  keys.insert(AutocompleteKey("name0", "value0-0"));
  keys.insert(AutocompleteKey("name0", "value0-1"));
  keys.insert(AutocompleteKey("name1", "value1"));
  AddKeys(0, keys);
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutocompleteKeysChecker(0, 1).Wait());
  EXPECT_EQ(2U, GetAllKeys(0).size());
}

IN_PROC_BROWSER_TEST_F(TwoClientAutocompleteSyncTest,
                       AddDuplicateNamesToDifferentProfiles) {
  ASSERT_TRUE(SetupClients());

  std::set<AutocompleteKey> keys0;
  keys0.insert(AutocompleteKey("name0", "value0-0"));
  keys0.insert(AutocompleteKey("name1", "value1"));
  AddKeys(0, keys0);

  std::set<AutocompleteKey> keys1;
  keys1.insert(AutocompleteKey("name0", "value0-1"));
  keys1.insert(AutocompleteKey("name2", "value2"));
  keys1.insert(AutocompleteKey("name3", "value3"));
  AddKeys(1, keys1);

  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(AutocompleteKeysChecker(0, 1).Wait());
  EXPECT_EQ(5U, GetAllKeys(0).size());
}

}  // namespace
