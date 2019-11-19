// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extensions_activity_monitor.h"

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/common/chrome_paths.h"
#include "components/sync/base/extensions_activity.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

namespace browser_sync {

namespace {

namespace keys = extensions::manifest_keys;

// Create and return an extension with the given path.
scoped_refptr<Extension> MakeExtension(const std::string& name) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII(name);

  base::DictionaryValue value;
  value.SetString(keys::kVersion, "1.0.0.0");
  value.SetString(keys::kName, name);
  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      path, extensions::Manifest::INVALID_LOCATION, value,
      Extension::NO_FLAGS, &error));
  EXPECT_TRUE(error.empty());
  return extension;
}

// Fire a bookmarks API event from the given extension the given
// number of times.
template <class T>
void FireBookmarksApiEvent(
    const scoped_refptr<Extension>& extension, int repeats) {
  scoped_refptr<T> bookmarks_function(new T());
  bookmarks_function->set_name(T::function_name());
  for (int i = 0; i < repeats; i++) {
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_BOOKMARKS_API_INVOKED,
        content::Source<Extension>(extension.get()),
        content::Details<const extensions::BookmarksFunction>(
            bookmarks_function.get()));
  }
}

class SyncChromeExtensionsActivityMonitorTest : public testing::Test {
 public:
  SyncChromeExtensionsActivityMonitorTest()
      : extension1_(MakeExtension("extension1")),
        extension2_(MakeExtension("extension2")),
        id1_(extension1_->id()),
        id2_(extension2_->id()) {}
  ~SyncChromeExtensionsActivityMonitorTest() override {}

 private:
  content::BrowserTaskEnvironment task_environment_;

 protected:
  ExtensionsActivityMonitor monitor_;
  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  // IDs of |extension{1,2}_|.
  const std::string& id1_;
  const std::string& id2_;
};

// NOTE: The tests below are DISABLED because they're flaky:
// https://code.google.com/p/chromium/issues/detail?id=172002

// Fire some mutating bookmark API events with extension 1, then fire
// some mutating and non-mutating bookmark API events with extension
// 2.  Only the mutating events should be recorded by the
// syncer::ExtensionsActivityMonitor.
TEST_F(SyncChromeExtensionsActivityMonitorTest, DISABLED_Basic) {
  FireBookmarksApiEvent<extensions::BookmarksRemoveFunction>(extension1_, 1);
  FireBookmarksApiEvent<extensions::BookmarksMoveFunction>(extension1_, 1);
  FireBookmarksApiEvent<extensions::BookmarksUpdateFunction>(extension1_, 2);
  FireBookmarksApiEvent<extensions::BookmarksCreateFunction>(extension1_, 3);
  FireBookmarksApiEvent<extensions::BookmarksSearchFunction>(extension1_, 5);
  const uint32_t writes_by_extension1 = 1 + 1 + 2 + 3;

  FireBookmarksApiEvent<extensions::BookmarksRemoveTreeFunction>(
      extension2_, 8);
  FireBookmarksApiEvent<extensions::BookmarksGetSubTreeFunction>(
      extension2_, 13);
  FireBookmarksApiEvent<extensions::BookmarksGetChildrenFunction>(
      extension2_, 21);
  FireBookmarksApiEvent<extensions::BookmarksGetTreeFunction>(extension2_, 33);
  const uint32_t writes_by_extension2 = 8;

  syncer::ExtensionsActivity::Records results;
  monitor_.GetExtensionsActivity()->GetAndClearRecords(&results);

  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(results.find(id1_) != results.end());
  EXPECT_TRUE(results.find(id2_) != results.end());
  EXPECT_EQ(writes_by_extension1, results[id1_].bookmark_write_count);
  EXPECT_EQ(writes_by_extension2, results[id2_].bookmark_write_count);
}

// Fire some mutating bookmark API events with both extensions.  Then
// get the records, fire some more mutating and non-mutating events,
// and put the old records back.  Those should be merged with the new
// records correctly.
TEST_F(SyncChromeExtensionsActivityMonitorTest, DISABLED_Put) {
  FireBookmarksApiEvent<extensions::BookmarksCreateFunction>(extension1_, 5);
  FireBookmarksApiEvent<extensions::BookmarksMoveFunction>(extension2_, 8);

  syncer::ExtensionsActivity::Records results;
  monitor_.GetExtensionsActivity()->GetAndClearRecords(&results);

  EXPECT_EQ(2U, results.size());
  EXPECT_EQ(5U, results[id1_].bookmark_write_count);
  EXPECT_EQ(8U, results[id2_].bookmark_write_count);

  FireBookmarksApiEvent<extensions::BookmarksGetTreeFunction>(extension2_, 3);
  FireBookmarksApiEvent<extensions::BookmarksUpdateFunction>(extension2_, 2);

  // Simulate a commit failure, which augments the active record set with the
  // refugee records.
  monitor_.GetExtensionsActivity()->PutRecords(results);
  syncer::ExtensionsActivity::Records new_records;
  monitor_.GetExtensionsActivity()->GetAndClearRecords(&new_records);

  EXPECT_EQ(2U, results.size());
  EXPECT_EQ(id1_, new_records[id1_].extension_id);
  EXPECT_EQ(id2_, new_records[id2_].extension_id);
  EXPECT_EQ(5U, new_records[id1_].bookmark_write_count);
  EXPECT_EQ(8U + 2U, new_records[id2_].bookmark_write_count);
}

// Fire some mutating bookmark API events and get the records multiple
// times.  The mintor should correctly clear its records every time
// they're returned.
TEST_F(SyncChromeExtensionsActivityMonitorTest, DISABLED_MultiGet) {
  FireBookmarksApiEvent<extensions::BookmarksCreateFunction>(extension1_, 5);

  syncer::ExtensionsActivity::Records results;
  monitor_.GetExtensionsActivity()->GetAndClearRecords(&results);

  EXPECT_EQ(1U, results.size());
  EXPECT_EQ(5U, results[id1_].bookmark_write_count);

  monitor_.GetExtensionsActivity()->GetAndClearRecords(&results);
  EXPECT_TRUE(results.empty());

  FireBookmarksApiEvent<extensions::BookmarksCreateFunction>(extension1_, 3);
  monitor_.GetExtensionsActivity()->GetAndClearRecords(&results);

  EXPECT_EQ(1U, results.size());
  EXPECT_EQ(3U, results[id1_].bookmark_write_count);
}

}  // namespace

}  // namespace browser_sync
