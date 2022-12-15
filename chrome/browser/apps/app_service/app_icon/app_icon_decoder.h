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
  // Decode images safely in a sandboxed service per ARC app icons' security
  // requests.
  class DecodeRequest : public ImageDecoder::ImageRequest {
   public:
    DecodeRequest(ui::ResourceScaleFactor scale_factor, AppIconDecoder& host);

    DecodeRequest(const DecodeRequest&) = delete;
    DecodeRequest& operator=(const DecodeRequest&) = delete;

    ~DecodeRequest() override;

    // ImageDecoder::ImageRequest
    void OnImageDecoded(const SkBitmap& bitmap) override;
    void OnDecodeImageFailed() override;

   private:
    ui::ResourceScaleFactor scale_factor_;
    AppIconDecoder& host_;
  };

  void OnIconRead(std::map<ui::ResourceScaleFactor, IconValuePtr> icon_data);

  void UpdateImageSkia(ui::ResourceScaleFactor scale_factor,
                       const SkBitmap& bitmap);

  void DiscardDecodeRequest();

  const base::FilePath base_path_;
  const std::string app_id_;
  int32_t size_in_dip_;
  base::OnceCallback<void(AppIconDecoder* decoder, IconValuePtr iv)> callback_;

  gfx::ImageSkia image_skia_;
  std::set<ui::ResourceScaleFactor> incomplete_scale_factors_;

  bool is_maskable_icon_;

  // Contains pending image decode requests.
  std::vector<std::unique_ptr<DecodeRequest>> decode_requests_;

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
