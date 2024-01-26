// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_CHROME_SCREEN_ENUMERATOR_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_CHROME_SCREEN_ENUMERATOR_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "media/capture/content/screen_enumerator.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-forward.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace aura {
class Window;
}

#elif BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
namespace webrtc {
class DesktopCapturer;
}

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  static void SetRootWindowsForTesting(
      std::vector<raw_ptr<aura::Window, VectorExperimental>> root_windows);
#elif BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  static void SetDesktopCapturerForTesting(
      std::unique_ptr<webrtc::DesktopCapturer> capturer);
#endif

  void EnumerateScreens(blink::mojom::MediaStreamType stream_type,
                        ScreensCallback screens_callback) const override;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_CHROME_SCREEN_ENUMERATOR_H_
