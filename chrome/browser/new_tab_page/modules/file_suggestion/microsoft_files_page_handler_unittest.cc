// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files_page_handler.h"

#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files.mojom.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTrendingFilesEndpoint[] =
    "https://graph.microsoft.com/v1.0/me/insights/trending";

const char kBaseWebUrl[] = "https://foo.com/Document%s.docx";

const char kDocIconUrl[] =
    "https://res.cdn.office.net/files/fabric-cdn-prod_20240925.001/assets/"
    "item-types/16/docx.png";

const char kBatchRequestUrl[] = "https://graph.microsoft.com/v1.0/$batch";

const char kRequestResultHistogramName[] =
    "NewTabPage.MicrosoftFiles.RequestResult";

const char kResponseResultHistogramName[] =
    "NewTabPage.MicrosoftFiles.ResponseResult";

const char kThrottlingTimeHistogramName[] =
    "NewTabPage.MicrosoftFiles.ThrottlingWaitTime";

const char kSubstitutionTypeHistogramName[] =
    "NewTabPage.MicrosoftFiles.SubstitutionType";

}  // namespace

class MicrosoftFilesPageHandlerTest : public testing::Test {
 public:
  MicrosoftFilesPageHandlerTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    profile_builder.AddTestingFactory(
        MicrosoftAuthServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<MicrosoftAuthService>();
        }));
    profile_ = profile_builder.Build();

    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kNtpSharepointModuleVisible, base::Value(true));

    // Set access token needed for requests.
    new_tab_page::mojom::AccessTokenPtr access_token =
        new_tab_page::mojom::AccessToken::New();
    access_token->token = "1234";
    access_token->expiration = base::Time::Now() + base::Hours(24);
    MicrosoftAuthServiceFactory::GetForProfile(profile_.get())
        ->SetAccessToken(std::move(access_token));
  }

  void SetUp() override {
    handler_ = std::make_unique<MicrosoftFilesPageHandler>(
        mojo::PendingReceiver<
            file_suggestion::mojom::MicrosoftFilesPageHandler>(),
        profile_.get());
  }

  std::string GetTimeNowAsString() {
    return TimeFormatAsIso8601(base::Time::Now());
  }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }
  MicrosoftFilesPageHandler& handler() { return *handler_; }
  TestingProfile& profile() { return *profile_; }
  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<MicrosoftFilesPageHandler> handler_;
  base::HistogramTester histogram_tester_;
};

class MicrosoftFilesPageHandlerTestForTrending
    : public MicrosoftFilesPageHandlerTest {
 public:
  MicrosoftFilesPageHandlerTestForTrending() {
    feature_list().InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{ntp_features::kNtpSharepointModule,
          {{ntp_features::kNtpSharepointModuleDataParam.name,
            "trending-insights"}}},
         {ntp_features::kNtpMicrosoftAuthenticationModule, {}}},
        /*disabled_features=*/{});
  }
};

class MicrosoftFilesPageHandlerTestForNonInsights
    : public MicrosoftFilesPageHandlerTest {
 public:
  MicrosoftFilesPageHandlerTestForNonInsights() {
    feature_list().InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{ntp_features::kNtpSharepointModule,
          {{ntp_features::kNtpSharepointModuleDataParam.name, "non-insights"}}},
         {ntp_features::kNtpMicrosoftAuthenticationModule, {}}},
        /*disabled_features=*/{});
  }
};

class MicrosoftFilesPageHandlerTestForCombinedSuggestions
    : public MicrosoftFilesPageHandlerTest {
 public:
  MicrosoftFilesPageHandlerTestForCombinedSuggestions() {
    feature_list().InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{ntp_features::kNtpSharepointModule,
          {{ntp_features::kNtpSharepointModuleDataParam.name, "combined"}}},
         {ntp_features::kNtpMicrosoftAuthenticationModule, {}}},
        /*disabled_features=*/{});
  }
};

class MicrosoftFilesPageHandlerTestForFakeNonInsights
    : public MicrosoftFilesPageHandlerTest {
 public:
  MicrosoftFilesPageHandlerTestForFakeNonInsights() {
    feature_list().InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{ntp_features::kNtpSharepointModule,
          {{ntp_features::kNtpSharepointModuleDataParam.name,
            "fake-non-insights"}}},
         {ntp_features::kNtpMicrosoftAuthenticationModule, {}}},
        /*disabled_features=*/{});
  }
};

class MicrosoftFilesPageHandlerTestForFakeTrending
    : public MicrosoftFilesPageHandlerTest {
 public:
  MicrosoftFilesPageHandlerTestForFakeTrending() {
    feature_list().InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{ntp_features::kNtpSharepointModule,
          {{ntp_features::kNtpSharepointModuleDataParam.name,
            "fake-trending"}}},
         {ntp_features::kNtpMicrosoftAuthenticationModule, {}}},
        /*disabled_features=*/{});
  }
};

