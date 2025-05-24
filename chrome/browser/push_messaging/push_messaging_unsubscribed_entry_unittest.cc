// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_unsubscribed_entry.h"

#include <stdint.h>

#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsEmpty;
using testing::UnorderedPointwise;

MATCHER(EntryEquals, "") {
  auto& lhs = testing::get<0>(arg);
  auto& rhs = testing::get<1>(arg);
  return lhs.origin() == rhs.origin() &&
         lhs.service_worker_registration_id() ==
             rhs.service_worker_registration_id();
}

class PushMessagingUnsubscribedEntryTest : public testing::Test {
 protected:
  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(PushMessagingUnsubscribedEntryTest, PersistAndGet) {
  ASSERT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()), IsEmpty());

  PushMessagingUnsubscribedEntry entry_1 =
      PushMessagingUnsubscribedEntry(GURL("https://example.test"), 1);
  entry_1.PersistToPrefs(profile());
  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()),
              UnorderedPointwise(EntryEquals(), {entry_1}));

  const base::Value::List& list = profile()->GetPrefs()->GetList(
      prefs::kPushMessagingUnsubscribedEntriesList);
  ASSERT_EQ(list.size(), 1u);
  ASSERT_TRUE(list[0].is_string());
  EXPECT_EQ(list[0].GetString(), "https://example.test/#1");
}

TEST_F(PushMessagingUnsubscribedEntryTest, PersistSameEntryTwice) {
  ASSERT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()), IsEmpty());

  PushMessagingUnsubscribedEntry entry_1 =
      PushMessagingUnsubscribedEntry(GURL("https://example.test"), 1);
  entry_1.PersistToPrefs(profile());
  PushMessagingUnsubscribedEntry entry_2 = entry_1;
  entry_2.PersistToPrefs(profile());
  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()),
              UnorderedPointwise(EntryEquals(), {entry_1}));
}

TEST_F(PushMessagingUnsubscribedEntryTest,
       DeserializeEntryWithAdditionalParts) {
  ASSERT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()), IsEmpty());

  {
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kPushMessagingUnsubscribedEntriesList);
    update.Get().Append("https://example.test#1#abcdef");
  }

  PushMessagingUnsubscribedEntry entry_1 =
      PushMessagingUnsubscribedEntry(GURL("https://example.test"), 1);
  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()),
              UnorderedPointwise(EntryEquals(), {entry_1}));
}

TEST_F(PushMessagingUnsubscribedEntryTest, DeserializeInvalidEntryOnlyOnePart) {
  ASSERT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()), IsEmpty());

  PushMessagingUnsubscribedEntry entry_1 =
      PushMessagingUnsubscribedEntry(GURL("https://example.test"), 1);
  entry_1.PersistToPrefs(profile());

  {
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kPushMessagingUnsubscribedEntriesList);
    update.Get().Append("https://example2.test");
  }

  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()),
              UnorderedPointwise(EntryEquals(), {entry_1}));
}

TEST_F(PushMessagingUnsubscribedEntryTest,
       DeserializeInvalidEntryInvalidOrigin) {
  ASSERT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()), IsEmpty());

  PushMessagingUnsubscribedEntry entry_1 =
      PushMessagingUnsubscribedEntry(GURL("https://example.test"), 1);
  entry_1.PersistToPrefs(profile());

  {
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kPushMessagingUnsubscribedEntriesList);
    update.Get().Append("example2.test#1");
  }

  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()),
              UnorderedPointwise(EntryEquals(), {entry_1}));
}

TEST_F(PushMessagingUnsubscribedEntryTest,
       DeserializeInvalidEntryInvalidServiceWorkerRegistration) {
  ASSERT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()), IsEmpty());

  PushMessagingUnsubscribedEntry entry_1 =
      PushMessagingUnsubscribedEntry(GURL("https://example.test"), 1);
  entry_1.PersistToPrefs(profile());

  {
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                prefs::kPushMessagingUnsubscribedEntriesList);
    update.Get().Append("https://example2.test#abc");
  }

  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()),
              UnorderedPointwise(EntryEquals(), {entry_1}));
}

TEST_F(PushMessagingUnsubscribedEntryTest, Delete) {
  ASSERT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()), IsEmpty());

  GURL origin_1("https://example.test");
  GURL origin_2("https://example2.test");
  PushMessagingUnsubscribedEntry entry_1 =
      PushMessagingUnsubscribedEntry(origin_1, 1);
  PushMessagingUnsubscribedEntry entry_2 =
      PushMessagingUnsubscribedEntry(origin_2, 2);
  entry_1.PersistToPrefs(profile());
  entry_2.PersistToPrefs(profile());
  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()),
              UnorderedPointwise(EntryEquals(), {entry_1, entry_2}));

  PushMessagingUnsubscribedEntry(origin_1, 1).DeleteFromPrefs(profile());
  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()),
              UnorderedPointwise(EntryEquals(), {entry_2}));

  // Deleting a non-existing entry is a no-opt.
  PushMessagingUnsubscribedEntry(origin_1, 1).DeleteFromPrefs(profile());
  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()),
              UnorderedPointwise(EntryEquals(), {entry_2}));

  // Deleting a non-existing entry is a no-opt.
  PushMessagingUnsubscribedEntry(origin_2, 2).DeleteFromPrefs(profile());
  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()), IsEmpty());
}

TEST_F(PushMessagingUnsubscribedEntryTest, DeleteAllFromPrefs) {
  ASSERT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()), IsEmpty());

  PushMessagingUnsubscribedEntry entry_1 =
      PushMessagingUnsubscribedEntry(GURL("https://example.test"), 1);
  entry_1.PersistToPrefs(profile());
  PushMessagingUnsubscribedEntry entry_2 =
      PushMessagingUnsubscribedEntry(GURL("https://example2.test"), 2);
  entry_2.PersistToPrefs(profile());
  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()),
              UnorderedPointwise(EntryEquals(), {entry_1, entry_2}));

  PushMessagingUnsubscribedEntry::DeleteAllFromPrefs(profile());
  EXPECT_THAT(PushMessagingUnsubscribedEntry::GetAll(profile()), IsEmpty());
}
