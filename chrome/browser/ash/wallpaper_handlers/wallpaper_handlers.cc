// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"

#include <tuple>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/devicetype.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/numerics/safe_conversions.h"
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
        data: "None."
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
          chromeos::switches::kTestWallpaperServer)) {
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

template <typename... Args>
GooglePhotosFetcher<Args...>::GooglePhotosFetcher(
    Profile* profile,
    const char* service_url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      service_url_(service_url),
      traffic_annotation_(traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(identity_manager_);
  DCHECK(service_url_);
  identity_manager_observation_.Observe(identity_manager_);
}

template <typename... Args>
GooglePhotosFetcher<Args...>::~GooglePhotosFetcher() = default;

template <typename... Args>
void GooglePhotosFetcher<Args...>::AddCallbackAndStartIfNecessary(
    ClientCallback callback) {
  pending_client_callbacks_.push_back(std::move(callback));
  if (pending_client_callbacks_.size() > 1)
    return;

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kPhotosModuleOAuth2Scope);

  DCHECK(!token_fetcher_);
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "wallpaper_google_photos_fetcher", identity_manager_, scopes,
      base::BindOnce(&GooglePhotosFetcher::OnTokenReceived,
                     base::Unretained(this) /*`this` owns `token_fetcher_`.*/),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
      signin::ConsentLevel::kSignin);
}

template <typename... Args>
void GooglePhotosFetcher<Args...>::OnTokenReceived(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    OnResponseReady(absl::nullopt);
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = GURL(service_url_);
  // Cookies should not be allowed.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      "application/json");
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + token_info.token);

  DCHECK(!url_loader_);
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation_);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(&GooglePhotosFetcher::OnJsonReceived,
                     base::Unretained(this) /*`this` owns `url_loader_`.*/));
}

template <typename... Args>
void GooglePhotosFetcher<Args...>::OnJsonReceived(
    std::unique_ptr<std::string> response_body) {
  const int net_error = url_loader_->NetError();
  url_loader_.reset();

  if (net_error != net::OK || !response_body) {
    OnResponseReady(absl::nullopt);
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce([](data_decoder::DataDecoder::ValueOrError result) {
        return std::move(result.value);
      })
          .Then(base::BindOnce(&GooglePhotosFetcher::OnResponseReady,
                               weak_factory_.GetWeakPtr())));
}

template <typename... Args>
void GooglePhotosFetcher<Args...>::OnResponseReady(
    absl::optional<base::Value> response) {
  std::tuple<Args...> args = ParseResponse(std::move(response));
  std::apply(
      [&](Args... args) {
        for (auto& callback : pending_client_callbacks_)
          std::move(callback).Run(std::forward<Args>(args)...);
        pending_client_callbacks_.clear();
      },
      args);
}

GooglePhotosCountFetcher::GooglePhotosCountFetcher(Profile* profile)
    : GooglePhotosFetcher(profile,
                          kGooglePhotosCountUrl,
                          kGooglePhotosCountTrafficAnnotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

GooglePhotosCountFetcher::~GooglePhotosCountFetcher() = default;

std::tuple<int> GooglePhotosCountFetcher::ParseResponse(
    absl::optional<base::Value> response) {
  if (!response.has_value())
    return std::make_tuple(-1);

  const base::Value* user = response->FindDictPath("user");
  if (!user)
    return std::make_tuple(-1);

  const std::string* count_string = user->FindStringKey("numPhotos");
  int64_t count;
  if (!count_string || !base::StringToInt64(*count_string, &count))
    return std::make_tuple(-1);

  return std::make_tuple(base::saturated_cast<int>(count));
}

}  // namespace wallpaper_handlers
