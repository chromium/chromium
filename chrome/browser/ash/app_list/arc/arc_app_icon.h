// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_ICON_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_ICON_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

struct ArcAppIconDescriptor;

namespace apps {
class ArcIconOnceLoader;
}

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

// A class that provides an ImageSkia for UI code to use. It handles ARC app
// icon resource loading, screen scale factor change etc. UI code that uses
// ARC app icon should host this class.
//
// Icon images are sometimes subject to post-processing effects, such as
// desaturating (graying out) disabled apps. Applying those effects are the
// responsibility of code that uses this ArcAppIcon class, not the
// responsibility of ArcAppIcon itself.
class ArcAppIcon {
 public:
  class Observer {
   public:
    // Invoked when a new image rep for an additional scale factor
    // is loaded and added to |image|.
    virtual void OnIconUpdated(ArcAppIcon* icon) = 0;

    // Invoked when failed to generate an icon.
    virtual void OnIconFailed(ArcAppIcon* icon) {}

   protected:
    virtual ~Observer() {}
  };

  enum IconType {
    kUncompressed,
    kCompressed,
    kAdaptive,
  };

  ArcAppIcon(content::BrowserContext* context,
             const std::string& app_id,
             int resource_size_in_dip,
             Observer* observer,
             IconType icon_type = IconType::kUncompressed);

  ArcAppIcon(const ArcAppIcon&) = delete;
  ArcAppIcon& operator=(const ArcAppIcon&) = delete;

  virtual ~ArcAppIcon();

  // Starts loading the icon at every supported scale factor. The |observer_|
  // will be notified as progress is made. "Supported" is in the same sense as
  // ui::GetSupportedResourceScaleFactors().
  virtual void LoadSupportedScaleFactors();

  // Whether every supported scale factor was successfully loaded. "Supported"
  // is in the same sense as ui::GetSupportedResourceScaleFactors().
  //
  // For the adaptive icon, if there is a non-adaptive icon for some scale
  // refactors, sets all scale factors as the non-adaptive icon, and copy
  // the decode result from |foreground_image_skia_| to |image_skia_|.
  bool EverySupportedScaleFactorIsLoaded();

  const std::string& app_id() const { return app_id_; }

  bool is_adaptive_icon() const {
    return is_adaptive_icons_.empty() ? true
                                      : is_adaptive_icons_.begin()->second;
  }

  // Returns |image_skia_| and valid if the |icon_type_| is
  // IconType::kUncompressed.
  const gfx::ImageSkia& image_skia() const {
    return image_skia_;
  }
  // Returns |compressed_images_| and valid if the |icon_type_| is
  // IconType::kCompressed.
  const std::map<ui::ResourceScaleFactor, std::string>& compressed_images()
      const {
    DCHECK_EQ(IconType::kCompressed, icon_type_);
    return compressed_images_;
  }
  // Returns |foreground_image_skia_| and valid if the |icon_type_| is
  // IconType::kAdaptive.
  const gfx::ImageSkia& foreground_image_skia() const {
    DCHECK_EQ(IconType::kAdaptive, icon_type_);
    return foreground_image_skia_;
  }
  // Returns |background_image_skia_| and valid if the |icon_type_| is
  // IconType::kAdaptive.
  const gfx::ImageSkia& background_image_skia() const {
    DCHECK_EQ(IconType::kAdaptive, icon_type_);
    return background_image_skia_;
  }

  // Disables async safe decoding requests when unit tests are executed. This is
  // done to avoid two problems. Problems come because icons are decoded at a
  // separate process created by ImageDecoder. ImageDecoder has 5 seconds delay
  // to stop since the last request (see its kBatchModeTimeoutSeconds for more
  // details). This is unacceptably long for unit tests because the test
  // framework waits until external process is finished. Another problem happens
  // when we issue a decoding request, but the process has not started its
  // processing yet by the time when a test exits. This might cause situation
  // when g_one_utility_thread_lock from in_process_utility_thread.cc gets
  // released in an acquired state which is crash condition in debug builds.
  static void DisableSafeDecodingForTesting();
  static bool IsSafeDecodingDisabledForTesting();

 protected:
  struct ReadResult {
    ReadResult(bool error,
               bool request_to_install,
               ui::ResourceScaleFactor scale_factor,
               bool resize_allowed,
               std::vector<std::string> unsafe_icon_data);
    ~ReadResult();

    const bool error;
    const bool request_to_install;
    const ui::ResourceScaleFactor scale_factor;
    const bool resize_allowed;
    const std::vector<std::string> unsafe_icon_data;
  };

  // Icon loading is performed in several steps. It is initiated by
  // LoadImageForScaleFactor request that specifies a required scale factor.
  // ArcAppListPrefs is used to resolve a path to resource. Content of file is
  // asynchronously read in context of browser file thread. On successful read,
  // an icon data is decoded to an image in the special utility process.
  // DecodeRequest is used to interact with the utility process, and each
  // active request is stored at |decode_requests_| vector. When decoding is
  // complete, results are returned in context of UI thread, and corresponding
  // request is removed from |decode_requests_|. In case of some requests are
  // not completed by the time of deleting this icon, they are automatically
  // canceled.
  // In case of the icon file is not available this requests ArcAppListPrefs to
  // install required resource from ARC side. ArcAppListPrefs notifies UI items
  // that new icon is available and corresponding item should invoke
  // LoadImageForScaleFactor again.
  virtual void LoadForScaleFactor(ui::ResourceScaleFactor scale_factor);

  virtual void OnIconRead(std::unique_ptr<ArcAppIcon::ReadResult> read_result);

