// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/app_list/search/omnibox/omnibox_result.h"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_util.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search_engines/template_url.h"
#include "content/public/test/browser_task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"

namespace app_list::test {

namespace {

using testing::_;

const char16_t kFullQuery[] = u"match";
const char16_t kExampleContents[] = u"matchurl.com/contents.com";
const char16_t kExampleDescription[] = u"description match";
const char kExampleUrl[] = "http://example.com/hello";
const int kRelevance = 750;
const double kAppListRelevance = 0.5;
const char16_t kExampleKeyword[] = u"example.com";

// Verify that it's safe to create static instances of the following.
static_assert(std::is_trivially_destructible<ACMatchClassification>::value &&
              std::is_trivially_destructible<ash::SearchResultTag>::value);

// Example contents is a URL.
const ACMatchClassification kExampleContentsClass[] = {
    {/*offset=*/0,
     /*style=*/ACMatchClassification::URL | ACMatchClassification::MATCH},
    {/*offset=*/5, /*style=*/ACMatchClassification::URL},
};

const ash::SearchResultTag kExpectedExampleContentsTags[] = {
    {/*styles=*/ash::SearchResultTag::URL, /*start=*/0, /*end=*/25},
};

// Example description is not a URL.
const ACMatchClassification kExampleDescriptionClass[] = {
    {/*offset=*/0, /*style=*/ACMatchClassification::NONE},
};

// The bytes of a 16x16 yellow square PNG image. Encoded in base64 for
// convenience.
const char kTestIconBase64[] =
    "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAAXNSR0IArs4c6QAAAB5JREFUOE"
    "9j/P+f4T8DBYBx1ACG0TBgGA0DhmERBgDtai/htUkdAAAAAABJRU5ErkJggg==";

// The same 16x16 image.
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

// Returns true if the given text vector has one text entry that equals the
// given text, and no tags.
bool IsSingletonTextVectorNoTags(
    const std::vector<ash::SearchResultTextItem>& v,
    const std::u16string& text) {
  return v.size() == 1 &&
         v[0].GetType() == ash::SearchResultTextItemType::kString &&
         v[0].GetText() == text && v[0].GetTextTags().empty();
}

// Returns true if the given text vector has one text entry that equals the
// given text, and tags that match the given tags.
//
// Would use an absl::Span to capture static array lengths, but absl isn't yet
// allowed in these unit tests.
template <int ArraySize>
bool IsSingletonTextVector(const std::vector<ash::SearchResultTextItem>& v,
                           const std::u16string& text,
                           const ash::SearchResultTag (&tags)[ArraySize]) {
  if (v.size() != 1 ||
      v[0].GetType() != ash::SearchResultTextItemType::kString ||
      v[0].GetText() != text) {
    return false;
  }

  // Check that tags match.
  const auto& result_tags = v[0].GetTextTags();
  if (result_tags.size() != ArraySize)
    return false;

  for (int i = 0; i < ArraySize; ++i) {
    if (result_tags[i].styles != tags[i].styles ||
        result_tags[i].range != tags[i].range) {
      return false;
    }
  }

  return true;
}

}  // namespace

class OmniboxResultTest : public testing::Test {
 public:
  OmniboxResultTest() = default;

  OmniboxResultTest(const OmniboxResultTest&) = delete;
  OmniboxResultTest& operator=(const OmniboxResultTest&) = delete;

  ~OmniboxResultTest() override = default;

