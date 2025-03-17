// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files_page_handler.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/microsoft_modules_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/search/ntp_features.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kTrendingFilesEndpoint[] =
    "https://graph.microsoft.com/v1.0/me/insights/trending";

constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("microsoft_files_page_handler", R"(
        semantics {
          sender: "Microsoft Files Page Handler"
          description:
            "The Microsoft Files Page Handler requests relevant "
            "user file suggestions from the Microsoft Graph API. "
            "The response will be used to display suggestions on "
            "the desktop NTP."
          trigger:
            "Each time a signed-in user navigates to the NTP while "
            "the Microsoft files module is enabled and the user's "
            "Microsoft account has been authenticated on the NTP."
          user_data {
            type: ACCESS_TOKEN
          }
          data: "OAuth2 access token identifying the Microsoft account."
          destination: OTHER
          destination_other: "Microsoft Graph API"
          internal {
            contacts {
              email: "chrome-desktop-ntp@google.com"
            }
          }
          last_reviewed: "2025-1-16"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature by (1) selecting "
            "a non-Google default search engine in Chrome "
            "settings under 'Search Engine', (2) signing out, "
            "(3) disabling the Microsoft files module or (4) "
            "disabling the Microsoft authentication module."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
            NTPCardsVisible {
              NTPCardsVisible: false
            }
            NTPSharepointCardVisible {
              NTPSharepointCardVisible: false
            }
          }
        })");

const int kMaxResponseSize = 1024 * 1024;

const int kNumberOfDaysPerWeek = 7;

const char kFakeTrendingData[] =
    R"({
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
            "webUrl": "https://foo.com/sites/SiteName/Shared/Spreadsheet.xlsx",
            "id": "1-abc"
        }
      },
      {
        "id": "2",
        "resourceVisualization": {
            "title": "Ppt",
            "type": "PowerPoint",
            "mediaType": "application/vnd.)"
    R"(openxmlformats-officedocument.presentationml.presentation"
        },
        "resourceReference": {
            "webUrl": "https://foo.com/sites/SiteName/Shared/Powerpoint.ppt",
            "id": "2-abc"
        }
      },
      {
        "id": "3",
        "resourceVisualization": {
            "title": "Document 2",
            "type": "Word",
            "mediaType": "application/vnd.)"
    R"(openxmlformats-officedocument.wordprocessingml.document"
        },
        "resourceReference": {
            "webUrl": "https://foo.com/sites/SiteName/Shared/Document2.docx",
            "id": "3-abc"
        }
      },
      {
        "id": "4",
        "resourceVisualization": {
            "title": "Numbers",
            "type": "Csv",
            "mediaType": "text/csv"
        },
        "resourceReference": {
            "webUrl": "https://foo.com/sites/SiteName/Shared/numbers.csv",
            "id": "4-abc"
        }
      },
      {
        "id": "5",
        "resourceVisualization": {
            "title": "Some pdf",
            "type": "Pdf",
            "mediaType": "application/pdf"
        },
        "resourceReference": {
            "webUrl": "https://foo.com/sites/SiteName/Shared/Some-pdf.pdf",
            "id": "5-abc"
        }
      }
  ]})";

constexpr base::TimeDelta kModuleDismissalDuration = base::Hours(12);

const char kNonInsightsRequestUrl[] = "https://graph.microsoft.com/v1.0/$batch";
const char kNonInsightsRequestBody[] = R"({
  "requests": [
  {
    "id": "recent",
    "method": "GET",
    "url": "/me/drive/recent?orderby=fileSystemInfo/lastAccessedDateTime+desc"
  },
  {
    "id": "shared",
    "method": "GET",
    "url": "/me/drive/sharedWithMe"
  }]})";

const char kNonInsightsFakeData[] =
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
    }
  ]
})";


std::string GetTimeNowAsString() {
  return TimeFormatAsIso8601(base::Time::Now());
}

// Emits the total number of Microsoft drive items found in the response. Note:
// The Microsoft Graph API by default returns a max of 100 files per endpoint.
// For the recent & shared files experiment arm, 2 endpoints are being used, so
// the max files returned may be 200.
void RecordResponseValueCount(int count) {
  base::UmaHistogramCustomCounts(
      /*name=*/"NewTabPage.MicrosoftFiles.ResponseResult",
      /*sample=*/count, /*min=*/1, /*exclusive_max=*/201, /*buckets=*/50);
}

