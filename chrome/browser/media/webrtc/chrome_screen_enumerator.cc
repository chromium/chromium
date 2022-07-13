// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/chrome_screen_enumerator.h"

#include "base/feature_list.h"
#include "base/task/bind_post_task.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ui/aura/window.h"
#endif

struct ScreenWithMetaData {
  ScreenWithMetaData(const content::DesktopMediaID& id, const gfx::Rect& bounds)
      : id(id), bounds(bounds) {}
  content::DesktopMediaID id;
  gfx::Rect bounds;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

std::vector<aura::Window*>* root_windows_for_testing_ = nullptr;

blink::mojom::StreamDevicesSetPtr EnumerateScreensAsh(
    blink::mojom::MediaStreamType stream_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<ScreenWithMetaData> screens_with_metadata;
  aura::Window::Windows root_windows =
      (root_windows_for_testing_) ? std::move(*root_windows_for_testing_)
                                  : ash::Shell::GetAllRootWindows();

  if (root_windows.empty()) {
    return blink::mojom::StreamDevicesSet::New();
  }

  for (aura::Window* window : root_windows) {
    content::DesktopMediaID media_id =
        content::DesktopMediaID::RegisterNativeWindow(
            content::DesktopMediaID::TYPE_SCREEN, window);
    gfx::Rect bounds = window->GetActualBoundsInScreen();
    if (window == ash::Shell::GetPrimaryRootWindow()) {
      screens_with_metadata.emplace(screens_with_metadata.begin(),
                                    std::move(media_id), bounds);
    } else {
      screens_with_metadata.emplace_back(std::move(media_id), bounds);
    }
  }
  base::ranges::stable_sort(
      screens_with_metadata,
      [](const ScreenWithMetaData& lhs, const ScreenWithMetaData& rhs) {
        return lhs.bounds.origin() < rhs.bounds.origin();
      });

  blink::mojom::StreamDevicesSetPtr stream_devices_set =
      blink::mojom::StreamDevicesSet::New();
  for (const ScreenWithMetaData& screen_with_metadata : screens_with_metadata) {
    const content::DesktopMediaID& media_id = screen_with_metadata.id;
    DCHECK_EQ(content::DesktopMediaID::Type::TYPE_SCREEN, media_id.type);

    // Add selected desktop source to the list.
    blink::MediaStreamDevice device(stream_type, /*id=*/media_id.ToString(),
                                    /*name=*/"Screen");
    device.display_media_info = media::mojom::DisplayMediaInformation::New(
        /*display_surface=*/media::mojom::DisplayCaptureSurfaceType::MONITOR,
        /*logical_surface=*/true,
        /*cursor=*/media::mojom::CursorCaptureType::NEVER,
        /*capture_handle=*/nullptr);
    stream_devices_set->stream_devices.push_back(
        blink::mojom::StreamDevices::New(/*audio_device=*/absl::nullopt,
                                         /*video_device=*/device));
  }
  return stream_devices_set;
}

}  // namespace

void SetRootWindowsForTesting(std::vector<aura::Window*>* root_windows) {
  root_windows_for_testing_ = root_windows;
}

#endif

ChromeScreenEnumerator::ChromeScreenEnumerator() = default;

ChromeScreenEnumerator::~ChromeScreenEnumerator() = default;

void ChromeScreenEnumerator::EnumerateScreens(
    blink::mojom::MediaStreamType stream_type,
    ScreensCallback screens_callback) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(base::FeatureList::IsEnabled(features::kGetDisplayMediaSet));
  DCHECK(base::FeatureList::IsEnabled(
      features::kGetDisplayMediaSetAutoSelectAllScreens));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(EnumerateScreensAsh, stream_type),
      base::BindOnce(
          [](ScreensCallback screens_callback,
             blink::mojom::StreamDevicesSetPtr stream_devices_set) {
            std::move(screens_callback)
                .Run(*stream_devices_set,
                     blink::mojom::MediaStreamRequestResult::OK);
          },
          std::move(screens_callback)));
#else
  // TODO(crbug.com/1300883): Implement for other platforms than Chrome OS ash.
  NOTREACHED();
  std::move(screens_callback)
      .Run(blink::mojom::StreamDevicesSet(),
           blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED);
#endif
}