  void SetUp() override {
    // We need the bookmark and template URL services, and to create URL
    // loaders.
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    profile_ = profile_builder.Build();

    app_list_controller_delegate_ =
        std::make_unique<::test::TestAppListControllerDelegate>();

    favicon_cache_ = std::make_unique<FaviconCache>(
        /*favicon_service=*/&favicon_service_, /*history_service=*/nullptr);

    // Ensure the bookmark model is loaded.
    bookmark_model_ =
        BookmarkModelFactory::GetForBrowserContext(profile_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
  }

  std::unique_ptr<OmniboxResult> CreateOmniboxResult(
      const std::string& destination_url,
      AutocompleteMatchType::Type type,
      const GURL& image_url = GURL(),
      const std::u16string query = kFullQuery) {
    AutocompleteMatch match;
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(query);
    match.search_terms_args->original_query = query;
    match.relevance = kRelevance;
    match.destination_url = GURL(destination_url);
    match.stripped_destination_url = match.destination_url;

    match.contents = kExampleContents;
    match.contents_class = ACMatchClassifications(
        std::begin(kExampleContentsClass), std::end(kExampleContentsClass));

    match.description = kExampleDescription;
    match.description_class =
        ACMatchClassifications(std::begin(kExampleDescriptionClass),
                               std::end(kExampleDescriptionClass));

    match.type = type;
    match.keyword = kExampleKeyword;
    match.image_url = image_url;

    return std::make_unique<OmniboxResult>(
        profile_.get(), app_list_controller_delegate_.get(),
        CreateResult(match, /*controller=*/nullptr, favicon_cache_.get(),
                     bookmark_model_, input_),
        /*query=*/query);
  }

  const GURL& GetLastOpenedUrl() const {
    return app_list_controller_delegate_->last_opened_url();
  }

 private:
  const AutocompleteInput input_;

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<::test::TestAppListControllerDelegate>
      app_list_controller_delegate_;

  std::unique_ptr<FaviconCache> favicon_cache_;

  // Replaces IPC with a call to an inprocess alternative.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;

  testing::NiceMock<favicon::MockFaviconService> favicon_service_;

  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

TEST_F(OmniboxResultTest, Basic) {
  std::unique_ptr<OmniboxResult> result =
      CreateOmniboxResult(kExampleUrl, AutocompleteMatchType::HISTORY_URL);

  EXPECT_EQ(kExampleContents, result->details());
  EXPECT_EQ(kExampleDescription, result->title());
  EXPECT_EQ(kAppListRelevance, result->relevance());
  EXPECT_FALSE(result->scoring().filtered());

  result->Open(0);
  EXPECT_EQ(kExampleUrl, GetLastOpenedUrl().spec());
}

// Test that multiple results for the same URL will be correctly de-duped based
// on their type.
TEST_F(OmniboxResultTest, Priority) {
  // Make sure rich entity results supplant all others, and that history results
  // supplant non-rich-entity results.
  const auto history_result = CreateOmniboxResult(
      "https://url1.com", AutocompleteMatchType::SEARCH_HISTORY);
  const auto rich_entity_result = CreateOmniboxResult(
      "https://url1.com", AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
      GURL("http://website/rich_image.jpg"));
  const auto other_result = CreateOmniboxResult(
      "https://url1.com", AutocompleteMatchType::SEARCH_OTHER_ENGINE);

  EXPECT_GT(rich_entity_result->dedup_priority(),
            history_result->dedup_priority());
  EXPECT_GT(history_result->dedup_priority(), other_result->dedup_priority());
}

// Test that metrics of the right types are logged for different types of
// answers.
TEST_F(OmniboxResultTest, Metrics) {
  // Add a URL to our bookmarks.
  bookmark_model_->AddURL(bookmark_model_->bookmark_bar_node(), 0, u"Title",
                          GURL("https://example.com"));

  // Bookmarked URLs belong to their own metrics category and have a specific
  // icon.
  const auto bookmarked_result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::HISTORY_URL);
  EXPECT_EQ(ash::OMNIBOX_BOOKMARK, bookmarked_result->metrics_type());
  EXPECT_EQ(&omnibox::kBookmarkIcon,
            bookmarked_result->icon().icon.GetVectorIcon().vector_icon());

  // Unbookmarked URLs belong to the general "recently visited" category and
  // have a generic icon.
  const auto unbookmarked_result = CreateOmniboxResult(
      "https://fake.com", AutocompleteMatchType::HISTORY_URL);
  EXPECT_EQ(ash::OMNIBOX_RECENTLY_VISITED_WEBSITE,
            unbookmarked_result->metrics_type());
  EXPECT_EQ(&ash::kOmniboxGenericIcon,
            unbookmarked_result->icon().icon.GetVectorIcon().vector_icon());
}

// Test that the Omnibox search results are specially handled.
TEST_F(OmniboxResultTest, OmniboxSearchResult) {
  // Omnibox-search-type results should be demarked and should have the remove
  // action set.
  const auto search_result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::SEARCH_SUGGEST);
  ASSERT_EQ(1u, search_result->actions().size());
  EXPECT_EQ(ash::SearchResultActionType::kRemove,
            search_result->actions()[0].type);

