// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/files/file_result.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/ash/thumbnail_loader.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ash/components/string_matching/tokenized_string_match.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace app_list {

namespace {

using ::ash::string_matching::FuzzyTokenizedStringMatch;
using ::ash::string_matching::TokenizedString;
using ::ash::string_matching::TokenizedStringMatch;

// The default relevance returned by CalculateRelevance.
constexpr double kDefaultRelevance = 0.5;

// Parameters for FuzzyTokenizedStringMatch.
constexpr bool kUseWeightedRatio = false;

// Flag to enable/disable diacritics stripping
constexpr bool kStripDiacritics = true;

// Flag to enable/disable acronym matcher.
constexpr bool kUseAcronymMatcher = true;

// The maximum penalty applied to a relevance by PenalizeRelevanceByAccessTime,
// which will multiply the relevance by a number in [`kMaxPenalty`, 1].
constexpr double kMaxPenalty = 0.6;

// The steepness of the penalty curve of PenalizeRelevanceByAccessTime. Larger
// values make the penalty increase faster as the last access time of the file
// increases. A value of 0.0029 results in a penalty multiplier of ~0.63 for a 1
// month old file.
// Note that files which have all been modified recently may end up with the
// same penalty, since this coefficient is not large enough to differentiate
// between them.
constexpr double kPenaltyCoeff = 0.0029;

constexpr int64_t kMillisPerDay = 1000 * 60 * 60 * 24;

// Generates ash::FileMetadata for the result at `file_path`.
// Performs blocking File IO, so should not be run on UI thread.
ash::FileMetadata GetFileMetadata(base::FilePath file_path,
                                  base::FilePath displayable_path) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI))
      << "FileIO attempted on UI thread.";

  ash::FileMetadata metadata;
  base::File::Info info;
  if (base::GetFileInfo(file_path, &info)) {
    metadata.file_info = info;
  }
  metadata.file_path = file_path;
  metadata.file_name = displayable_path.BaseName();
  metadata.displayable_folder_path = displayable_path.DirName();

  return metadata;
}

std::string StripHostedFileExtensions(const std::string& filename) {
  static const base::NoDestructor<std::vector<std::string>> hosted_extensions(
      {".GDOC", ".GSHEET", ".GSLIDES", ".GDRAW", ".GTABLE", ".GLINK", ".GFORM",
       ".GMAPS", ".GSITE"});

  for (const auto& extension : *hosted_extensions) {
    if (EndsWith(filename, extension, base::CompareCase::INSENSITIVE_ASCII)) {
      return filename.substr(0, filename.size() - extension.size());
    }
  }

  return filename;
}