TEST_F(MicrosoftFilesPageHandlerTestForFakeTrending, GetFakeTrendingFiles) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  handler().GetFiles(future.GetCallback());
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();
  // The fake trending data is hardcoded to having 5 suggestions.
  EXPECT_EQ(suggestions.size(), 5u);
}

// Verifies files are constructed correctly from a valid response.
TEST_F(MicrosoftFilesPageHandlerTestForTrending, GetTrendingFiles) {
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
              "webUrl": "https://foo.com/Document0.docx",
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
              "webUrl": "https://foo.com/Document1.docx",
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
              "webUrl": "https://foo.com/Document2.docx",
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
              "webUrl": "https://foo.com/Document3.docx",
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
              "webUrl": "https://foo.com/Document4.docx",
              "id": "4-xyz"
          }
        }
    ]})";
  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(
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

  histogram_tester().ExpectBucketCount(
      kRequestResultHistogramName, MicrosoftFilesRequestResult::kSuccess, 1);
  histogram_tester().ExpectBucketCount(kResponseResultHistogramName, 5, 1);
}

TEST_F(MicrosoftFilesPageHandlerTestForTrending,
       NoTrendingFilesOnMalformedResponse) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(
      kTrendingFilesEndpoint, "} {");

  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 0u);

  histogram_tester().ExpectBucketCount(
      kRequestResultHistogramName, MicrosoftFilesRequestResult::kJsonParseError,
      1);
}

TEST_F(MicrosoftFilesPageHandlerTestForTrending,
       NoTrendingFilesOnResponseMissingData) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  handler().GetFiles(future.GetCallback());

  // The `title` property is missing.
  std::string response = R"({
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

  test_url_loader_factory().SimulateResponseForPendingRequest(
      kTrendingFilesEndpoint, response);

  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 0u);
  histogram_tester().ExpectBucketCount(kResponseResultHistogramName, 1, 1);
}

// Verifies that prefs are accurately set on dismissal and restoring of module.
TEST_F(MicrosoftFilesPageHandlerTestForTrending, DismissAndRestoreModule) {
  EXPECT_EQ(profile().GetPrefs()->GetTime(
                prefs::kNtpMicrosoftFilesModuleLastDismissedTime),
            base::Time());

  handler().DismissModule();
  EXPECT_EQ(profile().GetPrefs()->GetTime(
                prefs::kNtpMicrosoftFilesModuleLastDismissedTime),
            base::Time::Now());

  handler().RestoreModule();
  EXPECT_EQ(profile().GetPrefs()->GetTime(
                prefs::kNtpMicrosoftFilesModuleLastDismissedTime),
            base::Time());
}

TEST_F(MicrosoftFilesPageHandlerTestForFakeNonInsights,
       GetFakeNonInsightsFiles) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  handler().GetFiles(future.GetCallback());
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 6u);
}

// Verifies files are constructed correctly from a valid response for
// recently used and shared files.
TEST_F(MicrosoftFilesPageHandlerTestForNonInsights, GetNonInsightsFiles) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  std::string response = base::StringPrintf(
      R"({
  "responses" : [
    {
      "id": "recent",
      "status": "200",
      "body": {
        "value": [
        {
          "id": "0",
          "name": "Document 0.docx",
          "webUrl": "https://foo.com/Document0.docx",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "fileSystemInfo": {
            "lastAccessedDateTime": "%s"
          },
          "lastModifiedDateTime": "%s"
        },
        {
          "id": "1",
          "name": "Document 1.docx",
          "webUrl": "https://foo.com/Document1.docx",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "fileSystemInfo": {
            "lastAccessedDateTime": "%s"
          },
          "lastModifiedDateTime": "%s",
          "remoteItem": {
            "shared": {
              "sharedDateTime": "%s",
              "sharedBy": {
                "user": {
                  "displayName": "User 1"
                }
              }
            }
          }
        }
        ]
      }
    },
    {
      "id": "shared",
      "status": "200",
      "body": {
        "value": [
        {
          "id": "2",
          "name": "Document 2.docx",
          "webUrl": "https://foo.com/Document2.docx",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "lastModifiedDateTime": "%s",
          "remoteItem": {
            "shared": {
              "sharedDateTime": "%s",
              "sharedBy": {
                "user": {
                  "displayName": "User 1"
                }
              }
            }
          }
        },
        {
          "id": "3",
          "name": "Document 3.docx",
          "webUrl": "https://foo.com/Document3.docx",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "lastModifiedDateTime": "%s",
          "remoteItem": {
            "shared": {
              "sharedDateTime": "%s",
              "sharedBy": {
                "user": {
                  "displayName": "User 1"
                }
              }
            }
          }
        }
        ]
      }
    }
  ]
  })",
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString());

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 4u);
  for (size_t i = 0; i < suggestions.size(); i++) {
    auto& suggestion = suggestions[i];
    EXPECT_EQ(suggestion->id, base::NumberToString(i));
    EXPECT_EQ(suggestion->title, "Document " + base::NumberToString(i));
    EXPECT_EQ(suggestion->item_url,
              base::StringPrintf(kBaseWebUrl, base::NumberToString(i)));
    EXPECT_EQ(suggestion->icon_url, kDocIconUrl);
  }

  histogram_tester().ExpectBucketCount(
      kRequestResultHistogramName, MicrosoftFilesRequestResult::kSuccess, 1);
  histogram_tester().ExpectBucketCount(kResponseResultHistogramName, 4, 1);
}

