// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/arc/arc_playstore_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/app/arc_playstore_search_request_state.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/ash/arc/icon_decode_request.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/extension_builder.h"

namespace app_list::test {

class ArcPlayStoreSearchProviderTest : public AppListTestBase {
 public:
  ArcPlayStoreSearchProviderTest() = default;

  ArcPlayStoreSearchProviderTest(const ArcPlayStoreSearchProviderTest&) =
      delete;
  ArcPlayStoreSearchProviderTest& operator=(
      const ArcPlayStoreSearchProviderTest&) = delete;

  ~ArcPlayStoreSearchProviderTest() override = default;

  // AppListTestBase:
  void SetUp() override {
    AppListTestBase::SetUp();
    arc_test_.SetUp(profile());
    controller_ = std::make_unique<::test::TestAppListControllerDelegate>();
  }

  void TearDown() override {
    controller_.reset();
    arc_test_.TearDown();
    AppListTestBase::TearDown();
  }

 protected:
  void CreateSearch(int max_results) {
    search_controller_ = std::make_unique<TestSearchController>();
    auto provider = std::make_unique<ArcPlayStoreSearchProvider>(
        max_results, profile_.get(), controller_.get());
    provider_ = provider.get();
    search_controller_->AddProvider(std::move(provider));
  }

  ArcPlayStoreSearchProvider* provider() { return provider_; }

  const SearchProvider::Results& LastResults() {
    return search_controller_->last_results();
  }

  void StartSearch(const std::u16string& query) {
    search_controller_->StartSearch(query);
  }

  scoped_refptr<const extensions::Extension> CreateExtension(
      const std::string& id) {
    return extensions::ExtensionBuilder("test").SetID(id).Build();
  }

  void AddExtension(const extensions::Extension* extension) {
    service()->AddExtension(extension);
  }

