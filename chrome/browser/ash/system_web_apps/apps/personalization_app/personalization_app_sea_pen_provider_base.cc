// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_base.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/feedback/feedback_constants.h"
#include "components/manta/features.h"
#include "components/manta/manta_status.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/display/screen.h"

namespace ash::personalization_app {

namespace {

constexpr int kSeaPenImageThumbnailSizeDip = 512;
constexpr int kMaxTextQueryHistoryItemNum = 3;

void AppendTextQueryHistory(
    std::map<uint32_t, const SeaPenImage> images,
    const mojom::SeaPenQueryPtr& query,
    std::vector<std::pair<std::string, std::map<uint32_t, const SeaPenImage>>>&
        text_query_history) {
  CHECK(query && query->is_text_query());

  if (text_query_history.size() >= kMaxTextQueryHistoryItemNum) {
    text_query_history.pop_back();
  }

  text_query_history.insert(
      text_query_history.begin(),
      std::make_pair(query->get_text_query(), std::move(images)));
}

}  // namespace

PersonalizationAppSeaPenProviderBase::PersonalizationAppSeaPenProviderBase(
    content::WebUI* web_ui,
    std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
        wallpaper_fetcher_delegate,
    manta::proto::FeatureName feature_name)
    : feature_name_(feature_name),
      profile_(Profile::FromWebUI(web_ui)),
      wallpaper_fetcher_delegate_(std::move(wallpaper_fetcher_delegate)),
      web_ui_(web_ui) {}

PersonalizationAppSeaPenProviderBase::~PersonalizationAppSeaPenProviderBase() =
    default;

void PersonalizationAppSeaPenProviderBase::BindInterface(
    mojo::PendingReceiver<::ash::personalization_app::mojom::SeaPenProvider>
        receiver) {
  CHECK(manta::features::IsMantaServiceEnabled());
  CHECK(::ash::features::IsSeaPenEnabled() ||
        ::ash::features::IsVcBackgroundReplaceEnabled());
  sea_pen_receiver_.reset();
  sea_pen_receiver_.Bind(std::move(receiver));
}

bool PersonalizationAppSeaPenProviderBase::IsEligibleForSeaPen() {
  return ::ash::personalization_app::IsEligibleForSeaPen(profile_);
}

bool PersonalizationAppSeaPenProviderBase::IsEligibleForSeaPenTextInput() {
  return ::ash::personalization_app::IsEligibleForSeaPenTextInput(profile_);
}

bool PersonalizationAppSeaPenProviderBase::IsManagedSeaPenEnabled() {
  return IsManagedSeaPenEnabledInternal();
}

bool PersonalizationAppSeaPenProviderBase::IsManagedSeaPenFeedbackEnabled() {
  if (!profile_->GetProfilePolicyConnector()->IsManaged()) {
    return true;
  }

  // Allow internal Google accounts to see and provide feedback.
  if (gaia::IsGoogleInternalAccountEmail(profile_->GetProfileUserName())) {
    DVLOG(1) << __func__ << " Google internal account";
    return true;
  }

  // Allow Demo Mode public accounts to see and provide feedback.
  if (features::IsSeaPenDemoModeEnabled() &&
      DemoSession::IsDeviceInDemoMode()) {
    DVLOG(1) << __func__ << " demo mode";
    const auto* user = GetUser(profile_);
    return DemoSession::Get() && user &&
           user->GetType() == user_manager::UserType::kPublicAccount;
  }

  return IsManagedSeaPenFeedbackEnabledInternal();
}

void PersonalizationAppSeaPenProviderBase::SetSeaPenObserver(
    mojo::PendingRemote<mojom::SeaPenObserver> observer) {
  // May already be bound if user refreshes page.
  sea_pen_observer_remote_.reset();
  sea_pen_observer_remote_.Bind(std::move(observer));
  SetSeaPenObserverInternal();
}

void PersonalizationAppSeaPenProviderBase::GetSeaPenThumbnails(
    const mojom::SeaPenQueryPtr query,
    GetSeaPenThumbnailsCallback callback) {
  // Search for wallpaper.
  if (query->is_text_query() &&
      query->get_text_query().length() >
          mojom::kMaximumGetSeaPenThumbnailsTextBytes) {
    sea_pen_receiver_.ReportBadMessage(
        "GetSeaPenThumbnails exceeded maximum text length");
    return;
  }
  auto* sea_pen_fetcher = GetOrCreateSeaPenFetcher();
  CHECK(sea_pen_fetcher);
  sea_pen_fetcher->FetchThumbnails(
      feature_name_, query,
      base::BindOnce(
          &PersonalizationAppSeaPenProviderBase::OnFetchThumbnailsDone,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), query.Clone()));
}

