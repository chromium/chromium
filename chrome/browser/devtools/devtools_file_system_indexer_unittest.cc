// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/devtools/devtools_file_system_indexer.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class DevToolsFileSystemIndexerTest : public testing::Test {
 public:
  void SetDone(base::OnceClosure quit_closure) {
    indexing_done_ = true;
    std::move(quit_closure).Run();
  }

  void SearchCallback(base::OnceClosure quit_closure,
                      const std::vector<std::string>& results) {
    search_results_.clear();
    for (const std::string& result : results) {
      search_results_.insert(
          base::FilePath::FromUTF8Unsafe(result).BaseName().AsUTF8Unsafe());
    }
    std::move(quit_closure).Run();
  }

 protected:
  void SetUp() override {
    indexer_ = new DevToolsFileSystemIndexer();
    indexing_done_ = false;
  }

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<DevToolsFileSystemIndexer> indexer_;
  std::set<std::string> search_results_;
  bool indexing_done_;
};

TEST_F(DevToolsFileSystemIndexerTest, BasicUsage) {
  base::FilePath base_test_path;
  base::RunLoop loop;
  base::PathService::Get(chrome::DIR_TEST_DATA, &base_test_path);
  base::FilePath index_path =
      base_test_path.Append(FILE_PATH_LITERAL("devtools"))
          .Append(FILE_PATH_LITERAL("indexer"));

  std::vector<std::string> excluded_folders;
  scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob> job =
      indexer_->IndexPath(
          index_path.AsUTF8Unsafe(), excluded_folders, base::DoNothing(),
          base::DoNothing(),
          base::BindOnce(&DevToolsFileSystemIndexerTest::SetDone,
                         base::Unretained(this), loop.QuitWhenIdleClosure()));

  loop.Run();
  ASSERT_TRUE(indexing_done_);

  base::RunLoop loop1;
  indexer_->SearchInPath(
      index_path.AsUTF8Unsafe(), "Hello",
      base::BindOnce(&DevToolsFileSystemIndexerTest::SearchCallback,
                     base::Unretained(this), loop1.QuitWhenIdleClosure()));
  loop1.Run();

  ASSERT_EQ(3lu, search_results_.size());
  ASSERT_EQ(1lu, search_results_.count("hello_world.c"));
  ASSERT_EQ(1lu, search_results_.count("hello_world.html"));
  ASSERT_EQ(1lu, search_results_.count("hello_world.js"));

  base::RunLoop loop2;
  indexer_->SearchInPath(
      index_path.AsUTF8Unsafe(), "FUNCTION",
      base::BindOnce(&DevToolsFileSystemIndexerTest::SearchCallback,
                     base::Unretained(this), loop2.QuitWhenIdleClosure()));
  loop2.Run();

  ASSERT_EQ(1lu, search_results_.size());
  ASSERT_EQ(1lu, search_results_.count("hello_world.js"));
}
