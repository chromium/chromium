// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/search/background/ntp_background.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"
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
// The url to download metadata of photo albums.
constexpr char kAlbumsUrl[] =
    "https://clients3.google.com/cast/chromecast/home/photo-album-metadata?"
    "f.req=%5B%2C%22512%22%2C%22512%22%2C%2250%22%2C%2C%2Ctrue%2C%2C%222%22"
    "%2C%220%22%5D&rt=b";
// The url to download metadata of photos within an album. A proto-format
// response is requested, since it is easy to parse.
constexpr char kAlbumPhotosBaseUrl[] =
    "https://clients3.google.com/cast/chromecast/home/settings/preview?rt=b";
// The format used to specify additional parameters to the Photos API. This
// requests photos for a to-be-specified album_id and photo_container_id, with
// a max page size of 100.
// TODO(crbug.com/855934): support pagination parameter.
const char kPhotosUrlRequestFormat[] = "[\"2\", [\"%s\",,,\"%s\"],,\"100\"]";

// The virtual device id required in GET request header for albums requests.
constexpr char kVirtualDeviceIdParam[] = "CAST-APP-DEVICE-ID";
constexpr char kVirtualDeviceIdValue[] = "CCCCCCCC01F9618E8D8126398CF4218E";
// The Authorization header includes an access token.
constexpr char kAuthHeaderParam[] = "Authorization";
constexpr char kAuthHeaderValue[] = "Bearer %s";

// The options to be added to an image URL, specifying resolution, cropping,
// etc. Options appear on an image URL after the '=' character.
// TODO(crbug.com/851990): Set options based on display resolution capability.
constexpr char kImageOptions[] = "=w3840-h2160-p-k-no-nd-mv";

constexpr char kScopePhotos[] = "https://www.googleapis.com/auth/photos";

}  // namespace

