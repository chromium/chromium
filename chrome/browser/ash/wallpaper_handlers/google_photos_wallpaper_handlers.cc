// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/google_photos_wallpaper_handlers.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/wallpaper/wallpaper_utils/wallpaper_language.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace wallpaper_handlers {

namespace {

// The URL to download an album's photos from a user's Google Photos library.
constexpr char kGooglePhotosAlbumUrl[] =
    "https://photosfirstparty-pa.googleapis.com/v1/chromeos/"
    "collectionById:read";

// The collectionById endpoint accepts a "return_order" parameter that
// determines what order the photos are returned in, using these values.
constexpr char kGooglePhotosAlbumCollectionOrder[] = "1";
constexpr char kGooglePhotosAlbumShuffledOrder[] = "2";

// The URL to download the owned albums in a user's Google Photos library.
constexpr char kGooglePhotosAlbumsUrl[] =
    "https://photosfirstparty-pa.googleapis.com/v1/chromeos/"
    "userCollections:read";

// The URL to download the shared albums in a user's Google Photos library.
constexpr char kGooglePhotosSharedAlbumsUrl[] =
    "https://photosfirstparty-pa.googleapis.com/v1/chromeos/"
    "sharedCollections:read";

constexpr net::NetworkTrafficAnnotationTag
    kGooglePhotosAlbumsTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("wallpaper_google_photos_albums",
                                            R"(
      semantics {
        sender: "ChromeOS Wallpaper Picker"
        description:
          "Within the Google Photos tile, the ChromeOS Wallpaper Picker "
          "shows the user the Google Photos albums they have created and "
          "the Google Photos albums they shared with other users so that "
          "they can pick a photo or turn on the daily refresh feature from "
          "within an album. This query fetches those albums."
        trigger: "When the user accesses the Google Photos tile within the "
                 "ChromeOS Wallpaper Picker app."
        data: "OAuth credentials for the user's Google Photos account."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "cros-p13n-eng@google.com"
          }
        }
        user_data {
          type: ACCESS_TOKEN
        }
        last_reviewed: "2025-02-07"
      }
      policy {
        cookies_allowed: NO
        setting: "N/A"
        policy_exception_justification:
          "Not implemented, considered not necessary."
      })");

// The URL to download whether the user is allowed to access Google Photos data.
constexpr char kGooglePhotosEnabledUrl[] =
    "https://photosfirstparty-pa.googleapis.com/v1/chromeos/userenabled:read";

constexpr net::NetworkTrafficAnnotationTag
    kGooglePhotosEnabledTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("wallpaper_google_photos_enabled",
                                            R"(
      semantics {
        sender: "ChromeOS Wallpaper Picker"
        description:
          "The ChromeOS Wallpaper Picker displays a tile to view and pick from "
          "a user's Google Photos library. This tile should not display any "
          "user data if there is an enterprise setting preventing the user "
          "from accessing Google Photos."
        trigger: "When the user opens the ChromeOS Wallpaper Picker app."
        data: "OAuth credentials for the user's Google Photos account."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "cros-p13n-eng@google.com"
          }
        }
        user_data {
          type: ACCESS_TOKEN
        }
        last_reviewed: "2025-02-07"
      }
      policy {
        cookies_allowed: NO
        setting: "N/A"
        policy_exception_justification:
          "Not implemented, considered not necessary."
      })");

// The URL to download a photo from a user's Google Photos library.
constexpr char kGooglePhotosPhotoUrl[] =
    "https://photosfirstparty-pa.googleapis.com/v1/chromeos/itemById:read";

// The URL to download all visible photos in a user's Google Photos library.
constexpr char kGooglePhotosPhotosUrl[] =
    "https://photosfirstparty-pa.googleapis.com/v1/chromeos/userItems:read";

constexpr net::NetworkTrafficAnnotationTag
    kGooglePhotosPhotosTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("wallpaper_google_photos_photos",
                                            R"(
      semantics {
        sender: "ChromeOS Wallpaper Picker"
        description:
          "Within the Google Photos tile, the ChromeOS Wallpaper Picker "
          "shows the user all the visible photos in their Google Photos "
          "library so that they can pick one as their wallpaper. "
          "Alternatively, the user can select an album within the Google "
          "Photos tile to pick a photo from there. This query fetches photos "
          "from one of those sources. This query might also fetch a single "
          "photo that has already been designated as a device's wallpaper."
        trigger: "When the user accesses the Google Photos tile or selects a "
                 "wallpaper photo within the ChromeOS Wallpaper Picker app, or "
                 "when a device is notified of a new Google Photos wallpaper "
                 "via cross-device sync."
        data: "OAuth credentials for the user's Google Photos account."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "cros-p13n-eng@google.com"
          }
        }
        user_data {
          type: ACCESS_TOKEN
        }
        last_reviewed: "2025-02-07"
      }
      policy {
        cookies_allowed: NO
        setting: "N/A"
        policy_exception_justification:
          "Not implemented, considered not necessary."
      })");

