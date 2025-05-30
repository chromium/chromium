// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/bookmarks/bookmark_html_writer.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/utility/importer/bookmark_html_reader.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/history/core/browser/history_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_data_importer/common/imported_bookmark_entry.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "content/public/test/browser_task_environment.h"
#include "skia/rusty_png_feature.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

constexpr std::u16string_view kBookmarkBarTitle = u"Bookmarks bar";

const int kIconWidth = 16;
const int kIconHeight = 16;

SkBitmap MakeTestSkBitmap(int w, int h) {
  SkBitmap bmp;
  bmp.allocN32Pixels(w, h);

  uint32_t* src_data = bmp.getAddr32(0, 0);
  for (int i = 0; i < w * h; i++) {
    src_data[i] = SkPreMultiplyARGB(i % 255, i % 250, i % 245, i % 240);
  }
  return bmp;
}

}  // namespace

class BookmarkHTMLWriterTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.GetPath().AppendASCII("bookmarks.html");

    ASSERT_TRUE(
        base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path_));
    test_data_path_ = test_data_path_.AppendASCII("bookmark_html_writer");

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        FaviconServiceFactory::GetInstance(),
        FaviconServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());

    profile_ = profile_builder.Build();
    profile_->BlockUntilHistoryProcessesPendingRequests();

    bookmarks::test::WaitForBookmarkModelToLoad(model());

    // In order to use the same golden files for all platforms, ensure that the
    // bookmark bar folder title is set to a constant value.
#if BUILDFLAG(IS_MAC)
    // Mac uses titlecase strings, so would have "Bookmarks Bar" by default.
    CHECK_NE(model()->bookmark_bar_node()->GetTitle(), kBookmarkBarTitle);
    model()->SetTitle(model()->bookmark_bar_node(),
                      std::u16string(kBookmarkBarTitle),
                      bookmarks::metrics::BookmarkEditSource::kOther);
#else
    // Other platforms do not use titlecase, and therefore already have
    // "Bookmarks bar".
    CHECK_EQ(model()->bookmark_bar_node()->GetTitle(), kBookmarkBarTitle);
