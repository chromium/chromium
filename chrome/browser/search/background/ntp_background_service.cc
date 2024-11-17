// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_service.h"

#include <string_view>

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/background/ntp_background.pb.h"
#include "chrome/browser/search/background/ntp_backgrounds.h"
#include "components/search/ntp_features.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

// Command line param to override the collections base URL, e.g. for testing.
constexpr char kCollectionsBaseUrlCmdlineSwitch[] = "collections-base-url";

// The default base URL to download prod collections.
constexpr char kCollectionsBaseUrl[] = "https://clients3.google.com";

// The default base URL to download alpha collections.
constexpr char kAlphaCollectionsBaseUrl[] = "https://clients5.google.com";

// The MIME type of the POST data sent to the server.
constexpr char kProtoMimeType[] = "application/x-protobuf";

// The path relative to kCollectionsBaseUrl to download the proto of the
// complete list of wallpaper collections.
constexpr char kCollectionsPath[] =
    "/cast/chromecast/home/wallpaper/collections?rt=b";
// The path relative to kCollectionsBaseUrl to download the metadata of the
// images in a collection.
constexpr char kCollectionImagesPath[] =
    "/cast/chromecast/home/wallpaper/collection-images?rt=b";
// The path relative to kCollectionsBaseUrl to download the metadata of the
// 'next' image in a collection.
constexpr char kNextCollectionImagePath[] =
    "/cast/chromecast/home/wallpaper/image?rt=b";

// The options to be added to an image URL, specifying resolution, cropping,
// etc. Options appear on an image URL after the '=' character.
// TODO(crbug.com/41408116): Set options based on display resolution capability.
constexpr char kImageOptions[] = "=w3840-h2160-p-k-no-nd-mv";

// Label added to request to filter out unwanted collections.
constexpr char kFilteringLabel[] = "chrome_desktop_ntp";

// Returns the configured collections base URL with |path| appended.
GURL GetUrl(std::string_view path) {
  return GURL(base::CommandLine::ForCurrentProcess()->HasSwitch(
                  kCollectionsBaseUrlCmdlineSwitch)
                  ? base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                        kCollectionsBaseUrlCmdlineSwitch)
              : base::FeatureList::IsEnabled(
                    ntp_features::kNtpAlphaBackgroundCollections)
                  ? kAlphaCollectionsBaseUrl
                  : kCollectionsBaseUrl)
      .Resolve(path);
}

}  // namespace

NtpBackgroundService::NtpBackgroundService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  default_image_options_ = kImageOptions;
  thumbnail_image_options_ = GetThumbnailImageOptions();
  collections_api_url_ = GetUrl(kCollectionsPath);
  collection_images_api_url_ = GetUrl(kCollectionImagesPath);
  next_image_api_url_ = GetUrl(kNextCollectionImagePath);
}

NtpBackgroundService::~NtpBackgroundService() = default;

void NtpBackgroundService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnNtpBackgroundServiceShuttingDown();
  }
  DCHECK(observers_.empty());
}