TEST_F(MicrosoftFilesPageHandlerTestForNonInsights,
       NoNonInsightFilesOnMalformedResponse) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              "}{");
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 0u);
}

// Ensures files are still created if one of the endpoint's return
// an empty value list, but the other has suggestions.
TEST_F(MicrosoftFilesPageHandlerTestForNonInsights,
       NonInsightFilesCreatedOnEmptyValueResponse) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  std::string response = base::StringPrintf(
      R"({
  "responses" : [
    {
      "id": "recent",
      "body": {
        "value": [
        {
          "id": "1",
          "name": "Document 1.docx",
          "webUrl": "https://foo.com/document1.docx",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "fileSystemInfo": {
            "lastAccessedDateTime": "%s"
          },
          "lastModifiedDateTime": "%s"
        }
        ]
      }
    },
    {
      "id": "shared",
      "body": {
        "value": []
      }
    }
  ]
  })",
      GetTimeNowAsString(), GetTimeNowAsString());

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 1u);

  histogram_tester().ExpectBucketCount(
      kRequestResultHistogramName, MicrosoftFilesRequestResult::kSuccess, 1);
  histogram_tester().ExpectBucketCount(kResponseResultHistogramName, 1, 1);
}

// Microsoft Graph API recently used or shared files endpoints may return
// non-file types, such as a folder. Ensure only files are created.
TEST_F(MicrosoftFilesPageHandlerTestForNonInsights,
       NonInsightFileNotCreatedForNonFiles) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  // Missing `file.mimeType` property.
  std::string response =
      base::StringPrintf(R"({
  "responses" : [
    {
      "id": "recent",
      "body": {
        "value": [
        {
          "id": "1",
          "name": "Folder",
          "webUrl": "https://foo.com/folder",
          "fileSystemInfo": {
            "lastAccessedDateTime": "%s"
          },
          "lastModifiedDateTime": "%s"
        }
        ]
      }
    },
    {
      "id": "shared",
      "body": {
        "value": []
      }
    }
  ]
  })",
                         GetTimeNowAsString(), GetTimeNowAsString());

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 0u);

  histogram_tester().ExpectBucketCount(
      kRequestResultHistogramName, MicrosoftFilesRequestResult::kSuccess, 1);
  histogram_tester().ExpectBucketCount(kResponseResultHistogramName, 1, 1);
}

// Verifies files aren't created for recent suggestions if there is
// missing properties in the response.
TEST_F(MicrosoftFilesPageHandlerTestForNonInsights,
       RecentFileNotCreatedWithMissingData) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  // `webUrl` is missing.
  std::string response = base::StringPrintf(
      R"({
  "responses" : [
    {
      "id": "recent",
      "body": {
        "value": [
        {
          "id": "1",
          "name": "File 1",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "fileSystemInfo": {
            "lastAccessedDateTime": "%s"
          },
          "remoteItem": {
            "shared": {
              "sharedDateTime": "%s",
              "sharedBy": {
                "user": {
                  "displayName": "User 1"
                }
              }
            }
          }
        }
        ]
      }
    },
    {
      "id": "shared",
      "body": {
        "value": []
      }
    }
  ]
  })",
      GetTimeNowAsString(), GetTimeNowAsString());

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 0u);
  histogram_tester().ExpectBucketCount(kResponseResultHistogramName, 1, 1);
}

// Verifies files are not created for shared suggestions if there is
// data missing in the response.
TEST_F(MicrosoftFilesPageHandlerTestForNonInsights,
       SharedFileNotCreatedWithMissingData) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  // Missing shared properties.
  std::string response =
      R"({
    "responses": [
      {
        "id": "shared",
        "body": {
          "value": [
            {
              "id": "1",
              "name": "Document",
              "webUrl": "https://foo.com/Document.docx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "lastModifiedDateTime": "2024-01-07T11:13:00Z"
            }
          ]
        }
      },
      {
        "id": "recent",
        "body": {
          "value": []
        }
      }
    ]
  })";

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 0u);
  histogram_tester().ExpectBucketCount(kResponseResultHistogramName, 1, 1);
}

