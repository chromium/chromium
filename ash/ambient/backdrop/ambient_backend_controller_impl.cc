// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/backdrop/ambient_backend_controller_impl.h"

#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_photo_cache.h"
#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_language.h"
#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chromeos/assistant/internal/ambient/backdrop_client_config.h"
#include "chromeos/assistant/internal/ambient/utils.h"
#include "chromeos/assistant/internal/proto/backdrop/backdrop.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {

namespace {

using BackdropClientConfig = chromeos::ambient::BackdropClientConfig;

constexpr char kProtoMimeType[] = "application/protobuf";

constexpr int kMaxPreviewImages = 4;

// Max body size in bytes to download.
constexpr int kMaxBodySizeBytes = 1 * 1024 * 1024;  // 1 MiB

constexpr net::NetworkTrafficAnnotationTag kAmbientBackendControllerNetworkTag =
    net::DefineNetworkTrafficAnnotation("ambient_backend_controller", R"(
        semantics {
          sender: "Ambient photo"
          description:
            "Download ambient image weather icon from Google."
          trigger:
            "Triggered periodically when the battery is charged and the user "
            "is idle."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "assistive-eng@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2023-01-13"
        }
        policy {
         cookies_allowed: NO
         setting:
           "This feature is off by default and can be overridden by user."
         policy_exception_justification:
           "This feature is set by user settings.ambient_mode.enabled pref. "
           "The user setting is per device and cannot be overriden by admin."
        })");

std::string GetClientId() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  DCHECK(prefs);

  std::string client_id =
      prefs->GetString(ambient::prefs::kAmbientBackdropClientId);
  if (client_id.empty()) {
    client_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
    prefs->SetString(ambient::prefs::kAmbientBackdropClientId, client_id);
  }

  return client_id;
}

std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
    const BackdropClientConfig::Request& request) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(request.url);
  resource_request->method = request.method;
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  auto language_tag = ash::GetLanguageTag();
  if (!language_tag.empty()) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAcceptLanguage, language_tag);
  }

  for (const auto& header : request.headers) {
    std::string encoded_value;
    if (header.needs_base_64_encoded)
      encoded_value = base::Base64Encode(header.value);
    else
      encoded_value = header.value;

    resource_request->headers.SetHeader(header.name, encoded_value);
  }

  return resource_request;
}

std::string BuildBackdropTopicDetails(
    const backdrop::ScreenUpdate::Topic& backdrop_topic) {
  std::string result;
  if (backdrop_topic.has_metadata_line_1())
    result += backdrop_topic.metadata_line_1();

  if (backdrop_topic.has_metadata_line_2()) {
    if (!result.empty())
      result += " ";
    result += backdrop_topic.metadata_line_2();
  }
  // Do not include metadata_line_3.
  return result;
}

::ambient::TopicType ToAmbientModeTopicType(
    const backdrop::ScreenUpdate_Topic& topic) {
  if (!topic.has_topic_type())
    return ::ambient::TopicType::kOther;

  switch (topic.topic_type()) {
    case backdrop::CURATED:
      return ::ambient::TopicType::kCurated;
    case backdrop::PERSONAL_PHOTO:
      return ::ambient::TopicType::kPersonal;
    case backdrop::FEATURED_PHOTO:
      return ::ambient::TopicType::kFeatured;
    case backdrop::GEO_PHOTO:
      return ::ambient::TopicType::kGeo;
    case backdrop::CULTURAL_INSTITUTE:
      return ::ambient::TopicType::kCulturalInstitute;
    case backdrop::RSS_TOPIC:
      return ::ambient::TopicType::kRss;
    case backdrop::CAPTURED_ON_PIXEL:
      return ::ambient::TopicType::kCapturedOnPixel;
    default:
      return ::ambient::TopicType::kOther;
  }
}