void NtpBackgroundService::FetchCollectionInfo() {
  if (collections_loader_ != nullptr)
    return;
  collection_error_info_.ClearError();

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("backdrop_collection_names_download",
                                          R"(
        semantics {
          sender: "Desktop NTP Background Selector"
          description:
            "The Chrome Desktop New Tab Page background selector displays a "
            "rich set of wallpapers for users to choose from. Each wallpaper "
            "belongs to a collection (e.g. Arts, Landscape etc.). The list of "
            "all available collections is obtained from the Backdrop wallpaper "
            "service."
          trigger:
            "Clicking the customize (pencil) icon on the New Tab page."
          data:
            "The Backdrop protocol buffer messages. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
            email: "chrome-desktop-ntp@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-06-13"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature by selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");

  ntp::background::GetCollectionsRequest request;
  // The language field may include the country code (e.g. "en-US").
  request.set_language(g_browser_process->GetApplicationLocale());
  request.add_filtering_label(kFilteringLabel);
  // Add some extra filtering information in case we need to target a specific
  // milestone post release.
  request.add_filtering_label(base::StrCat(
      {kFilteringLabel, ".M", version_info::GetMajorVersionNumber()}));
  // Add filtering for Panorama feature.
  request.add_filtering_label(base::StrCat({kFilteringLabel, ".panorama"}));
  request.add_filtering_label(base::StrCat({kFilteringLabel, ".gm3"}));
  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpBackgroundImageErrorDetection)) {
    request.add_filtering_label(
        base::StrCat({kFilteringLabel, ".error_detection"}));
  }

  std::string serialized_proto;
  request.SerializeToString(&serialized_proto);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = collections_api_url_;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  collections_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  collections_loader_->AttachStringForUpload(serialized_proto, kProtoMimeType);
  collections_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NtpBackgroundService::OnCollectionInfoFetchComplete,
                     base::Unretained(this)),
      1024 * 1024);
}

void NtpBackgroundService::OnCollectionInfoFetchComplete(
    std::unique_ptr<std::string> response_body) {
  collection_info_.clear();
  // The loader will be deleted when the request is handled.
  std::unique_ptr<network::SimpleURLLoader> loader_deleter(
      std::move(collections_loader_));

  if (!response_body) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DVLOG(1) << "Request failed with error: " << loader_deleter->NetError();
    collection_error_info_.error_type = ErrorType::NET_ERROR;
    collection_error_info_.net_error = loader_deleter->NetError();
    NotifyObservers(FetchComplete::COLLECTION_INFO);
    return;
  }

  ntp::background::GetCollectionsResponse collections_response;
  if (!collections_response.ParseFromString(*response_body)) {
    DVLOG(1) << "Deserializing Backdrop wallpaper proto for collection info "
                "failed.";
    collection_error_info_.error_type = ErrorType::SERVICE_ERROR;
    NotifyObservers(FetchComplete::COLLECTION_INFO);
    return;
  }

  for (int i = 0; i < collections_response.collections_size(); ++i) {
    ntp::background::Collection collection =
        collections_response.collections(i);
    if (collection.preview_size() > 0 &&
        collection.preview(0).has_image_url()) {
      collection_info_.push_back(CollectionInfo::CreateFromProto(
          collection, /*preview_image_url=*/AddOptionsToImageURL(
              collection.preview(0).image_url(), thumbnail_image_options_)));
    } else {
      collection_info_.push_back(CollectionInfo::CreateFromProto(
          collection, /*preview_image_url=*/std::nullopt));
    }
  }
  NotifyObservers(FetchComplete::COLLECTION_INFO);
}

void NtpBackgroundService::FetchCollectionImageInfo(
    const std::string& collection_id) {
  collection_images_error_info_.ClearError();
  // Ignore subsequent requests to fetch collection image info.
  // TODO(crbug.com/40916951): Prioritize the latest request to fetch collection
  // images.
  if (!requested_collection_id_.empty()) {
    return;
  }
  requested_collection_id_ = collection_id;
  pending_image_url_header_loaders_.clear();
  FetchCollectionImageInfoInternal(
      collection_id,
      base::BindOnce(&NtpBackgroundService::OnCollectionImageInfoFetchComplete,
                     base::Unretained(this)));
}

