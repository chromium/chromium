// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"

#include <map>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/devicetype.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/wallpaper_private.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace {

// The MIME type of the POST data sent to the server.
constexpr char kProtoMimeType[] = "application/x-protobuf";

// The url to download the proto of the complete list of wallpaper collections.
constexpr char kBackdropCollectionsUrl[] =
    "https://clients3.google.com/cast/chromecast/home/wallpaper/"
    "collections?rt=b";

// The url to download the proto of a specific wallpaper collection.
constexpr char kBackdropImagesUrl[] =
    "https://clients3.google.com/cast/chromecast/home/wallpaper/"
    "collection-images?rt=b";

// The url to download the proto of the info of a surprise me wallpaper.
constexpr char kBackdropSurpriseMeImageUrl[] =
    "https://clients3.google.com/cast/chromecast/home/wallpaper/"
    "image?rt=b";

// The label used to return exclusive content or filter unwanted images.
constexpr char kFilteringLabel[] = "chromebook";

// The label used to return exclusive content for Google branded chromebooks.
constexpr char kGoogleDeviceFilteringLabel[] = "google_branded_chromebook";

// The URL to download the albums in a user's Google Photos library.
constexpr char kGooglePhotosAlbumsUrl[] =
    "https://photosfirstparty-pa.googleapis.com/v1/chromeos/"
    "userCollections:read";

constexpr net::NetworkTrafficAnnotationTag
    kGooglePhotosAlbumsTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("wallpaper_google_photos_albums",
                                            R"(
      semantics {
        sender: "ChromeOS Wallpaper Picker"
        description:
          "Within the Google Photos tile, the ChromeOS Wallpaper Picker "
          "shows the user the Google Photos albums they have created so that "
          "they can pick a photo or turn on the surprise me feature from "
          "within an album. This query fetches those albums."
        trigger: "When the user accesses the Google Photos tile within the "
                 "ChromeOS Wallpaper Picker app."
        data: "OAuth credentials for the user's Google Photos account."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting: "N/A"
        policy_exception_justification:
          "Not implemented, considered not necessary."
      })");

// The URL to download the number of photos in a user's Google Photos library.
constexpr char kGooglePhotosCountUrl[] =
    "https://photosfirstparty-pa.googleapis.com/v1/chromeos/user:read";

constexpr net::NetworkTrafficAnnotationTag kGooglePhotosCountTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("wallpaper_google_photos_count",
                                        R"(
      semantics {
        sender: "ChromeOS Wallpaper Picker"
        description:
          "The ChromeOS Wallpaper Picker displays a tile to view and pick from "
          "a user's Google Photos library. The tile shows the library's number "
          "of photos, which this query fetches."
        trigger: "When the user opens the ChromeOS Wallpaper Picker app."
        data: "OAuth credentials for the user's Google Photos account."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting: "N/A"
        policy_exception_justification:
          "Not implemented, considered not necessary."
      })");

// Returns the corresponding test url if |kTestWallpaperServer| is present,
// otherwise returns |url| as is. See https://crbug.com/914144.
std::string MaybeConvertToTestUrl(std::string url) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kTestWallpaperServer)) {
    base::ReplaceFirstSubstringAfterOffset(&url, 0, "clients3",
                                           "chromecast-dev.sandbox");
  } else if (base::FeatureList::IsEnabled(
                 chromeos::features::kUseWallpaperStagingUrl)) {
    base::ReplaceFirstSubstringAfterOffset(&url, 0, "clients3",
                                           "chromecast-staging.sandbox");
  }
  return url;
}

}  // namespace

namespace wallpaper_handlers {

// Helper class for handling Backdrop service POST requests.
class BackdropFetcher {
 public:
  using OnFetchComplete = base::OnceCallback<void(const std::string& response)>;

  BackdropFetcher() = default;

  BackdropFetcher(const BackdropFetcher&) = delete;
  BackdropFetcher& operator=(const BackdropFetcher&) = delete;

  ~BackdropFetcher() = default;