// Emits the result of the request for files.
void RecordFilesRequestResult(MicrosoftFilesRequestResult result) {
  base::UmaHistogramEnumeration("NewTabPage.MicrosoftFiles.RequestResult",
                                result);
}

// Emits the time in seconds that should be waited before attempting another
// request.
void RecordThrottlingWaitTime(base::TimeDelta seconds) {
  base::UmaHistogramTimes("NewTabPage.MicrosoftFiles.ThrottlingWaitTime",
                          seconds);
}

}  // namespace

// static
void MicrosoftFilesPageHandler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kNtpMicrosoftFilesModuleLastDismissedTime,
                             base::Time());
  registry->RegisterTimePref(prefs::kNtpMicrosoftFilesModuleRetryAfterTime,
                             base::Time());
}

MicrosoftFilesPageHandler::MicrosoftFilesPageHandler(
    mojo::PendingReceiver<file_suggestion::mojom::MicrosoftFilesPageHandler>
        handler,
    Profile* profile)
    : handler_(this, std::move(handler)),
      microsoft_auth_service_(
          MicrosoftAuthServiceFactory::GetForProfile(profile)),
      pref_service_(profile->GetPrefs()),
      url_loader_factory_(profile->GetURLLoaderFactory()) {}

MicrosoftFilesPageHandler::~MicrosoftFilesPageHandler() = default;

void MicrosoftFilesPageHandler::GetFiles(GetFilesCallback callback) {
  // Return empty list of files if the module was recently dismissed.
  base::Time last_dismissed_time =
      pref_service_->GetTime(prefs::kNtpMicrosoftFilesModuleLastDismissedTime);
  if (last_dismissed_time != base::Time() &&
      base::Time::Now() - last_dismissed_time < kModuleDismissalDuration) {
    std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    return;
  }

  bool param_is_trending =
      ntp_features::kNtpSharepointModuleDataParam.Get() ==
      ntp_features::NtpSharepointModuleDataType::kTrendingInsights;
  bool param_is_non_insights =
      ntp_features::kNtpSharepointModuleDataParam.Get() ==
      ntp_features::NtpSharepointModuleDataType::kNonInsights;

  // Ensure requests aren't made when a throttling error must be waited out.
  base::Time retry_after_time =
      pref_service_->GetTime(prefs::kNtpMicrosoftFilesModuleRetryAfterTime);
  if ((param_is_trending || param_is_non_insights) &&
      (retry_after_time != base::Time() &&
       base::Time::Now() < retry_after_time)) {
    std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    return;
  }

  if (param_is_trending) {
    GetTrendingFiles(std::move(callback));
  } else if (param_is_non_insights) {
    GetRecentlyUsedAndSharedFiles(std::move(callback));
  } else {
    // Parse data immediately when displaying fake data.
    const std::string fake_data =
        ntp_features::kNtpSharepointModuleDataParam.Get() ==
                ntp_features::NtpSharepointModuleDataType::
                    kTrendingInsightsFakeData
            ? kFakeTrendingData
            : base::StringPrintf(kNonInsightsFakeData, GetTimeNowAsString(),
                                 GetTimeNowAsString(), GetTimeNowAsString(),
                                 GetTimeNowAsString(), GetTimeNowAsString(),
                                 GetTimeNowAsString(), GetTimeNowAsString(),
                                 GetTimeNowAsString(), GetTimeNowAsString(),
                                 GetTimeNowAsString(), GetTimeNowAsString(),
                                 GetTimeNowAsString());
    data_decoder::DataDecoder::ParseJsonIsolated(
        fake_data,
        base::BindOnce(&MicrosoftFilesPageHandler::OnJsonParsed,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void MicrosoftFilesPageHandler::DismissModule() {
  pref_service_->SetTime(prefs::kNtpMicrosoftFilesModuleLastDismissedTime,
                         base::Time::Now());
}

void MicrosoftFilesPageHandler::RestoreModule() {
  pref_service_->SetTime(prefs::kNtpMicrosoftFilesModuleLastDismissedTime,
                         base::Time());
}

void MicrosoftFilesPageHandler::GetTrendingFiles(GetFilesCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = GURL(kTrendingFilesEndpoint);
  const std::string access_token = microsoft_auth_service_->GetAccessToken();
  const std::string auth_header_value = "Bearer " + access_token;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      auth_header_value);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kCacheControl,
                                      "no-cache");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&MicrosoftFilesPageHandler::OnJsonReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      kMaxResponseSize);
}

void MicrosoftFilesPageHandler::GetRecentlyUsedAndSharedFiles(
    GetFilesCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = GURL(kNonInsightsRequestUrl);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kCacheControl,
                                      "no-cache");
  const std::string access_token = microsoft_auth_service_->GetAccessToken();
  const std::string auth_header_value = "Bearer " + access_token;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      auth_header_value);

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(kNonInsightsRequestBody);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&MicrosoftFilesPageHandler::OnJsonReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      kMaxResponseSize);
}

