// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_FUNCTION_BASE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_FUNCTION_BASE_H_

#include <string>
#include <vector>

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "extensions/browser/extension_function.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class SequencedTaskRunner;
}

namespace wallpaper_api_util {

extern const char kCancelWallpaperMessage[];

ash::WallpaperLayout GetLayoutEnum(const std::string& layout);

std::string GetLayoutString(const ash::WallpaperLayout& layout);

// This is used to record the wallpaper layout when the user sets a custom
// wallpaper or changes the existing custom wallpaper's layout.
void RecordCustomWallpaperLayout(const ash::WallpaperLayout& layout);

// Resize the image to |size|, encode it and save to |thumbnail_data_out|.
std::vector<uint8_t> GenerateThumbnail(const gfx::ImageSkia& image,
                                       const gfx::Size& size);

// A class to decode JPEG file.
class WallpaperDecoder : public ImageDecoder::ImageRequest {
 public:
  using DecodedCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;
  using CanceledCallback = base::OnceCallback<void()>;
  using FailedCallback = base::OnceCallback<void(const std::string&)>;

  explicit WallpaperDecoder(DecodedCallback decoded_cb,
                            CanceledCallback canceled_cb,
                            FailedCallback failed_cb);
  ~WallpaperDecoder() override;

  WallpaperDecoder(const WallpaperDecoder&) = delete;
  WallpaperDecoder& operator=(const WallpaperDecoder&) = delete;

  void Start(const std::vector<uint8_t>& image_data);
  void Cancel();

  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

 private:
  DecodedCallback decoded_cb_;
  CanceledCallback canceled_cb_;
  FailedCallback failed_cb_;

  base::AtomicFlag cancel_flag_;
};

}  // namespace wallpaper_api_util

// Wallpaper manager function base. It contains a image decoder to decode
// wallpaper data.
class WallpaperFunctionBase : public ExtensionFunction {
 public:
  static const int kWallpaperThumbnailWidth;
  static const int kWallpaperThumbnailHeight;

  WallpaperFunctionBase();

  // For tasks that are worth blocking shutdown, i.e. saving user's custom
  // wallpaper.
  static base::SequencedTaskRunner* GetBlockingTaskRunner();
  static base::SequencedTaskRunner* GetNonBlockingTaskRunner();

  // Asserts that the current task is sequenced with any other task that calls
  // this.
  static void AssertCalledOnWallpaperSequence(
      base::SequencedTaskRunner* task_runner);

 protected:
  ~WallpaperFunctionBase() override;

  // Holds an instance of WallpaperDecoder.
  static wallpaper_api_util::WallpaperDecoder* wallpaper_decoder_;

  // Starts to decode |data|. Must run on UI thread.
  void StartDecode(const std::vector<uint8_t>& data);

  // Handles cancel case. No error message should be set.
  void OnCancel();

  // Handles failure case. Sets error message.
  void OnFailure(const std::string& error);

 private:
  virtual void OnWallpaperDecoded(const gfx::ImageSkia& wallpaper) = 0;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_FUNCTION_BASE_H_