// Verifies that a "Retry-After" header is parsed and the earliest next retry
// time is persisted in prefs.
TEST_F(MicrosoftFilesPageHandlerTestForNonInsights, HandleThrottlingError) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  EXPECT_EQ(profile().GetPrefs()->GetTime(
                prefs::kNtpMicrosoftFilesModuleRetryAfterTime),
            base::Time());

  handler().GetFiles(future.GetCallback());

  auto head = network::CreateURLResponseHead(net::HTTP_TOO_MANY_REQUESTS);
  head->mime_type = "application/json";
  head->headers->AddHeader("Retry-After", "20");
  network::URLLoaderCompletionStatus status;
  std::string response = R"({
    "error": {
      "code": "TooManyRequests",
      "innerError": {
        "code": "429",
        "date": "2024-12-02T12:51:51",
        "message": "Please retry after",
        "request-id": "123-456-789-123-abcdefg",
        "status": "429"
      },
      "message": "Please retry again later."
    }})";

  test_url_loader_factory().AddResponse(GURL(kBatchRequestUrl), std::move(head),
                                        response, status);

  EXPECT_EQ(future.Get().size(), 0u);

  EXPECT_EQ(profile().GetPrefs()->GetTime(
                prefs::kNtpMicrosoftFilesModuleRetryAfterTime),
            base::Time::Now() + base::Seconds(20));

  histogram_tester().ExpectBucketCount(
      kRequestResultHistogramName,
      MicrosoftFilesRequestResult::kThrottlingError, 1);
  histogram_tester().ExpectTotalCount(kThrottlingTimeHistogramName, 1);
}

// Ensures requests aren't made until after the specified wait time when
// throttling occurs.
TEST_F(MicrosoftFilesPageHandlerTestForTrending, MakeRequestAfterRetryTimeout) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  profile().GetPrefs()->SetTime(prefs::kNtpMicrosoftFilesModuleRetryAfterTime,
                                base::Time::Now() + base::Seconds(10));

  handler().GetFiles(future.GetCallback());
  EXPECT_EQ(test_url_loader_factory().NumPending(), 0);
  EXPECT_EQ(future.Get().size(), 0u);

  future.Clear();

  // Fast forward past the retry time of 10 seconds.
  task_environment().FastForwardBy(base::Seconds(15));

  handler().GetFiles(future.GetCallback());
  EXPECT_EQ(test_url_loader_factory().NumPending(), 1);

  std::string response = R"({
    "value": [
        {
          "id": "1",
          "resourceVisualization": {
              "title": "Spreadsheet",
              "type": "Excel",
              "mediaType": "application/vnd.)"
                         R"(openxmlformats-officedocument.spreadsheetml.sheet"
          },
          "resourceReference": {
              "webUrl": "https://foo.com/Spreadsheet.xlsx",
              "id": "1-abc"
          }
        }
  ]})";

  test_url_loader_factory().SimulateResponseForPendingRequest(
      kTrendingFilesEndpoint, response);

  EXPECT_EQ(future.Get().size(), 1u);
}

// Ensures duplicate files are not found in the final file suggestion list.
TEST_F(MicrosoftFilesPageHandlerTestForNonInsights, RemoveDuplicates) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  handler().GetFiles(future.GetCallback());

  // Response includes duplicate for the file with id: "1"
  std::string response = base::StringPrintf(
      R"({
    "responses" : [
      {
        "id": "recent",
        "status": "200",
        "body": {
          "value": [
            {
              "id": "1",
              "name": "Document 1.docx",
              "webUrl": "https://foo.com/document1.docx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "fileSystemInfo": {
                "lastAccessedDateTime": "%s"
              },
              "remoteItem": {
                "shared": {
                  "sharedDateTime": "%s",
                  "sharedBy": {
                    "user": {
                      "displayName": "User 1"
                    }
                  }
                }
              },
              "lastModifiedDateTime": "%s"
            },
            {
              "id": "2",
              "name": "Presentation.pptx",
              "webUrl": "https://foo.com/presentation.pptx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.presentationml.presentation"
              },
              "fileSystemInfo": {
                "lastAccessedDateTime": "%s"
              },
              "lastModifiedDateTime": "%s"
            },
            {
              "id": "3",
              "name": "Document xyz.docx",
              "webUrl": "https://foo.com/documentxyz.docx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "fileSystemInfo": {
                "lastAccessedDateTime": "%s"
              },
              "lastModifiedDateTime": "%s"
            }
          ]
        }
      },
      {
        "id": "shared",
        "status": "200",
        "body": {
          "value": [
            {
              "id": "1",
              "name": "Document 1.docx",
              "webUrl": "https://foo.com/document1.xlsx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "lastModifiedDateTime": "%s",
              "remoteItem": {
                "shared": {
                  "sharedDateTime": "%s",
                  "sharedBy": {
                    "user": {
                      "displayName": "User 1"
                    }
                  }
                }
              }
            },
            {
              "id": "5",
              "name": "Shared Document.docx",
              "webUrl": "https://foo.com/document3.docx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "lastModifiedDateTime": "%s",
              "remoteItem": {
                "shared": {
                  "sharedDateTime": "%s",
                  "sharedBy": {
                    "user": {
                      "displayName": "User 2"
                    }
                  }
                }
              }
            },
            {
              "id": "6",
              "name": "Roadmap.pptx",
              "webUrl": "https://foo.com/roadmap.pptx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.presentationml.presentation"
              },
              "lastModifiedDateTime": "%s",
              "remoteItem": {
                "shared": {
                  "sharedDateTime": "%s",
                  "sharedBy": {
                    "user": {
                      "displayName": "User 1"
                    }
                  }
                }
              }
            }
          ]
        }
      }
    ]
  })",
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();
  EXPECT_EQ(suggestions.size(), 5u);
}