void MicrosoftFilesPageHandler::OnJsonReceived(
    GetFilesCallback callback,
    std::unique_ptr<std::string> response_body) {
  const int net_error = url_loader_->NetError();
  MicrosoftFilesRequestResult request_result =
      MicrosoftFilesRequestResult::kNetworkError;

  // Check for unauthorized and throttling errors.
  auto* response_info = url_loader_->ResponseInfo();
  if (net_error != net::OK && response_info && response_info->headers) {
    int64_t wait_time =
        response_info->headers->GetInt64HeaderValue("Retry-After");
    if (wait_time != -1) {
      request_result = MicrosoftFilesRequestResult::kThrottlingError;
      RecordThrottlingWaitTime(base::Seconds(wait_time));
      pref_service_->SetTime(prefs::kNtpMicrosoftFilesModuleRetryAfterTime,
                             base::Time::Now() + base::Seconds(wait_time));
    } else if (response_info->headers->response_code() ==
               net::HTTP_UNAUTHORIZED) {
      request_result = MicrosoftFilesRequestResult::kAuthError;
      microsoft_auth_service_->SetAuthStateError();
    }
  }

  url_loader_.reset();

  if (net_error == net::OK && response_body) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        *response_body,
        base::BindOnce(&MicrosoftFilesPageHandler::OnJsonParsed,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    RecordFilesRequestResult(request_result);
    std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
  }
}

void MicrosoftFilesPageHandler::OnJsonParsed(
    GetFilesCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    RecordFilesRequestResult(MicrosoftFilesRequestResult::kJsonParseError);
    std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    return;
  }

  bool is_trending_data =
      ntp_features::kNtpSharepointModuleDataParam.Get() ==
          ntp_features::NtpSharepointModuleDataType::kTrendingInsights ||
      ntp_features::kNtpSharepointModuleDataParam.Get() ==
          ntp_features::NtpSharepointModuleDataType::kTrendingInsightsFakeData;
  if (is_trending_data) {
    CreateTrendingFiles(std::move(callback), std::move(result->GetDict()));
  } else {
    CreateRecentlyUsedAndSharedFiles(std::move(callback),
                                     std::move(result->GetDict()));
  }
}

void MicrosoftFilesPageHandler::CreateTrendingFiles(GetFilesCallback callback,
                                                    base::Value::Dict result) {
  auto* suggestions = result.FindList("value");
  if (!suggestions) {
    RecordFilesRequestResult(MicrosoftFilesRequestResult::kContentError);
    std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    return;
  }

  RecordResponseValueCount(suggestions->size());

  std::vector<file_suggestion::mojom::FilePtr> created_suggestions;
  const size_t num_max_files =
      ntp_features::kNtpMicrosoftFilesModuleMaxFilesParam.Get();
  for (const auto& suggestion : *suggestions) {
    if (created_suggestions.size() == num_max_files) {
      break;
    }
    const auto& suggestion_dict = suggestion.GetDict();
    const std::string* id = suggestion_dict.FindString("id");
    const std::string* title =
        suggestion_dict.FindStringByDottedPath("resourceVisualization.title");
    const std::string* url =
        suggestion_dict.FindStringByDottedPath("resourceReference.webUrl");
    const std::string* mime_type = suggestion_dict.FindStringByDottedPath(
        "resourceVisualization.mediaType");

    if (!id || !title || !url || !mime_type) {
      RecordFilesRequestResult(MicrosoftFilesRequestResult::kContentError);
      std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
      return;
    }

    std::string file_extension =
        microsoft_modules_helper::GetFileExtension(*mime_type);
    // Skip creating file suggestion if there's an error mapping the mime-type
    // to an extension as the extension is needed for the file's `icon_url.`
    if (file_extension.empty()) {
      continue;
    }

    file_suggestion::mojom::FilePtr created_file =
        file_suggestion::mojom::File::New();
    created_file->id = *id;
    created_file->justification_text = l10n_util::GetStringUTF8(
        IDS_NTP_MODULES_MICROSOFT_FILES_TRENDING_JUSTIFICATION_TEXT);
    GURL icon_url = microsoft_modules_helper::GetFileIconUrl(*mime_type);
    if (!icon_url.is_valid()) {
      continue;
    }
    created_file->icon_url = icon_url;
    created_file->title = *title;
    created_file->item_url = GURL(*url);
    created_suggestions.push_back(std::move(created_file));
  }

  RecordFilesRequestResult(MicrosoftFilesRequestResult::kSuccess);
  std::move(callback).Run(std::move(created_suggestions));
}

