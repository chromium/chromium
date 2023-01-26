// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_DECODER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_DECODER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"

namespace gfx {
class ImageSkiaRep;
}

namespace apps {

// AppIconDecoder reads app icons from the icon image files in the local
// disk and provides an uncompressed icon, ImageSkia, for UI code to use.
//
// AppIconDecoder is used to decode for one icon uncompressed image only, and
// owned by AppIconReader. AppIconReader is responsible to free AppIconDecoder's
// objects once the decode is done.
class AppIconDecoder {
 public:
  AppIconDecoder(const base::FilePath& base_path,
                 const std::string& app_id,
                 int32_t size_in_dip,
                 base::OnceCallback<void(AppIconDecoder* decoder,
                                         IconValuePtr iv)> callback);
  AppIconDecoder(const AppIconDecoder&) = delete;
  AppIconDecoder& operator=(const AppIconDecoder&) = delete;
  ~AppIconDecoder();

  void Start();

 private:
  // Initializes the ImageSkia with placeholder bitmaps, decoded from
  // compiled-into-the-binary resources such as IDR_APP_DEFAULT_ICON.
  class ImageSource : public gfx::ImageSkiaSource {
   public:
    explicit ImageSource(int32_t size_in_dip);
    ImageSource(const ImageSource&) = delete;
    ImageSource& operator=(const ImageSource&) = delete;
    ~ImageSource() override;

   private:
    // gfx::ImageSkiaSource overrides:
    gfx::ImageSkiaRep GetImageForScale(float scale) override;

    const int32_t size_in_dip_;
  };

  // Decode images safely in a sandboxed service per ARC app icons' security
  // requests.
  class DecodeRequest : public ImageDecoder::ImageRequest {
   public:
    DecodeRequest(ui::ResourceScaleFactor scale_factor,
                  AppIconDecoder& host,
                  gfx::ImageSkia& image_skia,
                  std::set<ui::ResourceScaleFactor>& incomplete_scale_factors);

    DecodeRequest(const DecodeRequest&) = delete;
    DecodeRequest& operator=(const DecodeRequest&) = delete;

    ~DecodeRequest() override;

    // ImageDecoder::ImageRequest
    void OnImageDecoded(const SkBitmap& bitmap) override;
    void OnDecodeImageFailed() override;

   private:
    ui::ResourceScaleFactor scale_factor_;
    AppIconDecoder& host_;
    gfx::ImageSkia& image_skia_;
    std::set<ui::ResourceScaleFactor>& incomplete_scale_factors_;
  };

  class FakeDecodeRequestForTesting {
   public:
    FakeDecodeRequestForTesting(
        ui::ResourceScaleFactor scale_factor,
        AppIconDecoder& host,
        gfx::ImageSkia& image_skia,
        std::set<ui::ResourceScaleFactor>& incomplete_scale_factors);

    FakeDecodeRequestForTesting(const FakeDecodeRequestForTesting&) = delete;
    FakeDecodeRequestForTesting& operator=(const FakeDecodeRequestForTesting&) =
        delete;

    ~FakeDecodeRequestForTesting();

    void Start(std::vector<uint8_t> icon_data);

   private:
    void DecodeRequestReply(SkBitmap bitmap);

    ui::ResourceScaleFactor scale_factor_;
    AppIconDecoder& host_;
    gfx::ImageSkia& image_skia_;
    std::set<ui::ResourceScaleFactor>& incomplete_scale_factors_;
    base::WeakPtrFactory<FakeDecodeRequestForTesting> weak_ptr_factory_{this};
  };

  bool SetScaleFactors(
      const std::map<ui::ResourceScaleFactor, IconValuePtr>& icon_datas);

  void OnIconRead(std::map<ui::ResourceScaleFactor, IconValuePtr> icon_datas);

  void DecodeImage(ui::ResourceScaleFactor scale_factor,
                   std::vector<uint8_t> icon_data,
                   gfx::ImageSkia& image_skia,
                   std::set<ui::ResourceScaleFactor>& incomplete_scale_factors);

  void UpdateImageSkia(
      ui::ResourceScaleFactor scale_factor,
      const SkBitmap& bitmap,
      gfx::ImageSkia& image_skia,
      std::set<ui::ResourceScaleFactor>& incomplete_scale_factors);

  void DiscardDecodeRequest();

  void CompleteWithImageSkia(const gfx::ImageSkia& image_skia);

  const base::FilePath base_path_;
  const std::string app_id_;
  int32_t size_in_dip_;
  base::OnceCallback<void(AppIconDecoder* decoder, IconValuePtr iv)> callback_;

  gfx::ImageSkia image_skia_;
  gfx::ImageSkia foreground_image_skia_;
  gfx::ImageSkia background_image_skia_;

  std::set<ui::ResourceScaleFactor> incomplete_scale_factors_;
  std::set<ui::ResourceScaleFactor> foreground_incomplete_scale_factors_;
  std::set<ui::ResourceScaleFactor> background_incomplete_scale_factors_;

  bool is_maskable_icon_ = false;
  bool is_adaptive_icon_ = false;

  // Contains pending image decode requests.
  std::vector<std::unique_ptr<DecodeRequest>> decode_requests_;

  std::vector<std::unique_ptr<FakeDecodeRequestForTesting>>
      fake_decode_requests_for_testing_;

  base::WeakPtrFactory<AppIconDecoder> weak_ptr_factory_{this};
};

// This class is used for testing only to disable the out-of process icon
// decoding.
class ScopedDecodeRequestForTesting {
 public:
  ScopedDecodeRequestForTesting();
  ScopedDecodeRequestForTesting(const ScopedDecodeRequestForTesting&) = delete;
  ScopedDecodeRequestForTesting& operator=(
      const ScopedDecodeRequestForTesting&) = delete;
  ~ScopedDecodeRequestForTesting();
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_DECODER_H_