void NtpBackgroundService::OnCollectionImageInfoFetchComplete(
    ntp::background::GetImagesInCollectionResponse images_response,
    ErrorType error_type) {
  collection_images_.clear();

  if (error_type == ErrorType::NET_ERROR) {
    // This represents network errors (i.e. the server did not provide
    // a response).
    DVLOG(1) << "Request failed with error: "
             << collection_images_error_info_.net_error;
    collection_images_error_info_.error_type = error_type;
    NotifyObservers(FetchComplete::COLLECTION_IMAGE_INFO);
    return;
  } else if (error_type == ErrorType::SERVICE_ERROR) {
    DVLOG(1) << "Deserializing Backdrop wallpaper proto for image "
                "info failed.";
    collection_images_error_info_.error_type = error_type;
    NotifyObservers(FetchComplete::COLLECTION_IMAGE_INFO);
    return;
  }

  for (int i = 0; i < images_response.images_size(); ++i) {
    const ntp::background::Image image = images_response.images(i);
    collection_images_.push_back(CollectionImage::CreateFromProto(
        requested_collection_id_, image,
        /*default_image_url=*/
        AddOptionsToImageURL(image.image_url(), default_image_options_),
        /*thumbnail_image_url=*/
        AddOptionsToImageURL(image.image_url(), thumbnail_image_options_)));
  }
  NotifyObservers(FetchComplete::COLLECTION_IMAGE_INFO);
}

void NtpBackgroundService::VerifyImageURL(
    const GURL& url,
    base::OnceCallback<void(int)> image_url_headers_received_callback) {
  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ntp_image_url_verification",
                                          R"(
        semantics {
          sender: "Desktop NTP Background Selector"
          description:
            "The Chrome Desktop New Tab Page background selector displays a "
            "rich set of wallpapers for users to choose from. Each wallpaper "
            "belongs to a collection (e.g. Arts, Landscape etc.). "
            "This fetches a wallpaper image's link to make sure its "
            "resource is reachable."
          trigger:
            "When the user has a non-uploaded background image set and "
            "opens the New Tab Page, clicks the customize (pencil) icon "
            "on the New Tab page, or clicks a theme "
            "collection on the New Tab Page."
          data:
            "The HTTP headers for a NTP background image."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
            email: "chrome-desktop-ntp@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-07-13"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature by selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::unique_ptr<network::SimpleURLLoader> image_url_header_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  network::SimpleURLLoader* const image_url_header_loader_ptr =
      image_url_header_loader.get();
  image_url_header_loader_ptr->SetRetryOptions(
      /*max_retries=*/3,
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
          network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);
  auto it = pending_image_url_header_loaders_.insert(
      pending_image_url_header_loaders_.begin(),
      std::move(image_url_header_loader));
  image_url_header_loader_ptr->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&NtpBackgroundService::OnImageURLHeadersFetchComplete,
                     base::Unretained(this), std::move(it),
                     std::move(image_url_headers_received_callback),
                     base::TimeTicks::Now()));
}

void NtpBackgroundService::OnImageURLHeadersFetchComplete(
    URLLoaderList::iterator it,
    base::OnceCallback<void(int)> image_url_headers_received_callback,
    base::TimeTicks request_start,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  if (pending_image_url_header_loaders_.empty()) {
    return;
  }
  pending_image_url_header_loaders_.erase(it);
  int headers_response_code =
      headers ? headers->response_code() : net::ERR_FAILED;

  UMA_HISTOGRAM_SPARSE("NewTabPage.BackgroundService.Images.Headers.StatusCode",
                       headers_response_code);
  auto request_time = base::TimeTicks::Now() - request_start;
  UMA_HISTOGRAM_TIMES(
      "NewTabPage.BackgroundService.Images.Headers.RequestLatency",
      request_time);
  if (headers_response_code == net::HTTP_OK) {
    UMA_HISTOGRAM_TIMES(
        "NewTabPage.BackgroundService.Images.Headers.RequestLatency.Ok",
        request_time);
  } else if (headers_response_code == net::HTTP_NOT_FOUND) {
    UMA_HISTOGRAM_TIMES(
        "NewTabPage.BackgroundService.Images.Headers.RequestLatency.NotFound",
        request_time);
  } else {
    UMA_HISTOGRAM_TIMES(
        "NewTabPage.BackgroundService.Images.Headers.RequestLatency.Other",
        request_time);
  }

  std::move(image_url_headers_received_callback).Run(headers_response_code);
}

