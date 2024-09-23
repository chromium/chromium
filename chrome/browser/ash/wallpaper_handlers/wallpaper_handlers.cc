// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/devicetype.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_language.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers_metric_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_prefs.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace wallpaper_handlers {
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

// The label used to return exclusive Time of Day wallpapers.
constexpr char kTimeOfDayFilteringLabel[] = "chromebook_time_of_day";

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
    base::ReplaceFirstSubstringAfterOffset(&url, 0, "clients3", "clients1");
  } else if (base::FeatureList::IsEnabled(
                 ash::features::kUseWallpaperStagingUrl)) {
    base::ReplaceFirstSubstringAfterOffset(&url, 0, "clients3", "clients2");
  }
  return url;
}

// Attempts to parse `photo` as a `GooglePhotosPhoto`. If successful, adds the
// parsed photo to `parsed_response`.
void AddGooglePhotosPhotoIfValid(
    ash::personalization_app::mojom::FetchGooglePhotosPhotosResponsePtr&
        parsed_response,
    const base::Value::Dict* photo) {
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

// Returns the `GooglePhotosApi` associated with the specified `url`.
std::optional<GooglePhotosApi> ToGooglePhotosApi(const GURL& url) {
  const std::string& spec = url.spec();
  if (base::StartsWith(spec, kGooglePhotosEnabledUrl)) {
    return GooglePhotosApi::kGetEnabled;
  }
  if (base::StartsWith(spec, kGooglePhotosAlbumUrl)) {
    return GooglePhotosApi::kGetAlbum;
  }
  if (base::StartsWith(spec, kGooglePhotosAlbumsUrl)) {
    return GooglePhotosApi::kGetAlbums;
  }
  if (base::StartsWith(spec, kGooglePhotosSharedAlbumsUrl)) {
    return GooglePhotosApi::kGetSharedAlbums;
  }
  if (base::StartsWith(spec, kGooglePhotosPhotoUrl)) {
    return GooglePhotosApi::kGetPhoto;
  }
  if (base::StartsWith(spec, kGooglePhotosPhotosUrl)) {
    return GooglePhotosApi::kGetPhotos;
  }
  return std::nullopt;
}

}  // namespace

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
  if (ash::IsGoogleBrandedDevice()) {
    request.add_filtering_label(kGoogleDeviceFilteringLabel);
  }
  if (ash::features::IsTimeOfDayWallpaperEnabled()) {
    request.add_filtering_label(kTimeOfDayFilteringLabel);
  }
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
  if (ash::IsGoogleBrandedDevice()) {
    request.add_filtering_label(kGoogleDeviceFilteringLabel);
  }
  if (ash::features::IsTimeOfDayWallpaperEnabled()) {
    request.add_filtering_label(kTimeOfDayFilteringLabel);
  }
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
  if (ash::IsGoogleBrandedDevice()) {
    request.add_filtering_label(kGoogleDeviceFilteringLabel);
  }
  if (ash::features::IsTimeOfDayWallpaperEnabled()) {
    request.add_filtering_label(kTimeOfDayFilteringLabel);
  }
  if (!resume_token_.empty()) {
    request.set_resume_token(resume_token_);
  }
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

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kPhotosModuleOAuth2Scope);

  auto fetcher = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "wallpaper_google_photos_fetcher", identity_manager_, scopes,
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
      signin::ConsentLevel::kSignin);
  auto* fetcher_ptr = fetcher.get();
  fetcher_ptr->Start(base::BindOnce(
      &GooglePhotosFetcher::OnTokenReceived, weak_factory_.GetWeakPtr(),
      std::move(fetcher), service_url, /*start_time=*/base::TimeTicks::Now()));
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
    base::TimeTicks start_time,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Failed to authenticate Google Photos API request to "
               << service_url.spec() << ". Error message: " << error.ToString();
    OnResponseReady(service_url, start_time, std::nullopt);
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
                     weak_factory_.GetWeakPtr(), std::move(loader), service_url,
                     start_time));
}

template <typename T>
void GooglePhotosFetcher<T>::OnJsonReceived(
    std::unique_ptr<network::SimpleURLLoader> loader,
    const GURL& service_url,
    base::TimeTicks start_time,
    std::unique_ptr<std::string> response_body) {
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
    OnResponseReady(service_url, start_time, std::move(error_response));
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(
          [](const GURL& service_url,
             data_decoder::DataDecoder::ValueOrError result)
              -> std::optional<base::Value> {
            if (!result.has_value()) {
              LOG(ERROR) << "Failed to parse JSON response from Google Photos "
                            "API request to "
                         << service_url.spec()
                         << ". Error message: " << result.error();
              return std::nullopt;
            }
            return std::move(*result);
          },
          service_url)
          .Then(base::BindOnce(&GooglePhotosFetcher::OnResponseReady,
                               weak_factory_.GetWeakPtr(), service_url,
                               start_time)));
}