void PersonalizationAppSeaPenProviderBase::SelectSeaPenThumbnail(
    uint32_t id,
    const bool preview_mode,
    SelectSeaPenThumbnailCallback callback) {
  // Get high resolution image.
  const auto query_and_thumbnail = FindImageThumbnail(id);
  if (!query_and_thumbnail) {
    sea_pen_receiver_.ReportBadMessage("Unknown sea pen image selected");
    return;
  }

  // In case of CHROMEOS_VC_BACKGROUNDS, we use image stored already.
  if (feature_name_ == manta::proto::FeatureName::CHROMEOS_VC_BACKGROUNDS) {
    OnFetchWallpaperDone(
        std::move(callback), query_and_thumbnail->first, /*preview_mode=*/false,
        SeaPenImage(query_and_thumbnail->second->second.jpg_bytes,
                    query_and_thumbnail->second->second.id));
    return;
  }

  // In case of CHROMEOS_WALLPAPER, we need to send a second query.
  auto* sea_pen_fetcher = GetOrCreateSeaPenFetcher();
  CHECK(sea_pen_fetcher);

  sea_pen_fetcher->FetchWallpaper(
      feature_name_, query_and_thumbnail->second->second,
      query_and_thumbnail->first,
      base::BindOnce(
          &PersonalizationAppSeaPenProviderBase::OnFetchWallpaperDone,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          query_and_thumbnail->first->Clone(), preview_mode));
}

void PersonalizationAppSeaPenProviderBase::SelectRecentSeaPenImage(
    const uint32_t id,
    const bool preview_mode,
    SelectRecentSeaPenImageCallback callback) {
  if (recent_sea_pen_image_ids_.count(id) == 0) {
    sea_pen_receiver_.ReportBadMessage("Unknown recent sea pen image selected");
    return;
  }

  // Run any pending response callback.
  if (pending_select_recent_sea_pen_image_callback_) {
    std::move(pending_select_recent_sea_pen_image_callback_)
        .Run(/*success=*/false);
  }
  pending_select_recent_sea_pen_image_callback_ = std::move(callback);

  SelectRecentSeaPenImageInternal(
      id, preview_mode,
      base::BindOnce(
          &PersonalizationAppSeaPenProviderBase::OnRecentSeaPenImageSelected,
          weak_ptr_factory_.GetWeakPtr()));
}