void NtpBackgroundService::FetchReplacementCollectionPreviewImage(
    const std::string& collection_id,
    FetchReplacementImageCallback fetch_replacement_image_callback) {
  FetchCollectionImageInfoInternal(
      collection_id,
      base::BindOnce(&NtpBackgroundService::
                         OnFetchReplacementCollectionPreviewImageComplete,
                     base::Unretained(this),
                     std::move(fetch_replacement_image_callback)));
}

void NtpBackgroundService::OnFetchReplacementCollectionPreviewImageComplete(
    FetchReplacementImageCallback fetch_replacement_image_callback,
    ntp::background::GetImagesInCollectionResponse images_response,
    ErrorType error_type) {
  if (error_type != ErrorType::NONE || images_response.images_size() == 0) {
    std::move(fetch_replacement_image_callback).Run(std::nullopt);
    return;
  }
  // Attempt to find an image URL in the collection that works. We start with
  // the first image's index, and increment, if needed, in the callback
  // |OnReplacementCollectionPreviewImageHeadersReceived|.
  const int replacement_image_index = 0;
  const GURL replacement_image_url = AddOptionsToImageURL(
      images_response.images(replacement_image_index).image_url(),
      thumbnail_image_options_);
  VerifyImageURL(
      replacement_image_url,
      base::BindOnce(
          &NtpBackgroundService::
              OnReplacementCollectionPreviewImageHeadersReceived,
          base::Unretained(this), std::move(fetch_replacement_image_callback),
          images_response, replacement_image_index, replacement_image_url));
}

void NtpBackgroundService::OnReplacementCollectionPreviewImageHeadersReceived(
    FetchReplacementImageCallback fetch_replacement_image_callback,
    ntp::background::GetImagesInCollectionResponse images_response,
    int replacement_image_index,
    const GURL& replacement_image_url,
    int headers_response_code) {
  if (headers_response_code == net::HTTP_OK) {
    std::move(fetch_replacement_image_callback).Run(replacement_image_url);
  } else if (replacement_image_index == images_response.images_size() - 1) {
    std::move(fetch_replacement_image_callback).Run(std::nullopt);
  } else {
    replacement_image_index++;
    const GURL next_replacement_image_url = AddOptionsToImageURL(
        images_response.images(replacement_image_index).image_url(),
        thumbnail_image_options_);
    VerifyImageURL(
        next_replacement_image_url,
        base::BindOnce(&NtpBackgroundService::
                           OnReplacementCollectionPreviewImageHeadersReceived,
                       base::Unretained(this),
                       std::move(fetch_replacement_image_callback),
                       images_response, replacement_image_index,
                       next_replacement_image_url));
  }
}

void NtpBackgroundService::FetchNextCollectionImage(
    const std::string& collection_id,
    const std::optional<std::string>& resume_token) {
  next_image_error_info_.ClearError();
  if (next_image_loader_ != nullptr)
    return;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("backdrop_next_image_download",
                                          R"(
        semantics {
          sender: "Desktop NTP Background Selector"
          description:
            "The Chrome Desktop New Tab Page background selector displays a "
            "rich set of wallpapers for users to choose from. Each wallpaper "
            "belongs to a collection (e.g. Arts, Landscape etc.). The list of "
            "all available collections is obtained from the Backdrop wallpaper "
            "service."
          trigger:
            "Clicking the customize (pencil) icon on the New Tab page."
          data:
            "The Backdrop protocol buffer messages. ApplicationLocale is "
            "included in the request."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
            email: "chrome-desktop-ntp@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-06-13"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature by selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");

  requested_next_image_collection_id_ = collection_id;
  requested_next_image_resume_token_ = resume_token.value_or("");
  ntp::background::GetImageFromCollectionRequest request;
  *request.add_collection_ids() = requested_next_image_collection_id_;
  request.set_resume_token(requested_next_image_resume_token_);
  // The language field may include the country code (e.g. "en-US").
  request.set_language(g_browser_process->GetApplicationLocale());
  std::string serialized_proto;
  request.SerializeToString(&serialized_proto);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = next_image_api_url_;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  next_image_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  next_image_loader_->AttachStringForUpload(serialized_proto, kProtoMimeType);
  next_image_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NtpBackgroundService::OnNextImageInfoFetchComplete,
                     base::Unretained(this)),
      1024 * 1024);
}

