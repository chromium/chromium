// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_custom_background_service.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/background/ntp_custom_background_service_observer.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

const char kNtpCustomBackgroundURL[] = "background_url";
const char kNtpCustomBackgroundAttributionLine1[] = "attribution_line_1";
const char kNtpCustomBackgroundAttributionLine2[] = "attribution_line_2";
const char kNtpCustomBackgroundAttributionActionURL[] =
    "attribution_action_url";
const char kNtpCustomBackgroundCollectionId[] = "collection_id";
const char kNtpCustomBackgroundResumeToken[] = "resume_token";
const char kNtpCustomBackgroundRefreshTimestamp[] = "refresh_timestamp";
const char kNtpCustomBackgroundMainColor[] = "background_main_color";

constexpr char kSidePanelSnapshotImageOptions[] = "=w320-h180-p-k-no-nd-mv";

base::Value::Dict GetBackgroundInfoAsDict(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const absl::optional<std::string>& collection_id,
    const absl::optional<std::string>& resume_token,
    const absl::optional<int> refresh_timestamp) {
  base::Value::Dict background_info;
  background_info.Set(kNtpCustomBackgroundURL,
                      base::Value(background_url.spec()));
  background_info.Set(kNtpCustomBackgroundAttributionLine1,
                      base::Value(attribution_line_1));
  background_info.Set(kNtpCustomBackgroundAttributionLine2,
                      base::Value(attribution_line_2));
  background_info.Set(kNtpCustomBackgroundAttributionActionURL,
                      base::Value(action_url.spec()));
  background_info.Set(kNtpCustomBackgroundCollectionId,
                      base::Value(collection_id.value_or("")));
  background_info.Set(kNtpCustomBackgroundResumeToken,
                      base::Value(resume_token.value_or("")));
  background_info.Set(kNtpCustomBackgroundRefreshTimestamp,
                      base::Value(refresh_timestamp.value_or(0)));

  return background_info;
}

base::Value::Dict GetBackgroundInfoWithColor(
    const base::Value::Dict* background_info,
    const SkColor color) {
  base::Value::Dict new_background_info = background_info->Clone();
  new_background_info.Set(kNtpCustomBackgroundMainColor,
                          base::Value(static_cast<int>(color)));
  return new_background_info;
}

base::Value::Dict NtpCustomBackgroundDefaults() {
  base::Value::Dict defaults;
  defaults.Set(kNtpCustomBackgroundURL, base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundAttributionLine1,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundAttributionLine2,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundAttributionActionURL,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundCollectionId,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundResumeToken,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundRefreshTimestamp,
               base::Value(base::Value::Type::INTEGER));
  return defaults;
}

void CopyFileToProfilePath(const base::FilePath& from_path,
                           const base::FilePath& profile_path) {
  base::CopyFile(from_path,
                 profile_path.AppendASCII(
                     chrome::kChromeUIUntrustedNewTabPageBackgroundFilename));
}

void RemoveLocalBackgroundImageCopy(Profile* profile) {
  base::FilePath path = profile->GetPath().AppendASCII(
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::GetDeleteFileCallback(path));
}

// |GetBitmapMainColor| just wraps |CalculateKMeanColorOfBitmap|.
// As |CalculateKMeanColorOfBitmap| is overloaded, it cannot be bind for async
// call.
SkColor GetBitmapMainColor(const SkBitmap& bitmap) {
  return color_utils::CalculateKMeanColorOfBitmap(bitmap);
}

}  // namespace

// static
void NtpCustomBackgroundService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      prefs::kNtpCustomBackgroundDict, NtpCustomBackgroundDefaults(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kNtpCustomBackgroundLocalToDevice,
                                false);
}

// static
void NtpCustomBackgroundService::ResetProfilePrefs(Profile* profile) {
  profile->GetPrefs()->ClearPref(prefs::kNtpCustomBackgroundDict);
  profile->GetPrefs()->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice,
                                  false);
  RemoveLocalBackgroundImageCopy(profile);
}