void MicrosoftFilesPageHandler::CreateRecentlyUsedAndSharedFiles(
    GetFilesCallback callback,
    base::Value::Dict result) {
  auto* responses = result.FindList("responses");
  if (!responses) {
    RecordFilesRequestResult(MicrosoftFilesRequestResult::kContentError);
    std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    return;
  }

  // The response body should contain a list that has 2 dictionaries - one for
  // each request, with their own lists containing file data.
  if (responses->size() != 2) {
    RecordFilesRequestResult(MicrosoftFilesRequestResult::kContentError);
    std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    return;
  } else {
    const base::Value::List* first_response =
        (*responses)[0].GetDict().FindListByDottedPath("body.value");
    const base::Value::List* second_response =
        (*responses)[1].GetDict().FindListByDottedPath("body.value");
    if (first_response && second_response) {
      const int total_count = first_response->size() + second_response->size();
      RecordResponseValueCount(total_count);
    }
  }

  std::vector<std::pair<base::Time, file_suggestion::mojom::FilePtr>>
      unsorted_suggestions;
  // The response body should contain a value list for each request.
  for (const auto& response : *responses) {
    const auto& response_dict = response.GetDict();
    const std::string* response_id = response_dict.FindString("id");
    auto* suggestions = response_dict.FindListByDottedPath("body.value");

    if (!suggestions) {
      RecordFilesRequestResult(MicrosoftFilesRequestResult::kContentError);
      std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
      return;
    }
    int num_recent_suggestions = 0;
    for (const auto& suggestion : *suggestions) {
      // Only allow a couple suggestions from the recent endpoint as the
      // response sends the files ordered by the
      // `fileSystemInfo.lastAccessedTime` in descending order. All shared
      // suggestions should be added because there isn't a great way to request
      // for the files to be ordered by the shared date. The number of recent
      // suggestions is limited to avoid having to sort more files than needed
      // in `SortAndRemoveDuplicates`.
      if (*response_id == "recent" &&
          (num_recent_suggestions ==
           ntp_features::kNtpMicrosoftFilesModuleMaxFilesParam.Get())) {
        break;
      }

      const auto& suggestion_dict = suggestion.GetDict();
      const std::string* id = suggestion_dict.FindString("id");
      const std::string* title = suggestion_dict.FindString("name");
      const std::string* item_url = suggestion_dict.FindString("webUrl");
      const std::string* mime_type =
          suggestion_dict.FindStringByDottedPath("file.mimeType");
      const std::string* last_opened_time_str =
          suggestion_dict.FindStringByDottedPath(
              "fileSystemInfo.lastAccessedDateTime");
      const std::string* last_modified_time_str =
          suggestion_dict.FindString("lastModifiedDateTime");
      const std::string* shared_by = suggestion_dict.FindStringByDottedPath(
          "remoteItem.shared.sharedBy.user.displayName");
      const std::string* shared_time_str =
          suggestion_dict.FindStringByDottedPath(
              "remoteItem.shared.sharedDateTime");

      // There may be some suggestions that are not files (the file property
      // will be null), so skip those.
      if (!mime_type) {
        continue;
      }

      // Time used to sort the file suggestions. Files with more recent time
      // values will be ranked higher when displayed.
      base::Time sort_time;

      // `fileSystemInfo.lastAccessedTime` should be available for suggestions
      // from recent files. Shared files should not have null `shared`
      // properties.
      bool suggestion_has_formatted_time =
          *response_id == "recent"
              ? last_opened_time_str &&
                    base::Time::FromUTCString(last_opened_time_str->c_str(),
                                              &sort_time)
              : shared_by && shared_time_str &&
                    base::Time::FromUTCString(shared_time_str->c_str(),
                                              &sort_time);
      if (!id || !title || !item_url || !last_modified_time_str ||
          !suggestion_has_formatted_time) {
        RecordFilesRequestResult(MicrosoftFilesRequestResult::kContentError);
        std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
        return;
      }

      std::string file_extension =
          microsoft_modules_helper::GetFileExtension(*mime_type);
      // Skip creating file suggestion if there's an error mapping the mime-type
      // to an extension as the extension is needed for the file's `icon_url.`
      if (file_extension.empty()) {
        continue;
      }

      // Skip any recent files that were opened more than a week ago. It's safe
      // to assume any other file that comes after this one has a greater time
      // difference because the Microsoft Graph response is sorted in descending
      // order.
      base::TimeDelta time_difference =
          base::Time::Now().LocalMidnight() - sort_time.LocalMidnight();
      if (*response_id == "recent" &&
          time_difference.InDays() > kNumberOfDaysPerWeek) {
        break;
      }

      file_suggestion::mojom::FilePtr created_file =
          file_suggestion::mojom::File::New();
      created_file->id = *id;
      created_file->justification_text =
          *response_id == "shared"
              ? CreateJustificationTextForSharedFile(*shared_by)
              : CreateJustificationTextForRecentFile(sort_time);
      GURL icon_url = microsoft_modules_helper::GetFileIconUrl(*mime_type);
      if (!icon_url.is_valid()) {
        continue;
      }
      created_file->icon_url = icon_url;
      created_file->title =
          microsoft_modules_helper::GetFileName(*title, file_extension);
      created_file->item_url = GURL(*item_url);
      if (*response_id == "recent") {
        num_recent_suggestions++;
      }
      unsorted_suggestions.emplace_back(sort_time, std::move(created_file));
    }
  }

  std::vector<file_suggestion::mojom::FilePtr> sorted_suggestions =
      SortAndRemoveDuplicates(std::move(unsorted_suggestions));

  RecordFilesRequestResult(MicrosoftFilesRequestResult::kSuccess);
  std::move(callback).Run(std::move(sorted_suggestions));
}