  // Non-Omnibox-search-type results have no actions.
  const auto non_search_result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::HISTORY_URL);
  EXPECT_EQ(0u, non_search_result->actions().size());
}

// Test that search-what-you-typed results are specially handled.
TEST_F(OmniboxResultTest, SearchWhatYouTypedResult) {
  // Search-what-you-typed results should be marked for not needing update
  // animations.
  const auto search_what_you_typed_result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  EXPECT_TRUE(
      search_what_you_typed_result->CloneMetadata()->skip_update_animation);

  const auto other_result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::HISTORY_URL);
  EXPECT_FALSE(other_result->CloneMetadata()->skip_update_animation);
}

// Test that category is correctly set.
TEST_F(OmniboxResultTest, Category) {
  // Search suggestions belong to the "search and assistant" category.
  const auto search_result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::SEARCH_SUGGEST);
  EXPECT_EQ(ash::AppListSearchResultCategory::kSearchAndAssistant,
            search_result->category());

  // Others belong to the "web" category.
  const auto non_search_result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::HISTORY_URL);
  EXPECT_EQ(ash::AppListSearchResultCategory::kWeb,
            non_search_result->category());
}

// Test that a favicon is used as an icon if it exists.
TEST_F(OmniboxResultTest, Favicon) {
  // Capture the callback used to return the "fetched" icon.
  favicon_base::FaviconImageCallback return_icon_callback;
  EXPECT_CALL(favicon_service_,
              GetFaviconImageForPageURL(GURL("https://example.com"),
                                        /*callback=*/_, /*tracker=*/_))
      .WillOnce([&](auto, favicon_base::FaviconImageCallback callback, auto) {
        return_icon_callback = std::move(callback);
        return base::CancelableTaskTracker::kBadTaskId;
      });

  const auto result = CreateOmniboxResult("https://example.com",
                                          AutocompleteMatchType::HISTORY_URL);

  // The mock fetch result.
  favicon_base::FaviconImageResult mock_icon_result;
  mock_icon_result.image = gfx::Image(TestIcon());

  // Transmit fetched image back to the Omnibox result.
  std::move(return_icon_callback).Run(mock_icon_result);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      ImageSkiasEqual(TestIcon(), result->icon().icon.Rasterize(nullptr)));

  // A subsequent result with the same favicon should use the cached result.
  const auto next_result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::HISTORY_URL);
  EXPECT_TRUE(
      ImageSkiasEqual(TestIcon(), next_result->icon().icon.Rasterize(nullptr)));

  // Favicon shouldn't overwrite metrics type.
  EXPECT_EQ(ash::OMNIBOX_RECENTLY_VISITED_WEBSITE, next_result->metrics_type());
}

// Test that the attached media (e.g. sun icon for weather results) is used as
// the icon for a rich entity result.
TEST_F(OmniboxResultTest, RichEntityIcon) {
  // Set up mock icon response.
  std::string test_icon_png;
  ASSERT_TRUE(base::Base64Decode(kTestIconBase64, &test_icon_png));
  test_url_loader_factory_.AddResponse("https://example.com/icon.png",
                                       test_icon_png);

  // Construct a rich entity that points to the icon URL. This triggers a
  // download and parse.
  const auto result = CreateOmniboxResult(
      "https://url1.com", AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
      GURL("https://example.com/icon.png"));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ash::AppListSearchResultCategory::kSearchAndAssistant,
            result->category());
  EXPECT_TRUE(
      ImageSkiasEqual(TestIcon(), result->icon().icon.Rasterize(nullptr)));
}

