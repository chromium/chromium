// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files_page_handler.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTrendingFilesEndpoint[] =
    "https://graph.microsoft.com/v1.0/me/insights/trending";

const char kBaseWebUrl[] =
    "https://foo.com/sites/SiteName/Shared/Document%s.docx";

const char kDocIconUrl[] =
    "https://res.cdn.office.net/files/fabric-cdn-prod_20240925.001/assets/"
    "item-types/16/docx.png";

}  // namespace

class MicrosoftFilesPageHandlerTest : public testing::Test {
 public:
  MicrosoftFilesPageHandlerTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    profile_ = profile_builder.Build();
  }

  std::unique_ptr<MicrosoftFilesPageHandler> CreateHandler() {
    return std::make_unique<MicrosoftFilesPageHandler>(
        mojo::PendingReceiver<
            file_suggestion::mojom::MicrosoftFilesPageHandler>(),
        profile_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(MicrosoftFilesPageHandlerTest, GetFakeTrendingFiles) {
  feature_list_.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpSharepointModule,
      {{ntp_features::kNtpSharepointModuleDataParam.name, "fake-trending"}});
  std::unique_ptr<MicrosoftFilesPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  handler->GetFiles(future.GetCallback());
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();
  // The fake trending data is hardcoded to having 5 suggestions.
  EXPECT_EQ(suggestions.size(), 5u);
}

// Verifies files are constructed correctly from a valid response.
TEST_F(MicrosoftFilesPageHandlerTest, GetTrendingFiles) {
  feature_list_.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpSharepointModule,
      {{ntp_features::kNtpSharepointModuleDataParam.name,
        "trending-insights"}});
  std::unique_ptr<MicrosoftFilesPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  std::string response =
      R"({
    "value": [
        {
          "id": "0",
          "resourceVisualization": {
              "id": "0-abc",
              "title": "Document 0",
              "type": "Word",
            "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "resourceReference": {
              "webUrl": "https://foo.com/sites/SiteName/Shared/Document0.docx",
              "id": "0-xyz"
          }
        },
        {
          "id": "1",
          "resourceVisualization": {
              "id": "0-abc",
              "title": "Document 1",
              "type": "Word",
            "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "resourceReference": {
              "webUrl": "https://foo.com/sites/SiteName/Shared/Document1.docx",
              "id": "1-xyz"
          }
        },
        {
          "id": "2",
          "resourceVisualization": {
              "id": "0-abc",
              "title": "Document 2",
              "type": "Word",
            "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "resourceReference": {
              "webUrl": "https://foo.com/sites/SiteName/Shared/Document2.docx",
              "id": "2-xyz"
          }
        },
        {
          "id": "3",
          "resourceVisualization": {
              "id": "0-abc",
              "title": "Document 3",
              "type": "Word",
            "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "resourceReference": {
              "webUrl": "https://foo.com/sites/SiteName/Shared/Document3.docx",
              "id": "3-xyz"
          }
        },
        {
          "id": "4",
          "resourceVisualization": {
              "id": "0-abc",
              "title": "Document 4",
              "type": "Word",
            "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "resourceReference": {
              "webUrl": "https://foo.com/sites/SiteName/Shared/Document4.docx",
              "id": "4-xyz"
          }
        }
    ]})";
  handler->GetFiles(future.GetCallback());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTrendingFilesEndpoint, response);

  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 5u);

  for (size_t i = 0; i < suggestions.size(); i++) {
    auto& suggestion = suggestions[i];
    EXPECT_EQ(suggestion->id, base::NumberToString(i));
    EXPECT_EQ(suggestion->title, "Document " + base::NumberToString(i));
    EXPECT_EQ(suggestion->item_url,
              base::StringPrintf(kBaseWebUrl, base::NumberToString(i)));
    EXPECT_EQ(suggestion->icon_url, kDocIconUrl);
  }
}

TEST_F(MicrosoftFilesPageHandlerTest, NoTrendingFilesOnMalformedResponse) {
  feature_list_.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpSharepointModule,
      {{ntp_features::kNtpSharepointModuleDataParam.name,
        "trending-insights"}});
  std::unique_ptr<MicrosoftFilesPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  handler->GetFiles(future.GetCallback());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTrendingFilesEndpoint, "} {");

  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 0u);
}

TEST_F(MicrosoftFilesPageHandlerTest, NoTrendingFilesOnResponseMissingData) {
  feature_list_.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpSharepointModule,
      {{ntp_features::kNtpSharepointModuleDataParam.name,
        "trending-insights"}});
  std::unique_ptr<MicrosoftFilesPageHandler> handler = CreateHandler();
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  handler->GetFiles(future.GetCallback());

  // The `title` property is missing.
  std::string response = R"(
    "value": [
      {
        "id": "0",
        "resourceVisualization": {
            "id": "0-abc",
            "type": "Word",
            "mediaType": "application/vnd.)"
                         R"(openxmlformats-officedocument.spreadsheetml.sheet"
        },
        "resourceReference": {
            "webUrl": "https://foo.com/sites/SiteName/Shared/Document.xlsx",
            "id": "0-xyz"
        }
      }]})";

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      kTrendingFilesEndpoint, response);

  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 0u);
}