std::optional<std::string> GetStringValue(const base::Value::List& values,
                                          size_t field_number) {
  if (values.empty() || values.size() < field_number)
    return std::nullopt;

  const base::Value& v = values[field_number - 1];
  if (!v.is_string())
    return std::nullopt;

  return v.GetString();
}

std::optional<double> GetDoubleValue(const base::Value::List& values,
                                     size_t field_number) {
  if (values.empty() || values.size() < field_number)
    return std::nullopt;

  const base::Value& v = values[field_number - 1];
  if (!v.is_double() && !v.is_int())
    return std::nullopt;

  return v.GetDouble();
}

std::optional<bool> GetBoolValue(const base::Value::List& values,
                                 size_t field_number) {
  if (values.empty() || values.size() < field_number)
    return std::nullopt;

  const base::Value& v = values[field_number - 1];
  if (v.is_bool())
    return v.GetBool();

  if (v.is_int())
    return v.GetInt() > 0;

  return std::nullopt;
}

std::optional<WeatherInfo> ToWeatherInfo(const base::Value& result) {
  DCHECK(result.is_list());
  if (!result.is_list())
    return std::nullopt;

  WeatherInfo weather_info;
  const auto& list_result = result.GetList();

  weather_info.condition_description = GetStringValue(
      list_result, backdrop::WeatherInfo::kConditionDescriptionFieldNumber);
  weather_info.condition_icon_url = GetStringValue(
      list_result, backdrop::WeatherInfo::kConditionIconUrlFieldNumber);
  weather_info.temp_f =
      GetDoubleValue(list_result, backdrop::WeatherInfo::kTempFFieldNumber);
  weather_info.show_celsius =
      GetBoolValue(list_result, backdrop::WeatherInfo::kShowCelsiusFieldNumber)
          .value_or(false);

  return weather_info;
}

// Helper function to save the information we got from the backdrop server to a
// public struct so that they can be accessed by public codes.
ScreenUpdate ToScreenUpdate(
    const backdrop::ScreenUpdate& backdrop_screen_update) {
  ScreenUpdate screen_update;
  // Parse |AmbientModeTopic|.
  for (const auto& backdrop_topic : backdrop_screen_update.next_topics()) {
    DCHECK(backdrop_topic.has_url());

    auto topic_type = ToAmbientModeTopicType(backdrop_topic);
    if (!ambient::util::IsAmbientModeTopicTypeAllowed(topic_type)) {
      DVLOG(3) << "Filtering topic_type: "
               << backdrop::TopicSource_Name(backdrop_topic.topic_type());
      continue;
    }

    AmbientModeTopic ambient_topic;
    ambient_topic.topic_type = topic_type;

    // If the |portrait_image_url| field is not empty, we assume the image is
    // portrait.
    if (backdrop_topic.has_portrait_image_url()) {
      ambient_topic.url = backdrop_topic.portrait_image_url();
      ambient_topic.is_portrait = true;
    } else {
      ambient_topic.url = backdrop_topic.url();
    }

    if (backdrop_topic.has_related_topic()) {
      if (backdrop_topic.related_topic().has_portrait_image_url()) {
        ambient_topic.related_image_url =
            backdrop_topic.related_topic().portrait_image_url();
      } else {
        ambient_topic.related_image_url = backdrop_topic.related_topic().url();
      }
    }
    ambient_topic.details = BuildBackdropTopicDetails(backdrop_topic);
    ambient_topic.related_details =
        BuildBackdropTopicDetails(backdrop_topic.related_topic());
    screen_update.next_topics.emplace_back(ambient_topic);
  }

  // Parse |WeatherInfo|.
  if (backdrop_screen_update.has_weather_info()) {
    backdrop::WeatherInfo backdrop_weather_info =
        backdrop_screen_update.weather_info();
    WeatherInfo weather_info;
    if (backdrop_weather_info.has_condition_icon_url()) {
      weather_info.condition_icon_url =
          backdrop_weather_info.condition_icon_url();
    }

    if (backdrop_weather_info.has_temp_f())
      weather_info.temp_f = backdrop_weather_info.temp_f();

    if (backdrop_weather_info.has_show_celsius())
      weather_info.show_celsius = backdrop_weather_info.show_celsius();

    screen_update.weather_info = weather_info;
  }
  return screen_update;
}