  // Starts downloading the proto. |request_body| is a serialized proto and
  // will be used as the upload body.
  void Start(const GURL& url,
             const std::string& request_body,
             const net::NetworkTrafficAnnotationTag& traffic_annotation,
             OnFetchComplete callback) {
    DCHECK(!simple_loader_ && callback_.is_null());
    callback_ = std::move(callback);

    SystemNetworkContextManager* system_network_context_manager =
        g_browser_process->system_network_context_manager();
    // In unit tests, the browser process can return a null context manager.
    if (!system_network_context_manager) {
      std::move(callback_).Run(std::string());
      return;
    }

    network::mojom::URLLoaderFactory* loader_factory =
        system_network_context_manager->GetURLLoaderFactory();

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->method = "POST";
    resource_request->load_flags =
        net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

    simple_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    simple_loader_->AttachStringForUpload(request_body, kProtoMimeType);
    // |base::Unretained| is safe because this instance outlives
    // |simple_loader_|.
    simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory, base::BindOnce(&BackdropFetcher::OnURLFetchComplete,
                                       base::Unretained(this)));
  }

 private:
  // Called when the download completes.
  void OnURLFetchComplete(std::unique_ptr<std::string> response_body) {
    if (!response_body) {
      int response_code = -1;
      if (simple_loader_->ResponseInfo() &&
          simple_loader_->ResponseInfo()->headers) {
        response_code =
            simple_loader_->ResponseInfo()->headers->response_code();
      }

      LOG(ERROR) << "Downloading Backdrop wallpaper proto failed with error "
                    "code: "
                 << response_code;
      simple_loader_.reset();
      std::move(callback_).Run(std::string());
      return;
    }

    simple_loader_.reset();
    std::move(callback_).Run(*response_body);
  }

  // The url loader for the Backdrop service request.
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;

  // The fetcher request callback.
  OnFetchComplete callback_;
};

BackdropCollectionInfoFetcher::BackdropCollectionInfoFetcher() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

BackdropCollectionInfoFetcher::~BackdropCollectionInfoFetcher() = default;

void BackdropCollectionInfoFetcher::Start(OnCollectionsInfoFetched callback) {
  DCHECK(!backdrop_fetcher_ && callback_.is_null());
  callback_ = std::move(callback);
  backdrop_fetcher_ = std::make_unique<BackdropFetcher>();

  backdrop::GetCollectionsRequest request;
  // The language field may include the country code (e.g. "en-US").
  request.set_language(g_browser_process->GetApplicationLocale());
  request.add_filtering_label(kFilteringLabel);
  if (chromeos::IsGoogleBrandedDevice())
    request.add_filtering_label(kGoogleDeviceFilteringLabel);
  std::string serialized_proto;
  request.SerializeToString(&serialized_proto);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("wallpaper_backdrop_collection_names",
                                          R"(
        semantics {
          sender: "ChromeOS Wallpaper Picker"
          description:
            "The ChromeOS Wallpaper Picker app displays a rich set of "
            "wallpapers for users to choose from. Each wallpaper belongs to a "
            "collection (e.g. Arts, Landscape etc.). The list of all available "
            "collections is downloaded from the Backdrop wallpaper service."
          trigger: "When the user opens the ChromeOS Wallpaper Picker app."
          data:
            "The Backdrop protocol buffer messages. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "N/A"
          policy_exception_justification:
            "Not implemented, considered not necessary."
        })");

  // |base::Unretained| is safe because this instance outlives
  // |backdrop_fetcher_|.
  backdrop_fetcher_->Start(
      GURL(MaybeConvertToTestUrl(kBackdropCollectionsUrl)), serialized_proto,
      traffic_annotation,
      base::BindOnce(&BackdropCollectionInfoFetcher::OnResponseFetched,
                     base::Unretained(this)));
}

void BackdropCollectionInfoFetcher::OnResponseFetched(
    const std::string& response) {
  std::vector<backdrop::Collection> collections;
  backdrop::GetCollectionsResponse collections_response;
  bool success = false;
  if (response.empty() || !collections_response.ParseFromString(response)) {
    LOG(ERROR) << "Deserializing Backdrop wallpaper proto for collection info "
                  "failed.";
  } else {
    success = true;
    for (const auto& collection : collections_response.collections())
      collections.push_back(collection);
  }

  backdrop_fetcher_.reset();
  std::move(callback_).Run(success, collections);
}

