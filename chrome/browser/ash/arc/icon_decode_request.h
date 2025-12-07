// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ICON_DECODE_REQUEST_H_
#define CHROME_BROWSER_ASH_ARC_ICON_DECODE_REQUEST_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

class SkBitmap;

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

class IconDecodeRequest {
 public:
  using SetIconCallback = base::OnceCallback<void(const gfx::ImageSkia& icon)>;

  explicit IconDecodeRequest(int dimension_dip);
  IconDecodeRequest(const IconDecodeRequest&) = delete;
  IconDecodeRequest& operator=(const IconDecodeRequest&) = delete;
  ~IconDecodeRequest();

  // Disables async safe decoding requests when unit tests are executed.
  // Icons are decoded at a separate process. In unit tests these tasks may not
  // finish before the test exits, which causes a failure in the
  // base::CurrentThread::Get()->IsIdleForTesting() check in
  // content::~BrowserTaskEnvironment().
  static void DisableSafeDecodingForTesting();

  // Starts image decoding. Safe asynchronous decoding is used unless
  // DisableSafeDecodingForTesting() is called.
  void Start(const std::vector<uint8_t>& image_data,
             SetIconCallback set_icon_callback);

 private:
  void OnImageDecoded(SetIconCallback callback, const SkBitmap& bitmap);

  const int dimension_dip_;

  base::WeakPtrFactory<IconDecodeRequest> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ICON_DECODE_REQUEST_H_
