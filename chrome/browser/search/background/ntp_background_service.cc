// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_service.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/search/background/ntp_background.pb.h"
#include "chrome/browser/search/background/ntp_backgrounds.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

// The MIME type of the POST data sent to the server.
constexpr char kProtoMimeType[] = "application/x-protobuf";

// The url to download the proto of the complete list of wallpaper collections.
constexpr char kCollectionsUrl[] =
    "https://clients3.google.com/cast/chromecast/home/wallpaper/"
    "collections?rt=b";
// The url to download the metadata of the images in a collection.
constexpr char kCollectionImagesUrl[] =
    "https://clients3.google.com/cast/chromecast/home/wallpaper/"
    "collection-images?rt=b";
// The url to download the metadata of the 'next' image in a collection.
constexpr char kNextCollectionImageUrl[] =
    "https://clients3.google.com/cast/chromecast/home/wallpaper/"
    "image?rt=b";

// The options to be added to an image URL, specifying resolution, cropping,
// etc. Options appear on an image URL after the '=' character.
// TODO(crbug.com/874339): Set options based on display resolution capability.
constexpr char kImageOptions[] = "=w3840-h2160-p-k-no-nd-mv";

}  // namespace

NtpBackgroundService::NtpBackgroundService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  image_options_ = kImageOptions;
  collections_api_url_ = GURL(kCollectionsUrl);
  collection_images_api_url_ = GURL(kCollectionImagesUrl);
  next_image_api_url_ = GURL(kNextCollectionImageUrl);
}

NtpBackgroundService::~NtpBackgroundService() = default;

void NtpBackgroundService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnNtpBackgroundServiceShuttingDown();
  }
  DCHECK(!observers_.might_have_observers());
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
    collection_info_.push_back(
        CollectionInfo::CreateFromProto(collections_response.collections(i)));
  }

  NotifyObservers(FetchComplete::COLLECTION_INFO);
}

void NtpBackgroundService::FetchCollectionImageInfo(
    const std::string& collection_id) {
  collection_images_error_info_.ClearError();
  if (collections_image_info_loader_ != nullptr)
    return;

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

  requested_collection_id_ = collection_id;
  ntp::background::GetImagesInCollectionRequest request;
  request.set_collection_id(collection_id);
  // The language field may include the country code (e.g. "en-US").
  request.set_language(g_browser_process->GetApplicationLocale());
  std::string serialized_proto;
  request.SerializeToString(&serialized_proto);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = collection_images_api_url_;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  collections_image_info_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  collections_image_info_loader_->AttachStringForUpload(serialized_proto,
                                                        kProtoMimeType);
  collections_image_info_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NtpBackgroundService::OnCollectionImageInfoFetchComplete,
                     base::Unretained(this)),
      1024 * 1024);
}

void NtpBackgroundService::OnCollectionImageInfoFetchComplete(
    std::unique_ptr<std::string> response_body) {
  collection_images_.clear();
  // The loader will be deleted when the request is handled.
  std::unique_ptr<network::SimpleURLLoader> loader_deleter(
      std::move(collections_image_info_loader_));

  if (!response_body) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DVLOG(1) << "Request failed with error: " << loader_deleter->NetError();
    collection_images_error_info_.error_type = ErrorType::NET_ERROR;
    collection_images_error_info_.net_error = loader_deleter->NetError();
    NotifyObservers(FetchComplete::COLLECTION_IMAGE_INFO);
    return;
  }

  ntp::background::GetImagesInCollectionResponse images_response;
  if (!images_response.ParseFromString(*response_body)) {
    DVLOG(1) << "Deserializing Backdrop wallpaper proto for image info failed.";
    collection_images_error_info_.error_type = ErrorType::SERVICE_ERROR;
    NotifyObservers(FetchComplete::COLLECTION_IMAGE_INFO);
    return;
  }

  for (int i = 0; i < images_response.images_size(); ++i) {
    collection_images_.push_back(CollectionImage::CreateFromProto(
        requested_collection_id_, images_response.images(i), image_options_));
  }

  NotifyObservers(FetchComplete::COLLECTION_IMAGE_INFO);
}

void NtpBackgroundService::FetchNextCollectionImage(
    const std::string& collection_id,
    const base::Optional<std::string>& resume_token) {
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

  next_image_ =
      CollectionImage::CreateFromProto(requested_next_image_collection_id_,
                                       image_response.image(), image_options_);
  next_image_resume_token_ = image_response.resume_token();

  NotifyObservers(FetchComplete::NEXT_IMAGE_INFO);
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