void LogRelevance(ChromeSearchResult::ResultType result_type,
                  const double relevance) {
  // Relevance scores are between 0 and 1, so we scale to 0 to 100 for logging.
  DCHECK((relevance >= 0) && (relevance <= 1));
  const int scaled_relevance = floor(100 * relevance);
  switch (result_type) {
    case FileResult::ResultType::kFileSearch:
      UMA_HISTOGRAM_EXACT_LINEAR("Apps.AppList.FileSearchProvider.Relevance",
                                 scaled_relevance, /*exclusive_max=*/101);
      break;
    case FileResult::ResultType::kDriveSearch:
      UMA_HISTOGRAM_EXACT_LINEAR("Apps.AppList.DriveSearchProvider.Relevance",
                                 scaled_relevance, /*exclusive_max=*/101);
      break;
    case FileResult::ResultType::kZeroStateFile:
      UMA_HISTOGRAM_EXACT_LINEAR("Apps.AppList.ZeroStateFileProvider.Relevance",
                                 scaled_relevance, /*exclusive_max=*/101);
      break;
    case FileResult::ResultType::kZeroStateDrive:
      UMA_HISTOGRAM_EXACT_LINEAR(
          "Apps.AppList.ZeroStateDriveProvider.Relevance", scaled_relevance,
          /*exclusive_max=*/101);
      break;
    case FileResult::ResultType::kImageSearch:
      // TODO(b/260646344): add UMA metric
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

FileResult::FileResult(const std::string& id,
                       const base::FilePath& filepath,
                       const absl::optional<std::u16string>& details,
                       ResultType result_type,
                       DisplayType display_type,
                       float relevance,
                       const std::u16string& query,
                       Type type,
                       Profile* profile)
    : filepath_(filepath), type_(type), profile_(profile) {
  DCHECK(profile);
  set_id(id);
  SetCategory(Category::kFiles);
  SetDisplayType(display_type);

  set_relevance(relevance);
  if (display_type == DisplayType::kList) {
    // Chip and list results overlap, and only list results are fully launched.
    // So we only log metrics for list results.
    LogRelevance(result_type, relevance);
  }

  SetResultType(result_type);
  switch (result_type) {
    case ResultType::kZeroStateDrive:
      SetMetricsType(ash::ZERO_STATE_DRIVE);
      break;
    case ResultType::kZeroStateFile:
      SetMetricsType(ash::ZERO_STATE_FILE);
      break;
    case ResultType::kFileSearch:
      SetMetricsType(ash::FILE_SEARCH);
      break;
    case ResultType::kDriveSearch:
      SetMetricsType(ash::DRIVE_SEARCH);
      break;
    case ResultType::kImageSearch:
      SetMetricsType(ash::IMAGE_SEARCH);
      break;
    default:
      NOTREACHED();
  }

  SetTitle(base::UTF8ToUTF16(
      StripHostedFileExtensions(filepath.BaseName().value())));

  if (details)
    SetDetails(details.value());

  // Initialize the file metadata.
  SetFilePath(filepath_);
  if (result_type == ash::AppListSearchResultType::kImageSearch) {
    auto displayable_path =
        file_manager::util::GetDisplayablePath(profile_, filepath_)
            .value_or(filepath_);
    SetMetadataLoaderCallback(
        base::BindRepeating(&GetFileMetadata, filepath_, displayable_path));
  }

  UpdateIcon();

  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->AddObserver(this);
}

FileResult::~FileResult() {
  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->RemoveObserver(this);
}

void FileResult::Open(int event_flags) {
  switch (type_) {
    case Type::kFile:
      platform_util::OpenItem(profile_, filepath_,
                              platform_util::OpenItemType::OPEN_FILE,
                              platform_util::OpenOperationCallback());
      break;
    case Type::kDirectory:
    case Type::kSharedDirectory:
      platform_util::OpenItem(profile_, filepath_,
                              platform_util::OpenItemType::OPEN_FOLDER,
                              platform_util::OpenOperationCallback());
      break;
  }
}

absl::optional<std::string> FileResult::DriveId() const {
  return drive_id_;
}

// static
double FileResult::CalculateRelevance(
    const absl::optional<TokenizedString>& query,
    const base::FilePath& filepath,
    const absl::optional<base::Time>& last_accessed) {
  const std::u16string raw_title =
      base::UTF8ToUTF16(StripHostedFileExtensions(filepath.BaseName().value()));
  const TokenizedString title(raw_title, TokenizedString::Mode::kWords);

  const bool use_default_relevance =
      !query || query.value().text().empty() || title.text().empty();
  UMA_HISTOGRAM_BOOLEAN("Apps.AppList.FileResult.DefaultRelevanceUsed",
                        use_default_relevance);
  if (use_default_relevance)
    return kDefaultRelevance;
  double relevance;
  if (search_features::IsLauncherFuzzyMatchAcrossProvidersEnabled()) {
    FuzzyTokenizedStringMatch fuzzy_match;
    relevance = fuzzy_match.Relevance(query.value(), title, kUseWeightedRatio,
                                      kStripDiacritics, kUseAcronymMatcher);
  } else {
    TokenizedStringMatch match;
    relevance = match.Calculate(query.value(), title);
  }
  if (!last_accessed) {
    return relevance;
  }

  // Apply a gaussian penalty based on the time delta. `time_delta` is converted
  // into millisecond fractions of a day for numerical stability.
  const double time_delta =
      static_cast<double>(
          (base::Time::Now() - last_accessed.value()).InMilliseconds()) /
      kMillisPerDay;
  const double penalty =
      kMaxPenalty +
      (1.0 - kMaxPenalty) * std::exp(-kPenaltyCoeff * time_delta * time_delta);
  DCHECK(penalty > 0.0 && penalty <= 1.0);
  return relevance * penalty;
}

void FileResult::RequestThumbnail(ash::ThumbnailLoader* thumbnail_loader) {
  // Thumbnails are only available for list results or image results.
  gfx::Size size;
  if (display_type() == DisplayType::kList) {
    size = gfx::Size(kThumbnailDimension, kThumbnailDimension);
  } else if (display_type() == DisplayType::kImage) {
    size = gfx::Size(kImageSearchWidth, kImageSearchHeight);
  } else {
    NOTREACHED();
  }

  // Request a thumbnail for all file types. For unsupported types, this will
  // just call OnThumbnailLoaded with an error.
  thumbnail_loader->Load({filepath_, size},
                         base::BindOnce(&FileResult::OnThumbnailLoaded,
                                        weak_factory_.GetWeakPtr()));
}

void FileResult::OnColorModeChanged(bool dark_mode_enabled) {
  UpdateIcon();
}

void FileResult::OnThumbnailLoaded(const SkBitmap* bitmap,
                                   base::File::Error error) {
  if (!bitmap) {
    DCHECK_NE(error, base::File::Error::FILE_OK);
    base::UmaHistogramExactLinear(
        "Apps.AppList.FileResult.ThumbnailLoadedError", -error,
        -base::File::FILE_ERROR_MAX);
    return;
  }

  DCHECK_EQ(error, base::File::Error::FILE_OK);

  const bool is_list_display_type = display_type() == DisplayType::kList;

  const int dimension =
      is_list_display_type ? kThumbnailDimension : kImageSearchWidth;
  const auto shape = is_list_display_type
                         ? ash::SearchResultIconShape::kCircle
                         : ash::SearchResultIconShape::kRoundedRectangle;
  const auto image = ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFromBitmap(*bitmap, 1.0f));

  SetIcon(IconInfo(image, dimension, shape));
}

void FileResult::UpdateIcon() {
  // Do not set the default chromeos icon to the image search result.
  if (display_type() == DisplayType::kImage) {
    return;
  }

  // DarkLightModeController might be nullptr in tests.
  auto* dark_light_mode_controller = ash::DarkLightModeController::Get();
  const bool dark_background = dark_light_mode_controller &&
                               dark_light_mode_controller->IsDarkModeEnabled();

  if (display_type() == DisplayType::kContinue) {
    SetChipIcon(chromeos::GetChipIconForPath(filepath_, dark_background));
  } else {
    switch (type_) {
      case Type::kFile:
        SetIcon(IconInfo(
            ui::ImageModel::FromVectorIcon(chromeos::GetIconForPath(filepath_)),
            kSystemIconDimension));
        break;
      case Type::kDirectory:
        SetIcon(IconInfo(
            ui::ImageModel::FromVectorIcon(chromeos::GetIconFromType("folder")),
            kSystemIconDimension));
        break;
      case Type::kSharedDirectory:
        SetIcon(IconInfo(
            ui::ImageModel::FromVectorIcon(chromeos::GetIconFromType("shared")),
            kSystemIconDimension));
        break;
    }
  }
}

::std::ostream& operator<<(::std::ostream& os, const FileResult& result) {
  return os << "{" << result.title() << ", " << result.relevance() << "}";
}

}  // namespace app_list