void PersonalizationAppSeaPenProviderBase::GetRecentSeaPenImageIds(
    GetRecentSeaPenImageIdsCallback callback) {
  GetRecentSeaPenImageIdsInternal(base::BindOnce(
      &PersonalizationAppSeaPenProviderBase::OnGetRecentSeaPenImageIds,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PersonalizationAppSeaPenProviderBase::GetRecentSeaPenImageThumbnail(
    const uint32_t id,
    GetRecentSeaPenImageThumbnailCallback callback) {
  if (recent_sea_pen_image_ids_.count(id) == 0) {
    LOG(ERROR) << __func__ << " Invalid sea pen image received";
    std::move(callback).Run(nullptr);
    return;
  }

  GetRecentSeaPenImageThumbnailInternal(
      id,
      base::BindOnce(&PersonalizationAppSeaPenProviderBase::
                         OnGetRecentSeaPenImageThumbnail,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
}

wallpaper_handlers::SeaPenFetcher*
PersonalizationAppSeaPenProviderBase::GetOrCreateSeaPenFetcher() {
  if (!sea_pen_fetcher_) {
    sea_pen_fetcher_ =
        wallpaper_fetcher_delegate_->CreateSeaPenFetcher(profile_);
  }
  return sea_pen_fetcher_.get();
}

void PersonalizationAppSeaPenProviderBase::OnFetchThumbnailsDone(
    GetSeaPenThumbnailsCallback callback,
    const mojom::SeaPenQueryPtr& query,
    std::optional<std::vector<SeaPenImage>> images,
    manta::MantaStatusCode status_code) {
  if (!images) {
    std::move(callback).Run(std::nullopt, status_code);
    return;
  }
  if (last_query_ && last_query_->is_text_query() && !sea_pen_images_.empty()) {
    AppendTextQueryHistory(std::move(sea_pen_images_), last_query_,
                           text_query_history_);
  }

  last_query_ = query.Clone();
  sea_pen_images_.clear();
  std::vector<ash::personalization_app::mojom::SeaPenThumbnailPtr> result;
  for (auto& image : images.value()) {
    const auto image_id = image.id;
    auto [it, _] = sea_pen_images_.insert(
        std::pair<uint32_t, SeaPenImage>(image_id, std::move(image)));
    result.emplace_back(std::in_place, GetJpegDataUrl(it->second.jpg_bytes),
                        image_id);
  }
  std::move(callback).Run(std::move(result), status_code);

  NotifyTextQueryHistoryChanged();
}

void PersonalizationAppSeaPenProviderBase::OnFetchWallpaperDone(
    SelectSeaPenThumbnailCallback callback,
    const mojom::SeaPenQueryPtr& query,
    const bool preview_mode,
    std::optional<SeaPenImage> image) {
  if (!image) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  CHECK(query);
  OnFetchWallpaperDoneInternal(*image, query, preview_mode,
                               std::move(callback));
}

void PersonalizationAppSeaPenProviderBase::OnRecentSeaPenImageSelected(
    bool success) {
  DCHECK(pending_select_recent_sea_pen_image_callback_);
  std::move(pending_select_recent_sea_pen_image_callback_).Run(success);
}

void PersonalizationAppSeaPenProviderBase::OnGetRecentSeaPenImageIds(
    GetRecentSeaPenImageIdsCallback callback,
    const std::vector<uint32_t>& ids) {
  recent_sea_pen_image_ids_ = std::set<uint32_t>(ids.begin(), ids.end());
  std::move(callback).Run(ids);
}

void PersonalizationAppSeaPenProviderBase::OnGetRecentSeaPenImageThumbnail(
    const uint32_t id,
    GetRecentSeaPenImageThumbnailCallback callback,
    const gfx::ImageSkia& image,
    mojom::RecentSeaPenImageInfoPtr image_info) {
  if (image.isNull()) {
    DVLOG(1) << __func__ << " failed to decode image";
    std::move(callback).Run(nullptr);
    return;
  }

  auto thumbnail_url = GURL(webui::GetBitmapDataUrl(
      *WallpaperResizer::GetResizedImage(image, kSeaPenImageThumbnailSizeDip)
           .bitmap()));

  if (!image_info) {
    DVLOG(1) << __func__ << " Unable to get image info for image " << id;
    std::move(callback).Run(mojom::RecentSeaPenThumbnailData::New(
        std::move(thumbnail_url), nullptr));
    return;
  }

  std::move(callback).Run(mojom::RecentSeaPenThumbnailData::New(
      std::move(thumbnail_url), std::move(image_info)));
}

void PersonalizationAppSeaPenProviderBase::NotifyTextQueryHistoryChanged() {
  std::vector<mojom::TextQueryHistoryEntryPtr> history;
  for (auto& entry : text_query_history_) {
    std::vector<mojom::SeaPenThumbnailPtr> thumbnails;
    for (const auto& [_, thumbnail] : entry.second) {
      thumbnails.emplace_back(
          std::in_place, GetJpegDataUrl(thumbnail.jpg_bytes), thumbnail.id);
    }
    history.emplace_back(std::in_place, entry.first, std::move(thumbnails));
  }
  sea_pen_observer_remote_->OnTextQueryHistoryChanged(std::move(history));
}

std::optional<std::pair<mojom::SeaPenQueryPtr,
                        std::map<uint32_t, const SeaPenImage>::const_iterator>>
PersonalizationAppSeaPenProviderBase::FindImageThumbnail(const uint32_t id) {
  const auto image_it = sea_pen_images_.find(id);
  if (image_it != sea_pen_images_.end()) {
    return std::make_pair(last_query_->Clone(), image_it);
  }

  for (const auto& [query, image_map] : text_query_history_) {
    const auto history_it = image_map.find(id);
    if (history_it != image_map.end()) {
      return std::make_pair(mojom::SeaPenQuery::NewTextQuery(query),
                            history_it);
    }
  }
  return std::nullopt;
}

void PersonalizationAppSeaPenProviderBase::OpenFeedbackDialog(
    const mojom::SeaPenFeedbackMetadataPtr metadata) {
  const auto id = metadata->generation_seed;
  const auto query_and_thumbnail = FindImageThumbnail(id);
  if (!query_and_thumbnail) {
    return;
  }

  std::string feedback_text =
      wallpaper_handlers::GetFeedbackText(query_and_thumbnail->first, metadata);

  base::Value::Dict ai_metadata;
  ai_metadata.Set(feedback::kSeaPenMetadataKey, "true");

  base::RecordAction(base::UserMetricsAction("SeaPen_FeedbackPressed"));
  chrome::ShowFeedbackPage(
      /*browser=*/chrome::FindBrowserWithProfile(profile_),
      /*source=*/feedback::kFeedbackSourceAI,
      /*description_template=*/feedback_text,
      /*description_placeholder_text=*/
      base::UTF16ToUTF8(
          l10n_util::GetStringUTF16(IDS_SEA_PEN_FEEDBACK_PLACEHOLDER)),
      /*category_tag=*/std::string(),
      /*extra_diagnostics=*/std::string(),
      /*autofill_data=*/base::Value::Dict(), std::move(ai_metadata));
}

void PersonalizationAppSeaPenProviderBase::ShouldShowSeaPenIntroductionDialog(
    ShouldShowSeaPenIntroductionDialogCallback callback) {
  if (!features::IsSeaPenEnabled() &&
      !features::IsVcBackgroundReplaceEnabled()) {
    sea_pen_receiver_.ReportBadMessage(
        "Cannot call `ShouldShowSeaPenIntroductionDialog()` without Sea Pen "
        "feature enabled");
    return;
  }

  ShouldShowSeaPenIntroductionDialogInternal(std::move(callback));
}

void PersonalizationAppSeaPenProviderBase::
    HandleSeaPenIntroductionDialogClosed() {
  HandleSeaPenIntroductionDialogClosedInternal();
}

void PersonalizationAppSeaPenProviderBase::IsInTabletMode(
    IsInTabletModeCallback callback) {
  std::move(callback).Run(display::Screen::GetScreen()->InTabletMode());
}

void PersonalizationAppSeaPenProviderBase::MakeTransparent() {
  WallpaperControllerClientImpl::Get()->MakeTransparent(
      web_ui_->GetWebContents());
}

}  // namespace ash::personalization_app