NtpCustomBackgroundService::NtpCustomBackgroundService(Profile* profile)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      clock_(base::DefaultClock::GetInstance()),
      background_updated_timestamp_(base::TimeTicks::Now()) {
  background_service_ = NtpBackgroundServiceFactory::GetForProfile(profile_);
  theme_service_ = ThemeServiceFactory::GetForProfile(profile_);
  if (background_service_)
    background_service_observation_.Observe(background_service_.get());

  // Update theme info when the pref is changed via Sync.
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kNtpCustomBackgroundDict,
      base::BindRepeating(&NtpCustomBackgroundService::UpdateBackgroundFromSync,
                          weak_ptr_factory_.GetWeakPtr()));

  image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<ImageDecoderImpl>(),
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}

NtpCustomBackgroundService::~NtpCustomBackgroundService() = default;

void NtpCustomBackgroundService::Shutdown() {
  for (NtpCustomBackgroundServiceObserver& observer : observers_)
    observer.OnNtpCustomBackgroundServiceShuttingDown();
}

void NtpCustomBackgroundService::OnCollectionInfoAvailable() {}

void NtpCustomBackgroundService::OnCollectionImagesAvailable() {}

void NtpCustomBackgroundService::OnNextCollectionImageAvailable() {
  auto image = background_service_->next_image();
  std::string attribution1;
  std::string attribution2;
  if (image.attribution.size() > 0)
    attribution1 = image.attribution[0];
  if (image.attribution.size() > 1)
    attribution2 = image.attribution[1];

  std::string resume_token = background_service_->next_image_resume_token();
  int64_t timestamp = (clock_->Now() + base::Days(1)).ToTimeT();

  if (base::FeatureList::IsEnabled(
          ntp_features::kCustomizeChromeColorExtraction)) {
    FetchCustomBackgroundAndExtractBackgroundColor(image.image_url,
                                                   image.thumbnail_image_url);
  }

  base::Value::Dict background_info = GetBackgroundInfoAsDict(
      image.image_url, attribution1, attribution2, image.attribution_action_url,
      image.collection_id, resume_token, timestamp);

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pref_service_->SetDict(prefs::kNtpCustomBackgroundDict,
                         std::move(background_info));
}

void NtpCustomBackgroundService::OnNtpBackgroundServiceShuttingDown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  background_service_observation_.Reset();
  background_service_ = nullptr;
}

void NtpCustomBackgroundService::UpdateBackgroundFromSync() {
  // Any incoming change to synced background data should clear the local image.
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  RemoveLocalBackgroundImageCopy(profile_);
  NotifyAboutBackgrounds();
}

void NtpCustomBackgroundService::ResetCustomBackgroundInfo() {
  SetCustomBackgroundInfo(GURL(), GURL(), std::string(), std::string(), GURL(),
                          std::string());
}

void NtpCustomBackgroundService::UpdateCustomBackgroundColorAsync(
    const GURL& image_url,
    const gfx::Image& fetched_image,
    const image_fetcher::RequestMetadata& metadata) {
  if (metadata.http_response_code ==
      image_fetcher::RequestMetadata::ResponseCode::RESPONSE_CODE_INVALID) {
    return;
  }
  // Calculate the bitmap color asynchronously as it is slow (1-2 seconds for
  // the thumbnail). However, prefs should be updated on the main thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetBitmapMainColor, fetched_image.AsBitmap()),
      base::BindOnce(
          &NtpCustomBackgroundService::UpdateCustomBackgroundPrefsWithColor,
          weak_ptr_factory_.GetWeakPtr(), image_url));
}

