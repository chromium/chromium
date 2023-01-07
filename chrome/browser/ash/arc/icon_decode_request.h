// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ICON_DECODE_REQUEST_H_
#define CHROME_BROWSER_ASH_ARC_ICON_DECODE_REQUEST_H_

#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/image_decoder/image_decoder.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace arc {

// This is used for metrics, so do not remove or reorder existing entries.
enum class ArcAppShortcutStatus {
  kEmpty = 0,
  kNotEmpty = 1,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kNotEmpty
};

class IconDecodeRequest : public ImageDecoder::ImageRequest {
 public:
  using SetIconCallback = base::OnceCallback<void(const gfx::ImageSkia& icon)>;

  IconDecodeRequest(SetIconCallback set_icon_callback, int dimension_dip);

  IconDecodeRequest(const IconDecodeRequest&) = delete;
  IconDecodeRequest& operator=(const IconDecodeRequest&) = delete;

  ~IconDecodeRequest() override;

  // Disables async safe decoding requests when unit tests are executed.
  // Icons are decoded at a separate process created by ImageDecoder. In unit
  // tests these tasks may not finish before the test exits, which causes a
  // failure in the base::CurrentThread::Get()->IsIdleForTesting() check
  // in content::~BrowserTaskEnvironment().
  static void DisableSafeDecodingForTesting();

  // Starts image decoding. Safe asynchronous decoding is used unless
  // DisableSafeDecodingForTesting() is called.
  void StartWithOptions(const std::vector<uint8_t>& image_data);

  // ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& bitmap) override;
  void OnDecodeImageFailed() override;

 private:
  SetIconCallback set_icon_callback_;
  const int dimension_dip_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ICON_DECODE_REQUEST_H_