#endif
  }

  Profile* profile() { return profile_.get(); }

  BookmarkModel* model() {
    return BookmarkModelFactory::GetForBrowserContext(profile());
  }

  // Adds the following bookmark structure to the given parent:
  //
  //   F1
  //     url1 (with favicon)
  //     F2
  //       url2
  //   url3
  void PopulateBookmarks(const BookmarkNode* parent) {
    CHECK(parent);
    std::u16string f1_title = u"F1";
    std::u16string f2_title = u"F2";
    std::u16string url1_title = u"url1";
    std::u16string url2_title = u"url2";
    std::u16string url3_title = u"url3";
    GURL url1("http://url1");
    GURL url2("http://url2");
    GURL url3("http://url3");
    GURL url1_favicon("http://url1/icon.ico");

    const BookmarkNode* f1 = model()->AddFolder(parent, 0, f1_title);
    model()->AddURL(f1, 0, url1_title, url1);
    const BookmarkNode* f2 = model()->AddFolder(f1, 1, f2_title);
    model()->AddURL(f2, 0, url2_title, url2);
    model()->AddURL(parent, 1, url3_title, url3);

    // Set up a favicon for url1.
    HistoryServiceFactory::GetForProfile(profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->AddPage(url1, base::Time::Now(), history::SOURCE_BROWSED);
    FaviconServiceFactory::GetForProfile(profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->SetFavicons({url1}, url1_favicon, favicon_base::IconType::kFavicon,
                      gfx::Image::CreateFrom1xBitmap(
                          MakeTestSkBitmap(kIconWidth, kIconHeight)));
  }

  bookmark_html_writer::Result WriteBookmarksAndWait() {
    // Write to a temp file and return the async result.
    base::test::TestFuture<bookmark_html_writer::Result> future;
    bookmark_html_writer::WriteBookmarks(profile(), path_,
                                         future.GetCallback());
    return future.Get();
  }

  // Converts an ImportedBookmarkEntry to a string suitable for assertion
  // testing.
  std::u16string BookmarkEntryToString(
      const user_data_importer::ImportedBookmarkEntry& entry) {
    std::u16string result;
    result.append(u"on_toolbar=");
    if (entry.in_toolbar) {
      result.append(u"true");
    } else {
      result.append(u"false");
    }

    result.append(u" url=" + base::UTF8ToUTF16(entry.url.spec()));

    result.append(u" path=");
    for (size_t i = 0; i < entry.path.size(); ++i) {
      if (i != 0) {
        result.append(u"/");
      }
      result.append(entry.path[i]);
    }

    result.append(u" title=");
    result.append(entry.title);

    result.append(u" time=");
    result.append(base::TimeFormatFriendlyDateAndTime(entry.creation_time));
    return result;
  }

  // Creates a set of bookmark values to a string for assertion testing.
  std::u16string BookmarkValuesToString(bool on_toolbar,
                                        const GURL& url,
                                        std::u16string_view title,
                                        base::Time creation_time,
                                        std::u16string_view f1,
                                        std::u16string_view f2,
                                        std::u16string_view f3) {
    user_data_importer::ImportedBookmarkEntry entry;
    entry.in_toolbar = on_toolbar;
    entry.url = url;
    if (!f1.empty()) {
      entry.path.emplace_back(f1);
      if (!f2.empty()) {
        entry.path.emplace_back(f2);
        if (!f3.empty()) {
          entry.path.emplace_back(f3);
        }
      }
    }
    entry.title = title;
    entry.creation_time = creation_time;
    return BookmarkEntryToString(entry);
  }

  void AssertBookmarkEntryEquals(
      const user_data_importer::ImportedBookmarkEntry& entry,
      bool on_toolbar,
      const GURL& url,
      std::u16string_view title,
      base::Time creation_time,
      std::u16string_view f1,
      std::u16string_view f2,
      std::u16string_view f3) {
    EXPECT_EQ(BookmarkValuesToString(on_toolbar, url, title, creation_time, f1,
                                     f2, f3),
              BookmarkEntryToString(entry));
  }

  // Temp directory and path use for the output file.
  base::ScopedTempDir temp_dir_;
  base::FilePath path_;

  // Path to the test data directory containing the golden files.
  base::FilePath test_data_path_;

  // Set a fixed time, so that the output file contents is deterministic.
  base::subtle::ScopedTimeClockOverrides time_override{
      /*time_override=*/
      []() { return base::Time::FromSecondsSinceUnixEpoch(1234567890); },
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr};

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

// The CheckOutput* tests are intentionally change detector tests. This is
// because the export file must remain backwards compatible as it could be used
// to import bookmarks into other browsers.

TEST_F(BookmarkHTMLWriterTest, CheckOutputWhenNoBookmarks) {
  // No bookmarks in the model.

  // Export.
  ASSERT_EQ(WriteBookmarksAndWait(), bookmark_html_writer::Result::kSuccess);

  // Check against the golden file.
  EXPECT_TRUE(base::TextContentsEqual(
      path_, test_data_path_.AppendASCII("no_bookmarks.html")));
}

TEST_F(BookmarkHTMLWriterTest, CheckOutputWhenNoBookmarksWithAccount) {
  // Permanent account folders exist, but there are no local or account
  // bookmarks.
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  // Export.
  ASSERT_EQ(WriteBookmarksAndWait(), bookmark_html_writer::Result::kSuccess);

  // Check against the golden file.
  EXPECT_TRUE(base::TextContentsEqual(
      path_, test_data_path_.AppendASCII("no_bookmarks.html")));
}

TEST_F(BookmarkHTMLWriterTest, CheckOutputWhenBookmarksInLocalBookmarkBar) {
  // Populate the BookmarkModel. This creates the following bookmark structure:
  //
  // Bookmarks bar
  //   F1
  //     url1
  //     F2
  //       url2
  //   url3
  PopulateBookmarks(model()->bookmark_bar_node());

  // Export.
  ASSERT_EQ(WriteBookmarksAndWait(), bookmark_html_writer::Result::kSuccess);

  // Check against the golden file.
  const char* kGoldenFilename =
      skia::IsRustyPngEnabled() ? "bookmarks_in_bookmarks_bar.html"
                                : "bookmarks_in_bookmarks_bar_with_libpng.html";
  EXPECT_TRUE(base::TextContentsEqual(
      path_, test_data_path_.AppendASCII(kGoldenFilename)));
}

TEST_F(BookmarkHTMLWriterTest, CheckOutputWhenBookmarksInAccountBookmarkBar) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  // Populate the BookmarkModel. This creates the following bookmark structure:
  //
  // Bookmarks bar
  //   F1
  //     url1
  //     F2
  //       url2
  //   url3
  PopulateBookmarks(model()->account_bookmark_bar_node());

  // Export.
  ASSERT_EQ(WriteBookmarksAndWait(), bookmark_html_writer::Result::kSuccess);

  // Check against the golden file.
  const char* kGoldenFilename =
      skia::IsRustyPngEnabled() ? "bookmarks_in_bookmarks_bar.html"
                                : "bookmarks_in_bookmarks_bar_with_libpng.html";
  EXPECT_TRUE(base::TextContentsEqual(
      path_, test_data_path_.AppendASCII(kGoldenFilename)));
}

TEST_F(BookmarkHTMLWriterTest, CheckOutputWhenBookmarksInLocalOther) {
  // Populate the BookmarkModel. This creates the following bookmark structure:
  //
  // Other bookmarks
  //   F1
  //     url1
  //     F2
  //       url2
  //   url3
  PopulateBookmarks(model()->other_node());

  // Export.
  ASSERT_EQ(WriteBookmarksAndWait(), bookmark_html_writer::Result::kSuccess);

  // Check against the golden file.
  const char* kGoldenFilename = skia::IsRustyPngEnabled()
                                    ? "bookmarks_in_other.html"
                                    : "bookmarks_in_other_with_libpng.html";
  EXPECT_TRUE(base::TextContentsEqual(
      path_, test_data_path_.AppendASCII(kGoldenFilename)));
}

TEST_F(BookmarkHTMLWriterTest, CheckOutputWhenBookmarksInAccountOther) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSyncEnableBookmarksInTransportMode};
  model()->CreateAccountPermanentFolders();

  // Populate the BookmarkModel. This creates the following bookmark structure:
  //
  // Other bookmarks
  //   F1
  //     url1
  //     F2
  //       url2
  //   url3
  PopulateBookmarks(model()->account_other_node());

  // Export.
  ASSERT_EQ(WriteBookmarksAndWait(), bookmark_html_writer::Result::kSuccess);

  // Check against the golden file.
  const char* kGoldenFilename = skia::IsRustyPngEnabled()
                                    ? "bookmarks_in_other.html"
                                    : "bookmarks_in_other_with_libpng.html";
  EXPECT_TRUE(base::TextContentsEqual(
      path_, test_data_path_.AppendASCII(kGoldenFilename)));
}