TEST_F(MicrosoftFilesPageHandlerTestForNonInsights,
       NoFilesOnUnauthorizedRequestCode) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  handler().GetFiles(future.GetCallback());

  auto head = network::CreateURLResponseHead(net::HTTP_UNAUTHORIZED);
  head->mime_type = "application/json";
  network::URLLoaderCompletionStatus status;
  test_url_loader_factory().AddResponse(GURL(kBatchRequestUrl), std::move(head),
                                        "", status);

  EXPECT_EQ(future.Get().size(), 0u);

  histogram_tester().ExpectBucketCount(
      kRequestResultHistogramName, MicrosoftFilesRequestResult::kAuthError, 1);
}

// Ensures that for the recent & shared experiment arm, the response should have
// a dictionary for each request.
TEST_F(MicrosoftFilesPageHandlerTestForNonInsights,
       NoFilesOnMissingResponseValue) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;

  // Missing the second dictionary for shared files.
  std::string response =
      R"({
    "responses" : [
    {
    "id": "recent",
    "body": {
      "value": [
      {
        "id": "1",
        "name": "Document 1.docx",
        "webUrl": "https://foo.com/document1.docx",
        "file": {
          "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
        },
        "fileSystemInfo": {
          "lastAccessedDateTime": "2024-01-07T19:13:00Z"
        },
        "lastModifiedDateTime": "2024-01-07T19:13:00Z"
      }
      ]
    }
    }]})";

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);

  EXPECT_EQ(future.Get().size(), 0u);

  histogram_tester().ExpectBucketCount(
      kRequestResultHistogramName, MicrosoftFilesRequestResult::kContentError,
      1);
}

TEST_F(MicrosoftFilesPageHandlerTestForNonInsights, JustificationText_Today) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;
  base::Time time_now = base::Time::Now();
  std::string time_now_str = TimeFormatAsIso8601(time_now);

  std::string response = base::StringPrintf(
      R"({
    "responses" : [
      {
        "id": "recent",
        "body": {
          "value": [
          {
            "id": "1",
            "name": "Document 1.docx",
            "webUrl": "https://foo.com/document1.docx",
            "file": {
              "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
            },
            "fileSystemInfo": {
              "lastAccessedDateTime": "%s"
            },
            "lastModifiedDateTime": "%s"
          }
          ]
        }
      },
      {
        "id": "shared",
        "body": {
          "value": []
        }
      }
    ]
    })",
      time_now_str, time_now_str);

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 1u);

  EXPECT_EQ(suggestions[0]->justification_text, "You opened today");
}

TEST_F(MicrosoftFilesPageHandlerTestForNonInsights,
       JustificationText_Yesterday) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;
  base::Time time_yesterday = base::Time::Now() - base::Days(1);

  std::string time_yesterday_str = TimeFormatAsIso8601(time_yesterday);

  std::string response = base::StringPrintf(
      R"({
    "responses" : [
      {
        "id": "recent",
        "body": {
          "value": [
          {
            "id": "1",
            "name": "Document 1.docx",
            "webUrl": "https://foo.com/document1.docx",
            "file": {
              "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
            },
            "fileSystemInfo": {
              "lastAccessedDateTime": "%s"
            },
            "lastModifiedDateTime": "%s"
          }
          ]
        }
      },
      {
        "id": "shared",
        "body": {
          "value": []
        }
      }
    ]
    })",
      time_yesterday_str, time_yesterday_str);

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 1u);

  EXPECT_EQ(suggestions[0]->justification_text, "You opened yesterday");
}

TEST_F(MicrosoftFilesPageHandlerTestForNonInsights,
       JustificationText_7DaysAgo) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;
  base::Time time_last_week = base::Time::Now() - base::Days(7);

  std::string time_last_week_str = TimeFormatAsIso8601(time_last_week);

  std::string response = base::StringPrintf(
      R"({
    "responses" : [
      {
        "id": "recent",
        "body": {
          "value": [
          {
            "id": "1",
            "name": "Document 1.docx",
            "webUrl": "https://foo.com/document1.docx",
            "file": {
              "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
            },
            "fileSystemInfo": {
              "lastAccessedDateTime": "%s"
            },
            "lastModifiedDateTime": "%s"
          }
          ]
        }
      },
      {
        "id": "shared",
        "body": {
          "value": []
        }
      }
    ]
    })",
      time_last_week_str, time_last_week_str);

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 1u);

  EXPECT_EQ(suggestions[0]->justification_text, "You opened in the past week");
}

