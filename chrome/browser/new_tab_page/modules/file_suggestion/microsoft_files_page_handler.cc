// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files_page_handler.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/file_suggestion.mojom.h"
#include "components/search/ntp_features.h"
#include "net/base/mime_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"

namespace {

const char kTrendingFilesEndpoint[] =
    "https://graph.microsoft.com/v1.0/me/insights/trending";

const char kBaseIconUrl[] =
    "https://res.cdn.office.net/files/fabric-cdn-prod_20240925.001/assets/"
    "item-types/16/";

const char kTrendingJustificationText[] = "Trending in your organization";

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

std::string GetFileExtension(std::string mime_type) {
  base::FilePath::StringType extension;
  net::GetPreferredExtensionForMimeType(mime_type, &extension);
  std::string result;

#if BUILDFLAG(IS_WIN)
  // `extension` will be of std::wstring type on Windows which needs to be
  // handled differently than std::string. See base/files/file_path.h for more
  // info.
  result = base::WideToUTF8(extension);
#else
  result = extension;
#endif

  return result;
}

GURL GetFileIconUrl(std::string extension) {
  std::string path = extension + ".png";
  return GURL(kBaseIconUrl).Resolve(path);
}

}  // namespace

// static
void MicrosoftFilesPageHandler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kNtpMicrosoftFilesModuleLastDismissedTime,
                             base::Time());
}

MicrosoftFilesPageHandler::MicrosoftFilesPageHandler(
    mojo::PendingReceiver<file_suggestion::mojom::MicrosoftFilesPageHandler>
        handler,
    Profile* profile)
    : handler_(this, std::move(handler)),
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
  // Parse data immediately when displaying fake data.
  if (ntp_features::kNtpSharepointModuleDataParam.Get() ==
      ntp_features::NtpSharepointModuleDataType::kTrendingInsightsFakeData) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        kFakeTrendingData,
        base::BindOnce(&MicrosoftFilesPageHandler::OnJsonParsed,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else if (ntp_features::kNtpSharepointModuleDataParam.Get() ==
             ntp_features::NtpSharepointModuleDataType::kTrendingInsights) {
    GetTrendingFiles(std::move(callback));
  } else {
    // TODO(376515309) Request recently used/shared files.
    std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
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
  // TODO(389714511): Pass in access token once available.
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer <accesstoken>");
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

void MicrosoftFilesPageHandler::OnJsonReceived(
    GetFilesCallback callback,
    std::unique_ptr<std::string> response_body) {
  const int net_error = url_loader_->NetError();
  url_loader_.reset();

  if (net_error == net::OK && response_body) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        *response_body,
        base::BindOnce(&MicrosoftFilesPageHandler::OnJsonParsed,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
  }
}

void MicrosoftFilesPageHandler::OnJsonParsed(
    GetFilesCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    return;
  }

  auto* suggestions = result->GetDict().FindList("value");
  if (!suggestions) {
    std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
    return;
  }

  std::vector<file_suggestion::mojom::FilePtr> created_suggestions;
  for (const auto& suggestion : *suggestions) {
    const auto& suggestion_dict = suggestion.GetDict();
    const std::string* id = suggestion_dict.FindString("id");
    const std::string* title =
        suggestion_dict.FindStringByDottedPath("resourceVisualization.title");
    const std::string* url =
        suggestion_dict.FindStringByDottedPath("resourceReference.webUrl");
    const std::string* mime_type = suggestion_dict.FindStringByDottedPath(
        "resourceVisualization.mediaType");

    if (!id || !title || !url || !mime_type) {
      std::move(callback).Run(std::vector<file_suggestion::mojom::FilePtr>());
      return;
    }

    std::string file_extension = GetFileExtension(*mime_type);
    // Skip creating file suggestion if there's an error mapping the mime-type
    // to an extension as the extension is needed for the file's `icon_url.`
    if (file_extension.empty()) {
      continue;
    }

    file_suggestion::mojom::FilePtr created_file =
        file_suggestion::mojom::File::New();
    created_file->id = *id;
    created_file->justification_text = kTrendingJustificationText;
    created_file->icon_url = GetFileIconUrl(file_extension);
    created_file->title = *title;
    created_file->item_url = GURL(*url);
    created_suggestions.push_back(std::move(created_file));
  }

  std::move(callback).Run(std::move(created_suggestions));
}