// Attempts to parse `photo` as a `GooglePhotosPhoto`. If successful, adds the
// parsed photo to `parsed_response`.
void AddGooglePhotosPhotoIfValid(
    ash::personalization_app::mojom::FetchGooglePhotosPhotosResponsePtr&
        parsed_response,
    const base::DictValue* photo) {
  if (!photo) {
    return;
  }

  const auto* id = photo->FindStringByDottedPath("itemId.mediaKey");
  const auto* dedup_key = photo->FindString("dedupKey");
  const auto* filename = photo->FindString("filename");
  const auto* timestamp_string = photo->FindString("creationTimestamp");
  const auto* url = photo->FindStringByDottedPath("photo.servingUrl");
  const auto* location_list =
      photo->FindListByDottedPath("locationFeature.name");
  const auto* location_wrapper = location_list && !location_list->empty()
                                     ? location_list->front().GetIfDict()
                                     : nullptr;
  const auto* location =
      location_wrapper ? location_wrapper->FindString("text") : nullptr;

  base::Time timestamp;
  if (!id || !filename || !timestamp_string ||
      !base::Time::FromUTCString(timestamp_string->c_str(), &timestamp) ||
      !url) {
    LOG(ERROR) << "Failed to parse item in Google Photos photos response.";
    return;
  }

  std::string name = base::FilePath(*filename).RemoveExtension().value();
  std::u16string date = base::TimeFormatFriendlyDate(timestamp);

  // A photo from Google Photos may or may not have a location set.
  parsed_response->photos->push_back(
      ash::personalization_app::mojom::GooglePhotosPhoto::New(
          *id, dedup_key ? std::make_optional(*dedup_key) : std::nullopt, name,
          date, GURL(*url),
          location ? std::make_optional(*location) : std::nullopt));
}
}  // namespace

template <typename T>
GooglePhotosFetcher<T>::GooglePhotosFetcher(
    Profile* profile,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      traffic_annotation_(traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(identity_manager_);
  identity_manager_observation_.Observe(identity_manager_.get());
}

template <typename T>
GooglePhotosFetcher<T>::~GooglePhotosFetcher() = default;

template <typename T>
void GooglePhotosFetcher<T>::AddRequestAndStartIfNecessary(
    const GURL& service_url,
    ClientCallback callback) {
  pending_client_callbacks_[service_url].push_back(std::move(callback));
  if (pending_client_callbacks_[service_url].size() > 1) {
    return;
  }

  auto fetcher = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      signin::OAuthConsumerId::kWallpaperGooglePhotosFetcher, identity_manager_,
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
      signin::ConsentLevel::kSignin);
  auto* fetcher_ptr = fetcher.get();
  fetcher_ptr->Start(base::BindOnce(&GooglePhotosFetcher::OnTokenReceived,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(fetcher), service_url));
}

template <typename T>
std::optional<base::Value> GooglePhotosFetcher<T>::CreateErrorResponse(
    int error_code) {
  return std::nullopt;
}

template <typename T>
void GooglePhotosFetcher<T>::OnTokenReceived(
    std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> fetcher,
    const GURL& service_url,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Failed to authenticate Google Photos API request to "
               << service_url.spec() << ". Error message: " << error.ToString();
    OnResponseReady(service_url, std::nullopt);
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";

  // By default, the server will not serialize repeated proto fields if they are
  // empty. This makes it impossible for us to tell if the server has broken
  // compatibility with the client or just has nothing to return. Append a
  // special parameter to request that the server always serializes repeated
  // proto fields and any other fields which might otherwise be omitted by
  // default (https://cloud.google.com/apis/docs/system-parameters#definitions).
  resource_request->url = net::AppendOrReplaceQueryParameter(
      service_url, "$outputDefaults", "true");

  // Cookies should not be allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + token_info.token);
  auto language_tag = ash::GetLanguageTag();
  if (!language_tag.empty()) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAcceptLanguage, language_tag);
  }

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation_);
  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&GooglePhotosFetcher::OnJsonReceived,
                     weak_factory_.GetWeakPtr(), std::move(loader),
                     service_url));
}

