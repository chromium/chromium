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
#include "ash/controls/contextual_tooltip.h"
#include "ash/public/cpp/image_util.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/manta/features.h"
#include "components/manta/manta_status.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::personalization_app {

namespace {

constexpr int kSeaPenImageThumbnailSizeDip = 512;

/**
 * Serializes a sea pen query information `query` into json
 * string format based on the query type. Such as {creation_time:<number>,
 * freeform_query:<string>} or {creation_time:<number>,
 * user_visible_query_text:<string>, user_visible_query_template:<string>,
 * template_id:<number>, options:{<chip_number>:<option_number>, ...}}. For
 * example:
 * {"creation_time":"13349580387513653", "freeform_query":"test freeform query"}
 * {"creation_time":"13349580387513653", "user_visible_query_text": "test
 * template query", "user_visible_query_template": "test template",
 * "template_id":"2","options":{"4":"34","5":"40"}}
 *
 * @param query  pointer to the sea pen query
 * @return query information in string format
 */
std::string SeaPenQueryToJsonString(const mojom::SeaPenQueryPtr& query) {
  base::Value::Dict query_dict = base::Value::Dict();
  query_dict.Set(wallpaper_constants::kSeaPenCreationTimeKey,
                 base::TimeToValue(base::Time::Now()));

  switch (query->which()) {
    case mojom::SeaPenQuery::Tag::kTextQuery:
      query_dict.Set(wallpaper_constants::kSeaPenFreeformQueryKey,
                     query->get_text_query());
      break;
    case mojom::SeaPenQuery::Tag::kTemplateQuery:
      query_dict.Set(wallpaper_constants::kSeaPenTemplateIdKey,
                     base::NumberToString(static_cast<int32_t>(
                         query->get_template_query()->id)));
      base::Value::Dict options_dict = base::Value::Dict();
      for (const auto& [chip, option] : query->get_template_query()->options) {
        options_dict.Set(base::NumberToString(static_cast<int32_t>(chip)),
                         base::NumberToString(static_cast<int32_t>(option)));
      }
      query_dict.Set(wallpaper_constants::kSeaPenTemplateOptionsKey,
                     std::move(options_dict));
      query_dict.Set(wallpaper_constants::kSeaPenUserVisibleQueryTextKey,
                     query->get_template_query()->user_visible_query->text);
      query_dict.Set(
          wallpaper_constants::kSeaPenUserVisibleQueryTemplateKey,
          query->get_template_query()->user_visible_query->template_title);
      break;
  }

  return base::WriteJson(query_dict).value_or("");
}

// Constructs the xmp metadata string from the string query information.
std::string QueryInfoToXmpString(const std::string& query_info) {
  static constexpr char kXmpData[] = R"(
            <x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="XMP Core 6.0.0">
               <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
                  <rdf:Description rdf:about="" xmlns:dc="http://purl.org/dc/elements/1.1/">
                     <dc:description>%s</dc:description>
                  </rdf:Description>
               </rdf:RDF>
            </x:xmpmeta>)";
  return base::StringPrintf(kXmpData, query_info.c_str());
}

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