BackdropImageInfoFetcher::BackdropImageInfoFetcher(
    const std::string& collection_id)
    : collection_id_(collection_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

BackdropImageInfoFetcher::~BackdropImageInfoFetcher() = default;

void BackdropImageInfoFetcher::Start(OnImagesInfoFetched callback) {
  DCHECK(!backdrop_fetcher_ && callback_.is_null());
  callback_ = std::move(callback);
  backdrop_fetcher_ = std::make_unique<BackdropFetcher>();

  backdrop::GetImagesInCollectionRequest request;
  // The language field may include the country code (e.g. "en-US").
  request.set_language(g_browser_process->GetApplicationLocale());
  request.set_collection_id(collection_id_);
  request.add_filtering_label(kFilteringLabel);
  if (chromeos::IsGoogleBrandedDevice())
    request.add_filtering_label(kGoogleDeviceFilteringLabel);
  std::string serialized_proto;
  request.SerializeToString(&serialized_proto);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("wallpaper_backdrop_images_info", R"(
        semantics {
          sender: "ChromeOS Wallpaper Picker"
          description:
            "When user clicks on a particular wallpaper collection on the "
            "ChromeOS Wallpaper Picker, it displays the preview of the iamges "
            "and descriptive texts for each image. Such information is "
            "downloaded from the Backdrop wallpaper service."
          trigger:
            "When the ChromeOS Wallpaper Picker app is open and the user "
            "clicks on a particular collection."
          data:
            "The Backdrop protocol buffer messages. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "N/A"
          policy_exception_justification:
            "Not implemented, considered not necessary."
        })");

  // |base::Unretained| is safe because this instance outlives
  // |backdrop_fetcher_|.
  backdrop_fetcher_->Start(
      GURL(MaybeConvertToTestUrl(kBackdropImagesUrl)), serialized_proto,
      traffic_annotation,
      base::BindOnce(&BackdropImageInfoFetcher::OnResponseFetched,
                     base::Unretained(this)));
}

void BackdropImageInfoFetcher::OnResponseFetched(const std::string& response) {
  std::vector<backdrop::Image> images;
  backdrop::GetImagesInCollectionResponse images_response;
  bool success = false;
  if (response.empty() || !images_response.ParseFromString(response)) {
    LOG(ERROR) << "Deserializing Backdrop wallpaper proto for collection "
               << collection_id_ << " failed";
  } else {
    success = true;
    for (const auto& image : images_response.images())
      images.push_back(image);
  }

  backdrop_fetcher_.reset();
  std::move(callback_).Run(success, collection_id_, images);
}

BackdropSurpriseMeImageFetcher::BackdropSurpriseMeImageFetcher(
    const std::string& collection_id,
    const std::string& resume_token)
    : collection_id_(collection_id), resume_token_(resume_token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

BackdropSurpriseMeImageFetcher::~BackdropSurpriseMeImageFetcher() = default;

void BackdropSurpriseMeImageFetcher::Start(OnSurpriseMeImageFetched callback) {
  DCHECK(!backdrop_fetcher_ && callback_.is_null());
  callback_ = std::move(callback);
  backdrop_fetcher_ = std::make_unique<BackdropFetcher>();

  backdrop::GetImageFromCollectionRequest request;
  // The language field may include the country code (e.g. "en-US").
  request.set_language(g_browser_process->GetApplicationLocale());
  request.add_collection_ids(collection_id_);
  request.add_filtering_label(kFilteringLabel);
  if (chromeos::IsGoogleBrandedDevice())
    request.add_filtering_label(kGoogleDeviceFilteringLabel);
  if (!resume_token_.empty())
    request.set_resume_token(resume_token_);
  std::string serialized_proto;
  request.SerializeToString(&serialized_proto);

  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "wallpaper_backdrop_surprise_me_image",
          R"(
        semantics {
          sender: "ChromeOS Wallpaper Picker"
          description:
            "POST request that fetches information about the wallpaper that "
            "should be set next for the user that enabled surprise me feature "
            "in the Chrome OS Wallpaper Picker. For these users, wallpaper is "
            "periodically changed to a random wallpaper selected by the "
            "Backdrop wallpaper service."
          trigger:
            "When the ChromeOS Wallpaper Picker app is open and the user turns "
            "on the surprise me feature."
          data:
            "The Backdrop protocol buffer messages. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "N/A"
          policy_exception_justification:
            "Not implemented, considered not necessary."
        })");

  // |base::Unretained| is safe because this instance outlives
  // |backdrop_fetcher_|.
  backdrop_fetcher_->Start(
      GURL(MaybeConvertToTestUrl(kBackdropSurpriseMeImageUrl)),
      serialized_proto, traffic_annotation,
      base::BindOnce(&BackdropSurpriseMeImageFetcher::OnResponseFetched,
                     base::Unretained(this)));
}

void BackdropSurpriseMeImageFetcher::OnResponseFetched(
    const std::string& response) {
  backdrop::GetImageFromCollectionResponse surprise_me_image_response;
  if (response.empty() ||
      !surprise_me_image_response.ParseFromString(response)) {
    LOG(ERROR) << "Deserializing surprise me wallpaper proto for collection "
               << collection_id_ << " failed";
    backdrop_fetcher_.reset();
    std::move(callback_).Run(/*success=*/false, backdrop::Image(),
                             std::string());
    return;
  }

  backdrop_fetcher_.reset();
  std::move(callback_).Run(/*success=*/true, surprise_me_image_response.image(),
                           surprise_me_image_response.resume_token());
}

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
  identity_manager_observation_.Observe(identity_manager_);
}