// Ensures files accessed more than a week ago do not get added to the files
// list.
TEST_F(MicrosoftFilesPageHandlerTestForNonInsights, FilterOlderFiles) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;
  base::Time time_last_week = base::Time::Now() - base::Days(8);

  std::string time_last_week_str = TimeFormatAsIso8601(time_last_week);

  std::string response = base::StringPrintf(
      R"({
    "responses" : [
    {
      "id": "recent",
      "body": {
        "value": [
        {
          "id": "1",
          "name": "Document 1.docx",
          "webUrl": "https://foo.com/document1.docx",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "fileSystemInfo": {
            "lastAccessedDateTime": "%s"
          },
          "lastModifiedDateTime": "%s"
        }
        ]
      }
    },
    {
      "id": "shared",
      "body": {
        "value": []
      }
    }
    ]
    })",
      time_last_week_str, time_last_week_str);

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 0u);
}

TEST_F(MicrosoftFilesPageHandlerTestForNonInsights, SkipFileWithMissingFields) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;
  // `webUrl` is missing for the first recent file.
  std::string response = base::StringPrintf(
      R"({
  "responses" : [
    {
      "id": "recent",
      "body": {
        "value": [
        {
          "id": "1",
          "name": "File 1",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "fileSystemInfo": {
            "lastAccessedDateTime": "%s"
          },
          "remoteItem": {
            "shared": {
              "sharedDateTime": "%s",
              "sharedBy": {
                "user": {
                  "displayName": "User 1"
                }
              }
            }
          }
        },
        {
          "id": "2",
          "webUrl": "https://www.url.com",
          "name": "File 1",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "fileSystemInfo": {
            "lastAccessedDateTime": "%s"
          },
          "remoteItem": {
            "shared": {
              "sharedDateTime": "%s",
              "sharedBy": {
                "user": {
                  "displayName": "User 1"
                }
              }
            }
          }
        }
        ]
      }
    },
    {
      "id": "shared",
      "body": {
        "value": []
      }
    }
  ]
  })",
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString());

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  // Only file suggestions with all necessary fields should be created.
  EXPECT_EQ(suggestions.size(), 1u);
}

