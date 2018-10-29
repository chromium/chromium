// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_helper_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"

using content::BrowserContext;
using content::BrowserThread;

namespace {
typedef BrowsingDataHelperCallback<BrowsingDataDatabaseHelper::DatabaseInfo>
    TestCompletionCallback;

const char kTestIdentifier1[] = "http_www.google.com_0";

const char kTestIdentifierExtension[] =
  "chrome-extension_behllobkkfkfnphdnhnkndlbkcpglgmj_0";

class BrowsingDataDatabaseHelperTest : public InProcessBrowserTest {
 public:
  virtual void CreateDatabases() {
    storage::DatabaseTracker* db_tracker =
        BrowserContext::GetDefaultStoragePartition(browser()->profile())
            ->GetDatabaseTracker();
    base::RunLoop run_loop;
    db_tracker->task_runner()->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          base::string16 db_name = base::ASCIIToUTF16("db");
          base::string16 description = base::ASCIIToUTF16("db_description");
          int64_t size;
          db_tracker->DatabaseOpened(kTestIdentifier1, db_name, description, 1,
                                     &size);
          db_tracker->DatabaseClosed(kTestIdentifier1, db_name);
          base::FilePath db_path1 =
              db_tracker->GetFullDBFilePath(kTestIdentifier1, db_name);
          base::CreateDirectory(db_path1.DirName());
          ASSERT_EQ(0, base::WriteFile(db_path1, nullptr, 0));
          db_tracker->DatabaseOpened(kTestIdentifierExtension, db_name,
                                     description, 1, &size);
          db_tracker->DatabaseClosed(kTestIdentifierExtension, db_name);
          base::FilePath db_path2 =
              db_tracker->GetFullDBFilePath(kTestIdentifierExtension, db_name);
          base::CreateDirectory(db_path2.DirName());
          ASSERT_EQ(0, base::WriteFile(db_path2, nullptr, 0));
          std::vector<storage::OriginInfo> origins;
          db_tracker->GetAllOriginsInfo(&origins);
          ASSERT_EQ(2U, origins.size());
        }),
        run_loop.QuitClosure());
    run_loop.Run();
  }
};

IN_PROC_BROWSER_TEST_F(BrowsingDataDatabaseHelperTest, FetchData) {
  CreateDatabases();
  scoped_refptr<BrowsingDataDatabaseHelper> database_helper(
      new BrowsingDataDatabaseHelper(browser()->profile()));
  std::list<BrowsingDataDatabaseHelper::DatabaseInfo> database_info_list;
  base::RunLoop run_loop;
  database_helper->StartFetching(base::BindLambdaForTesting(
      [&](const std::list<BrowsingDataDatabaseHelper::DatabaseInfo>& list) {
        database_info_list = list;
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_EQ(1UL, database_info_list.size());
  EXPECT_EQ(std::string(kTestIdentifier1),
            database_info_list.begin()->identifier.ToString());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataDatabaseHelperTest, CannedAddDatabase) {
  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");
  const char origin_str1[] = "http_host1_1";
  const char origin_str2[] = "http_host2_1";
  const char db1[] = "db1";
  const char db2[] = "db2";
  const char db3[] = "db3";

  scoped_refptr<CannedBrowsingDataDatabaseHelper> helper(
      new CannedBrowsingDataDatabaseHelper(browser()->profile()));
  helper->AddDatabase(origin1, db1, std::string());
  helper->AddDatabase(origin1, db2, std::string());
  helper->AddDatabase(origin2, db3, std::string());

  TestCompletionCallback callback;
  helper->StartFetching(
      base::Bind(&TestCompletionCallback::callback,
                 base::Unretained(&callback)));

  std::list<BrowsingDataDatabaseHelper::DatabaseInfo> result =
      callback.result();

  ASSERT_EQ(3u, result.size());
  auto info = result.begin();
  EXPECT_EQ(origin_str1, info->identifier.ToString());
  EXPECT_STREQ(db1, info->database_name.c_str());
  info++;
  EXPECT_EQ(origin_str1, info->identifier.ToString());
  EXPECT_STREQ(db2, info->database_name.c_str());
  info++;
  EXPECT_EQ(origin_str2, info->identifier.ToString());
  EXPECT_STREQ(db3, info->database_name.c_str());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataDatabaseHelperTest, CannedUnique) {
  const GURL origin("http://host1:1/");
  const char origin_str[] = "http_host1_1";
  const char db[] = "db1";

  scoped_refptr<CannedBrowsingDataDatabaseHelper> helper(
      new CannedBrowsingDataDatabaseHelper(browser()->profile()));
  helper->AddDatabase(origin, db, std::string());
  helper->AddDatabase(origin, db, std::string());

  TestCompletionCallback callback;
  helper->StartFetching(
      base::Bind(&TestCompletionCallback::callback,
                 base::Unretained(&callback)));

  std::list<BrowsingDataDatabaseHelper::DatabaseInfo> result =
      callback.result();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(origin_str, result.begin()->identifier.ToString());
  EXPECT_STREQ(db, result.begin()->database_name.c_str());
}
}  // namespace