template <typename T>
void GooglePhotosFetcher<T>::OnResponseReady(
    const GURL& service_url,
    base::TimeTicks start_time,
    std::optional<base::Value> response) {
  auto result =
      ParseResponse(response.has_value() ? response->GetIfDict() : nullptr);

  if (auto api = ToGooglePhotosApi(service_url)) {
    RecordGooglePhotosApiResponseParsed(
        api.value(), /*response_time=*/base::TimeTicks::Now() - start_time,
        GetResultCount(result));
  } else {
    NOTREACHED_IN_MIGRATION()
        << "Google Photos API request made to an unrecognized endpoint: "
        << service_url.spec();
  }

  for (auto& callback : pending_client_callbacks_[service_url])
    std::move(callback).Run(mojo::Clone(result));
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

GooglePhotosAlbumsFetcher::~GooglePhotosAlbumsFetcher() {
  // Records Ash.Wallpaper.GooglePhotos.Api.GetAlbums.RefreshCount metric
  // at the end of the session.
  RecordGooglePhotosApiRefreshCount(GooglePhotosApi::kGetAlbums,
                                    albums_api_refresh_counter_);
}

void GooglePhotosAlbumsFetcher::AddRequestAndStartIfNecessary(
    const std::optional<std::string>& resume_token,
    base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback) {
  GURL service_url = GURL(kGooglePhotosAlbumsUrl);
  if (resume_token.has_value()) {
    service_url = net::AppendQueryParameter(service_url, "resume_token",
                                            resume_token.value());
    // Increase the refresh counter every time the user scrolls down to the
    // bottom of the page to fetch more albums with a valid refresh token.
    albums_api_refresh_counter_++;
  }
  GooglePhotosFetcher::AddRequestAndStartIfNecessary(service_url,
                                                     std::move(callback));
}

GooglePhotosAlbumsCbkArgs GooglePhotosAlbumsFetcher::ParseResponse(
    const base::Value::Dict* response) {
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

std::optional<size_t> GooglePhotosAlbumsFetcher::GetResultCount(
    const GooglePhotosAlbumsCbkArgs& result) {
  return result && result->albums ? std::make_optional(result->albums->size())
                                  : std::nullopt;
}

GooglePhotosSharedAlbumsFetcher::GooglePhotosSharedAlbumsFetcher(
    Profile* profile)
    : GooglePhotosFetcher(profile, kGooglePhotosAlbumsTrafficAnnotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

GooglePhotosSharedAlbumsFetcher::~GooglePhotosSharedAlbumsFetcher() {
  // Records Ash.Wallpaper.GooglePhotos.Api.GetSharedAlbums.RefreshCount metric
  // at the end of the session.
  RecordGooglePhotosApiRefreshCount(GooglePhotosApi::kGetSharedAlbums,
                                    shared_albums_api_refresh_counter_);
}

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
    // Increase the refresh counter every time the user scrolls down to the
    // bottom of the page to fetch more shared albums with a valid refresh
    // token.
    shared_albums_api_refresh_counter_++;
  }
  GooglePhotosFetcher::AddRequestAndStartIfNecessary(service_url,
                                                     std::move(callback));
}

GooglePhotosAlbumsCbkArgs GooglePhotosSharedAlbumsFetcher::ParseResponse(
    const base::Value::Dict* response) {
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

std::optional<size_t> GooglePhotosSharedAlbumsFetcher::GetResultCount(
    const GooglePhotosAlbumsCbkArgs& result) {
  return result && result->albums ? std::make_optional(result->albums->size())
                                  : std::nullopt;
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
    const base::Value::Dict* response) {
  if (!response) {
    return GooglePhotosEnablementState::kError;
  }

  const auto* state = response->FindStringByDottedPath("status.userState");

  if (!state) {
    LOG(ERROR) << "Failed to parse Google Photos enabled response.";
    return GooglePhotosEnablementState::kError;
  }

  return *state == "USER_PERMITTED"
             ? GooglePhotosEnablementState::kEnabled
             : *state == "USER_DASHER_DISABLED"
                   ? GooglePhotosEnablementState::kDisabled
                   : GooglePhotosEnablementState::kError;
}

std::optional<size_t> GooglePhotosEnabledFetcher::GetResultCount(
    const GooglePhotosEnablementState& result) {
  return result != GooglePhotosEnablementState::kError ? std::make_optional(1u)
                                                       : std::nullopt;
}

GooglePhotosPhotosFetcher::GooglePhotosPhotosFetcher(Profile* profile)
    : GooglePhotosFetcher(profile, kGooglePhotosPhotosTrafficAnnotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

GooglePhotosPhotosFetcher::~GooglePhotosPhotosFetcher() {
  // Records Ash.Wallpaper.GooglePhotos.Api.GetPhotos.RefreshCount metric
  // at the end of the session.
  RecordGooglePhotosApiRefreshCount(GooglePhotosApi::kGetPhotos,
                                    photos_api_refresh_counter_);
}
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
    // Increase the refresh counter every time the user scrolls down to the
    // bottom of the page to fetch more photos with a valid refresh token.
    photos_api_refresh_counter_++;
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
    auto empty_list_response =
        base::Value::Dict().Set("item", base::Value::List());
    return base::Value(std::move(empty_list_response));
  }
  return std::nullopt;
}

GooglePhotosPhotosCbkArgs GooglePhotosPhotosFetcher::ParseResponse(
    const base::Value::Dict* response) {
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

std::optional<size_t> GooglePhotosPhotosFetcher::GetResultCount(
    const GooglePhotosPhotosCbkArgs& result) {
  return result && result->photos ? std::make_optional(result->photos->size())
                                  : std::nullopt;
}

}  // namespace wallpaper_handlers