// Helper function to extract image URLs from screen update response for
// screen saver preview.
std::vector<GURL> ToPreviewUrls(const ScreenUpdate& screen_update) {
  std::vector<GURL> preview_urls;
  for (const auto& topic : screen_update.next_topics) {
    preview_urls.emplace_back(topic.url);
    if (preview_urls.size() == kMaxPreviewImages) {
      break;
    }
  }
  return preview_urls;
}

bool IsArtSettingVisible(const ArtSetting& art_setting) {
  const auto& album_id = art_setting.album_id;

  return album_id == kAmbientModeEarthAndSpaceAlbumId ||
         album_id == kAmbientModeFeaturedPhotoAlbumId;
}

}  // namespace

// Helper class for handling Backdrop service requests.
class BackdropURLLoader {
 public:
  BackdropURLLoader() = default;
  BackdropURLLoader(const BackdropURLLoader&) = delete;
  BackdropURLLoader& operator=(const BackdropURLLoader&) = delete;
  ~BackdropURLLoader() = default;

  // Starts downloading the proto. |request_body| is a serialized proto and
  // will be used as the upload body if it is a POST request.
  void Start(
      std::unique_ptr<network::ResourceRequest> resource_request,
      const std::optional<std::string>& request_body,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      network::SimpleURLLoader::BodyAsStringCallbackDeprecated callback) {
    // No ongoing downloading task.
    DCHECK(!simple_loader_);

    loader_factory_ = AmbientClient::Get()->GetURLLoaderFactory();
    simple_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    if (request_body)
      simple_loader_->AttachStringForUpload(*request_body, kProtoMimeType);

    // |base::Unretained| is safe because this instance outlives
    // |simple_loader_|.
    simple_loader_->DownloadToString(
        loader_factory_.get(),
        base::BindOnce(&BackdropURLLoader::OnUrlDownloaded,
                       base::Unretained(this), std::move(callback)),
        kMaxBodySizeBytes);
  }

 private:
  // Called when the download completes.
  void OnUrlDownloaded(
      network::SimpleURLLoader::BodyAsStringCallbackDeprecated callback,
      std::unique_ptr<std::string> response_body) {
    loader_factory_.reset();

    if (simple_loader_->NetError() == net::OK && response_body) {
      simple_loader_.reset();
      std::move(callback).Run(std::move(response_body));
      return;
    }

    int response_code = -1;
    if (simple_loader_->ResponseInfo() &&
        simple_loader_->ResponseInfo()->headers) {
      response_code = simple_loader_->ResponseInfo()->headers->response_code();
    }

    LOG(ERROR) << "Downloading Backdrop proto failed with error code: "
               << response_code << " with network error"
               << simple_loader_->NetError();
    simple_loader_.reset();
    std::move(callback).Run(std::make_unique<std::string>());
  }

  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
};

AmbientBackendControllerImpl::AmbientBackendControllerImpl()
    : backdrop_client_config_(ash::AmbientClient::Get()->ShouldUseProdServer()
                                  ? BackdropClientConfig::ServerType::kProd
                                  : BackdropClientConfig::ServerType::kDev) {}

AmbientBackendControllerImpl::~AmbientBackendControllerImpl() = default;

void AmbientBackendControllerImpl::FetchScreenUpdateInfo(
    int num_topics,
    bool show_pair_personal_portraits,
    const gfx::Size& screen_size,
    OnScreenUpdateInfoFetchedCallback callback) {
  Shell::Get()->ambient_controller()->RequestAccessToken(base::BindOnce(
      &AmbientBackendControllerImpl::FetchScreenUpdateInfoInternal,
      weak_factory_.GetWeakPtr(), num_topics, show_pair_personal_portraits,
      screen_size, std::move(callback)));
}