void NtpBackgroundService::OnNextImageInfoFetchComplete(
    std::unique_ptr<std::string> response_body) {
  // The loader will be deleted when the request is handled.
  std::unique_ptr<network::SimpleURLLoader> loader_deleter(
      std::move(next_image_loader_));

  if (!response_body) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DVLOG(1) << "Request failed with error: " << loader_deleter->NetError();
    next_image_error_info_.error_type = ErrorType::NET_ERROR;
    next_image_error_info_.net_error = loader_deleter->NetError();
    NotifyObservers(FetchComplete::NEXT_IMAGE_INFO);
    return;
  }

  ntp::background::GetImageFromCollectionResponse image_response;
  if (!image_response.ParseFromString(*response_body)) {
    DVLOG(1) << "Deserializing Backdrop wallpaper proto for next image failed.";
    next_image_error_info_.error_type = ErrorType::SERVICE_ERROR;
    NotifyObservers(FetchComplete::NEXT_IMAGE_INFO);
    return;
  }

  const ntp::background::Image image = image_response.image();
  next_image_ = CollectionImage::CreateFromProto(
      requested_next_image_collection_id_, image,
      /*default_image_url=*/
      AddOptionsToImageURL(image.image_url(), default_image_options_),
      /*thumbnail_image_url=*/
      AddOptionsToImageURL(image.image_url(), thumbnail_image_options_));
  next_image_resume_token_ = image_response.resume_token();

  NotifyObservers(FetchComplete::NEXT_IMAGE_INFO);
}

