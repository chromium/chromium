// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/importer_unittest_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/importer/edge_importer_utils_win.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/importer/importer_test_registry_overrider_win.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace {

struct FaviconGroup {
  const wchar_t* favicon_url;
  const wchar_t* site_url;
};

class TestObserver : public ProfileWriter,
                     public importer::ImporterProgressObserver {
 public:
  explicit TestObserver(
      const std::vector<BookmarkInfo>& expected_bookmark_entries,
      const std::vector<FaviconGroup>& expected_favicon_groups,
      base::OnceClosure quit_closure)
      : ProfileWriter(nullptr),
        bookmark_count_(0),
        expected_bookmark_entries_(expected_bookmark_entries),
        expected_favicon_groups_(expected_favicon_groups),
        favicon_count_(0),
        quit_closure_(std::move(quit_closure)) {}

  // importer::ImporterProgressObserver:
  void ImportStarted() override {}
  void ImportItemStarted(importer::ImportItem item) override {}
  void ImportItemEnded(importer::ImportItem item) override {}
  void ImportEnded() override {
    std::move(quit_closure_).Run();
    EXPECT_EQ(expected_bookmark_entries_.size(), bookmark_count_);
    EXPECT_EQ(expected_favicon_groups_.size(), favicon_count_);
  }

  // ProfileWriter:
  bool BookmarkModelIsLoaded() const override {
    // Profile is ready for writing.
    return true;
  }

  bool TemplateURLServiceIsLoaded() const override { return true; }

  void AddBookmarks(const std::vector<ImportedBookmarkEntry>& bookmarks,
                    const std::u16string& top_level_folder_name) override {
    ASSERT_EQ(expected_bookmark_entries_.size(), bookmarks.size());
    for (size_t i = 0; i < bookmarks.size(); ++i) {
      EXPECT_NO_FATAL_FAILURE(
          TestEqualBookmarkEntry(bookmarks[i], expected_bookmark_entries_[i]))
          << i;
      ++bookmark_count_;
    }
  }

  void AddFavicons(const favicon_base::FaviconUsageDataList& usage) override {
    // Importer should group the favicon information for each favicon URL.
    ASSERT_EQ(expected_favicon_groups_.size(), usage.size());
    for (size_t i = 0; i < expected_favicon_groups_.size(); ++i) {
      GURL favicon_url(
          base::WideToUTF16(expected_favicon_groups_[i].favicon_url));
      std::set<GURL> urls;
      urls.insert(
          GURL(base::WideToUTF16(expected_favicon_groups_[i].site_url)));

      bool expected_favicon_url_found = false;
      for (size_t j = 0; j < usage.size(); ++j) {
        if (usage[j].favicon_url == favicon_url) {
          EXPECT_EQ(urls, usage[j].urls);
          expected_favicon_url_found = true;
          break;
        }
      }
      EXPECT_TRUE(expected_favicon_url_found);
    }
    favicon_count_ += usage.size();
  }

 private:
  ~TestObserver() override {}

  // This is the count of bookmark entries observed during the test.
  size_t bookmark_count_;
  // This is the expected list of bookmark entries to observe during the test.
  std::vector<BookmarkInfo> expected_bookmark_entries_;
  // This is the expected list of favicon groups to observe during the test.
  std::vector<FaviconGroup> expected_favicon_groups_;
  // This is the count of favicon groups observed during the test.
  size_t favicon_count_;
  // the closure to quit the RunLoop
  base::OnceClosure quit_closure_;
};

bool DecompressDatabase(const base::FilePath& data_path) {
  base::FilePath output_file = data_path.Append(
      L"DataStore\\Data\\nouser1\\120712-0049\\DBStore\\spartan.edb");
  base::FilePath gzip_file = output_file.AddExtension(L".gz");
  std::string gzip_data;
  if (!base::ReadFileToString(gzip_file, &gzip_data))
    return false;
  if (!compression::GzipUncompress(gzip_data, &gzip_data))
    return false;
  return base::WriteFile(output_file, gzip_data);
}

const char kDummyFaviconImageData[] =
    "\x42\x4D"           // Magic signature 'BM'
    "\x1E\x00\x00\x00"   // File size
    "\x00\x00\x00\x00"   // Reserved
    "\x1A\x00\x00\x00"   // Offset of the pixel data
    "\x0C\x00\x00\x00"   // Header Size
    "\x01\x00\x01\x00"   // Size: 1x1
    "\x01\x00"           // Reserved
    "\x18\x00"           // 24-bits
    "\x00\xFF\x00\x00";  // The pixel

}  // namespace

// These tests need to be browser tests in order to be able to run the OOP
// import (via ExternalProcessImporterHost) which launches a utility process.
class EdgeImporterBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // This will launch the browser test and thus needs to happen last.
    InProcessBrowserTest::SetUp();
  }

  base::ScopedTempDir temp_dir_;

  // Overrides the default registry key for Edge tests.
  ImporterTestRegistryOverrider test_registry_overrider_;
};