// Test that results have generic icons for their result type.
TEST_F(OmniboxResultTest, GenericIcon) {
  const auto domain_result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::HISTORY_URL);
  EXPECT_EQ(&ash::kOmniboxGenericIcon,
            domain_result->icon().icon.GetVectorIcon().vector_icon());
  const auto search_result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::SEARCH_SUGGEST);
  EXPECT_EQ(&ash::kSearchIcon,
            search_result->icon().icon.GetVectorIcon().vector_icon());
}

// Test that URLs with descriptions have their contents and descriptions
// swapped.
TEST_F(OmniboxResultTest, UrlText) {
  // Uses the default example contents and description.
  const auto result = CreateOmniboxResult("https://example.com",
                                          AutocompleteMatchType::HISTORY_URL);

  // The output title should be the input description and the output details
  // should be the input contents.
  EXPECT_TRUE(IsSingletonTextVectorNoTags(result->title_text_vector(),
                                          kExampleDescription));
  EXPECT_TRUE(IsSingletonTextVector(result->details_text_vector(),
                                    kExampleContents,
                                    kExpectedExampleContentsTags));

  // Accessible name should not be set.
  EXPECT_TRUE(result->accessible_name().empty());
}

// Test that descriptions are displayed for rich entities.
TEST_F(OmniboxResultTest, RichEntityText) {
  // Uses the default example contents and description.
  const auto result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
      GURL("https://example.com/icon.png"));

  const std::u16string expected_description =
      std::u16string(kExampleDescription) + u" - Google Search";

  // Both contents and description should be displayed.
  EXPECT_TRUE(IsSingletonTextVector(result->title_text_vector(),
                                    kExampleContents,
                                    kExpectedExampleContentsTags));
  EXPECT_TRUE(IsSingletonTextVectorNoTags(result->details_text_vector(),
                                          expected_description));

  // Accessible name should be set.
  EXPECT_NE(std::u16string::npos, result->accessible_name().find(u"contents"));
}

// Test that search results put the search engine in the description, and have
// no accessible name.
TEST_F(OmniboxResultTest, SearchResultText) {
  // Uses the default example contents and description.
  const auto result = CreateOmniboxResult(
      "https://example.com", AutocompleteMatchType::SEARCH_SUGGEST);

  // Title should be populated.
  EXPECT_TRUE(IsSingletonTextVector(result->title_text_vector(),
                                    kExampleContents,
                                    kExpectedExampleContentsTags));
  // Details should contain the search engine. This test relies on the test
  // environment having Google as the default search engine.
  EXPECT_TRUE(IsSingletonTextVectorNoTags(result->details_text_vector(),
                                          u"Google Search"));

  // Accessible name should not be set.
  EXPECT_TRUE(result->accessible_name().empty());
}

TEST_F(OmniboxResultTest, RelevanceWithFuzzyMatchCutoff) {
  scoped_feature_list_.InitAndEnableFeature(search_features::kLauncherFuzzyMatchForOmnibox);
  std::unique_ptr<OmniboxResult> result_high_fuzzy_relevance =
      CreateOmniboxResult(kExampleUrl, AutocompleteMatchType::HISTORY_URL,
                          GURL(), kExampleDescription);
  std::unique_ptr<OmniboxResult> result_low_fuzzy_relevance =
      CreateOmniboxResult(kExampleUrl, AutocompleteMatchType::HISTORY_URL,
                          GURL(), u"different");

  EXPECT_EQ(kAppListRelevance, result_high_fuzzy_relevance->relevance());
  EXPECT_EQ(kAppListRelevance, result_low_fuzzy_relevance->relevance());
  EXPECT_TRUE(result_low_fuzzy_relevance->scoring().filtered());
  EXPECT_FALSE(result_high_fuzzy_relevance->scoring().filtered());
}

}  // namespace app_list::test