NtpBackgroundService::NtpBackgroundService(
    identity::IdentityManager* const identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const base::Optional<GURL>& collections_api_url_override,
    const base::Optional<GURL>& collection_images_api_url_override,
    const base::Optional<GURL>& albums_api_url_override,
    const base::Optional<GURL>& photos_api_base_url_override,
    const base::Optional<std::string>& image_options_override)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  collections_api_url_ =
      collections_api_url_override.value_or(GURL(kCollectionsUrl));
  collection_images_api_url_ =
      collection_images_api_url_override.value_or(GURL(kCollectionImagesUrl));
  albums_api_url_ = albums_api_url_override.value_or(GURL(kAlbumsUrl));
  photos_api_base_url_ =
      photos_api_base_url_override.value_or(GURL(kAlbumPhotosBaseUrl));
  image_options_ = image_options_override.value_or(kImageOptions);
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
            "Clicking the the settings (gear) icon on the New Tab page."
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
  resource_request->load_flags = net::LOAD_DO_NOT_SEND_AUTH_DATA;

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
    DLOG(WARNING) << "Request failed with error: "
                  << loader_deleter->NetError();
    collection_error_info_.error_type = ErrorType::NET_ERROR;
    collection_error_info_.net_error = loader_deleter->NetError();
    NotifyObservers(FetchComplete::COLLECTION_INFO);
    return;
  }

  ntp::background::GetCollectionsResponse collections_response;
  if (!collections_response.ParseFromString(*response_body)) {
    DLOG(WARNING)
        << "Deserializing Backdrop wallpaper proto for collection info "
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
            "Clicking the the settings (gear) icon on the New Tab page."
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
  resource_request->load_flags = net::LOAD_DO_NOT_SEND_AUTH_DATA;

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
    DLOG(WARNING) << "Request failed with error: "
                  << loader_deleter->NetError();
    collection_images_error_info_.error_type = ErrorType::NET_ERROR;
    collection_images_error_info_.net_error = loader_deleter->NetError();
    NotifyObservers(FetchComplete::COLLECTION_IMAGE_INFO);
    return;
  }

  ntp::background::GetImagesInCollectionResponse images_response;
  if (!images_response.ParseFromString(*response_body)) {
    DLOG(WARNING)
        << "Deserializing Backdrop wallpaper proto for image info failed.";
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

void NtpBackgroundService::FetchAlbumInfo() {
  // We're still waiting for the last request to come back.
  if (token_fetcher_ || albums_loader_)
    return;
  album_error_info_.ClearError();

  // Clear any stale data that may have been fetched with a previous token.
  // This is particularly important if the current token fetch results in an
  // auth error because the user has since signed out.
  album_info_.clear();
  identity::ScopeSet scopes{kScopePhotos};
  token_fetcher_ = std::make_unique<identity::PrimaryAccountAccessTokenFetcher>(
      "ntp_backgrounds_service", identity_manager_, scopes,
      base::BindOnce(&NtpBackgroundService::GetAccessTokenForAlbumCallback,
                     base::Unretained(this)),
      identity::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void NtpBackgroundService::GetAccessTokenForAlbumCallback(
    GoogleServiceAuthError error,
    identity::AccessTokenInfo access_token_info) {
  token_fetcher_.reset();

  if (error != GoogleServiceAuthError::AuthErrorNone()) {
    DLOG(WARNING) << "Failed to retrieve token with error: "
                  << error.ToString();
    if (error.state() ==
        GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS) {
      album_error_info_.error_type = ErrorType::AUTH_ERROR;
    } else {
      album_error_info_.error_type = ErrorType::NET_ERROR;
    }
    NotifyObservers(FetchComplete::ALBUM_INFO);
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("google_photos_album_names_download",
                                          R"(
        semantics {
          sender: "Desktop NTP Background Selector"
          description:
            "The Chrome Desktop New Tab Page background selector allows "
            "signed-in users to select images from their Google Photos albums. "
            "The set of albums is obtained from the Backdrop service."
          trigger:
            "Clicking the the settings (gear) icon on the New Tab page."
          data:
            "The Backdrop protocol buffer messages and an access token with "
            "limited scope to obtain the user's Google Photos data."
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

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = albums_api_url_;
  resource_request->method = "GET";
  resource_request->headers.SetHeader(kVirtualDeviceIdParam,
                                      kVirtualDeviceIdValue);
  resource_request->headers.SetHeader(
      kAuthHeaderParam,
      base::StringPrintf(kAuthHeaderValue, access_token_info.token.c_str()));

  albums_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation);
  albums_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NtpBackgroundService::OnAlbumInfoFetchComplete,
                     base::Unretained(this)),
      1024 * 1024);
}

void NtpBackgroundService::OnAlbumInfoFetchComplete(
    std::unique_ptr<std::string> response_body) {
  // The loader will be deleted when the request is handled.
  std::unique_ptr<network::SimpleURLLoader> loader_deleter(
      std::move(albums_loader_));

  if (!response_body) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DLOG(WARNING) << "Request failed with error: "
                  << loader_deleter->NetError();
    album_error_info_.error_type = ErrorType::NET_ERROR;
    album_error_info_.net_error = loader_deleter->NetError();
    NotifyObservers(FetchComplete::ALBUM_INFO);
    return;
  }

  ntp::background::PersonalAlbumsResponse albums_response;
  if (!albums_response.ParseFromString(*response_body) ||
      albums_response.error_on_server()) {
    DLOG(WARNING) << "Deserializing personal albums response proto failed.";
    album_error_info_.error_type = ErrorType::SERVICE_ERROR;
    NotifyObservers(FetchComplete::ALBUM_INFO);
    return;
  }

  for (int i = 0; i < albums_response.album_meta_data_size(); ++i) {
    album_info_.push_back(
        AlbumInfo::CreateFromProto(albums_response.album_meta_data(i)));
  }

  NotifyObservers(FetchComplete::ALBUM_INFO);
}

void NtpBackgroundService::FetchAlbumPhotos(
    const std::string& album_id,
    const std::string& photo_container_id) {
  // We're still waiting for the last request to come back.
  if (token_fetcher_ || albums_photo_info_loader_)
    return;
  album_photos_error_info_.ClearError();

  // Clear any stale data that may have been fetched with a previous token.
  // This is particularly important if the current token fetch results in an
  // auth error because the user has since signed out.
  album_photos_.clear();
  requested_album_id_ = album_id;
  requested_photo_container_id_ = photo_container_id;
  identity::ScopeSet scopes{kScopePhotos};
  token_fetcher_ = std::make_unique<identity::PrimaryAccountAccessTokenFetcher>(
      "ntp_backgrounds_service", identity_manager_, scopes,
      base::BindOnce(&NtpBackgroundService::GetAccessTokenForPhotosCallback,
                     base::Unretained(this)),
      identity::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void NtpBackgroundService::GetAccessTokenForPhotosCallback(
    GoogleServiceAuthError error,
    identity::AccessTokenInfo access_token_info) {
  token_fetcher_.reset();

  if (error != GoogleServiceAuthError::AuthErrorNone()) {
    DLOG(WARNING) << "Failed to retrieve token with error: "
                  << error.ToString();
    album_photos_error_info_.error_type = ErrorType::AUTH_ERROR;
    NotifyObservers(FetchComplete::ALBUM_PHOTOS);
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("google_photos_metadata_download",
                                          R"(
        semantics {
          sender: "Desktop NTP Background Selector"
          description:
            "The Chrome Desktop New Tab Page background selector allows "
            "signed-in users to select images from their Google Photos albums. "
            "The photos in an album are obtained from the Backdrop service."
          trigger:
            "Clicking the the settings (gear) icon on the New Tab page."
          data:
            "The Backdrop protocol buffer messages and an access token with "
            "limited scope to obtain the user's Google Photos data."
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

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetAlbumPhotosApiUrl();
  resource_request->method = "GET";
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_AUTH_DATA | net::LOAD_DO_NOT_SEND_COOKIES;
  resource_request->headers.SetHeader(
      kAuthHeaderParam,
      base::StringPrintf(kAuthHeaderValue, access_token_info.token.c_str()));

  albums_photo_info_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  albums_photo_info_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NtpBackgroundService::OnAlbumPhotosFetchComplete,
                     base::Unretained(this)),
      1024 * 1024);
}

void NtpBackgroundService::OnAlbumPhotosFetchComplete(
    std::unique_ptr<std::string> response_body) {
  // The loader will be deleted when the request is handled.
  std::unique_ptr<network::SimpleURLLoader> loader_deleter(
      std::move(albums_photo_info_loader_));

  if (!response_body) {
    // This represents network errors (i.e. the server did not provide a
    // response).
    DLOG(WARNING) << "Request failed with error: "
                  << loader_deleter->NetError();
    album_photos_error_info_.error_type = ErrorType::NET_ERROR;
    album_photos_error_info_.net_error = loader_deleter->NetError();
    NotifyObservers(FetchComplete::ALBUM_PHOTOS);
    return;
  }

  ntp::background::SettingPreviewResponse photos_response;
  if (!photos_response.ParseFromString(*response_body) ||
      photos_response.status() == ntp::background::ErrorCode::SERVER_ERROR) {
    DLOG(WARNING) << "Deserializing personal photos response proto failed.";
    album_photos_error_info_.error_type = ErrorType::SERVICE_ERROR;
    NotifyObservers(FetchComplete::ALBUM_PHOTOS);
    return;
  }

  for (int i = 0; i < photos_response.preview_size(); ++i) {
    album_photos_.emplace_back(
        requested_album_id_, requested_photo_container_id_,
        photos_response.preview(i).preview_url(), image_options_);
  }

  NotifyObservers(FetchComplete::ALBUM_PHOTOS);
}

void NtpBackgroundService::AddObserver(NtpBackgroundServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void NtpBackgroundService::RemoveObserver(
    NtpBackgroundServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool NtpBackgroundService::IsValidBackdropUrl(const GURL& url) const {
  for (auto& image : collection_images_) {
    if (image.image_url == url)
      return true;
  }
  return false;
}

void NtpBackgroundService::AddValidBackdropUrlForTesting(const GURL& url) {
  CollectionImage image;
  image.image_url = url;
  collection_images_.push_back(image);
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
      case FetchComplete::ALBUM_INFO:
        observer.OnAlbumInfoAvailable();
        break;
      case FetchComplete::ALBUM_PHOTOS:
        observer.OnAlbumPhotosAvailable();
        break;
    }
  }
}

GURL NtpBackgroundService::FormatAlbumPhotosBaseApiUrl(
    const std::string& album_id,
    const std::string& photo_container_id) const {
  GURL api_url = photos_api_base_url_;

  api_url = net::AppendQueryParameter(
      api_url, "f.req",
      base::StringPrintf(kPhotosUrlRequestFormat, album_id.c_str(),
                         photo_container_id.c_str()));
  return api_url;
}

GURL NtpBackgroundService::GetAlbumPhotosApiUrl() const {
  return FormatAlbumPhotosBaseApiUrl(requested_album_id_,
                                     requested_photo_container_id_);
}

GURL NtpBackgroundService::GetCollectionsLoadURLForTesting() const {
  return collections_api_url_;
}

GURL NtpBackgroundService::GetImagesURLForTesting() const {
  return collection_images_api_url_;
}

GURL NtpBackgroundService::GetAlbumsURLForTesting() const {
  return albums_api_url_;
}

GURL NtpBackgroundService::GetAlbumPhotosApiUrlForTesting(
    const std::string& album_id,
    const std::string& photo_container_id) const {
  return FormatAlbumPhotosBaseApiUrl(album_id, photo_container_id);
}