IN_PROC_BROWSER_TEST_F(EdgeImporterBrowserTest, EdgeImporter) {
  const BookmarkInfo kEdgeBookmarks[] = {
      {true,
       2,
       {"Links", "SubFolderOfLinks"},
       L"SubLink",
       "http://www.links-sublink.com/"},
      {true, 1, {"Links"}, L"TheLink", "http://www.links-thelink.com/"},
      {false, 0, {}, L"Google Home Page", "http://www.google.com/"},
      {false, 0, {}, L"TheLink", "http://www.links-thelink.com/"},
      {false, 1, {"SubFolder"}, L"Title", "http://www.link.com/"},
      {false, 0, {}, L"WithPortAndQuery", "http://host:8080/cgi?q=query"},
      {false, 1, {"a"}, L"\x4E2D\x6587", "http://chinese-title-favorite/"},
      {false, 0, {}, L"SubFolder", "http://www.subfolder.com/"},
      {false, 0, {}, L"InvalidFavicon", "http://www.invalid-favicon.com/"},
  };
  std::vector<BookmarkInfo> bookmark_entries(
      kEdgeBookmarks, kEdgeBookmarks + std::size(kEdgeBookmarks));

  const FaviconGroup kEdgeFaviconGroup[] = {
      {L"http://www.links-sublink.com/favicon.ico",
       L"http://www.links-sublink.com"},
      {L"http://www.links-thelink.com/favicon.ico",
       L"http://www.links-thelink.com"},
      {L"http://www.google.com/favicon.ico", L"http://www.google.com"},
      {L"http://www.links-thelink.com/favicon.ico",
       L"http://www.links-thelink.com"},
      {L"http://www.link.com/favicon.ico", L"http://www.link.com"},
      {L"http://host:8080/favicon.ico", L"http://host:8080/cgi?q=query"},
      {L"http://chinese-title-favorite/favicon.ico",
       L"http://chinese-title-favorite"},
      {L"http://www.subfolder.com/favicon.ico", L"http://www.subfolder.com"},
  };

  std::vector<FaviconGroup> favicon_groups(
      kEdgeFaviconGroup, kEdgeFaviconGroup + std::size(kEdgeFaviconGroup));

  base::FilePath data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  data_path = data_path.AppendASCII("edge_profile");

  base::FilePath temp_path = temp_dir_.GetPath();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CopyDirectory(data_path, temp_path, true));
    ASSERT_TRUE(DecompressDatabase(temp_path.AppendASCII("edge_profile")));
  }

  std::wstring key_path(importer::GetEdgeSettingsKey());
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_CURRENT_USER, key_path.c_str(), KEY_WRITE));
  key.WriteValue(L"FavoritesESEEnabled", 1);
  ASSERT_FALSE(importer::IsEdgeFavoritesLegacyMode());

  // Starts to import the above settings.
  // Deletes itself.
  base::RunLoop loop;
  ExternalProcessImporterHost* host = new ExternalProcessImporterHost;
  scoped_refptr<TestObserver> observer(new TestObserver(
      bookmark_entries, favicon_groups, loop.QuitWhenIdleClosure()));
  host->set_observer(observer.get());

  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_EDGE;
  source_profile.source_path = temp_path.AppendASCII("edge_profile");

  host->StartImportSettings(source_profile, browser()->profile(),
                            importer::FAVORITES, observer.get());
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(EdgeImporterBrowserTest, EdgeImporterLegacyFallback) {
  // We only do legacy fallback on versions < Version::WIN10_TH2.
  if (base::win::GetVersion() >= base::win::Version::WIN10_TH2)
    return;

  const BookmarkInfo kEdgeBookmarks[] = {
      {false, 0, {}, L"Google", "http://www.google.com/"}};
  std::vector<BookmarkInfo> bookmark_entries(
      kEdgeBookmarks, kEdgeBookmarks + std::size(kEdgeBookmarks));
  const FaviconGroup kEdgeFaviconGroup[] = {
      {L"http://www.google.com/favicon.ico", L"http://www.google.com/"}};
  std::vector<FaviconGroup> favicon_groups(
      kEdgeFaviconGroup, kEdgeFaviconGroup + std::size(kEdgeFaviconGroup));

  base::FilePath data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  data_path = data_path.AppendASCII("edge_profile");

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CopyDirectory(data_path, temp_dir_.GetPath(), true));
    ASSERT_TRUE(importer::IsEdgeFavoritesLegacyMode());
  }

  // Starts to import the above settings.
  // Deletes itself.
  base::RunLoop loop;
  ExternalProcessImporterHost* host = new ExternalProcessImporterHost;
  scoped_refptr<TestObserver> observer(new TestObserver(
      bookmark_entries, favicon_groups, loop.QuitWhenIdleClosure()));
  host->set_observer(observer.get());

  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_EDGE;
  base::FilePath source_path = temp_dir_.GetPath().AppendASCII("edge_profile");
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(
        source_path.AppendASCII("Favorites\\Google.url:favicon:$DATA"),
        kDummyFaviconImageData));
  }
  source_profile.source_path = source_path;

  host->StartImportSettings(source_profile, browser()->profile(),
                            importer::FAVORITES, observer.get());
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(EdgeImporterBrowserTest, EdgeImporterNoDatabase) {
  std::vector<BookmarkInfo> bookmark_entries;
  std::vector<FaviconGroup> favicon_groups;

  std::wstring key_path(importer::GetEdgeSettingsKey());
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_CURRENT_USER, key_path.c_str(), KEY_WRITE));
  key.WriteValue(L"FavoritesESEEnabled", 1);
  ASSERT_FALSE(importer::IsEdgeFavoritesLegacyMode());

  // Starts to import the above settings.
  // Deletes itself.
  base::RunLoop loop;
  ExternalProcessImporterHost* host = new ExternalProcessImporterHost;
  scoped_refptr<TestObserver> observer(new TestObserver(
      bookmark_entries, favicon_groups, loop.QuitWhenIdleClosure()));
  host->set_observer(observer.get());

  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_EDGE;
  source_profile.source_path = temp_dir_.GetPath();

  host->StartImportSettings(source_profile, browser()->profile(),
                            importer::FAVORITES, observer.get());
  loop.Run();
}