void NtpCustomBackgroundService::FetchCustomBackgroundAndExtractBackgroundColor(
    const GURL& image_url,
    const GURL& fetch_url) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ntp_custom_background",
                                          R"(
          semantics {
            sender: "Desktop Chrome background fetcher"
            description:
              "Fetch New Tab Page background image for color calculation."
            trigger:
              "User selects new collection image background on the New Tab "
              "Page."
            data: "The only data sent is the URL to an image."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts {
                email: "chrome-desktop-ntp@google.com"
              }
            }
            user_data {
              type: NONE
            }
            last_reviewed: "2023-01-09"
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users cannot disable this feature. The feature is enabled by "
              "default."
            chrome_policy {
              NTPCustomBackgroundEnabled {
                NTPCustomBackgroundEnabled: true
              }
            }
          })");

  image_fetcher::ImageFetcherParams params(traffic_annotation,
                                           "NtpCustomBackgrounds");
  image_fetcher_->FetchImage(
      fetch_url,
      base::BindOnce(
          &NtpCustomBackgroundService::UpdateCustomBackgroundColorAsync,
          weak_ptr_factory_.GetWeakPtr(), image_url),
      std::move(params));
}

void NtpCustomBackgroundService::UpdateCustomBackgroundPrefsWithColor(
    const GURL& image_url,
    SkColor color) {
  // Update background color only if the selected background is still the same.
  const base::Value::Dict& background_info =
      pref_service_->GetDict(prefs::kNtpCustomBackgroundDict);

  GURL current_bg_url(
      background_info.Find(kNtpCustomBackgroundURL)->GetString());
  if (current_bg_url == image_url) {
    pref_service_->SetDict(prefs::kNtpCustomBackgroundDict,
                           GetBackgroundInfoWithColor(&background_info, color));
    theme_service_->BuildAutogeneratedThemeFromColor(color);
  }
}

void NtpCustomBackgroundService::SetCustomBackgroundInfo(
    const GURL& background_url,
    const GURL& thumbnail_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const std::string& collection_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsCustomBackgroundDisabledByPolicy()) {
    return;
  }
  // Store current background info before it is changed so it can be used if
  // RevertBackgroundChanges is called.
  if (previous_background_info_ == absl::nullopt) {
    previous_background_info_ = absl::make_optional(
        pref_service_->GetValue(prefs::kNtpCustomBackgroundDict).Clone());
    previous_local_background_ = false;
  }

  bool is_backdrop_collection =
      background_service_ &&
      background_service_->IsValidBackdropCollection(collection_id);
  bool is_backdrop_url =
      background_service_ &&
      background_service_->IsValidBackdropUrl(background_url);

  bool need_forced_refresh =
      pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice) &&
      pref_service_->FindPreference(prefs::kNtpCustomBackgroundDict)
          ->IsDefaultValue();
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  RemoveLocalBackgroundImageCopy(profile_);

  background_updated_timestamp_ = base::TimeTicks::Now();

  if (!background_url.is_valid() && !collection_id.empty() &&
      is_backdrop_collection) {
    background_service_->FetchNextCollectionImage(collection_id, absl::nullopt);
  } else if (background_url.is_valid() && is_backdrop_url) {
    if (base::FeatureList::IsEnabled(
            ntp_features::kCustomizeChromeColorExtraction) &&
        thumbnail_url.is_valid()) {
      FetchCustomBackgroundAndExtractBackgroundColor(background_url,
                                                     thumbnail_url);
    }
    base::Value::Dict background_info = GetBackgroundInfoAsDict(
        background_url, attribution_line_1, attribution_line_2, action_url,
        collection_id, absl::nullopt, absl::nullopt);
    pref_service_->SetDict(prefs::kNtpCustomBackgroundDict,
                           std::move(background_info));
  } else {
    pref_service_->ClearPref(prefs::kNtpCustomBackgroundDict);

    // If this device was using a local image and did not have a non-local
    // background saved, UpdateBackgroundFromSync will not fire. Therefore, we
    // need to force a refresh here.
    if (need_forced_refresh) {
      NotifyAboutBackgrounds();
    }
  }
}

