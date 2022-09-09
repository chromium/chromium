// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_CHROME_SCREEN_ENUMERATOR_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_CHROME_SCREEN_ENUMERATOR_H_

#include "media/capture/content/screen_enumerator.h"

#include <vector>

#include "base/callback.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-forward.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace aura {
class Window;
}

void SetRootWindowsForTesting(std::vector<aura::Window*>* root_windows);

#endif

// Whereas ScreenEnumerator is exposed in content/,
// the current concrete implementation uses elements
// from chrome/browser/.
class ChromeScreenEnumerator : public media::ScreenEnumerator {
 public:
  ChromeScreenEnumerator();
  ~ChromeScreenEnumerator() override;

  using ScreensCallback = base::OnceCallback<void(
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      blink::mojom::MediaStreamRequestResult result)>;

  void EnumerateScreens(blink::mojom::MediaStreamType stream_type,
                        ScreensCallback screens_callback) const override;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_CHROME_SCREEN_ENUMERATOR_H_