void NtpBackgroundService::FetchCollectionImageInfoInternal(
    const std::string& collection_id,
    base::OnceCallback<void(ntp::background::GetImagesInCollectionResponse,
                            ErrorType)> collection_images_received_callback) {
  collection_images_error_info_.ClearError();

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("backdrop_collection_images_download",
                                          R"(
        semantics {
          sender: "Desktop NTP Background Selector"
          description:
            "The Chrome Desktop New Tab Page background selector displays a "
            "rich set of wallpapers for users to choose from. Each wallpaper "
            "belongs to a collection (e.g. Arts, Landscape etc.). The list of "
            "all available collections is obtained from the Backdrop wallpaper "
            "service."
          trigger:
            "Clicking the customize (pencil) icon on the New Tab page."
          data:
            "The Backdrop protocol buffer messages. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
            email: "chrome-desktop-ntp@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-06-13"
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature by selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = collection_images_api_url_;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::unique_ptr<network::SimpleURLLoader> collection_image_info_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  network::SimpleURLLoader* const collection_image_info_loader_ptr =
      collection_image_info_loader.get();
  collection_image_info_loader->SetRetryOptions(
      /*max_retries=*/3,
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
          network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);
  auto it = pending_collection_image_info_loaders_.insert(
      pending_collection_image_info_loaders_.begin(),
      std::move(collection_image_info_loader));

  ntp::background::GetImagesInCollectionRequest request;
  request.set_collection_id(collection_id);
  // The language field may include the country code (e.g. "en-US").
  request.set_language(g_browser_process->GetApplicationLocale());
  std::string serialized_proto;
  request.SerializeToString(&serialized_proto);
  collection_image_info_loader_ptr->AttachStringForUpload(serialized_proto,
                                                          kProtoMimeType);
  collection_image_info_loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(
          [](URLLoaderList& pending_collection_image_info_loaders,
             URLLoaderList::iterator it,
             base::OnceCallback<void(
                 ntp::background::GetImagesInCollectionResponse, ErrorType)>
                 collection_images_received_callback,
             std::unique_ptr<std::string> response_body) {
            if (pending_collection_image_info_loaders.empty()) {
              return;
            }
            pending_collection_image_info_loaders.erase(it);

            ntp::background::GetImagesInCollectionResponse images_response;
            if (!response_body) {
              std::move(collection_images_received_callback)
                  .Run(std::move(images_response), ErrorType::NET_ERROR);
            } else if (!images_response.ParseFromString(*response_body)) {
              std::move(collection_images_received_callback)
                  .Run(std::move(images_response), ErrorType::SERVICE_ERROR);
            } else {
              std::move(collection_images_received_callback)
                  .Run(std::move(images_response), ErrorType::NONE);
            }
          },
          std::ref(pending_collection_image_info_loaders_), std::move(it),
          std::move(collection_images_received_callback)),
      1024 * 1024);
}

const std::vector<CollectionInfo>& NtpBackgroundService::collection_info()
    const {
  return collection_info_;
}

const std::vector<CollectionImage>& NtpBackgroundService::collection_images()
    const {
  return collection_images_;
}

void NtpBackgroundService::AddObserver(NtpBackgroundServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void NtpBackgroundService::RemoveObserver(
    NtpBackgroundServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool NtpBackgroundService::IsValidBackdropUrl(const GURL& url) const {
  for (auto& ntp_background : GetNtpBackgrounds()) {
    if (ntp_background == url) {
      return true;
    }
  }

  for (auto& image : collection_images_) {
    if (image.image_url == url)
      return true;
  }
  return false;
}

bool NtpBackgroundService::IsValidBackdropCollection(
    const std::string& collection_id) const {
  for (auto& collection : collection_info_) {
    if (collection.collection_id == collection_id)
      return true;
  }
  return false;
}

void NtpBackgroundService::AddValidBackdropUrlForTesting(const GURL& url) {
  CollectionImage image;
  image.image_url = url;
  collection_images_.push_back(image);
}

void NtpBackgroundService::AddValidBackdropCollectionForTesting(
    const std::string& collection_id) {
  CollectionInfo collection;
  collection.collection_id = collection_id;
  collection_info_.push_back(collection);
}

void NtpBackgroundService::AddValidBackdropUrlWithThumbnailForTesting(
    const GURL& url,
    const GURL& thumbnail_url) {
  CollectionImage image;
  image.image_url = url;
  image.thumbnail_image_url = thumbnail_url;
  collection_images_.push_back(image);
}

void NtpBackgroundService::SetNextCollectionImageForTesting(
    const CollectionImage& image) {
  next_image_ = image;
}

const GURL& NtpBackgroundService::GetThumbnailUrl(const GURL& image_url) {
  for (auto& image : collection_images_) {
    if (image.image_url == image_url)
      return image.thumbnail_image_url;
  }
  return GURL::EmptyGURL();
}

void NtpBackgroundService::NotifyObservers(FetchComplete fetch_complete) {
  for (auto& observer : observers_) {
    switch (fetch_complete) {
      case FetchComplete::COLLECTION_INFO:
        observer.OnCollectionInfoAvailable();
        break;
      case FetchComplete::COLLECTION_IMAGE_INFO:
        requested_collection_id_.clear();
        observer.OnCollectionImagesAvailable();
        break;
      case FetchComplete::NEXT_IMAGE_INFO:
        observer.OnNextCollectionImageAvailable();
        break;
    }
  }
}

std::string NtpBackgroundService::GetImageOptionsForTesting() {
  return kImageOptions;
}

GURL NtpBackgroundService::GetCollectionsLoadURLForTesting() const {
  return collections_api_url_;
}

GURL NtpBackgroundService::GetImagesURLForTesting() const {
  return collection_images_api_url_;
}

GURL NtpBackgroundService::GetNextImageURLForTesting() const {
  return next_image_api_url_;
}