 private:
  std::unique_ptr<::test::TestAppListControllerDelegate> controller_;
  std::unique_ptr<TestSearchController> search_controller_;
  raw_ptr<ArcPlayStoreSearchProvider> provider_ = nullptr;
  ArcAppTest arc_test_;
};

TEST_F(ArcPlayStoreSearchProviderTest, Basic) {
  constexpr size_t kMaxResults = 12;
  constexpr char16_t kQuery[] = u"Play App";

  CreateSearch(kMaxResults);
  EXPECT_TRUE(LastResults().empty());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  AddExtension(CreateExtension(extension_misc::kGmailAppId).get());

  // Check that the result size of a query doesn't exceed the |kMaxResults|.
  StartSearch(kQuery);
  const SearchProvider::Results& results = LastResults();
  ASSERT_GT(results.size(), 0u);
  // Play Store returns |kMaxResults| results, but the first one (GMail) already
  // has Chrome extension installed, so it will be skipped.
  ASSERT_EQ(kMaxResults - 1, results.size());

  // Check that information is correctly set in each result.
  for (size_t i = 0; i < results.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing result %zu", i));
    EXPECT_EQ(results[i]->title(),
              base::StrCat({kQuery, u" ", base::NumberToString16(i)}));
    EXPECT_EQ(results[i]->display_type(), ash::SearchResultDisplayType::kList);
    EXPECT_EQ(base::UTF16ToUTF8(results[i]->formatted_price()),
              base::StringPrintf("$%zu.22", i));
    EXPECT_EQ(results[i]->rating(), i);
    const bool is_instant_app = i % 2 == 0;
    EXPECT_EQ(results[i]->result_type(),
              is_instant_app ? ash::AppListSearchResultType::kInstantApp
                             : ash::AppListSearchResultType::kPlayStoreApp);
  }
}
// Tests that provider reports valid results if the app instance responds with a
// non empty result list and PHONESKY_RESULT_INVALID_DATA status code (which can
// happen if the Play Store returns a list of results that contains some invalid
// items).
TEST_F(ArcPlayStoreSearchProviderTest, PartiallyFailedQuery) {
  constexpr size_t kMaxResults = 12;

  CreateSearch(kMaxResults);
  EXPECT_TRUE(LastResults().empty());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  AddExtension(CreateExtension(extension_misc::kGmailAppId).get());

  const std::u16string kQuery =
      u"PartiallyFailedQueryWithCode-" +
      base::NumberToString16(static_cast<int>(
          arc::ArcPlayStoreSearchRequestState::PHONESKY_RESULT_INVALID_DATA));

  StartSearch(kQuery);

  const SearchProvider::Results& results = LastResults();
  ASSERT_GT(results.size(), 0u);
  // Play Store returns |kMaxResults / 2| results, but the first one (GMail)
  // already has Chrome extension installed, so it will be skipped.
  ASSERT_EQ(kMaxResults / 2 - 1, results.size());

  // Check that information is correctly set in each result.
  for (size_t i = 0; i < results.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing result %zu", i));
    EXPECT_EQ(results[i]->title(),
              base::StrCat({kQuery, u" ", base::NumberToString16(i)}));
    EXPECT_EQ(results[i]->display_type(), ash::SearchResultDisplayType::kList);
    EXPECT_EQ(base::UTF16ToUTF8(results[i]->formatted_price()),
              base::StringPrintf("$%zu.22", i));
    EXPECT_EQ(results[i]->rating(), i);
    const bool is_instant_app = i % 2 == 0;
    EXPECT_EQ(results[i]->result_type(),
              is_instant_app ? ash::AppListSearchResultType::kInstantApp
                             : ash::AppListSearchResultType::kPlayStoreApp);
  }
}

// Tests that the search provider can handle Play Store suggestions without
// rating and formatted price.
TEST_F(ArcPlayStoreSearchProviderTest, ResultsWithoutPriceAndRating) {
  constexpr size_t kMaxResults = 12;

  CreateSearch(kMaxResults);
  EXPECT_TRUE(LastResults().empty());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  AddExtension(CreateExtension(extension_misc::kGmailAppId).get());

  const std::u16string kQuery = u"QueryWithoutRatingAndPrice";

  StartSearch(kQuery);

  const SearchProvider::Results& results = LastResults();
  ASSERT_GT(results.size(), 0u);
  // Play Store returns |kMaxResults| results, but the first one (GMail)
  // already has Chrome extension installed, so it will be skipped.
  ASSERT_EQ(kMaxResults - 1, results.size());

  // Check that information is correctly set in each result.
  for (size_t i = 0; i < results.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing result %zu", i));
    EXPECT_EQ(results[i]->title(),
              base::StrCat({kQuery, u" ", base::NumberToString16(i)}));
    EXPECT_EQ(results[i]->display_type(), ash::SearchResultDisplayType::kList);
    EXPECT_EQ(base::UTF16ToUTF8(results[i]->formatted_price()), "");
    EXPECT_EQ(results[i]->rating(), -1);
    const bool is_instant_app = i % 2 == 0;
    EXPECT_EQ(results[i]->result_type(),
              is_instant_app ? ash::AppListSearchResultType::kInstantApp
                             : ash::AppListSearchResultType::kPlayStoreApp);
  }
}

// Tests that results without icon are ignored.
TEST_F(ArcPlayStoreSearchProviderTest, IgnoreResultsWithoutIcon) {
  constexpr size_t kMaxResults = 12;

  CreateSearch(kMaxResults);
  EXPECT_TRUE(LastResults().empty());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  AddExtension(CreateExtension(extension_misc::kGmailAppId).get());

  const std::u16string kQuery = u"QueryWithSomeResultsMissingIcon";

  StartSearch(kQuery);

  const SearchProvider::Results& results = LastResults();
  ASSERT_GT(results.size(), 0u);
  // Play Store returns |kMaxResults| results, but the first one (GMail)
  // already has Chrome extension installed, so it will be skipped, and
  // items after kMaxResults / 2 are missing the icon and are expected to be
  // ignored.
  ASSERT_EQ(kMaxResults / 2, results.size());

  // Check that information is correctly set in each result.
  for (size_t i = 0; i < results.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing result %zu", i));
    EXPECT_EQ(results[i]->title(),
              base::StrCat({kQuery, u" ", base::NumberToString16(i)}));
    EXPECT_EQ(results[i]->display_type(), ash::SearchResultDisplayType::kList);
    EXPECT_EQ(base::UTF16ToUTF8(results[i]->formatted_price()),
              base::StringPrintf("$%zu.22", i));
    EXPECT_EQ(results[i]->rating(), i);
    const bool is_instant_app = i % 2 == 0;
    EXPECT_EQ(results[i]->result_type(),
              is_instant_app ? ash::AppListSearchResultType::kInstantApp
                             : ash::AppListSearchResultType::kPlayStoreApp);
  }
}

TEST_F(ArcPlayStoreSearchProviderTest, FailedQuery) {
  constexpr size_t kMaxResults = 12;
  constexpr char16_t kQuery[] = u"Play App";

  CreateSearch(kMaxResults);
  EXPECT_TRUE(LastResults().empty());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  // Test for empty queries.
  // Create a non-empty query.
  StartSearch(kQuery);
  EXPECT_GT(LastResults().size(), 0u);

  // Test for queries with a failure state code.
  constexpr char16_t kFailedQueryPrefix[] = u"FailedQueryWithCode-";
  using RequestState = arc::ArcPlayStoreSearchRequestState;
  const std::array<RequestState, 15> kErrorStates = {
      RequestState::PLAY_STORE_PROXY_NOT_AVAILABLE,
      RequestState::FAILED_TO_CALL_CANCEL,
      RequestState::FAILED_TO_CALL_FINDAPPS,
      RequestState::REQUEST_HAS_INVALID_PARAMS,
      RequestState::REQUEST_TIMEOUT,
      RequestState::PHONESKY_RESULT_REQUEST_CODE_UNMATCHED,
      RequestState::PHONESKY_RESULT_SESSION_ID_UNMATCHED,
      RequestState::PHONESKY_REQUEST_REQUEST_CODE_UNMATCHED,
      RequestState::PHONESKY_APP_DISCOVERY_NOT_AVAILABLE,
      RequestState::PHONESKY_VERSION_NOT_SUPPORTED,
      RequestState::PHONESKY_UNEXPECTED_EXCEPTION,
      RequestState::PHONESKY_MALFORMED_QUERY,
      RequestState::PHONESKY_INTERNAL_ERROR,
      RequestState::PHONESKY_RESULT_INVALID_DATA,
      RequestState::CHROME_GOT_INVALID_RESULT,
  };
  static_assert(
      kErrorStates.size() == static_cast<size_t>(RequestState::STATE_COUNT) - 3,
      "Missing entries");
  for (const auto& error_state : kErrorStates) {
    // Create a non-empty query.
    StartSearch(kQuery);
    EXPECT_GT(LastResults().size(), 0u);

    // Fabricate a failing query and it should clear the result list.
    StartSearch(kFailedQueryPrefix +
                base::NumberToString16(static_cast<int>(error_state)));
    EXPECT_EQ(0u, LastResults().size());
  }
}

}  // namespace app_list::test
