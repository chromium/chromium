// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_icon.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_list/arc/arc_app_icon_descriptor.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/ui/ash/shelf/arc_app_shelf_id.h"
#include "chrome/grit/component_extension_resources.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"

namespace {

bool disable_safe_decoding_for_testing = false;

data_decoder::DataDecoder& GetDataDecoder() {
  static base::NoDestructor<data_decoder::DataDecoder> data_decoder;
  return *data_decoder;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon::ReadResult

ArcAppIcon::ReadResult::ReadResult(bool error,
                                   bool request_to_install,
                                   ui::ResourceScaleFactor scale_factor,
                                   bool resize_allowed,
                                   std::vector<std::string> unsafe_icon_data)
    : error(error),
      request_to_install(request_to_install),
      scale_factor(scale_factor),
      resize_allowed(resize_allowed),
      unsafe_icon_data(std::move(unsafe_icon_data)) {}
ArcAppIcon::ReadResult::~ReadResult() = default;

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon::Source

// Initializes the ImageSkia with placeholder bitmaps, decoded from
// compiled-into-the-binary resources such as IDR_APP_DEFAULT_ICON, and
// schedules the asynchronous loading of the app's actual bitmaps.
class ArcAppIcon::Source : public gfx::ImageSkiaSource {
 public:
  Source(const base::WeakPtr<ArcAppIcon>& host, int resource_size_in_dip);

  Source(const Source&) = delete;
  Source& operator=(const Source&) = delete;

  ~Source() override;

 private:
  // gfx::ImageSkiaSource overrides:
  gfx::ImageSkiaRep GetImageForScale(float scale) override;

  // Used to load images asynchronously. NULLed out when the ArcAppIcon is
  // destroyed.
  base::WeakPtr<ArcAppIcon> host_;

  const int resource_size_in_dip_;

  // A map from a pair of a resource ID and size in DIP to an image. This
  // is a cache to avoid resizing IDR icons in GetImageForScale every time.
  static base::LazyInstance<std::map<std::pair<int, int>, gfx::ImageSkia>>::
      DestructorAtExit default_icons_cache_;
};

base::LazyInstance<std::map<std::pair<int, int>, gfx::ImageSkia>>::
    DestructorAtExit ArcAppIcon::Source::default_icons_cache_ =
        LAZY_INSTANCE_INITIALIZER;

ArcAppIcon::Source::Source(const base::WeakPtr<ArcAppIcon>& host,
                           int resource_size_in_dip)
    : host_(host),
      resource_size_in_dip_(resource_size_in_dip) {
}

ArcAppIcon::Source::~Source() {
}

gfx::ImageSkiaRep ArcAppIcon::Source::GetImageForScale(float scale) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Host loads icon asynchronously, so use default icon so far.
  int resource_id;
  if (host_ && host_->app_id() == arc::kPlayStoreAppId) {
    // Don't request icon from Android side. Use overloaded Chrome icon for Play
    // Store that is adapted according Chrome style.
    const int resource_size_in_px =
        static_cast<int>(resource_size_in_dip_ * scale + 0.5);
    resource_id = resource_size_in_px <= 32 ? IDR_ARC_SUPPORT_ICON_32_PNG
                                            : IDR_ARC_SUPPORT_ICON_192_PNG;
  } else {
    if (host_)
      host_->LoadForScaleFactor(ui::GetSupportedResourceScaleFactor(scale));
    resource_id = IDR_APP_DEFAULT_ICON;
  }

  // Check |default_icons_cache_| and returns the existing one if possible.
  const auto key = std::make_pair(resource_id, resource_size_in_dip_);
  const auto it = default_icons_cache_.Get().find(key);
  if (it != default_icons_cache_.Get().end())
    return it->second.GetRepresentation(scale);

  const gfx::ImageSkia* default_image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  CHECK(default_image);
  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      *default_image, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(resource_size_in_dip_, resource_size_in_dip_));

  // Add the resized image to the cache to avoid executing the expensive resize
  // operation many times. Caching the result is safe because unlike ARC icons
  // that can be updated dynamically, IDR icons are static.
  default_icons_cache_.Get().insert(std::make_pair(key, resized_image));
  return resized_image.GetRepresentation(scale);
}