template <typename T>
GooglePhotosFetcher<T>::~GooglePhotosFetcher() = default;

template <typename T>
void GooglePhotosFetcher<T>::AddRequestAndStartIfNecessary(
    const std::string& service_url,
    ClientCallback callback) {
  pending_client_callbacks_[service_url].push_back(std::move(callback));
  if (pending_client_callbacks_[service_url].size() > 1)
    return;

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kPhotosModuleOAuth2Scope);

  DCHECK(token_fetchers_.find(service_url) == token_fetchers_.end());
  token_fetchers_[service_url] =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "wallpaper_google_photos_fetcher", identity_manager_, scopes,
          base::BindOnce(
              &GooglePhotosFetcher::OnTokenReceived,
              base::Unretained(this), /*`this` owns `token_fetchers_`.*/
              service_url),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);
}

template <typename T>
void GooglePhotosFetcher<T>::OnTokenReceived(
    const std::string& service_url,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    OnResponseReady(service_url, absl::nullopt);
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = GURL(service_url);
  // Cookies should not be allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + token_info.token);

  DCHECK(url_loaders_.find(service_url) == url_loaders_.end());
  url_loaders_[service_url] = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation_);
  url_loaders_[service_url]->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&GooglePhotosFetcher::OnJsonReceived,
                     base::Unretained(this), /*`this` owns `url_loaders_`.*/
                     service_url));
}

template <typename T>
void GooglePhotosFetcher<T>::OnJsonReceived(
    const std::string& service_url,
    std::unique_ptr<std::string> response_body) {
  const int net_error = url_loaders_[service_url]->NetError();
  if (net_error != net::OK || !response_body) {
    OnResponseReady(service_url, absl::nullopt);
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce([](data_decoder::DataDecoder::ValueOrError result) {
        return std::move(result.value);
      })
          .Then(base::BindOnce(&GooglePhotosFetcher::OnResponseReady,
                               weak_factory_.GetWeakPtr(), service_url)));
}

template <typename T>
void GooglePhotosFetcher<T>::OnResponseReady(
    const std::string& service_url,
    absl::optional<base::Value> response) {
  T args = ParseResponse(std::move(response));
  for (auto& callback : pending_client_callbacks_[service_url])
    std::move(callback).Run(mojo::Clone(args));

  token_fetchers_.erase(service_url);
  url_loaders_.erase(service_url);
  pending_client_callbacks_.erase(service_url);
}