void PersonalizationAppSeaPenProviderBase::SearchWallpaper(
    const mojom::SeaPenQueryPtr query,
    SearchWallpaperCallback callback) {
  // Search for wallpaper.
  if (query->is_text_query() && query->get_text_query().length() >
                                    mojom::kMaximumSearchWallpaperTextBytes) {
    sea_pen_receiver_.ReportBadMessage(
        "SearchWallpaper exceeded maximum text length");
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
    sea_pen_receiver_.ReportBadMessage("Unknown wallpaper image selected");
    return;
  }

  auto* sea_pen_fetcher = GetOrCreateSeaPenFetcher();
  CHECK(sea_pen_fetcher);
  // |last_query_| is set when calling SearchWallpaper() to fetch thumbnails. It
  // should not be null when a thumbnail is selected.
  CHECK(last_query_);
  sea_pen_fetcher->FetchWallpaper(
      feature_name_, it->second, last_query_,
      base::BindOnce(
          &PersonalizationAppSeaPenProviderBase::OnFetchWallpaperDone,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PersonalizationAppSeaPenProviderBase::SelectRecentSeaPenImage(
    const base::FilePath& path,
    SelectRecentSeaPenImageCallback callback) {
  if (recent_sea_pen_images_.count(path) == 0) {
    sea_pen_receiver_.ReportBadMessage("Unknown wallpaper image selected");
    return;
  }

  // Run any pending response callback.
  if (pending_select_recent_sea_pen_image_callback_) {
    std::move(pending_select_recent_sea_pen_image_callback_)
        .Run(/*success=*/false);
  }
  pending_select_recent_sea_pen_image_callback_ = std::move(callback);

  SelectRecentSeaPenImageInternal(
      path,
      base::BindOnce(
          &PersonalizationAppSeaPenProviderBase::OnRecentSeaPenImageSelected,
          weak_ptr_factory_.GetWeakPtr()));
}

void PersonalizationAppSeaPenProviderBase::GetRecentSeaPenImages(
    GetRecentSeaPenImagesCallback callback) {
  GetRecentSeaPenImagesInternal(base::BindOnce(
      &PersonalizationAppSeaPenProviderBase::OnGetRecentSeaPenImages,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PersonalizationAppSeaPenProviderBase::GetRecentSeaPenImageThumbnail(
    const base::FilePath& path,
    GetRecentSeaPenImageThumbnailCallback callback) {
  if (recent_sea_pen_images_.count(path) == 0) {
    LOG(ERROR) << __func__ << " Invalid sea pen image received";
    std::move(callback).Run(GURL());
    return;
  }

  GetRecentSeaPenImageThumbnailInternal(
      path,
      base::BindOnce(&PersonalizationAppSeaPenProviderBase::
                         OnGetRecentSeaPenImageThumbnail,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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
    SearchWallpaperCallback callback,
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
  const std::string query_info =
      QueryInfoToXmpString(SeaPenQueryToJsonString(last_query_));

  OnFetchWallpaperDoneInternal(*image, query_info, std::move(callback));
}

void PersonalizationAppSeaPenProviderBase::OnRecentSeaPenImageSelected(
    bool success) {
  DCHECK(pending_select_recent_sea_pen_image_callback_);
  std::move(pending_select_recent_sea_pen_image_callback_).Run(success);
}

void PersonalizationAppSeaPenProviderBase::OnGetRecentSeaPenImages(
    GetRecentSeaPenImagesCallback callback,
    const std::vector<base::FilePath>& images) {
  recent_sea_pen_images_ =
      std::set<base::FilePath>(images.begin(), images.end());
  std::move(callback).Run(images);
}

void PersonalizationAppSeaPenProviderBase::OnGetRecentSeaPenImageThumbnail(
    GetRecentSeaPenImageThumbnailCallback callback,
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // Do not call |mojom::ReportBadMessage| here. The message is valid, but
    // the jpeg file may be corrupt or unreadable.
    std::move(callback).Run(GURL());
    return;
  }
  std::move(callback).Run(GURL(webui::GetBitmapDataUrl(
      *WallpaperResizer::GetResizedImage(image, kSeaPenImageThumbnailSizeDip)
           .bitmap())));
}

void PersonalizationAppSeaPenProviderBase::OpenFeedbackDialog(
    const mojom::SeaPenFeedbackMetadataPtr metadata) {
  const std::string hashtag = "#AIWallpaper";
  const std::string feedback_type =
      metadata->is_positive ? "Positive" : "Negative";
  CHECK(last_query_);
  const std::string user_visible_query_text =
      (last_query_->is_text_query())
          ? last_query_->get_text_query()
          : last_query_->get_template_query()->user_visible_query->text;
  const std::string description_template =
      hashtag + " " + feedback_type + ": " + user_visible_query_text + "\n";

  base::Value::Dict ai_metadata;
  ai_metadata.Set("from_chromeos", "true");
  ai_metadata.Set("log_id", metadata->log_id);

  base::RecordAction(base::UserMetricsAction("SeaPen_FeedbackPressed"));
  chrome::ShowFeedbackPage(
      /*browser=*/chrome::FindBrowserWithProfile(profile_),
      /*source=*/chrome::kFeedbackSourceAI, description_template,
      /*description_placeholder_text=*/
      base::UTF16ToUTF8(
          l10n_util::GetStringUTF16(IDS_SEA_PEN_FEEDBACK_PLACEHOLDER)),
      /*category_tag=*/std::string(),
      /*extra_diagnostics=*/std::string(),
      /*autofill_data=*/base::Value::Dict(), std::move(ai_metadata));
}

void PersonalizationAppSeaPenProviderBase::ShouldShowSeaPenTermsOfServiceDialog(
    ShouldShowSeaPenTermsOfServiceDialogCallback callback) {
  if (!features::IsSeaPenEnabled() &&
      !features::IsVcBackgroundReplaceEnabled()) {
    sea_pen_receiver_.ReportBadMessage(
        "Cannot call `ShouldShowSeaPenWallpaperTermsDialog()` without Sea Pen "
        "feature enabled");
    return;
  }

  // TODO(b/315032845): confirm how to store and retrieve the terms of service
  // records instead of using contextual tooltip.
  std::move(callback).Run(contextual_tooltip::ShouldShowNudge(
      profile_->GetPrefs(),
      contextual_tooltip::TooltipType::kSeaPenWallpaperTermsDialog,
      /*recheck_delay=*/nullptr));
}

void PersonalizationAppSeaPenProviderBase::
    HandleSeaPenTermsOfServiceAccepted() {
  // TODO(b/315032845): confirm how to store and retrieve the terms of service
  // records instead of using contextual tooltip.
  contextual_tooltip::HandleGesturePerformed(
      profile_->GetPrefs(),
      contextual_tooltip::TooltipType::kSeaPenWallpaperTermsDialog);
}

}  // namespace ash::personalization_app