class ArcAppIcon::DecodeRequest : public ImageDecoder::ImageRequest {
 public:
  DecodeRequest(
      ArcAppIcon& host,
      const ArcAppIconDescriptor& descriptor,
      bool resize_allowed,
      bool retain_padding,
      gfx::ImageSkia& image_skia,
      std::map<ui::ResourceScaleFactor, base::Time>& incomplete_scale_factors);

  DecodeRequest(const DecodeRequest&) = delete;
  DecodeRequest& operator=(const DecodeRequest&) = delete;

  ~DecodeRequest() override;

  // ImageDecoder::ImageRequest
  void OnImageDecoded(const SkBitmap& bitmap) override;
  void OnDecodeImageFailed() override;

 private:
  const raw_ref<ArcAppIcon> host_;
  const ArcAppIconDescriptor descriptor_;
  const bool resize_allowed_;
  const bool retain_padding_;
  const raw_ref<gfx::ImageSkia> image_skia_;
  const raw_ref<std::map<ui::ResourceScaleFactor, base::Time>>
      incomplete_scale_factors_;
};

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon::DecodeRequest

ArcAppIcon::DecodeRequest::DecodeRequest(
    ArcAppIcon& host,
    const ArcAppIconDescriptor& descriptor,
    bool resize_allowed,
    bool retain_padding,
    gfx::ImageSkia& image_skia,
    std::map<ui::ResourceScaleFactor, base::Time>& incomplete_scale_factors)
    : ImageRequest(&GetDataDecoder()),
      host_(host),
      descriptor_(descriptor),
      resize_allowed_(resize_allowed),
      retain_padding_(retain_padding),
      image_skia_(image_skia),
      incomplete_scale_factors_(incomplete_scale_factors) {}

ArcAppIcon::DecodeRequest::~DecodeRequest() {
  ImageDecoder::Cancel(this);
}

void ArcAppIcon::DecodeRequest::OnImageDecoded(const SkBitmap& bitmap) {
  DCHECK(!bitmap.isNull() && !bitmap.empty());

  const int expected_dim = descriptor_.GetSizeInPixels();

  // If retain_padding is true, that means the foreground or the
  // background image has paddings which should be chopped by AppService, so the
  // raw image is forwarded without resize.
  if (!retain_padding_ &&
      (bitmap.width() != expected_dim || bitmap.height() != expected_dim)) {
    if (!resize_allowed_) {
      VLOG(2) << "Decoded ARC icon has unexpected dimension " << bitmap.width()
              << "x" << bitmap.height() << ". Expected " << expected_dim << ".";
      host_->MaybeRequestIcon(descriptor_.scale_factor);
    } else {
      host_->UpdateImageSkia(descriptor_.scale_factor,
                             skia::ImageOperations::Resize(
                                 bitmap, skia::ImageOperations::RESIZE_BEST,
                                 expected_dim, expected_dim),
                             *image_skia_, *incomplete_scale_factors_);
    }
  } else {
    host_->UpdateImageSkia(descriptor_.scale_factor, bitmap, *image_skia_,
                           *incomplete_scale_factors_);
  }

  host_->DiscardDecodeRequest(this, true /* bool is_decode_success */);
}

void ArcAppIcon::DecodeRequest::OnDecodeImageFailed() {
  VLOG(2) << "Failed to decode ARC icon.";
  host_->MaybeRequestIcon(descriptor_.scale_factor);
  host_->DiscardDecodeRequest(this, false /* bool is_decode_success*/);
}

////////////////////////////////////////////////////////////////////////////////
// ArcAppIcon

// static
void ArcAppIcon::DisableSafeDecodingForTesting() {
  disable_safe_decoding_for_testing = true;
}

// static
bool ArcAppIcon::IsSafeDecodingDisabledForTesting() {
  return disable_safe_decoding_for_testing;
}