GooglePhotosAlbumsFetcher::GooglePhotosAlbumsFetcher(Profile* profile)
    : GooglePhotosFetcher(profile, kGooglePhotosAlbumsTrafficAnnotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

GooglePhotosAlbumsFetcher::~GooglePhotosAlbumsFetcher() = default;

void GooglePhotosAlbumsFetcher::AddRequestAndStartIfNecessary(
    const absl::optional<std::string>& resume_token,
    base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback) {
  std::string service_url =
      resume_token.has_value()
          ? base::StrCat({kGooglePhotosAlbumsUrl,
                          "?resume_token=", resume_token.value()})
          : kGooglePhotosAlbumsUrl;
  GooglePhotosFetcher::AddRequestAndStartIfNecessary(service_url,
                                                     std::move(callback));
}

GooglePhotosAlbumsCbkArgs GooglePhotosAlbumsFetcher::ParseResponse(
    absl::optional<base::Value> response) {
  auto parsed_response =
      ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponse::New();
  if (!response.has_value())
    return parsed_response;

  const std::string* resume_token = response->FindStringPath("resumeToken");
  if (resume_token && !resume_token->empty())
    parsed_response->resume_token = *resume_token;

  // The photos listed under "item" are the albums' cover photos.
  const base::Value* response_photos = response->FindListPath("item");
  if (!response_photos)
    return parsed_response;

  // Populate the ID -> URL mapping for the each album's cover photo.
  std::map<std::string, std::string> cover_photo_urls_by_id;
  for (const auto& response_photo : response_photos->GetList()) {
    const std::string* id = response_photo.FindStringPath("itemId.mediaKey");
    const std::string* url = response_photo.FindStringPath("photo.servingUrl");
    if (id && url)
      cover_photo_urls_by_id.emplace(*id, *url);
  }

  const base::Value* response_albums = response->FindListPath("collection");
  if (!response_albums)
    return parsed_response;

  parsed_response->albums =
      std::vector<ash::personalization_app::mojom::GooglePhotosAlbumPtr>();
  for (const auto& response_album : response_albums->GetList()) {
    const std::string* album_id =
        response_album.FindStringPath("collectionId.mediaKey");
    const std::string* title = response_album.FindStringPath("name");
    const std::string* num_photos_string =
        response_album.FindStringPath("numPhotos");

    // TODO(b/214577469): Get cover photo URL directly from `response_album`.
    const std::string* cover_photo_id =
        response_album.FindStringPath("coverItemId.mediaKey");
    auto cover_photo_url_iter =
        cover_photo_id ? cover_photo_urls_by_id.find(*cover_photo_id)
                       : cover_photo_urls_by_id.end();

    int64_t num_photos;
    if (!album_id || !title || !num_photos_string ||
        !base::StringToInt64(*num_photos_string, &num_photos) ||
        num_photos == 0 ||
        cover_photo_url_iter == cover_photo_urls_by_id.end()) {
      continue;
    }

    parsed_response->albums->push_back(
        ash::personalization_app::mojom::GooglePhotosAlbum::New(
            *album_id, *title, base::saturated_cast<int>(num_photos),
            GURL(cover_photo_url_iter->second)));
  }
  return parsed_response;
}

GooglePhotosCountFetcher::GooglePhotosCountFetcher(Profile* profile)
    : GooglePhotosFetcher(profile, kGooglePhotosCountTrafficAnnotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

GooglePhotosCountFetcher::~GooglePhotosCountFetcher() = default;

void GooglePhotosCountFetcher::AddRequestAndStartIfNecessary(
    base::OnceCallback<void(int)> callback) {
  GooglePhotosFetcher::AddRequestAndStartIfNecessary(kGooglePhotosCountUrl,
                                                     std::move(callback));
}

int GooglePhotosCountFetcher::ParseResponse(
    absl::optional<base::Value> response) {
  if (!response.has_value())
    return -1;

  const base::Value* user = response->FindDictPath("user");
  if (!user)
    return -1;

  const std::string* count_string = user->FindStringKey("numPhotos");
  int64_t count;
  if (!count_string || !base::StringToInt64(*count_string, &count))
    return -1;

  return base::saturated_cast<int>(count);
}

}  // namespace wallpaper_handlers