TEST_F(MicrosoftFilesPageHandlerTestForCombinedSuggestions,
       GetCombinedSuggestions) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;
  std::string response = base::StringPrintf(
      R"({
    "responses": [
      {
        "id": "recent",
        "status": "200",
        "body": {
          "value": [
            {
              "id": "1",
              "name": "Document 1.docx",
              "webUrl": "https://foo.com/document1.docx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "fileSystemInfo": {
                "lastAccessedDateTime": "%s"
              },
              "lastModifiedDateTime": "%s"
            },
            {
              "id": "2",
              "name": "Presentation.pptx",
              "webUrl": "https://foo.com/presentation.pptx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.presentationml.presentation"
              },
              "fileSystemInfo": {
                "lastAccessedDateTime": "%s"
              },
              "lastModifiedDateTime": "%s"
            },
            {
              "id": "3",
              "name": "Document xyz.docx",
              "webUrl": "https://foo.com/documentxyz.docx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "fileSystemInfo": {
                "lastAccessedDateTime": "%s"
              },
              "lastModifiedDateTime": "%s"
            }
          ]
        }
      },
      {
        "id": "shared",
        "status": "200",
        "body": {
          "value": [
            {
              "id": "4",
              "name": "Shared Spreadsheet.xlsx",
              "webUrl": "https://foo.com/SharedSpreadsheet.xlsx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.spreadsheetml.sheet"
              },
              "lastModifiedDateTime": "%s",
              "remoteItem": {
                "shared": {
                  "sharedDateTime": "%s",
                  "sharedBy": {
                    "user": {
                      "displayName": "User 1"
                    }
                  }
                }
              }
            },
            {
              "id": "5",
              "name": "Shared Document.docx",
              "webUrl": "https://foo.com/document3.docx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "lastModifiedDateTime": "%s",
              "remoteItem": {
                "shared": {
                  "sharedDateTime": "%s",
                  "sharedBy": {
                    "user": {
                      "displayName": "User 2"
                    }
                  }
                }
              }
            },
            {
              "id": "6",
              "name": "Roadmap.pptx",
              "webUrl": "https://foo.com/roadmap.pptx",
              "file": {
                "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.presentationml.presentation"
              },
              "lastModifiedDateTime": "%s",
              "remoteItem": {
                "shared": {
                  "sharedDateTime": "%s",
                  "sharedBy": {
                    "user": {
                      "displayName": "User 1"
                    }
                  }
                }
              }
            }
          ]
        }
      },
      {
        "id": "trending",
        "status": "200",
        "body": {
          "value": [
            {
              "id": "50",
              "resourceVisualization": {
                  "id": "0-abc",
                  "title": "Trending 1",
                  "type": "Word",
                "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "resourceReference": {
                  "webUrl": "https://foo.com/Trending1.docx",
                  "id": "0-xyz"
              }
            },
            {
              "id": "51",
              "resourceVisualization": {
                  "id": "0-abc",
                  "title": "Trending 2",
                  "type": "Word",
                "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "resourceReference": {
                  "webUrl": "https://foo.com/Trending2.docx",
                  "id": "1-xyz"
              }
            },
            {
              "id": "52",
              "resourceVisualization": {
                  "id": "0-abc",
                  "title": "Trending 3",
                  "type": "Word",
                "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "resourceReference": {
                  "webUrl": "https://foo.com/Trending3.docx",
                  "id": "2-xyz"
              }
            },
            {
              "id": "53",
              "resourceVisualization": {
                  "id": "0-abc",
                  "title": "Trending 4",
                  "type": "Word",
                "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "resourceReference": {
                  "webUrl": "https://foo.com/Trending4.docx",
                  "id": "3-xyz"
              }
            },
            {
              "id": "54",
              "resourceVisualization": {
                  "id": "0-abc",
                  "title": "Trending 5",
                  "type": "Word",
                "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
              },
              "resourceReference": {
                  "webUrl": "https://foo.com/Trending5.docx",
                  "id": "4-xyz"
              }
            }
          ]
        }

      }
    ]
    })",
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString());

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 6u);

  // The last 2 suggestions are trending files.
  EXPECT_EQ(suggestions[4]->title, "Trending 1");
  EXPECT_EQ(suggestions[5]->title, "Trending 2");

  histogram_tester().ExpectBucketCount(
      kRequestResultHistogramName, MicrosoftFilesRequestResult::kSuccess, 1);
  histogram_tester().ExpectBucketCount(kResponseResultHistogramName, 11, 1);
  histogram_tester().ExpectBucketCount(
      kSubstitutionTypeHistogramName, MicrosoftFilesSubstitutionType::kNone, 1);
}

// Ensures that when non-insight files do not fill up the card based on their
// allotted amount, trending files will be used to fill the card.
TEST_F(MicrosoftFilesPageHandlerTestForCombinedSuggestions,
       TrendingAddedOnInsufficientNonInsightFiles) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;
  // 3 non-insight files and 4 trending are found in `response.`
  std::string response = base::StringPrintf(
      R"({
  "responses": [
    {
      "id": "recent",
      "status": "200",
      "body": {
        "value": [
          {
            "id": "1",
            "name": "Document 1.docx",
            "webUrl": "https://foo.com/document1.docx",
            "file": {
              "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
            },
            "fileSystemInfo": {
              "lastAccessedDateTime": "%s"
            },
            "lastModifiedDateTime": "%s"
          },
          {
            "id": "2",
            "name": "Presentation.pptx",
            "webUrl": "https://foo.com/presentation.pptx",
            "file": {
              "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.presentationml.presentation"
            },
            "fileSystemInfo": {
              "lastAccessedDateTime": "%s"
            },
            "lastModifiedDateTime": "%s"
          }
        ]
      }
    },
    {
      "id": "shared",
      "status": "200",
      "body": {
        "value": [
          {
            "id": "4",
            "name": "Shared Spreadsheet.xlsx",
            "webUrl": "https://foo.com/SharedSpreadsheet.xlsx",
            "file": {
              "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.spreadsheetml.sheet"
            },
            "lastModifiedDateTime": "%s",
            "remoteItem": {
              "shared": {
                "sharedDateTime": "%s",
                "sharedBy": {
                  "user": {
                    "displayName": "User 1"
                  }
                }
              }
            }
          }
        ]
      }
    },
    {
      "id": "trending",
      "status": "200",
      "body": {
        "value": [
          {
            "id": "50",
            "resourceVisualization": {
                "id": "0-abc",
                "title": "Document 0",
                "type": "Word",
              "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
            },
            "resourceReference": {
                "webUrl": "https://foo.com/Document0.docx",
                "id": "0-xyz"
            }
          },
          {
            "id": "51",
            "resourceVisualization": {
                "id": "0-abc",
                "title": "Document 1",
                "type": "Word",
              "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
            },
            "resourceReference": {
                "webUrl": "https://foo.com/Document1.docx",
                "id": "1-xyz"
            }
          },
          {
            "id": "52",
            "resourceVisualization": {
                "id": "0-abc",
                "title": "Document 2",
                "type": "Word",
              "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
            },
            "resourceReference": {
                "webUrl": "https://foo.com/Document2.docx",
                "id": "2-xyz"
            }
          },
          {
            "id": "53",
            "resourceVisualization": {
                "id": "0-abc",
                "title": "Document 3",
                "type": "Word",
              "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
            },
            "resourceReference": {
                "webUrl": "https://foo.com/Document3.docx",
                "id": "3-xyz"
            }
          }
        ]
      }

    }
  ]
  })",
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString());

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 6u);
  histogram_tester().ExpectBucketCount(
      kRequestResultHistogramName, MicrosoftFilesRequestResult::kSuccess, 1);
  histogram_tester().ExpectBucketCount(kResponseResultHistogramName, 7, 1);
  histogram_tester().ExpectBucketCount(
      kSubstitutionTypeHistogramName,
      MicrosoftFilesSubstitutionType::kExtraTrending, 1);
}

