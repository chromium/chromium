// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/omnibox/open_tab_result.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_list/search/common/search_result_util.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"

namespace app_list::test {

namespace {

using testing::_;
using testing::Mock;

using ::ash::string_matching::TokenizedString;

// A 16x16 yellow square image.
gfx::ImageSkia TestIcon() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(SK_ColorYELLOW);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Returns true if the pixels of the two given images are identical.
bool ImageSkiasEqual(const gfx::ImageSkia& a, const gfx::ImageSkia& b) {
  return gfx::BitmapsAreEqual(*a.bitmap(), *b.bitmap());
}

}  // namespace

class OpenTabResultTest : public testing::Test {
 public:
  OpenTabResultTest() = default;

  OpenTabResultTest(const OpenTabResultTest&) = delete;
  OpenTabResultTest& operator=(const OpenTabResultTest&) = delete;

  ~OpenTabResultTest() override = default;

  void SetUp() override {
    app_list_controller_delegate_ =
        std::make_unique<::test::TestAppListControllerDelegate>();

    favicon_cache_ = std::make_unique<FaviconCache>(
        /*favicon_service=*/&favicon_service_, /*history_service=*/nullptr);
  }

  std::unique_ptr<OpenTabResult> MakeResult(const std::u16string& query,
                                            const std::u16string& description,
                                            const std::u16string& url) {
    AutocompleteMatch match;
    match.description = description;
    match.destination_url = GURL(url);
    match.relevance = 1000;
    match.type = AutocompleteMatchType::OPEN_TAB;
    TokenizedString tokenized_query(query, TokenizedString::Mode::kCamelCase);
    return std::make_unique<OpenTabResult>(
        /*profile=*/nullptr, app_list_controller_delegate_.get(),
        CreateResult(match, /*controller=*/nullptr, favicon_cache_.get(),
                     /*bookmark_model=*/nullptr, AutocompleteInput()),
        tokenized_query);
  }

  const GURL& GetLastOpenedUrl() const {
    return app_list_controller_delegate_->last_opened_url();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<::test::TestAppListControllerDelegate>
      app_list_controller_delegate_;
  std::unique_ptr<FaviconCache> favicon_cache_;

 protected:
  testing::NiceMock<favicon::MockFaviconService> favicon_service_;
};

TEST_F(OpenTabResultTest, Basic) {
  std::unique_ptr<OpenTabResult> result =
      MakeResult(u"query", u"queryabc", u"http://www.website.com");

  EXPECT_EQ(result->title(), u"queryabc");
  EXPECT_EQ(StringFromTextVector(result->details_text_vector()),
            base::StrCat({u"http://www.website.com/",
                          l10n_util::GetStringFUTF16(IDS_APP_LIST_OPEN_TAB_HINT,
                                                     u" - ")}));
  EXPECT_EQ(result->accessible_name(),
            base::StrCat({u"queryabc, http://www.website.com/",
                          l10n_util::GetStringFUTF16(IDS_APP_LIST_OPEN_TAB_HINT,
                                                     u", ")}));
  result->Open(0);
  EXPECT_EQ("http://www.website.com/", GetLastOpenedUrl().spec());
  EXPECT_EQ(result->DriveId(), std::nullopt);
}

TEST_F(OpenTabResultTest, ManuallyCalculateRelevance) {
  std::unique_ptr<OpenTabResult> result1 =
      MakeResult(u"query", u"queryabc", u"http://www.website.com");
  std::unique_ptr<OpenTabResult> result2 =
      MakeResult(u"queryabc", u"queryabc", u"http://www.website.com");

  // The results were given the same |match.relevance|, but the closer query
  // should have higher score.
  EXPECT_GT(result2->relevance(), result1->relevance());
}

TEST_F(OpenTabResultTest, Favicon) {
  // Capture the callback used to return the "fetched" icon.
  favicon_base::FaviconImageCallback return_icon_callback;
  EXPECT_CALL(favicon_service_,
              GetFaviconImageForPageURL(GURL("http://www.website.com"),
                                        /*callback=*/_, /*tracker=*/_))
      .WillOnce([&](auto, favicon_base::FaviconImageCallback callback, auto) {
        return_icon_callback = std::move(callback);
        return base::CancelableTaskTracker::kBadTaskId;
      });

  std::unique_ptr<OpenTabResult> result =
      MakeResult(u"query", u"queryabc", u"http://www.website.com");
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(&favicon_service_));
  base::RunLoop().RunUntilIdle();

  // The mock fetch result.
  favicon_base::FaviconImageResult mock_icon_result;
  mock_icon_result.image = gfx::Image(TestIcon());

  // Transmit fetched image back to the Omnibox result.
  std::move(return_icon_callback).Run(mock_icon_result);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      ImageSkiasEqual(TestIcon(), result->icon().icon.Rasterize(nullptr)));
}
}  // namespace app_list::test