ArcAppIcon::ArcAppIcon(content::BrowserContext* context,
                       const std::string& app_id,
                       int resource_size_in_dip,
                       Observer* observer,
                       IconType icon_type)
    : context_(context),
      app_id_(app_id),
      mapped_app_id_(arc::GetAppFromAppOrGroupId(context, app_id)),
      resource_size_in_dip_(resource_size_in_dip),
      observer_(observer),
      icon_type_(icon_type) {
  CHECK(observer_);

  gfx::Size resource_size(resource_size_in_dip, resource_size_in_dip);
  const std::vector<ui::ResourceScaleFactor>& scale_factors =
      ui::GetSupportedResourceScaleFactors();
  switch (icon_type) {
    case IconType::kAdaptive:
      foreground_image_skia_ = gfx::ImageSkia(
          std::make_unique<Source>(weak_ptr_factory_.GetWeakPtr(),
                                   resource_size_in_dip),
          resource_size);

      // ArcAppIcon::Source::GetImageForScale calls host_->LoadForScaleFactor to
      // read both the foreground and background files, so the
      // |background_image_skia_| doesn't need to set the host to call
      // LoadForScaleFactor again. Otherwise, it might duplicate the opened
      // files number, and cause the system crash,
      background_image_skia_ = gfx::ImageSkia(
          std::make_unique<Source>(nullptr, resource_size_in_dip),
          resource_size);
      for (const auto scale_factor : scale_factors) {
        foreground_incomplete_scale_factors_.insert(
            {scale_factor, base::Time::Now()});
        background_incomplete_scale_factors_.insert(
            {scale_factor, base::Time::Now()});
        is_adaptive_icons_.insert({scale_factor, true});
      }
      // Deliberately fall through to IconType::kUncompressed to update
      // |image_skia_| and |incomplete_scale_factors_|.
      [[fallthrough]];
    case IconType::kUncompressed:
      image_skia_ = gfx::ImageSkia(
          std::make_unique<Source>(weak_ptr_factory_.GetWeakPtr(),
                                   resource_size_in_dip),
          resource_size);
      // Deliberately fall through to IconType::kCompressed to update
      // |incomplete_scale_factors_|.
      [[fallthrough]];
    case IconType::kCompressed:
      for (const auto scale_factor : scale_factors) {
        incomplete_scale_factors_.insert({scale_factor, base::Time::Now()});
        if (icon_type != IconType::kAdaptive)
          is_adaptive_icons_.insert({scale_factor, false});
      }
      break;
  }
}

ArcAppIcon::~ArcAppIcon() = default;

void ArcAppIcon::LoadSupportedScaleFactors() {
  switch (icon_type_) {
    case IconType::kCompressed:
      for (auto scale_factor : incomplete_scale_factors_)
        LoadForScaleFactor(scale_factor.first);
      break;
    case IconType::kAdaptive:
      for (auto scale_factor : foreground_incomplete_scale_factors_) {
        foreground_image_skia_.GetRepresentation(
            ui::GetScaleForResourceScaleFactor(scale_factor.first));
      }
      for (auto scale_factor : background_incomplete_scale_factors_) {
        background_image_skia_.GetRepresentation(
            ui::GetScaleForResourceScaleFactor(scale_factor.first));
      }
      // Deliberately fall through to IconType::kCompressed to update
      // |image_skia_|.
      [[fallthrough]];
    case IconType::kUncompressed:
      // Calling GetRepresentation indirectly calls LoadForScaleFactor but also
      // first initializes image_skia_ with the placeholder icons (e.g.
      // IDR_APP_DEFAULT_ICON), via ArcAppIcon::Source::GetImageForScale.
      for (auto scale_factor : incomplete_scale_factors_) {
        image_skia_.GetRepresentation(
            ui::GetScaleForResourceScaleFactor(scale_factor.first));
      }
      break;
  }
}

bool ArcAppIcon::EverySupportedScaleFactorIsLoaded() {
  switch (icon_type_) {
    case IconType::kUncompressed:
      // Deliberately fall through to IconType::kCompressed to check
      // |incomplete_scale_factors_|.
      [[fallthrough]];
    case IconType::kCompressed:
      return incomplete_scale_factors_.empty();
    case IconType::kAdaptive: {
      // Check whether there is non-adaptive icon for any scale.
      bool is_adaptive_icon = true;
      for (const auto& it : is_adaptive_icons_) {
        if (!it.second) {
          is_adaptive_icon = false;
          break;
        }
      }

      // All scales have adaptive icon.
      if (is_adaptive_icon) {
        return foreground_incomplete_scale_factors_.empty() &&
               background_incomplete_scale_factors_.empty();
      }

      // Some scales have non-adaptive icons. Copy the decode image
      // representation from |foreground_image_skia_| to |image_skia_|.
      for (auto it = is_adaptive_icons_.begin(); it != is_adaptive_icons_.end();
           it++) {
        if (it->second &&
            !base::Contains(foreground_incomplete_scale_factors_, it->first)) {
          it->second = false;
          float scale = ui::GetScaleForResourceScaleFactor(it->first);
          image_skia_.RemoveRepresentation(scale);
          image_skia_.AddRepresentation(
              foreground_image_skia_.GetRepresentation(scale));
          image_skia_.RemoveUnsupportedRepresentationsForScale(scale);
          incomplete_scale_factors_.erase(it->first);
        }
      }

      return incomplete_scale_factors_.empty();
    }
  }
}

