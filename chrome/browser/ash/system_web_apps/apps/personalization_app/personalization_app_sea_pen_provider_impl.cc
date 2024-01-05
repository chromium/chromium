// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper/wallpaper_enumerator.h"
#include "chrome/browser/ash/wallpaper_handlers/sea_pen_fetcher.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/manta/features.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "content/public/browser/web_ui.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::personalization_app {

namespace {
constexpr int kSeaPenImageThumbnailSizeDip = 512;
constexpr char kMonthName[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Converts a base::Time time into a string in the format "mmm dd, yyyy" such as
// "Jan 08, 2023".
std::string GetTimeInfo(base::Time time) {
  base::Time::Exploded exploded_time;
  time.UTCExplode(&exploded_time);
  return base::StringPrintf("%s %02d, %04d",
                            kMonthName[exploded_time.month - 1],
                            exploded_time.day_of_month, exploded_time.year);
}

/**
 * Serializes a sea pen query into json string format based on the query type
 * such as {freeform_query: <string>} or {template_id: <number>, options:
 * {<chip_number>:<option_number>, ...}}. For example:
 * {"freeform_query":"test query"}
 * {"template_id":"2","options":{"4":"34","5":"40"}}
 *
 * @param query  pointer to the sea pen query
 * @return query information in string format
 */
std::string SeaPenQueryToJsonString(const mojom::SeaPenQueryPtr& query) {
  std::string query_info;
  base::Value::Dict query_dict = base::Value::Dict();
  query_dict.Set("creation_time", GetTimeInfo(base::Time::Now()));

  switch (query->which()) {
    case mojom::SeaPenQuery::Tag::kTextQuery:
      query_dict.Set("freeform_query", query->get_text_query());
      break;
    case mojom::SeaPenQuery::Tag::kTemplateQuery:
      query_dict.Set("template_id", base::NumberToString(static_cast<int32_t>(
                                        query->get_template_query()->id)));
      base::Value::Dict options_dict = base::Value::Dict();
      for (const auto& [chip, option] : query->get_template_query()->options) {
        options_dict.Set(base::NumberToString(static_cast<int32_t>(chip)),
                         base::NumberToString(static_cast<int32_t>(option)));
      }
      query_dict.Set("options", std::move(options_dict));
      break;
  }

  base::JSONWriter::Write(query_dict, &query_info);
  return query_info;
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

PersonalizationAppSeaPenProviderImpl::PersonalizationAppSeaPenProviderImpl(
    content::WebUI* web_ui,
    std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
        wallpaper_fetcher_delegate)
    : profile_(Profile::FromWebUI(web_ui)),
      wallpaper_fetcher_delegate_(std::move(wallpaper_fetcher_delegate)) {}

PersonalizationAppSeaPenProviderImpl::~PersonalizationAppSeaPenProviderImpl() =
    default;

void PersonalizationAppSeaPenProviderImpl::BindInterface(
    mojo::PendingReceiver<::ash::personalization_app::mojom::SeaPenProvider>
        receiver) {
  CHECK(manta::features::IsMantaServiceEnabled() &&
        features::IsSeaPenEnabled());
  sea_pen_receiver_.reset();
  sea_pen_receiver_.Bind(std::move(receiver));
}

void PersonalizationAppSeaPenProviderImpl::SearchWallpaper(
    const mojom::SeaPenQueryPtr query,
    SearchWallpaperCallback callback) {
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
      query, base::BindOnce(
                 &PersonalizationAppSeaPenProviderImpl::OnFetchThumbnailsDone,
                 weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PersonalizationAppSeaPenProviderImpl::SelectSeaPenThumbnail(
    uint32_t id,
    SelectSeaPenThumbnailCallback callback) {
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
      it->second, last_query_,
      base::BindOnce(
          &PersonalizationAppSeaPenProviderImpl::OnFetchWallpaperDone,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PersonalizationAppSeaPenProviderImpl::SelectRecentSeaPenImage(
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

  ash::WallpaperController* wallpaper_controller = WallpaperController::Get();
  DCHECK(wallpaper_controller);
  wallpaper_controller->SetSeaPenWallpaperFromFile(
      GetAccountId(profile_), path,
      base::BindOnce(
          &PersonalizationAppSeaPenProviderImpl::OnRecentSeaPenImageSelected,
          weak_ptr_factory_.GetWeakPtr()));
}

void PersonalizationAppSeaPenProviderImpl::GetRecentSeaPenImages(
    GetRecentSeaPenImagesCallback callback) {
  base::FilePath wallpaper_dir;
  CHECK(
      base::PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpaper_dir));
  const base::FilePath sea_pen_wallpaper_dir = wallpaper_dir.Append("sea_pen/");
  ash::EnumerateJpegFilesFromDir(
      profile_, sea_pen_wallpaper_dir,
      base::BindOnce(
          &PersonalizationAppSeaPenProviderImpl::OnGetRecentSeaPenImages,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PersonalizationAppSeaPenProviderImpl::GetRecentSeaPenImageThumbnail(
    const base::FilePath& path,
    GetRecentSeaPenImageThumbnailCallback callback) {
  if (recent_sea_pen_images_.count(path) == 0) {
    LOG(ERROR) << __func__ << " Invalid sea pen image received";
    std::move(callback).Run(GURL());
    return;
  }
  image_util::DecodeImageFile(
      base::BindOnce(&PersonalizationAppSeaPenProviderImpl::
                         OnGetRecentSeaPenImageThumbnail,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      path);
}

void PersonalizationAppSeaPenProviderImpl::DeleteRecentSeaPenImage(
    const base::FilePath& path,
    DeleteRecentSeaPenImageCallback callback) {
  if (recent_sea_pen_images_.count(path) == 0) {
    sea_pen_receiver_.ReportBadMessage("Invalid Sea Pen image received");
    return;
  }

  auto* wallpaper_controller = ash::WallpaperController::Get();
  DCHECK(wallpaper_controller);

  wallpaper_controller->DeleteRecentSeaPenImage(GetAccountId(profile_), path,
                                                std::move(callback));
}

wallpaper_handlers::SeaPenFetcher*
PersonalizationAppSeaPenProviderImpl::GetOrCreateSeaPenFetcher() {
  if (!sea_pen_fetcher_) {
    sea_pen_fetcher_ =
        wallpaper_fetcher_delegate_->CreateSeaPenFetcher(profile_);
  }
  return sea_pen_fetcher_.get();
}

void PersonalizationAppSeaPenProviderImpl::OnFetchThumbnailsDone(
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
    result.emplace_back(absl::in_place, GetJpegDataUrl(it->second.jpg_bytes),
                        image_id);
  }
  std::move(callback).Run(std::move(result), status_code);
}

void PersonalizationAppSeaPenProviderImpl::OnFetchWallpaperDone(
    SelectSeaPenThumbnailCallback callback,
    std::optional<SeaPenImage> image) {
  if (!image) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  CHECK(last_query_);
  const std::string query_info =
      QueryInfoToXmpString(SeaPenQueryToJsonString(last_query_));

  auto* wallpaper_controller = ash::WallpaperController::Get();
  wallpaper_controller->SetSeaPenWallpaper(GetAccountId(profile_), *image,
                                           query_info, std::move(callback));
}

void PersonalizationAppSeaPenProviderImpl::OnRecentSeaPenImageSelected(
    bool success) {
  DCHECK(pending_select_recent_sea_pen_image_callback_);
  std::move(pending_select_recent_sea_pen_image_callback_).Run(success);
}

void PersonalizationAppSeaPenProviderImpl::OnGetRecentSeaPenImages(
    GetRecentSeaPenImagesCallback callback,
    const std::vector<base::FilePath>& images) {
  recent_sea_pen_images_ =
      std::set<base::FilePath>(images.begin(), images.end());
  std::move(callback).Run(images);
}

void PersonalizationAppSeaPenProviderImpl::OnGetRecentSeaPenImageThumbnail(
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

}  // namespace ash::personalization_app