// Ensures that when trending files do not fill up the card based on their
// allotted amount, non-insight files will be used to fill the card.
TEST_F(MicrosoftFilesPageHandlerTestForCombinedSuggestions,
       NonInsightFilesAddedOnInsufficientTrendingFiles) {
  base::test::TestFuture<std::vector<file_suggestion::mojom::FilePtr>> future;
  // 5 non-insight files and 1 trending are found in `response.`
  std::string response = base::StringPrintf(
      R"({
    "responses": [
    {
    "id": "recent",
    "status": "200",
    "body": {
      "value": [
        {
          "id": "1",
          "name": "Document 1.docx",
          "webUrl": "https://foo.com/document1.docx",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "fileSystemInfo": {
            "lastAccessedDateTime": "%s"
          },
          "lastModifiedDateTime": "%s"
        },
        {
          "id": "2",
          "name": "Presentation.pptx",
          "webUrl": "https://foo.com/presentation.pptx",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.presentationml.presentation"
          },
          "fileSystemInfo": {
            "lastAccessedDateTime": "%s"
          },
          "lastModifiedDateTime": "%s"
        },
        {
          "id": "3",
          "name": "Presentation 3.pptx",
          "webUrl": "https://foo.com/presentation3.pptx",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.presentationml.presentation"
          },
          "fileSystemInfo": {
            "lastAccessedDateTime": "%s"
          },
          "lastModifiedDateTime": "%s"
        }
      ]
    }
    },
    {
    "id": "shared",
    "status": "200",
    "body": {
      "value": [
        {
          "id": "4",
          "name": "Shared Spreadsheet.xlsx",
          "webUrl": "https://foo.com/SharedSpreadsheet.xlsx",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.spreadsheetml.sheet"
          },
          "lastModifiedDateTime": "%s",
          "remoteItem": {
            "shared": {
              "sharedDateTime": "%s",
              "sharedBy": {
                "user": {
                  "displayName": "User 1"
                }
              }
            }
          }
        },
        {
          "id": "5",
          "name": "Spreadsheet.xlsx",
          "webUrl": "https://foo.com/Spreadsheet.xlsx",
          "file": {
            "mimeType": "application/vnd.)"
      R"(openxmlformats-officedocument.spreadsheetml.sheet"
          },
          "lastModifiedDateTime": "%s",
          "remoteItem": {
            "shared": {
              "sharedDateTime": "%s",
              "sharedBy": {
                "user": {
                  "displayName": "User 1"
                }
              }
            }
          }
        }
      ]
    }
    },
    {
    "id": "trending",
    "status": "200",
    "body": {
      "value": [
        {
          "id": "50",
          "resourceVisualization": {
              "id": "0-abc",
              "title": "Trending",
              "type": "Word",
            "mediaType": "application/vnd.)"
      R"(openxmlformats-officedocument.wordprocessingml.document"
          },
          "resourceReference": {
              "webUrl": "https://foo.com/trending.docx",
              "id": "0-xyz"
          }
        }
      ]
    }

    }
    ]
    })",
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString(), GetTimeNowAsString(), GetTimeNowAsString(),
      GetTimeNowAsString());

  handler().GetFiles(future.GetCallback());

  test_url_loader_factory().SimulateResponseForPendingRequest(kBatchRequestUrl,
                                                              response);
  const std::vector<file_suggestion::mojom::FilePtr>& suggestions =
      future.Get();

  EXPECT_EQ(suggestions.size(), 6u);
  // Ensure the trending file is the last suggestion.
  EXPECT_EQ(suggestions[5]->title, "Trending");
  histogram_tester().ExpectBucketCount(
      kRequestResultHistogramName, MicrosoftFilesRequestResult::kSuccess, 1);
  histogram_tester().ExpectBucketCount(kResponseResultHistogramName, 6, 1);
  histogram_tester().ExpectBucketCount(
      kSubstitutionTypeHistogramName,
      MicrosoftFilesSubstitutionType::kExtraNonInsights, 1);
}