void ArcAppIcon::LoadForScaleFactor(ui::ResourceScaleFactor scale_factor) {
  // We provide Play Store icon from Chrome resources and it is not expected
  // that we have external load request.
  DCHECK_NE(app_id(), arc::kPlayStoreAppId);

  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  const ArcAppIconDescriptor descriptor(resource_size_in_dip_, scale_factor);
  std::vector<base::FilePath> paths;
  std::vector<base::FilePath> default_app_paths;
  switch (icon_type_) {
    case IconType::kAdaptive: {
      base::FilePath foreground_path =
          prefs->GetForegroundIconPath(mapped_app_id_, descriptor);
      base::FilePath background_path =
          prefs->GetBackgroundIconPath(mapped_app_id_, descriptor);
      if (foreground_path.empty() || background_path.empty())
        return;
      paths.emplace_back(foreground_path);
      paths.emplace_back(background_path);

      default_app_paths.emplace_back(
          prefs->MaybeGetForegroundIconPathForDefaultApp(mapped_app_id_,
                                                         descriptor));
      default_app_paths.emplace_back(
          prefs->MaybeGetBackgroundIconPathForDefaultApp(mapped_app_id_,
                                                         descriptor));
      // Deliberately fall through to IconType::kCompressed to add |path| to
      // |paths|. For the migration scenario, when the foreground icon file
      // doesn't exist, load the original icon file to resolve the icon lag
      // issue.
      [[fallthrough]];
    }
    case IconType::kUncompressed: {
      // Deliberately fall through to IconType::kCompressed to add |path| to
      // |paths|.
      [[fallthrough]];
    }
    case IconType::kCompressed: {
      base::FilePath path = prefs->GetIconPath(mapped_app_id_, descriptor);
      if (path.empty())
        return;
      paths.emplace_back(path);
      default_app_paths.emplace_back(
          prefs->MaybeGetIconPathForDefaultApp(mapped_app_id_, descriptor));
      break;
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ArcAppIcon::ReadOnBackgroundThread, icon_type_,
                     scale_factor, paths, default_app_paths),
      base::BindOnce(&ArcAppIcon::OnIconRead, weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppIcon::MaybeRequestIcon(ui::ResourceScaleFactor scale_factor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(context_);
  DCHECK(prefs);

  // ArcAppListPrefs notifies the ArcAppIconLoader (which is an
  // ArcAppListPrefs::Observer) when the icon is updated, and
  // ArcAppIconLoader::OnAppIconUpdated calls ArcAppIcon::LoadForScaleFactor on
  // the corresponding ArcAppIcon.
  prefs->MaybeRequestIcon(
      mapped_app_id_,
      ArcAppIconDescriptor(resource_size_in_dip_, scale_factor));
}

// static
std::unique_ptr<ArcAppIcon::ReadResult> ArcAppIcon::ReadOnBackgroundThread(
    ArcAppIcon::IconType icon_type,
    ui::ResourceScaleFactor scale_factor,
    const std::vector<base::FilePath>& paths,
    const std::vector<base::FilePath>& default_app_paths) {
  DCHECK(!paths.empty());

  switch (icon_type) {
    case IconType::kUncompressed:
      // Deliberately fall through to IconType::kCompressed.
      [[fallthrough]];
    case IconType::kCompressed:
      DCHECK_EQ(1u, paths.size());
      return ArcAppIcon::ReadSingleIconFile(scale_factor, paths[0],
                                            default_app_paths[0]);
    case IconType::kAdaptive:
      return ArcAppIcon::ReadAdaptiveIconFiles(scale_factor, paths,
                                               default_app_paths);
  }
}

// static
std::unique_ptr<ArcAppIcon::ReadResult> ArcAppIcon::ReadSingleIconFile(
    ui::ResourceScaleFactor scale_factor,
    const base::FilePath& path,
    const base::FilePath& default_app_path) {
  DCHECK(!path.empty());

  base::FilePath path_to_read;
  // Allow resizing only for default app icons.
  bool resize_allowed;
  if (base::PathExists(path)) {
    path_to_read = path;
    resize_allowed = false;
  } else {
    if (default_app_path.empty() || !base::PathExists(default_app_path)) {
      return std::make_unique<ArcAppIcon::ReadResult>(
          false /* error */, true /* request_to_install */, scale_factor,
          false /* resize_allowed */,
          std::vector<std::string>() /* unsafe_icon_data */);
    }
    path_to_read = default_app_path;
    resize_allowed = true;
  }

  bool request_to_install = path_to_read != path;
  return ArcAppIcon::ReadFile(request_to_install, scale_factor, resize_allowed,
                              path_to_read);
}

// static
std::unique_ptr<ArcAppIcon::ReadResult> ArcAppIcon::ReadAdaptiveIconFiles(
    ui::ResourceScaleFactor scale_factor,
    const std::vector<base::FilePath>& paths,
    const std::vector<base::FilePath>& default_app_paths) {
  DCHECK_EQ(3u, paths.size());

  const base::FilePath& foreground_path = paths[0];
  const base::FilePath& background_path = paths[1];
  if (!base::PathExists(foreground_path)) {
    // For the migration scenario, when the foreground icon file doesn't
    // exist, load the original icon file to resolve the icon lag issue.
    if (!paths[2].empty() && base::PathExists(paths[2])) {
      return ArcAppIcon::ReadFile(true /* request_to_install */, scale_factor,
                                  false /* resize_allowed */, paths[2]);
    }

    // Check and read the default app icon path for the default app if the files
    // exist.
    return ReadDefaultAppAdaptiveIconFiles(scale_factor, default_app_paths);
  }

  if (!base::PathExists(background_path)) {
    // For non-adaptive icon, there could be a |foreground_icon_path| file
    // only without a |background_icon_path| file.
    return ArcAppIcon::ReadFile(false /* request_to_install */, scale_factor,
                                false /* resize_allowed */, foreground_path);
  }

  return ArcAppIcon::ReadFiles(false /* request_to_install */, scale_factor,
                               false /* resize_allowed */, foreground_path,
                               background_path);
}

// static
std::unique_ptr<ArcAppIcon::ReadResult>
ArcAppIcon::ReadDefaultAppAdaptiveIconFiles(
    ui::ResourceScaleFactor scale_factor,
    const std::vector<base::FilePath>& default_app_paths) {
  // Check the default app icon path, and read the icon files for the default
  // app if the files exist.
  DCHECK_EQ(3u, default_app_paths.size());
  const base::FilePath& default_app_foreground_path = default_app_paths[0];
  const base::FilePath& default_app_background_path = default_app_paths[1];
  if (default_app_foreground_path.empty() ||
      !base::PathExists(default_app_foreground_path)) {
    // For the migration scenario, when the foreground icon file doesn't
    // exist, load the original icon file to resolve the icon lag issue for
    // the default app.
    if (default_app_paths.size() == 3u && !default_app_paths[2].empty() &&
        base::PathExists(default_app_paths[2])) {
      return ArcAppIcon::ReadFile(true /* request_to_install */, scale_factor,
                                  true /* resize_allowed */,
                                  default_app_paths[2]);
    }

    return std::make_unique<ArcAppIcon::ReadResult>(
        false /* error */, true /* request_to_install */, scale_factor,
        false /* resize_allowed */,
        std::vector<std::string>() /* unsafe_icon_data */);
  }

  if (default_app_background_path.empty() ||
      !base::PathExists(default_app_background_path)) {
    // For non-adaptive icon, there could be a |default_app_foreground_path|
    // file only without a |default_app_background_path| file.
    return ArcAppIcon::ReadFile(true /* request_to_install */, scale_factor,
                                true /* resize_allowed */,
                                default_app_foreground_path);
  }

  return ArcAppIcon::ReadFiles(
      true /* request_to_install */, scale_factor, true /* resize_allowed */,
      default_app_foreground_path, default_app_background_path);
}

// static
std::unique_ptr<ArcAppIcon::ReadResult> ArcAppIcon::ReadFile(
    bool request_to_install,
    ui::ResourceScaleFactor scale_factor,
    bool resize_allowed,
    const base::FilePath& path) {
  // Read the file from disk. Sometimes, the file could be deleted, e.g. when
  // running unit tests, so check whether the file exists again.
  std::string unsafe_icon_data;
  if (path.empty() || !base::PathExists(path) ||
      !base::ReadFileToString(path, &unsafe_icon_data) ||
      unsafe_icon_data.empty()) {
    VLOG(2) << "Failed to read an ARC icon from file " << path.MaybeAsASCII();

    // If |unsafe_icon_data| is empty typically means we have a file corruption
    // on cached icon file. Send request to re install the icon.
    request_to_install |= unsafe_icon_data.empty();
    return std::make_unique<ArcAppIcon::ReadResult>(
        true /* error */, request_to_install, scale_factor,
        false /* resize_allowed */,
        std::vector<std::string>() /* unsafe_icon_data */);
  }

  return std::make_unique<ArcAppIcon::ReadResult>(
      false /* error */, request_to_install, scale_factor, resize_allowed,
      std::vector<std::string>{std::move(unsafe_icon_data)});
}

// static
std::unique_ptr<ArcAppIcon::ReadResult> ArcAppIcon::ReadFiles(
    bool request_to_install,
    ui::ResourceScaleFactor scale_factor,
    bool resize_allowed,
    const base::FilePath& foreground_path,
    const base::FilePath& background_path) {
  // Read the file from disk. Sometimes, the file could be deleted, e.g. when
  // running unit tests, so check whether the file exists again.
  std::string unsafe_foreground_icon_data;
  std::string unsafe_background_icon_data;
  if (foreground_path.empty() || !base::PathExists(foreground_path) ||
      !base::ReadFileToString(foreground_path, &unsafe_foreground_icon_data) ||
      unsafe_foreground_icon_data.empty()) {
    VLOG(2) << "Failed to read an ARC icon from file "
            << foreground_path.MaybeAsASCII();

    // If |unsafe_icon_data| is empty typically means we have a file corruption
    // on cached icon file. Send request to re install the icon.
    request_to_install |= unsafe_foreground_icon_data.empty();
    return std::make_unique<ArcAppIcon::ReadResult>(
        true /* error */, true /* request_to_install */, scale_factor,
        false /* resize_allowed */,
        std::vector<std::string>() /* unsafe_icon_data */);
  }

  if (background_path.empty() || !base::PathExists(background_path) ||
      !base::ReadFileToString(background_path, &unsafe_background_icon_data) ||
      unsafe_background_icon_data.empty()) {
    VLOG(2) << "Failed to read an ARC icon from file "
            << background_path.MaybeAsASCII();

    // If |unsafe_icon_data| is empty typically means we have a file corruption
    // on cached icon file. Send request to re install the icon.
    request_to_install |= unsafe_background_icon_data.empty();
    return std::make_unique<ArcAppIcon::ReadResult>(
        true /* error */, true /* request_to_install */, scale_factor,
        false /* resize_allowed */,
        std::vector<std::string>() /* unsafe_icon_data */);
  }

  return std::make_unique<ArcAppIcon::ReadResult>(
      false /* error */, request_to_install, scale_factor, resize_allowed,
      std::vector<std::string>{std::move(unsafe_foreground_icon_data),
                               std::move(unsafe_background_icon_data)});
}

void ArcAppIcon::OnIconRead(
    std::unique_ptr<ArcAppIcon::ReadResult> read_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (read_result->request_to_install)
    MaybeRequestIcon(read_result->scale_factor);

  if (read_result->unsafe_icon_data.empty()) {
    observer_->OnIconFailed(this);
    return;
  }

  switch (icon_type_) {
    case IconType::kUncompressed: {
      DCHECK_EQ(1u, read_result->unsafe_icon_data.size());
      DecodeImage(std::move(read_result->unsafe_icon_data[0]),
                  ArcAppIconDescriptor(resource_size_in_dip_,
                                       read_result->scale_factor),
                  read_result->resize_allowed, false /* retain_padding */,
                  image_skia_, incomplete_scale_factors_);
      return;
    }
    case IconType::kCompressed: {
      DCHECK_EQ(1u, read_result->unsafe_icon_data.size());
      UpdateCompressed(read_result->scale_factor,
                       std::move(read_result->unsafe_icon_data[0]));
      return;
    }
    case IconType::kAdaptive: {
      // For non-adaptive icons, |foreground_icon_path| is used to save the icon
      // without |background_icon_path|, so |unsafe_icon_data| could have one
      // element for |foreground_icon_path| only.
      if (read_result->unsafe_icon_data.size() == 1) {
        is_adaptive_icons_[read_result->scale_factor] = false;
        DecodeImage(std::move(read_result->unsafe_icon_data[0]),
                    ArcAppIconDescriptor(resource_size_in_dip_,
                                         read_result->scale_factor),
                    read_result->resize_allowed, false /* retain_padding */,
                    image_skia_, incomplete_scale_factors_);
        return;
      }

      DCHECK_EQ(2u, read_result->unsafe_icon_data.size());
      DecodeImage(std::move(read_result->unsafe_icon_data[0]),
                  ArcAppIconDescriptor(resource_size_in_dip_,
                                       read_result->scale_factor),
                  read_result->resize_allowed, true /* retain_padding */,
                  foreground_image_skia_, foreground_incomplete_scale_factors_);
      DecodeImage(std::move(read_result->unsafe_icon_data[1]),
                  ArcAppIconDescriptor(resource_size_in_dip_,
                                       read_result->scale_factor),
                  read_result->resize_allowed, true /* retain_padding */,
                  background_image_skia_, background_incomplete_scale_factors_);
      return;
    }
  }
}

void ArcAppIcon::DecodeImage(
    std::string unsafe_icon_data,
    const ArcAppIconDescriptor& descriptor,
    bool resize_allowed,
    bool retain_padding,
    gfx::ImageSkia& image_skia,
    std::map<ui::ResourceScaleFactor, base::Time>& incomplete_scale_factors) {
  decode_requests_.emplace_back(std::make_unique<DecodeRequest>(
      *this, descriptor, resize_allowed, retain_padding, image_skia,
      incomplete_scale_factors));
  if (disable_safe_decoding_for_testing) {
    SkBitmap bitmap;
    if (!unsafe_icon_data.empty() &&
        gfx::PNGCodec::Decode(
            reinterpret_cast<const unsigned char*>(&unsafe_icon_data.front()),
            unsafe_icon_data.length(), &bitmap)) {
      decode_requests_.back()->OnImageDecoded(bitmap);
    } else {
      decode_requests_.back()->OnDecodeImageFailed();
    }
  } else {
    ImageDecoder::Start(decode_requests_.back().get(),
                        std::move(unsafe_icon_data));
  }
}

void ArcAppIcon::UpdateImageSkia(
    ui::ResourceScaleFactor scale_factor,
    const SkBitmap& bitmap,
    gfx::ImageSkia& image_skia,
    std::map<ui::ResourceScaleFactor, base::Time>& incomplete_scale_factors) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(ui::IsScaleFactorSupported(scale_factor));

  gfx::ImageSkiaRep image_rep(bitmap,
                              ui::GetScaleForResourceScaleFactor(scale_factor));
  image_skia.RemoveRepresentation(image_rep.scale());
  image_skia.AddRepresentation(image_rep);
  image_skia.RemoveUnsupportedRepresentationsForScale(image_rep.scale());

  // TODO(crbug.com/40131344): Track the adaptive icon load time in a separate
  // UMA.
  if (icon_loaded_count_++ < 5) {
    base::UmaHistogramTimes(
        "Arc.IconLoadFromFileTime.uncompressedFirst5",
        base::Time::Now() - incomplete_scale_factors[scale_factor]);
  } else {
    base::UmaHistogramTimes(
        "Arc.IconLoadFromFileTime.uncompressedOthers",
        base::Time::Now() - incomplete_scale_factors[scale_factor]);
  }
  incomplete_scale_factors.erase(scale_factor);
  observer_->OnIconUpdated(this);
}

void ArcAppIcon::UpdateCompressed(ui::ResourceScaleFactor scale_factor,
                                  std::string data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  compressed_images_[scale_factor] = std::move(data);

  if (icon_loaded_count_++ < 5) {
    base::UmaHistogramTimes(
        "Arc.IconLoadFromFileTime.compressedFirst5",
        base::Time::Now() - incomplete_scale_factors_[scale_factor]);
  } else {
    base::UmaHistogramTimes(
        "Arc.IconLoadFromFileTime.compressedOthers",
        base::Time::Now() - incomplete_scale_factors_[scale_factor]);
  }
  incomplete_scale_factors_.erase(scale_factor);
  observer_->OnIconUpdated(this);
}

void ArcAppIcon::DiscardDecodeRequest(DecodeRequest* request,
                                      bool is_decode_success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto it = base::ranges::find(decode_requests_, request,
                               &std::unique_ptr<DecodeRequest>::get);
  DCHECK(it != decode_requests_.end());
  decode_requests_.erase(it);

  if (!is_decode_success)
    observer_->OnIconFailed(this);
}