void AmbientBackendControllerImpl::FetchPreviewImages(
    const gfx::Size& preview_size,
    OnPreviewImagesFetchedCallback callback) {
  constexpr int num_topics = kMaxPreviewImages;
  OnScreenUpdateInfoFetchedCallback combined_callback =
      base::BindOnce(&ToPreviewUrls).Then(std::move(callback));
  Shell::Get()->ambient_controller()->RequestAccessToken(base::BindOnce(
      &AmbientBackendControllerImpl::FetchScreenUpdateInfoInternal,
      weak_factory_.GetWeakPtr(), num_topics, false, preview_size,
      std::move(combined_callback)));
}

void AmbientBackendControllerImpl::GetSettings(GetSettingsCallback callback) {
  Shell::Get()->ambient_controller()->RequestAccessToken(
      base::BindOnce(&AmbientBackendControllerImpl::StartToGetSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AmbientBackendControllerImpl::UpdateSettings(
    const AmbientSettings settings,
    UpdateSettingsCallback callback) {
  auto* ambient_controller = Shell::Get()->ambient_controller();

  // Clear disk cache when Settings changes.
  // TODO(wutao): Use observer pattern. Need to future narrow down
  // the clear up only on albums changes, not on temperature unit
  // changes.
  ambient_photo_cache::Clear(ambient_photo_cache::Store::kPrimary);

  ambient_controller->RequestAccessToken(base::BindOnce(
      &AmbientBackendControllerImpl::StartToUpdateSettings,
      weak_factory_.GetWeakPtr(), settings, std::move(callback)));
}

void AmbientBackendControllerImpl::FetchPersonalAlbums(
    int banner_width,
    int banner_height,
    int num_albums,
    const std::string& resume_token,
    OnPersonalAlbumsFetchedCallback callback) {
  Shell::Get()->ambient_controller()->RequestAccessToken(
      base::BindOnce(&AmbientBackendControllerImpl::FetchPersonalAlbumsInternal,
                     weak_factory_.GetWeakPtr(), banner_width, banner_height,
                     num_albums, resume_token, std::move(callback)));
}

void AmbientBackendControllerImpl::FetchWeather(
    std::optional<std::string> weather_client_id,
    FetchWeatherCallback callback) {
  auto response_handler =
      [](FetchWeatherCallback callback,
         std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
         std::unique_ptr<std::string> response) {
        constexpr char kJsonPrefix[] = ")]}'\n";

        if (response && response->length() > strlen(kJsonPrefix)) {
          auto json_handler =
              [](FetchWeatherCallback callback,
                 data_decoder::DataDecoder::ValueOrError result) {
                if (result.has_value()) {
                  std::move(callback).Run(ToWeatherInfo(*result));
                } else {
                  DVLOG(1) << "Failed to parse weather json.";
                  std::move(callback).Run(std::nullopt);
                }
              };

          data_decoder::DataDecoder::ParseJsonIsolated(
              response->substr(strlen(kJsonPrefix)),
              base::BindOnce(json_handler, std::move(callback)));
        } else {
          std::move(callback).Run(std::nullopt);
        }
      };

  const auto* user = user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(user->HasGaiaAccount());
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateFetchWeatherInfoRequest(
          user->GetAccountId().GetGaiaId(), GetClientId(), weather_client_id);
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateResourceRequest(request);
  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(std::move(resource_request), /*request_body=*/std::nullopt,
                    kAmbientBackendControllerNetworkTag,
                    base::BindOnce(response_handler, std::move(callback),
                                   std::move(backdrop_url_loader)));
}

const std::array<const char*, 2>&
AmbientBackendControllerImpl::GetBackupPhotoUrls() const {
  return chromeos::ambient::kBackupPhotoUrls;
}

std::array<const char*, 2>
AmbientBackendControllerImpl::GetTimeOfDayVideoPreviewImageUrls(
    AmbientVideo video) const {
  return chromeos::ambient::GetTimeOfDayVideoPreviewImageUrls(video);
}

const char* AmbientBackendControllerImpl::GetPromoBannerUrl() const {
  return chromeos::ambient::kTimeOfDayBannerImageUrl;
}

const char* AmbientBackendControllerImpl::GetTimeOfDayProductName() const {
  return chromeos::ambient::kTimeOfDayProductName;
}

void AmbientBackendControllerImpl::FetchScreenUpdateInfoInternal(
    int num_topics,
    bool show_pair_personal_portraits,
    const gfx::Size& screen_size,
    OnScreenUpdateInfoFetchedCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    LOG(ERROR) << "Failed to fetch access token for ScreenUpdate";
    std::move(callback).Run(ash::ScreenUpdate());
    return;
  }

  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateFetchScreenUpdateRequest({
          {/*gaia_id*/ gaia_id,
           /*token*/ access_token,
           /*client_id*/ GetClientId()},
          /*num_topics*/ num_topics,
          /*show_pair_personal_portraits*/ show_pair_personal_portraits,
      });
  auto resource_request = CreateResourceRequest(request);

  DCHECK(!screen_size.IsEmpty());
  resource_request->url =
      net::AppendQueryParameter(resource_request->url, "device-screen-width",
                                base::NumberToString(screen_size.width()));
  resource_request->url =
      net::AppendQueryParameter(resource_request->url, "device-screen-height",
                                base::NumberToString(screen_size.height()));

  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), request.body,
      kAmbientBackendControllerNetworkTag,
      base::BindOnce(&AmbientBackendControllerImpl::OnScreenUpdateInfoFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnScreenUpdateInfoFetched(
    OnScreenUpdateInfoFetchedCallback callback,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  // Parse the |ScreenUpdate| out from the response string.
  // Note that the |backdrop_screen_update| can be an empty instance if the
  // parsing has failed.
  backdrop::ScreenUpdate backdrop_screen_update =
      BackdropClientConfig::ParseScreenUpdateFromResponse(*response);

  // Store the information to a public struct and notify the caller.
  auto screen_update = ToScreenUpdate(backdrop_screen_update);
  std::move(callback).Run(screen_update);
}

void AmbientBackendControllerImpl::StartToGetSettings(
    GetSettingsCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    std::move(callback).Run(/*topic_source=*/std::nullopt);
    return;
  }

  std::string client_id = GetClientId();
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateGetSettingsRequest(gaia_id, access_token,
                                                       client_id);
  auto resource_request = CreateResourceRequest(request);

  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), request.body,
      kAmbientBackendControllerNetworkTag,
      base::BindOnce(&AmbientBackendControllerImpl::OnGetSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnGetSettings(
    GetSettingsCallback callback,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  auto settings = BackdropClientConfig::ParseGetSettingsResponse(*response);
  // |art_settings| should not be empty if parsed successfully.
  if (settings.art_settings.empty()) {
    std::move(callback).Run(std::nullopt);
  } else {
    for (auto& art_setting : settings.art_settings) {
      art_setting.visible = IsArtSettingVisible(art_setting);
      art_setting.enabled = art_setting.enabled && art_setting.visible;
    }
    std::move(callback).Run(settings);
  }
}

void AmbientBackendControllerImpl::StartToUpdateSettings(
    const AmbientSettings& settings,
    UpdateSettingsCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    std::move(callback).Run(/*success=*/false, settings);
    return;
  }

  std::string client_id = GetClientId();
  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateUpdateSettingsRequest(gaia_id, access_token,
                                                          client_id, settings);
  auto resource_request = CreateResourceRequest(request);

  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), request.body,
      kAmbientBackendControllerNetworkTag,
      base::BindOnce(&AmbientBackendControllerImpl::OnUpdateSettings,
                     weak_factory_.GetWeakPtr(), std::move(callback), settings,
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnUpdateSettings(
    UpdateSettingsCallback callback,
    const AmbientSettings& settings,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  const bool success =
      BackdropClientConfig::ParseUpdateSettingsResponse(*response);

  if (success) {
    // Store information about the ambient mode settings in a user pref so that
    // it can be uploaded as a histogram.
    Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetInteger(
        ambient::prefs::kAmbientModePhotoSourcePref,
        static_cast<int>(ambient::AmbientSettingsToPhotoSource(settings)));
  }

  std::move(callback).Run(success, settings);
}

void AmbientBackendControllerImpl::FetchPersonalAlbumsInternal(
    int banner_width,
    int banner_height,
    int num_albums,
    const std::string& resume_token,
    OnPersonalAlbumsFetchedCallback callback,
    const std::string& gaia_id,
    const std::string& access_token) {
  if (gaia_id.empty() || access_token.empty()) {
    DVLOG(2) << "Failed to fetch access token";
    // Returns an empty instance to indicate the failure.
    std::move(callback).Run(ash::PersonalAlbums());
    return;
  }

  BackdropClientConfig::Request request =
      backdrop_client_config_.CreateFetchPersonalAlbumsRequest(
          banner_width, banner_height, num_albums, resume_token, gaia_id,
          access_token);
  std::unique_ptr<network::ResourceRequest> resource_request =
      CreateResourceRequest(request);
  auto backdrop_url_loader = std::make_unique<BackdropURLLoader>();
  auto* loader_ptr = backdrop_url_loader.get();
  loader_ptr->Start(
      std::move(resource_request), /*request_body=*/std::nullopt,
      kAmbientBackendControllerNetworkTag,
      base::BindOnce(&AmbientBackendControllerImpl::OnPersonalAlbumsFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(backdrop_url_loader)));
}

void AmbientBackendControllerImpl::OnPersonalAlbumsFetched(
    OnPersonalAlbumsFetchedCallback callback,
    std::unique_ptr<BackdropURLLoader> backdrop_url_loader,
    std::unique_ptr<std::string> response) {
  DCHECK(backdrop_url_loader);

  // Parse the |PersonalAlbumsResponse| out from the response string.
  // Note that the |personal_albums| can be an empty instance if the parsing has
  // failed.
  ash::PersonalAlbums personal_albums =
      BackdropClientConfig::ParsePersonalAlbumsResponse(*response);
  std::move(callback).Run(std::move(personal_albums));
}

void AmbientBackendControllerImpl::FetchSettingsAndAlbums(
    int banner_width,
    int banner_height,
    int num_albums,
    OnSettingsAndAlbumsFetchedCallback callback) {
  auto on_done = base::BarrierClosure(
      /*num_callbacks=*/2,
      base::BindOnce(&AmbientBackendControllerImpl::OnSettingsAndAlbumsFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));

  GetSettings(base::BindOnce(&AmbientBackendControllerImpl::OnSettingsFetched,
                             weak_factory_.GetWeakPtr(), on_done));

  FetchPersonalAlbums(
      banner_width, banner_height, num_albums, /*resume_token=*/"",
      base::BindOnce(&AmbientBackendControllerImpl::OnAlbumsFetched,
                     weak_factory_.GetWeakPtr(), on_done));
}

void AmbientBackendControllerImpl::OnSettingsFetched(
    base::RepeatingClosure on_done,
    const std::optional<ash::AmbientSettings>& settings) {
  settings_ = settings;
  std::move(on_done).Run();
}

void AmbientBackendControllerImpl::OnAlbumsFetched(
    base::RepeatingClosure on_done,
    ash::PersonalAlbums personal_albums) {
  personal_albums_ = std::move(personal_albums);
  std::move(on_done).Run();
}

void AmbientBackendControllerImpl::OnSettingsAndAlbumsFetched(
    OnSettingsAndAlbumsFetchedCallback callback) {
  std::move(callback).Run(settings_, std::move(personal_albums_));
}

}  // namespace ash