void NtpCustomBackgroundService::SelectLocalBackgroundImage(
    const base::FilePath& path) {
  if (IsCustomBackgroundDisabledByPolicy()) {
    return;
  }
  previous_background_info_.reset();
  previous_local_background_ = true;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&CopyFileToProfilePath, path, profile_->GetPath()),
      base::BindOnce(&NtpCustomBackgroundService::SetBackgroundToLocalResource,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NtpCustomBackgroundService::RefreshBackgroundIfNeeded() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Do not refresh background & color if extension theme is in use.
  if (theme_service_->UsingExtensionTheme()) {
    return;
  }

  const base::Value::Dict& background_info =
      profile_->GetPrefs()->GetDict(prefs::kNtpCustomBackgroundDict);
  int64_t refresh_timestamp = 0;
  const base::Value* timestamp_value =
      background_info.Find(kNtpCustomBackgroundRefreshTimestamp);
  if (timestamp_value)
    refresh_timestamp = timestamp_value->GetInt();
  if (refresh_timestamp == 0)
    return;

  if (clock_->Now().ToTimeT() > refresh_timestamp) {
    std::string collection_id =
        background_info.Find(kNtpCustomBackgroundCollectionId)->GetString();
    std::string resume_token =
        background_info.Find(kNtpCustomBackgroundResumeToken)->GetString();
    background_service_->FetchNextCollectionImage(collection_id, resume_token);
  }
}

absl::optional<CustomBackground>
NtpCustomBackgroundService::GetCustomBackground() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice)) {
    auto custom_background = absl::make_optional<CustomBackground>();
    // Add a timestamp to the url to prevent the browser from using a cached
    // version when "Upload an image" is used multiple times.
    std::string time_string = std::to_string(base::Time::Now().ToTimeT());
    std::string local_string(chrome::kChromeUIUntrustedNewTabPageBackgroundUrl);
    GURL timestamped_url(local_string + "?ts=" + time_string);
    custom_background->custom_background_url = timestamped_url;
    custom_background->is_uploaded_image = true;
    custom_background->custom_background_snapshot_url = GURL();
    custom_background->custom_background_attribution_line_1 = std::string();
    custom_background->custom_background_attribution_line_2 = std::string();
    custom_background->custom_background_attribution_action_url = GURL();
    custom_background->collection_id = "";
    custom_background->daily_refresh_enabled = false;
    return custom_background;
  }

  // Attempt to get custom background URL from preferences.
  if (IsCustomBackgroundPrefValid()) {
    auto custom_background = absl::make_optional<CustomBackground>();
    const base::Value::Dict& background_info =
        pref_service_->GetDict(prefs::kNtpCustomBackgroundDict);
    GURL custom_background_url(
        background_info.Find(kNtpCustomBackgroundURL)->GetString());

    std::string collection_id;
    const base::Value* id_value =
        background_info.Find(kNtpCustomBackgroundCollectionId);
    if (id_value)
      collection_id = id_value->GetString();

    // Set custom background information in theme info (attributions are
    // optional).
    const base::Value* daily_refresh_timestamp =
        background_info.Find(kNtpCustomBackgroundRefreshTimestamp);
    const base::Value* attribution_line_1 =
        background_info.Find(kNtpCustomBackgroundAttributionLine1);
    const base::Value* attribution_line_2 =
        background_info.Find(kNtpCustomBackgroundAttributionLine2);
    const base::Value* attribution_action_url =
        background_info.Find(kNtpCustomBackgroundAttributionActionURL);
    const base::Value* color =
        base::FeatureList::IsEnabled(
            ntp_features::kCustomizeChromeColorExtraction)
            ? background_info.Find(kNtpCustomBackgroundMainColor)
            : nullptr;
    custom_background->custom_background_url = custom_background_url;
    custom_background->is_uploaded_image = false;
    custom_background->collection_id = collection_id;
    custom_background->daily_refresh_enabled =
        daily_refresh_timestamp && daily_refresh_timestamp->GetInt() != 0;
    std::string custom_background_url_spec = custom_background_url.spec();
    size_t image_options_index = custom_background_url_spec.find("=");
    if (image_options_index != std::string::npos) {
      custom_background->custom_background_snapshot_url =
          GURL(custom_background_url_spec.substr(0, image_options_index) +
               kSidePanelSnapshotImageOptions);
    } else {
      custom_background->custom_background_snapshot_url =
          GURL(custom_background_url_spec + kSidePanelSnapshotImageOptions);
    }
    if (attribution_line_1) {
      custom_background->custom_background_attribution_line_1 =
          background_info.Find(kNtpCustomBackgroundAttributionLine1)
              ->GetString();
    }
    if (attribution_line_2) {
      custom_background->custom_background_attribution_line_2 =
          background_info.Find(kNtpCustomBackgroundAttributionLine2)
              ->GetString();
    }
    if (attribution_action_url) {
      GURL action_url(
          background_info.Find(kNtpCustomBackgroundAttributionActionURL)
              ->GetString());

      if (!action_url.SchemeIsCryptographic()) {
        custom_background->custom_background_attribution_action_url = GURL();
      } else {
        custom_background->custom_background_attribution_action_url =
            action_url;
      }
    }
    if (color) {
      custom_background->custom_background_main_color =
          static_cast<uint32_t>(color->GetInt());
    }
    return custom_background;
  }

  return absl::nullopt;
}