std::vector<file_suggestion::mojom::FilePtr>
MicrosoftFilesPageHandler::SortAndRemoveDuplicates(
    std::vector<std::pair<base::Time, file_suggestion::mojom::FilePtr>>
        suggestions) {
  // Sort the suggestions in descending order based on 1) for recent files - the
  // last time the file was accessed by the user 2) for shared files - the time
  // the file was shared with the user.
  std::ranges::stable_sort(
      suggestions, std::greater<base::Time>{},
      [&](const auto& suggestion) { return suggestion.first; });

  std::vector<file_suggestion::mojom::FilePtr> final_suggestions;

  const size_t num_max_files =
      ntp_features::kNtpMicrosoftFilesModuleMaxFilesParam.Get();
  for (const auto& suggestion : suggestions) {
    if (final_suggestions.size() == num_max_files) {
      break;
    }
    // Ensure duplicates are not added to the final file list.
    bool is_duplicate = false;
    for (const auto& file : final_suggestions) {
      if (suggestion.second->id == file->id) {
        is_duplicate = true;
        break;
      }
    }
    if (!is_duplicate) {
      file_suggestion::mojom::FilePtr file_copy = suggestion.second->Clone();
      final_suggestions.push_back(std::move(file_copy));
    }
  }
  return final_suggestions;
}

std::string MicrosoftFilesPageHandler::CreateJustificationTextForRecentFile(
    base::Time opened_time) {
  base::Time time_now = base::Time::Now();
  base::TimeDelta time_difference =
      time_now.LocalMidnight() - opened_time.LocalMidnight();
  // The difference between the time now and `opened_time` in days.
  int num_days_difference = time_difference.InDays();

  switch (num_days_difference) {
    case 0:
      return l10n_util::GetStringUTF8(
          IDS_NTP_MODULES_MICROSOFT_FILES_OPENED_TODAY_JUSTIFICATION_TEXT);
    case 1:
      return l10n_util::GetStringUTF8(
          IDS_NTP_MODULES_MICROSOFT_FILES_OPENED_YESTERDAY_JUSTIFICATION_TEXT);
    default:
      return l10n_util::GetStringUTF8(
          IDS_NTP_MODULES_MICROSOFT_FILES_OPENED_PAST_WEEK_JUSTIFICATION_TEXT);
  }
}

std::string MicrosoftFilesPageHandler::CreateJustificationTextForSharedFile(
    std::string shared_by) {
  return l10n_util::GetStringFUTF8(
      IDS_NTP_MODULES_MICROSOFT_FILES_SHARED_BY_JUSTIFICATION_TEXT,
      base::UTF8ToUTF16(shared_by));
}