template <typename T>
void GooglePhotosFetcher<T>::OnJsonReceived(
    std::unique_ptr<network::SimpleURLLoader> loader,
    const GURL& service_url,
    std::optional<std::string> response_body) {
  const int net_error = loader->NetError();
  if (net_error != net::OK || !response_body) {
    LOG(ERROR) << "Google Photos API request to " << service_url.spec()
               << " failed.";
    auto* response_info = loader->ResponseInfo();
    std::optional<base::Value> error_response;
    if (response_info && response_info->headers) {
      LOG(ERROR) << "HTTP response code: "
                 << response_info->headers->response_code();
      error_response =
          CreateErrorResponse(response_info->headers->response_code());
    }
    OnResponseReady(service_url, std::move(error_response));
    return;
  }

  // JSONReader is now safe for rule of 2.
  base::JSONReader::Result result =
      base::JSONReader::ReadAndReturnValueWithError(*response_body,
                                                    base::JSON_PARSE_RFC);
  if (!result.has_value()) {
    LOG(ERROR) << "Failed to parse JSON response from Google Photos "
                  "API request to "
               << service_url.spec()
               << ". Error message: " << result.error().message;
    OnResponseReady(service_url, std::nullopt);
    return;
  }

  OnResponseReady(service_url, std::move(*result));
}

template <typename T>
void GooglePhotosFetcher<T>::OnResponseReady(
    const GURL& service_url,
    std::optional<base::Value> response) {
  auto result =
      ParseResponse(response.has_value() ? response->GetIfDict() : nullptr);

  for (auto& callback : pending_client_callbacks_[service_url]) {
    std::move(callback).Run(mojo::Clone(result));
  }
  pending_client_callbacks_.erase(service_url);
}

template <typename T>
bool GooglePhotosFetcher<T>::IsGooglePhotosIntegrationPolicyEnabled() const {
  PrefService* pref_service = profile_->GetPrefs();
  return pref_service->GetBoolean(
      prefs::kWallpaperGooglePhotosIntegrationEnabled);
}