// Tests bookmark_html_writer by populating a BookmarkModel, writing it out by
// way of bookmark_html_writer, then using the importer to read it back in.
TEST_F(BookmarkHTMLWriterTest, ExportThenImport) {
  // Create test PNG representing favicon for url1.
  SkBitmap bitmap = MakeTestSkBitmap(kIconWidth, kIconHeight);
  std::optional<std::vector<uint8_t>> icon_data =
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false);

  // Populate the BookmarkModel. This creates the following bookmark structure:
  // Bookmarks bar
  //   F1
  //     url1
  //     F2
  //       url2
  //   url3
  //   url4
  // Other
  //   url1
  //   url2
  //   F3
  //     F4
  //       url1
  // Mobile
  //   url1
  //   <bookmark without a title.>
  std::u16string f1_title = u"F\"&;<1\"";
  std::u16string f2_title = u"F2";
  std::u16string f3_title = u"F 3";
  std::u16string f4_title = u"F4";
  std::u16string url1_title = u"url 1";
  std::u16string url2_title = u"url&2";
  std::u16string url3_title = u"url\"3";
  std::u16string url4_title = u"url\"&;";
  std::u16string unnamed_bookmark_title = u"";
  GURL url1("http://url1");
  GURL url1_favicon("http://url1/icon.ico");
  GURL url2("http://url2");
  GURL url3("http://url3");
  GURL url4("javascript:alert(\"Hello!\");");
  GURL unnamed_bookmark_url("about:blank");
  base::Time t1(base::Time::Now());
  base::Time t2(t1 + base::Hours(1));
  base::Time t3(t1 + base::Hours(1));
  base::Time t4(t1 + base::Hours(1));
  const BookmarkNode* f1 =
      model()->AddFolder(model()->bookmark_bar_node(), 0, f1_title);
  model()->AddURL(f1, 0, url1_title, url1, nullptr, t1);
  HistoryServiceFactory::GetForProfile(profile(),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->AddPage(url1, base::Time::Now(), history::SOURCE_BROWSED);
  FaviconServiceFactory::GetForProfile(profile(),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->SetFavicons({url1}, url1_favicon, favicon_base::IconType::kFavicon,
                    gfx::Image::CreateFrom1xBitmap(bitmap));
  const BookmarkNode* f2 = model()->AddFolder(f1, 1, f2_title);
  model()->AddURL(f2, 0, url2_title, url2, nullptr, t2);
  model()->AddURL(model()->bookmark_bar_node(), 1, url3_title, url3, nullptr,
                  t3);

  model()->AddURL(model()->other_node(), 0, url1_title, url1, nullptr, t1);
  model()->AddURL(model()->other_node(), 1, url2_title, url2, nullptr, t2);
  const BookmarkNode* f3 =
      model()->AddFolder(model()->other_node(), 2, f3_title);
  const BookmarkNode* f4 = model()->AddFolder(f3, 0, f4_title);
  model()->AddURL(f4, 0, url1_title, url1, nullptr, t1);
  model()->AddURL(model()->bookmark_bar_node(), 2, url4_title, url4, nullptr,
                  t4);
  model()->AddURL(model()->mobile_node(), 0, url1_title, url1, nullptr, t1);
  model()->AddURL(model()->mobile_node(), 1, unnamed_bookmark_title,
                  unnamed_bookmark_url, nullptr, t2);

  // Export.
  ASSERT_EQ(WriteBookmarksAndWait(), bookmark_html_writer::Result::kSuccess);

  // Clear favicon so that it would be read from file.
  FaviconServiceFactory::GetForProfile(profile(),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->SetFavicons({url1}, url1_favicon, favicon_base::IconType::kFavicon,
                    gfx::Image());

  // Read the bookmarks back in.
  std::vector<user_data_importer::ImportedBookmarkEntry> parsed_bookmarks;
  std::vector<user_data_importer::SearchEngineInfo> parsed_search_engines;
  favicon_base::FaviconUsageDataList favicons;
  bookmark_html_reader::ImportBookmarksFile(
      base::RepeatingCallback<bool(void)>(),
      base::RepeatingCallback<bool(const GURL&)>(), path_, &parsed_bookmarks,
      &parsed_search_engines, &favicons);

  // Check loaded favicon (url1 is represented by 4 separate bookmarks).
  EXPECT_EQ(4U, favicons.size());
  for (size_t i = 0; i < favicons.size(); i++) {
    if (url1_favicon == favicons[i].favicon_url) {
      EXPECT_EQ(1U, favicons[i].urls.size());
      auto iter = favicons[i].urls.find(url1);
      ASSERT_TRUE(iter != favicons[i].urls.end());
      ASSERT_TRUE(*iter == url1);
      ASSERT_TRUE(favicons[i].png_data == icon_data.value());
    }
  }

  // Since we did not populate the BookmarkModel with any entry which can be
  // imported as search engine, verify that we got back no search engines.
  ASSERT_EQ(0U, parsed_search_engines.size());

  // Verify we got back what we wrote.
  ASSERT_EQ(9U, parsed_bookmarks.size());
  AssertBookmarkEntryEquals(parsed_bookmarks[0], true, url1, url1_title, t1,
                            kBookmarkBarTitle, f1_title, std::u16string());
  AssertBookmarkEntryEquals(parsed_bookmarks[1], true, url2, url2_title, t2,
                            kBookmarkBarTitle, f1_title, f2_title);
  AssertBookmarkEntryEquals(parsed_bookmarks[2], true, url3, url3_title, t3,
                            kBookmarkBarTitle, std::u16string(),
                            std::u16string());
  AssertBookmarkEntryEquals(parsed_bookmarks[3], true, url4, url4_title, t4,
                            kBookmarkBarTitle, std::u16string(),
                            std::u16string());
  AssertBookmarkEntryEquals(parsed_bookmarks[4], false, url1, url1_title, t1,
                            std::u16string(), std::u16string(),
                            std::u16string());
  AssertBookmarkEntryEquals(parsed_bookmarks[5], false, url2, url2_title, t2,
                            std::u16string(), std::u16string(),
                            std::u16string());
  AssertBookmarkEntryEquals(parsed_bookmarks[6], false, url1, url1_title, t1,
                            f3_title, f4_title, std::u16string());
  AssertBookmarkEntryEquals(parsed_bookmarks[7], false, url1, url1_title, t1,
                            std::u16string(), std::u16string(),
                            std::u16string());
  AssertBookmarkEntryEquals(parsed_bookmarks[8], false, unnamed_bookmark_url,
                            unnamed_bookmark_title, t2, std::u16string(),
                            std::u16string(), std::u16string());
}
