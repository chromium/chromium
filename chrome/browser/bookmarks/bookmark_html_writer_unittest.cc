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

#include "base/containers/flat_set.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/utility/importer/bookmark_html_reader.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_usage_data.h"
#include "components/history/core/browser/history_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/png_codec.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

const int kIconWidth = 16;
const int kIconHeight = 16;

void MakeTestSkBitmap(int w, int h, SkBitmap* bmp) {
  bmp->allocN32Pixels(w, h);

  uint32_t* src_data = bmp->getAddr32(0, 0);
  for (int i = 0; i < w * h; i++) {
    src_data[i] = SkPreMultiplyARGB(i % 255, i % 250, i % 245, i % 240);
  }
}

}  // namespace

class BookmarkHTMLWriterTest : public testing::Test {
 protected:
  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.GetPath().AppendASCII("bookmarks.html");
  }

  // Converts an ImportedBookmarkEntry to a string suitable for assertion
  // testing.
  std::u16string BookmarkEntryToString(const ImportedBookmarkEntry& entry) {
    std::u16string result;
    result.append(u"on_toolbar=");
    if (entry.in_toolbar)
      result.append(u"true");
    else
      result.append(u"false");

    result.append(u" url=" + base::UTF8ToUTF16(entry.url.spec()));

    result.append(u" path=");
    for (size_t i = 0; i < entry.path.size(); ++i) {
      if (i != 0)
        result.append(u"/");
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
                                        const std::u16string& title,
                                        base::Time creation_time,
                                        const std::u16string& f1,
                                        const std::u16string& f2,
                                        const std::u16string& f3) {
    ImportedBookmarkEntry entry;
    entry.in_toolbar = on_toolbar;
    entry.url = url;
    if (!f1.empty()) {
      entry.path.push_back(f1);
      if (!f2.empty()) {
        entry.path.push_back(f2);
        if (!f3.empty())
          entry.path.push_back(f3);
      }
    }
    entry.title = title;
    entry.creation_time = creation_time;
    return BookmarkEntryToString(entry);
  }

  void AssertBookmarkEntryEquals(const ImportedBookmarkEntry& entry,
                                 bool on_toolbar,
                                 const GURL& url,
                                 const std::u16string& title,
                                 base::Time creation_time,
                                 const std::u16string& f1,
                                 const std::u16string& f2,
                                 const std::u16string& f3) {
    EXPECT_EQ(BookmarkValuesToString(on_toolbar, url, title, creation_time,
                                     f1, f2, f3),
              BookmarkEntryToString(entry));
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath path_;
};

// Class that will notify message loop when file is written.
class BookmarksObserver : public BookmarksExportObserver {
 public:
  explicit BookmarksObserver(base::RunLoop* loop) : loop_(loop) {
    DCHECK(loop);
  }

  BookmarksObserver(const BookmarksObserver&) = delete;
  BookmarksObserver& operator=(const BookmarksObserver&) = delete;

  void OnExportFinished(Result result) override {
    EXPECT_EQ(Result::kSuccess, result);
    loop_->Quit();
  }

 private:
  raw_ptr<base::RunLoop> loop_;
};

// Tests bookmark_html_writer by populating a BookmarkModel, writing it out by
// way of bookmark_html_writer, then using the importer to read it back in.
TEST_F(BookmarkHTMLWriterTest, Test) {
  content::BrowserTaskEnvironment task_environment;

  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(BookmarkModelFactory::GetInstance(),
                                    BookmarkModelFactory::GetDefaultFactory());
  profile_builder.AddTestingFactory(FaviconServiceFactory::GetInstance(),
                                    FaviconServiceFactory::GetDefaultFactory());
  profile_builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                                    HistoryServiceFactory::GetDefaultFactory());

  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  profile->BlockUntilHistoryProcessesPendingRequests();

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile.get());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  // Create test PNG representing favicon for url1.
  SkBitmap bitmap;
  MakeTestSkBitmap(kIconWidth, kIconHeight, &bitmap);
  std::vector<unsigned char> icon_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &icon_data);

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
  const BookmarkNode* f1 = model->AddFolder(
      model->bookmark_bar_node(), 0, f1_title);
  model->AddURL(f1, 0, url1_title, url1, nullptr, t1);
  HistoryServiceFactory::GetForProfile(profile.get(),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->AddPage(url1, base::Time::Now(), history::SOURCE_BROWSED);
  FaviconServiceFactory::GetForProfile(profile.get(),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->SetFavicons({url1}, url1_favicon, favicon_base::IconType::kFavicon,
                    gfx::Image::CreateFrom1xBitmap(bitmap));
  const BookmarkNode* f2 = model->AddFolder(f1, 1, f2_title);
  model->AddURL(f2, 0, url2_title, url2, nullptr, t2);
  model->AddURL(model->bookmark_bar_node(), 1, url3_title, url3, nullptr, t3);

  model->AddURL(model->other_node(), 0, url1_title, url1, nullptr, t1);
  model->AddURL(model->other_node(), 1, url2_title, url2, nullptr, t2);
  const BookmarkNode* f3 = model->AddFolder(model->other_node(), 2, f3_title);
  const BookmarkNode* f4 = model->AddFolder(f3, 0, f4_title);
  model->AddURL(f4, 0, url1_title, url1, nullptr, t1);
  model->AddURL(model->bookmark_bar_node(), 2, url4_title, url4, nullptr, t4);
  model->AddURL(model->mobile_node(), 0, url1_title, url1, nullptr, t1);
  model->AddURL(model->mobile_node(), 1, unnamed_bookmark_title,
                unnamed_bookmark_url, nullptr, t2);

  base::RunLoop run_loop;

  // Write to a temp file.
  BookmarksObserver observer(&run_loop);
  bookmark_html_writer::WriteBookmarks(profile.get(), path_, &observer);
  run_loop.Run();
  if (HasFailure()) {
    // WriteBookmarks has failed, no point in trying to read the file.
    return;
  }

  // Clear favicon so that it would be read from file.
  FaviconServiceFactory::GetForProfile(profile.get(),
                                       ServiceAccessType::EXPLICIT_ACCESS)
      ->SetFavicons({url1}, url1_favicon, favicon_base::IconType::kFavicon,
                    gfx::Image());

  // Read the bookmarks back in.
  std::vector<ImportedBookmarkEntry> parsed_bookmarks;
  std::vector<importer::SearchEngineInfo> parsed_search_engines;
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
      ASSERT_TRUE(favicons[i].png_data == icon_data);
    }
  }

  // Since we did not populate the BookmarkModel with any entry which can be
  // imported as search engine, verify that we got back no search engines.
  ASSERT_EQ(0U, parsed_search_engines.size());

  // Verify we got back what we wrote.
  ASSERT_EQ(9U, parsed_bookmarks.size());
  // Windows and ChromeOS builds use Sentence case.
  std::u16string bookmark_folder_name =
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_FOLDER_NAME);
  AssertBookmarkEntryEquals(parsed_bookmarks[0], true, url1, url1_title, t1,
                            bookmark_folder_name, f1_title, std::u16string());
  AssertBookmarkEntryEquals(parsed_bookmarks[1], true, url2, url2_title, t2,
                            bookmark_folder_name, f1_title, f2_title);
  AssertBookmarkEntryEquals(parsed_bookmarks[2], true, url3, url3_title, t3,
                            bookmark_folder_name, std::u16string(),
                            std::u16string());
  AssertBookmarkEntryEquals(parsed_bookmarks[3], true, url4, url4_title, t4,
                            bookmark_folder_name, std::u16string(),
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