GooglePhotosAlbumsFetcher::GooglePhotosAlbumsFetcher(Profile* profile)
    : GooglePhotosFetcher(profile, kGooglePhotosAlbumsTrafficAnnotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

GooglePhotosAlbumsFetcher::~GooglePhotosAlbumsFetcher() = default;

void GooglePhotosAlbumsFetcher::AddRequestAndStartIfNecessary(
    const std::optional<std::string>& resume_token,
    base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback) {
  GURL service_url = GURL(kGooglePhotosAlbumsUrl);
  if (resume_token.has_value()) {
    service_url = net::AppendQueryParameter(service_url, "resume_token",
                                            resume_token.value());
  }
  GooglePhotosFetcher::AddRequestAndStartIfNecessary(service_url,
                                                     std::move(callback));
}

GooglePhotosAlbumsCbkArgs GooglePhotosAlbumsFetcher::ParseResponse(
    const base::DictValue* response) {
  auto parsed_response =
      ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse::New();
  if (!response) {
    return parsed_response;
  }

  const auto* resume_token = response->FindString("resumeToken");
  if (resume_token && !resume_token->empty()) {
    parsed_response->resume_token = *resume_token;
  }

  const auto* response_albums = response->FindList("collection");
  if (!response_albums) {
    return parsed_response;
  }

  parsed_response->albums =
      std::vector<ash::personalization_app::mojom::GooglePhotosAlbumPtr>();
  for (const auto& untyped_response_album : *response_albums) {
    DCHECK(untyped_response_album.is_dict());
    const auto& response_album = untyped_response_album.GetDict();
    const auto* album_id =
        response_album.FindStringByDottedPath("collectionId.mediaKey");
    const auto* title = response_album.FindString("name");
    const auto* num_photos_string = response_album.FindString("numPhotos");
    const auto* cover_photo_url =
        response_album.FindString("coverItemServingUrl");
    const auto* timestamp_string =
        response_album.FindString("latestModificationTimestamp");

    int64_t num_photos;
    base::Time timestamp;
    if (!album_id || !title || !num_photos_string ||
        !base::StringToInt64(*num_photos_string, &num_photos) ||
        num_photos < 1 || !cover_photo_url || !timestamp_string ||
        !base::Time::FromUTCString(timestamp_string->c_str(), &timestamp)) {
      LOG(ERROR) << "Failed to parse item in Google Photos albums response.";
      continue;
    }

    parsed_response->albums->push_back(
        ash::personalization_app::mojom::GooglePhotosAlbum::New(
            *album_id, *title, base::saturated_cast<int>(num_photos),
            GURL(*cover_photo_url), timestamp, /*is_shared=*/false));
  }
  return parsed_response;
}

GooglePhotosSharedAlbumsFetcher::GooglePhotosSharedAlbumsFetcher(
    Profile* profile)
    : GooglePhotosFetcher(profile, kGooglePhotosAlbumsTrafficAnnotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

GooglePhotosSharedAlbumsFetcher::~GooglePhotosSharedAlbumsFetcher() = default;

void GooglePhotosSharedAlbumsFetcher::AddRequestAndStartIfNecessary(
    const std::optional<std::string>& resume_token,
    base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback) {
  if (!IsGooglePhotosIntegrationPolicyEnabled()) {
    DVLOG(1) << __FUNCTION__
             << ": Skipping due to disabled policy: "
                "WallpaperGooglePhotosIntegrationEnabled.";
    std::move(callback).Run(ash::personalization_app::mojom::
                                FetchGooglePhotosAlbumsResponse::New());
    return;
  }

  GURL service_url = GURL(kGooglePhotosSharedAlbumsUrl);
  if (resume_token.has_value()) {
    service_url = net::AppendQueryParameter(service_url, "resume_token",
                                            resume_token.value());
  }
  GooglePhotosFetcher::AddRequestAndStartIfNecessary(service_url,
                                                     std::move(callback));
}

GooglePhotosAlbumsCbkArgs GooglePhotosSharedAlbumsFetcher::ParseResponse(
    const base::DictValue* response) {
  auto parsed_response =
      ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse::New();
  if (!response) {
    return parsed_response;
  }

  const auto* resume_token = response->FindString("resumeToken");
  if (resume_token && !resume_token->empty()) {
    parsed_response->resume_token = *resume_token;
  }

  const auto* response_albums = response->FindList("collection");
  if (!response_albums) {
    return parsed_response;
  }

  parsed_response->albums =
      std::vector<ash::personalization_app::mojom::GooglePhotosAlbumPtr>();
  for (const auto& untyped_response_album : *response_albums) {
    DCHECK(untyped_response_album.is_dict());
    const auto& response_album = untyped_response_album.GetDict();
    const auto* album_id =
        response_album.FindStringByDottedPath("collectionId.mediaKey");
    const auto* title = response_album.FindString("name");
    const auto* cover_photo_url =
        response_album.FindString("coverItemServingUrl");
    const auto* timestamp_string =
        response_album.FindString("latestModificationTimestamp");

    base::Time timestamp;
    if (!album_id || !title || !cover_photo_url || !timestamp_string ||
        !base::Time::FromUTCString(timestamp_string->c_str(), &timestamp)) {
      LOG(ERROR) << "Failed to parse item in Google Photos albums response.";
      continue;
    }

    // `num_image` is always 0 for shared albums.
    parsed_response->albums->push_back(
        ash::personalization_app::mojom::GooglePhotosAlbum::New(
            *album_id, *title, /*num_image=*/0, GURL(*cover_photo_url),
            timestamp, /*is_shared=*/true));
  }
  return parsed_response;
}

GooglePhotosEnabledFetcher::GooglePhotosEnabledFetcher(Profile* profile)
    : GooglePhotosFetcher(profile, kGooglePhotosEnabledTrafficAnnotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

GooglePhotosEnabledFetcher::~GooglePhotosEnabledFetcher() = default;

void GooglePhotosEnabledFetcher::AddRequestAndStartIfNecessary(
    base::OnceCallback<void(GooglePhotosEnablementState)> callback) {
  if (!IsGooglePhotosIntegrationPolicyEnabled()) {
    DVLOG(1) << __FUNCTION__
             << ": Skipping due to disabled policy: "
                "WallpaperGooglePhotosIntegrationEnabled.";
    std::move(callback).Run(GooglePhotosEnablementState::kDisabled);
    return;
  }

  GooglePhotosFetcher::AddRequestAndStartIfNecessary(
      GURL(kGooglePhotosEnabledUrl), std::move(callback));
}

GooglePhotosEnablementState GooglePhotosEnabledFetcher::ParseResponse(
    const base::DictValue* response) {
  if (!response) {
    return GooglePhotosEnablementState::kError;
  }

  const auto* state = response->FindStringByDottedPath("status.userState");

  if (!state) {
    LOG(ERROR) << "Failed to parse Google Photos enabled response.";
    return GooglePhotosEnablementState::kError;
  }

  return *state == "USER_PERMITTED" ? GooglePhotosEnablementState::kEnabled
         : *state == "USER_DASHER_DISABLED"
             ? GooglePhotosEnablementState::kDisabled
             : GooglePhotosEnablementState::kError;
}

GooglePhotosPhotosFetcher::GooglePhotosPhotosFetcher(Profile* profile)
    : GooglePhotosFetcher(profile, kGooglePhotosPhotosTrafficAnnotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

GooglePhotosPhotosFetcher::~GooglePhotosPhotosFetcher() = default;

void GooglePhotosPhotosFetcher::AddRequestAndStartIfNecessary(
    const std::optional<std::string>& item_id,
    const std::optional<std::string>& album_id,
    const std::optional<std::string>& resume_token,
    bool shuffle,
    base::OnceCallback<void(GooglePhotosPhotosCbkArgs)> callback) {
  if (!IsGooglePhotosIntegrationPolicyEnabled()) {
    DVLOG(1) << __FUNCTION__
             << ": Skipping due to disabled policy: "
                "WallpaperGooglePhotosIntegrationEnabled.";
    auto parsed_response =
        ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse::New();
    parsed_response->photos =
        std::vector<ash::personalization_app::mojom::GooglePhotosPhotoPtr>();
    std::move(callback).Run(mojo::Clone(parsed_response));
    return;
  }

  GURL service_url;
  if (item_id.has_value()) {
    DCHECK(!album_id.has_value() && !resume_token.has_value() && !shuffle);
    service_url = net::AppendQueryParameter(GURL(kGooglePhotosPhotoUrl),
                                            "item_id", item_id.value());
  } else if (album_id.has_value()) {
    service_url = net::AppendQueryParameter(GURL(kGooglePhotosAlbumUrl),
                                            "collection_id", album_id.value());
    service_url =
        net::AppendQueryParameter(service_url, "return_order",
                                  shuffle ? kGooglePhotosAlbumShuffledOrder
                                          : kGooglePhotosAlbumCollectionOrder);
  } else {
    DCHECK(!shuffle);
    service_url = GURL(kGooglePhotosPhotosUrl);
  }

  if (resume_token.has_value()) {
    service_url = net::AppendQueryParameter(service_url, "resume_token",
                                            resume_token.value());
  }
  GooglePhotosFetcher::AddRequestAndStartIfNecessary(service_url,
                                                     std::move(callback));
}

std::optional<base::Value> GooglePhotosPhotosFetcher::CreateErrorResponse(
    int error_code) {
  // If the server gives us 404, that means the request succeeded, but no
  // photos with the given attributes exist. We return an empty list of photos
  // to communicate this back to the caller.
  if (error_code == net::HTTP_NOT_FOUND) {
    auto empty_list_response = base::DictValue().Set("item", base::ListValue());
    return base::Value(std::move(empty_list_response));
  }
  return std::nullopt;
}

GooglePhotosPhotosCbkArgs GooglePhotosPhotosFetcher::ParseResponse(
    const base::DictValue* response) {
  auto parsed_response =
      ash::personalization_app::mojom::FetchGooglePhotosPhotosResponse::New();
  if (!response) {
    return parsed_response;
  }

  const auto* resume_token = response->FindString("resumeToken");
  if (resume_token && !resume_token->empty()) {
    parsed_response->resume_token = *resume_token;
  }

  // The `base::Value` at key "item" can be a single photos or a list of photos.
  const auto* photo_or_photos = response->Find("item");
  if (!photo_or_photos) {
    return parsed_response;
  }

  parsed_response->photos =
      std::vector<ash::personalization_app::mojom::GooglePhotosPhotoPtr>();
  if (auto* photos = photo_or_photos->GetIfList()) {
    for (const auto& photo : *photos) {
      AddGooglePhotosPhotoIfValid(parsed_response, photo.GetIfDict());
    }
  } else if (auto* photo = photo_or_photos->GetIfDict()) {
    AddGooglePhotosPhotoIfValid(parsed_response, photo);
  }
  return parsed_response;
}

}  // namespace wallpaper_handlers
