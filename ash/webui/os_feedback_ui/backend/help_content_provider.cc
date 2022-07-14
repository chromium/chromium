// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/help_content_provider.h"

#include <memory>

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {
namespace feedback {
namespace {

using ::ash::os_feedback_ui::mojom::HelpContent;
using ::ash::os_feedback_ui::mojom::HelpContentPtr;
using ::ash::os_feedback_ui::mojom::HelpContentType;
using ::ash::os_feedback_ui::mojom::SearchRequestPtr;
using ::ash::os_feedback_ui::mojom::SearchResponse;
using ::ash::os_feedback_ui::mojom::SearchResponsePtr;

constexpr char kHelpContentProviderUrl[] =
    "https://scone-pa.clients6.google.com/v1/search/list?key=";

constexpr char kGoogleSupportSiteUrl[] = "https://support.google.com";

// Response with 5 items takes ~7KB. A loose upper bound of 64KB is chosen to
// avoid breaking the flow in case the response is longer.
//
// The current design is to request maximum 5 items. If requirement is changed
// to support significant larger max results, then this should be calculated
// dynamically.
constexpr int kMaxBodySize = 64 * 1024;

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("help_content_provider", R"(
        semantics {
          sender: "HelpContentProvider"
          description:
            "Users can press Alt+Shift+i to report a bug or a feedback in "
            "general. The CrOS feedback tool tries to search for help contents "
            "as the users are entering text. The results are displayed as "
            "suggested help contents."
          trigger:
            "When user enters text descriping the issue in CrOS Feedback Tool."
          data:
            "The free-form text that user has entered."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings and is only activated "
            "by direct user request."
          chrome_policy {
            UserFeedbackAllowed {
              UserFeedbackAllowed: false
            }
          }
        })");

std::unique_ptr<network::ResourceRequest> CreateResourceRequest() {
  auto resource_request = std::make_unique<network::ResourceRequest>();

  resource_request->url =
      GURL(base::StrCat({kHelpContentProviderUrl, google_apis::GetAPIKey()}));
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");

  return resource_request;
}

bool IsLoaderSuccessful(const network::SimpleURLLoader* loader) {
  DCHECK(loader);

  if (loader->NetError() != net::OK) {
    LOG(ERROR) << "HelpContentProvider url loader network error: "
               << loader->NetError();
    return false;
  }

  if (!loader->ResponseInfo() || !loader->ResponseInfo()->headers) {
    LOG(ERROR) << "HelpContentProvider invalid response or "
                  "missing headers";
    return false;
  }

  // Success response codes are 2xx.
  auto response_code = loader->ResponseInfo()->headers->response_code();
  if (response_code < 200 || response_code >= 300) {
    LOG(ERROR) << "HelpContentProvider non-successful response code: "
               << loader->ResponseInfo()->headers->response_code();
    return false;
  }
  return true;
}

//  Sample json string:
// [
//     {
//       "url":
//       "/chromebook/thread/110208459?hl=en-gb",
//       "title": "Bluetooth Headphones",
//       "snippet": "I have ...",
//       "resultType": "CT_SUPPORT_FORUM_THREAD",
//       ...
//     },
// ]
HelpContentPtr GetHelpContent(const base::Value::Dict& data) {
  HelpContentPtr help_content = HelpContent::New();

  const std::string* title = data.FindString("title");
  if (title != nullptr) {
    help_content->title = base::UTF8ToUTF16(*title);
  }

  const std::string* url = data.FindString("url");
  if (url != nullptr) {
    if (url->empty() || url->at(0) == '/') {
      // The url returned from search is relative or empty.
      help_content->url = GURL(base::StrCat({kGoogleSupportSiteUrl, *url}));
    } else {
      help_content->url = GURL(*url);
    }
  }

  const std::string* result_type = data.FindString("resultType");
  help_content->content_type = (result_type == nullptr)
                                   ? HelpContentType::kUnknown
                                   : ToHelpContentType(*result_type);

  return help_content;
}

}  // namespace

std::string ConvertSearchRequestToJson(
    const std::string& app_locale,
    const os_feedback_ui::mojom::SearchRequestPtr& request) {
  base::Value::Dict request_dict;

  request_dict.Set("helpcenter", "chromeos");
  request_dict.Set("query", request->query);
  request_dict.Set("language", app_locale);
  request_dict.Set("max_results", base::NumberToString(request->max_results));

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  VLOG(2) << request_content;
  return request_content;
}

// The result_type comes from the enum ContentType defined in file:
//  google3/customer_support/content/proto/support_content_enums.proto
HelpContentType ToHelpContentType(const std::string& result_type) {
  // TODO(xiangdongkong): Confirm the mappings.
  if (result_type == "CT_ANSWER") {
    return HelpContentType::kArticle;
  }

  if (result_type.find("FORUM") != std::string::npos) {
    return HelpContentType::kForum;
  }
  LOG(WARNING) << "HelpContentProvider unknown content type: " << result_type;
  return HelpContentType::kUnknown;
}

void PopulateSearchResponse(const base::Value& search_result,
                            SearchResponsePtr& search_response) {
  if (!search_result.is_dict()) {
    LOG(WARNING)
        << "HelpContentProvider the response json is not a dictionary: "
        << search_result;
    return;
  }
  DVLOG(2) << "HelpContentProvider response body after safe parsed: "
           << search_result;
  const base::Value::Dict& dict = search_result.GetDict();

  // Extract totalResults.
  const std::string* total_results_str = dict.FindString("totalResults");
  int total_results = 0;
  if (total_results_str &&
      base::StringToInt(*total_results_str, &total_results)) {
    search_response->total_results = total_results;
  }

  // Extract resource.
  const base::Value::List* resources = dict.FindList("resource");
  if (resources == nullptr) {
    return;
  }
  // Extract HelpContents.
  for (auto& resource : *resources) {
    if (!resource.is_dict()) {
      continue;
    }
    search_response->results.push_back(GetHelpContent(resource.GetDict()));
  }
}

HelpContentProvider::HelpContentProvider(
    const std::string& app_locale,
    content::BrowserContext* browser_context)
    : app_locale_(app_locale),
      url_loader_factory_(browser_context->GetDefaultStoragePartition()
                              ->GetURLLoaderFactoryForBrowserProcess()) {}

HelpContentProvider::HelpContentProvider(
    const std::string& app_locale,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : app_locale_(app_locale), url_loader_factory_(url_loader_factory) {}

HelpContentProvider::~HelpContentProvider() = default;

void HelpContentProvider::GetHelpContents(
    os_feedback_ui::mojom::SearchRequestPtr request,
    GetHelpContentsCallback callback) {
  auto resource_request = CreateResourceRequest();

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  url_loader->AttachStringForUpload(
      ConvertSearchRequestToJson(app_locale_, request), "application/json");
  url_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&HelpContentProvider::OnHelpContentSearchResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(url_loader)),
      kMaxBodySize);
}

void HelpContentProvider::BindInterface(
    mojo::PendingReceiver<os_feedback_ui::mojom::HelpContentProvider>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

void HelpContentProvider::OnHelpContentSearchResponse(
    GetHelpContentsCallback callback,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<std::string> response_body) {
  if (IsLoaderSuccessful(url_loader.get()) && response_body) {
    DVLOG(2) << "HelpContentProvider response body: " << *response_body;
    // Send the JSON string to a dedicated service for safe parsing.
    data_decoder_.ParseJson(
        *response_body,
        base::BindOnce(&HelpContentProvider::OnResponseJsonParsed,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    SearchResponsePtr response = SearchResponse::New();
    std::move(callback).Run(std::move(response));
  }
}

void HelpContentProvider::OnResponseJsonParsed(
    GetHelpContentsCallback callback,
    data_decoder::DataDecoder::ValueOrError result) {
  SearchResponsePtr response = SearchResponse::New();

  if (result.has_value()) {
    PopulateSearchResponse(*result, response);
  } else {
    LOG(ERROR)
        << "HelpContentProvider data decoder failed to parse json. Error: "
        << result.error();
  }

  std::move(callback).Run(std::move(response));
}

}  // namespace feedback
}  // namespace ash