 private:
  friend class ArcAppIconLoader;
  friend class apps::ArcIconOnceLoader;

  class Source;
  class DecodeRequest;

  void MaybeRequestIcon(ui::ResourceScaleFactor scale_factor);
  static std::unique_ptr<ArcAppIcon::ReadResult> ReadOnBackgroundThread(
      ArcAppIcon::IconType icon_type,
      ui::ResourceScaleFactor scale_factor,
      const std::vector<base::FilePath>& paths,
      const std::vector<base::FilePath>& default_app_paths);
  static std::unique_ptr<ArcAppIcon::ReadResult> ReadSingleIconFile(
      ui::ResourceScaleFactor scale_factor,
      const base::FilePath& path,
      const base::FilePath& default_app_path);

  // For the adaptive icon, currently, there are 3 images returned from the ARC
  // side:
  // (1) Icon_png_data, the adaptive icon generated by the ARC side, for
  // backward compatibility.
  // (2) Foreground_icon_png_data, the foreground image for
  // the adaptive icon. Some icons are not adaptive icons, and don’t have the
  // background images, then the foreground image is the app icon.
  // (3) Background_icon_png_data, the background image for the adaptive icon.
  // Some icons are not adaptive icons, and don’t have the background images.
  //
  // There are a few scenarios for the adaptive icon feature:
  // A. For the adaptive icon, the foreground image and the background image are
  // merged by the Chromium side, and applied with the mask, to generate the
  // adaptive icon.
  // B. For the non adaptive icon, the Chromium side adds a white background to
  // the foreground image, then applies the mask to generate the adaptive icon.
  // C. For the migration scenario (from the adaptive icon feature disable to
  // enable), since neither foreground images and background images present on
  // the system, the Chromium side sends requests to the ARC side to load the
  // foreground and background images. However, it might take a few seconds to
  // get the images files, so for users, it has a long lag for the ARC icon
  // loading. To resolve the ARC icon lag issue, the old icon_png_data on the
  // system is used to generate the icon, the same as the previous
  // implementation, and at the same time, sending the request to the ARC side
  // to request the new foreground and background images.
  //
  // TODO(crbug.com/40131344): Remove the migration handling code, which reads
  // the old icon_png_data, when the adaptive icon feature is enabled in the
  // stable release, and the adaptive icon flag is removed.
  static std::unique_ptr<ArcAppIcon::ReadResult> ReadAdaptiveIconFiles(
      ui::ResourceScaleFactor scale_factor,
      const std::vector<base::FilePath>& paths,
      const std::vector<base::FilePath>& default_app_paths);
  static std::unique_ptr<ArcAppIcon::ReadResult>
  ReadDefaultAppAdaptiveIconFiles(
      ui::ResourceScaleFactor scale_factor,
      const std::vector<base::FilePath>& default_app_paths);
  static std::unique_ptr<ArcAppIcon::ReadResult> ReadFile(
      bool request_to_install,
      ui::ResourceScaleFactor scale_factor,
      bool resize_allowed,
      const base::FilePath& path);
  static std::unique_ptr<ArcAppIcon::ReadResult> ReadFiles(
      bool request_to_install,
      ui::ResourceScaleFactor scale_factor,
      bool resize_allowed,
      const base::FilePath& foreground_path,
      const base::FilePath& background_path);
  void DecodeImage(
      std::string unsafe_icon_data,
      const ArcAppIconDescriptor& descriptor,
      bool resize_allowed,
      bool retain_padding,
      gfx::ImageSkia& image_skia,
      std::map<ui::ResourceScaleFactor, base::Time>& incomplete_scale_factors);
  void UpdateImageSkia(
      ui::ResourceScaleFactor scale_factor,
      const SkBitmap& bitmap,
      gfx::ImageSkia& image_skia,
      std::map<ui::ResourceScaleFactor, base::Time>& incomplete_scale_factors);
  void UpdateCompressed(ui::ResourceScaleFactor scale_factor, std::string data);
  void DiscardDecodeRequest(DecodeRequest* request, bool is_decode_success);

  const raw_ptr<content::BrowserContext> context_;
  const std::string app_id_;
  // Contains app id that is actually used to read an icon resource to support
  // shelf group mapping to shortcut.
  const std::string mapped_app_id_;
  const int resource_size_in_dip_;
  const raw_ptr<Observer> observer_;
  const IconType icon_type_;
  // Used to separate first 5 loaded app icons and other app icons.
  // Only one form of app icons will be loaded, compressed or uncompressed, so
  // only one counter is needed.
  int icon_loaded_count_ = 0;

  // For some apps, some scales have the adaptive icons, but some are not. So
  // using a map to present which scale is the adaptive icon, which is not.
  std::map<ui::ResourceScaleFactor, bool> is_adaptive_icons_;

  gfx::ImageSkia image_skia_;
  std::map<ui::ResourceScaleFactor, std::string> compressed_images_;
  gfx::ImageSkia foreground_image_skia_;
  gfx::ImageSkia background_image_skia_;

  std::map<ui::ResourceScaleFactor, base::Time> incomplete_scale_factors_;
  std::map<ui::ResourceScaleFactor, base::Time>
      foreground_incomplete_scale_factors_;
  std::map<ui::ResourceScaleFactor, base::Time>
      background_incomplete_scale_factors_;

  // Contains pending image decode requests.
  std::vector<std::unique_ptr<DecodeRequest>> decode_requests_;

  base::WeakPtrFactory<ArcAppIcon> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_ICON_H_
