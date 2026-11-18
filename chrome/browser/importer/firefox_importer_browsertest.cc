// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

namespace {

class TestFirefoxImportObserver : public ProfileWriter,
                                  public importer::ImporterProgressObserver {
 public:
  explicit TestFirefoxImportObserver(base::OnceClosure quit_closure)
      : ProfileWriter(nullptr), quit_closure_(std::move(quit_closure)) {}

  // importer::ImporterProgressObserver:
  void ImportStarted() override {}
  void ImportItemStarted(user_data_importer::ImportItem item) override {}
  void ImportItemEnded(user_data_importer::ImportItem item) override {}
  void ImportEnded() override { std::move(quit_closure_).Run(); }

  // ProfileWriter:
  bool BookmarkModelIsLoaded() const override { return true; }
  bool TemplateURLServiceIsLoaded() const override { return true; }

  void AddBookmarks(
      const std::vector<user_data_importer::ImportedBookmarkEntry>& bookmarks,
      const std::u16string& top_level_folder_name) override {
    bookmarks_ = bookmarks;
  }

  void AddFavicons(const favicon_base::FaviconUsageDataList& usage) override {
    favicons_ = usage;
  }

  const std::vector<user_data_importer::ImportedBookmarkEntry>& bookmarks()
      const {
    return bookmarks_;
  }
  const favicon_base::FaviconUsageDataList& favicons() const {
    return favicons_;
  }

 private:
  ~TestFirefoxImportObserver() override = default;

  std::vector<user_data_importer::ImportedBookmarkEntry> bookmarks_;
  favicon_base::FaviconUsageDataList favicons_;
  base::OnceClosure quit_closure_;
};

using FirefoxImporterBrowserTest = InProcessBrowserTest;

// End-to-end out-of-process Firefox import test exercising
// ExternalProcessImporterHost for bookmarks and favicons.
IN_PROC_BROWSER_TEST_F(FirefoxImporterBrowserTest,
                       ImportBookmarksAndFaviconsOutOfProcess) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
  test_data_path =
      test_data_path.AppendASCII("import").AppendASCII("firefox").AppendASCII(
          "57.0.1");
  ASSERT_TRUE(base::DirectoryExists(test_data_path));

  // Copy into a temp directory because ImporterHost acquires a
  // FirefoxProfileLock, which creates `.parentlock` inside `source_path`.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath profile_path = temp_dir.GetPath().AppendASCII("profile");
  ASSERT_TRUE(
      base::CopyDirectory(test_data_path, profile_path, /*recursive=*/true));

  base::test::TestFuture<void> import_finished;
  auto observer = base::MakeRefCounted<TestFirefoxImportObserver>(
      import_finished.GetCallback());

  // Deletes itself upon completion.
  auto* host = new ExternalProcessImporterHost;
  host->set_observer(observer.get());

  user_data_importer::SourceProfile source_profile;
  source_profile.importer_type = user_data_importer::TYPE_FIREFOX;
  source_profile.source_path = profile_path;

  host->StartImportSettings(source_profile, browser()->GetProfile(),
                            user_data_importer::FAVORITES, observer.get());
  ASSERT_TRUE(import_finished.Wait());

  EXPECT_EQ(6u, observer->bookmarks().size());
  EXPECT_EQ(4u, observer->favicons().size());
  for (const auto& favicon : observer->favicons()) {
    SCOPED_TRACE(favicon.favicon_url);
    SkBitmap decoded = gfx::PNGCodec::Decode(favicon.png_data);
    EXPECT_FALSE(decoded.isNull());
    if (!decoded.isNull()) {
      EXPECT_EQ(gfx::kFaviconSize, decoded.width());
      EXPECT_EQ(gfx::kFaviconSize, decoded.height());
    }
  }
}

}  // namespace
