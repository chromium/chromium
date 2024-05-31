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
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/feedback/feedback_constants.h"
#include "components/manta/features.h"
#include "components/manta/manta_status.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::personalization_app {

namespace {

constexpr int kSeaPenImageThumbnailSizeDip = 512;

}  // namespace

PersonalizationAppSeaPenProviderBase::PersonalizationAppSeaPenProviderBase(
    content::WebUI* web_ui,
    std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
        wallpaper_fetcher_delegate,
    manta::proto::FeatureName feature_name)
    : feature_name_(feature_name),
      profile_(Profile::FromWebUI(web_ui)),
      wallpaper_fetcher_delegate_(std::move(wallpaper_fetcher_delegate)) {}

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
  last_query_ = query.Clone();
  auto* sea_pen_fetcher = GetOrCreateSeaPenFetcher();
  CHECK(sea_pen_fetcher);
  sea_pen_fetcher->FetchThumbnails(
      feature_name_, query,
      base::BindOnce(
          &PersonalizationAppSeaPenProviderBase::OnFetchThumbnailsDone,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PersonalizationAppSeaPenProviderBase::SelectSeaPenThumbnail(
    uint32_t id,
    SelectSeaPenThumbnailCallback callback) {
  // Get high resolution image.
  const auto it = sea_pen_images_.find(id);
  if (it == sea_pen_images_.end()) {
    sea_pen_receiver_.ReportBadMessage("Unknown sea pen image selected");
    return;
  }

  // In case of CHROMEOS_WALLPAPER, we need to send a second query.
  if (feature_name_ == manta::proto::FeatureName::CHROMEOS_WALLPAPER) {
    auto* sea_pen_fetcher = GetOrCreateSeaPenFetcher();
    CHECK(sea_pen_fetcher);
    // |last_query_| is set when calling GetSeaPenThumbnails() to fetch
    // thumbnails. It should not be null when a thumbnail is selected.
    CHECK(last_query_);
    sea_pen_fetcher->FetchWallpaper(
        feature_name_, it->second, last_query_,
        base::BindOnce(
            &PersonalizationAppSeaPenProviderBase::OnFetchWallpaperDone,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    // In case of CHROMEOS_VC_BACKGROUNDS, we use image stored already.
    OnFetchWallpaperDone(std::move(callback),
                         SeaPenImage(it->second.jpg_bytes, it->second.id));
  }
}

void PersonalizationAppSeaPenProviderBase::SelectRecentSeaPenImage(
    const uint32_t id,
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
      id,
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
    std::optional<std::vector<SeaPenImage>> images,
    manta::MantaStatusCode status_code) {
  if (!images) {
    std::move(callback).Run(std::nullopt, status_code);
    return;
  }
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
}

void PersonalizationAppSeaPenProviderBase::OnFetchWallpaperDone(
    SelectSeaPenThumbnailCallback callback,
    std::optional<SeaPenImage> image) {
  if (!image) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  CHECK(last_query_);
  OnFetchWallpaperDoneInternal(*image, last_query_, std::move(callback));
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

void PersonalizationAppSeaPenProviderBase::OpenFeedbackDialog(
    const mojom::SeaPenFeedbackMetadataPtr metadata) {
  CHECK(last_query_);

  std::string feedback_text =
      wallpaper_handlers::GetFeedbackText(last_query_, metadata);

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

}  // namespace ash::personalization_app