void NtpCustomBackgroundService::AddObserver(
    NtpCustomBackgroundServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void NtpCustomBackgroundService::RemoveObserver(
    NtpCustomBackgroundServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool NtpCustomBackgroundService::IsCustomBackgroundDisabledByPolicy() {
  // |prefs::kNtpCustomBackgroundDict| is managed by policy only if
  // |policy::key::kNTPCustomBackgroundEnabled| is set to false and therefore
  // should be empty.
  bool managed =
      pref_service_->IsManagedPreference(prefs::kNtpCustomBackgroundDict);
  if (managed) {
    DCHECK(pref_service_->GetDict(prefs::kNtpCustomBackgroundDict).empty());
  }
  return managed;
}

bool NtpCustomBackgroundService::IsCustomBackgroundSet() {
  return pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice) ||
         IsCustomBackgroundPrefValid();
}

void NtpCustomBackgroundService::AddValidBackdropUrlForTesting(
    const GURL& url) const {
  background_service_->AddValidBackdropUrlForTesting(url);
}

void NtpCustomBackgroundService::AddValidBackdropCollectionForTesting(
    const std::string& collection_id) const {
  background_service_->AddValidBackdropCollectionForTesting(collection_id);
}

void NtpCustomBackgroundService::SetNextCollectionImageForTesting(
    const CollectionImage& image) const {
  background_service_->SetNextCollectionImageForTesting(image);
}

void NtpCustomBackgroundService::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void NtpCustomBackgroundService::SetBackgroundToLocalResource() {
  background_updated_timestamp_ = base::TimeTicks::Now();
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, true);
  NotifyAboutBackgrounds();
}

bool NtpCustomBackgroundService::IsCustomBackgroundPrefValid() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& background_info =
      profile_->GetPrefs()->GetDict(prefs::kNtpCustomBackgroundDict);

  const base::Value* background_url =
      background_info.Find(kNtpCustomBackgroundURL);
  if (!background_url)
    return false;

  return GURL(background_url->GetString()).is_valid();
}

void NtpCustomBackgroundService::NotifyAboutBackgrounds() {
  for (NtpCustomBackgroundServiceObserver& observer : observers_)
    observer.OnCustomBackgroundImageUpdated();
}

void NtpCustomBackgroundService::RevertBackgroundChanges() {
  if (previous_background_info_.has_value()) {
    pref_service_->Set(prefs::kNtpCustomBackgroundDict,
                       *previous_background_info_);
  }
  if (previous_local_background_) {
    SetBackgroundToLocalResource();
  }
  previous_background_info_.reset();
  previous_local_background_ = false;
}

void NtpCustomBackgroundService::ConfirmBackgroundChanges() {
  previous_background_info_.reset();
  previous_local_background_ = false;
}
