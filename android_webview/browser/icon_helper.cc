// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/icon_helper.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/notreached.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

using content::BrowserThread;
using content::WebContents;

namespace android_webview {

namespace {

const int kLargestIconSize = 192;

}  // namespace

IconHelper::IconHelper(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      listener_(nullptr),
      missing_favicon_urls_() {}

IconHelper::~IconHelper() {
}

void IconHelper::SetListener(Listener* listener) {
  listener_ = listener;
}

void IconHelper::DownloadFaviconCallback(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (http_status_code == 404) {
    MarkUnableToDownloadFavicon(image_url);
    return;
  }

  if (bitmaps.size() == 0) {
    return;
  }

  // We can protentially have multiple frames of the icon
  // in different sizes. We need more fine grain API spec
  // to let clients pick out the frame they want.

  if (listener_) {
    std::vector<size_t> best_indices;
    SelectFaviconFrameIndices(original_bitmap_sizes,
                              std::vector<int>(1U, kLargestIconSize),
                              &best_indices, nullptr);

    listener_->OnReceivedIcon(
        image_url,
        bitmaps[best_indices.size() == 0 ? 0 : best_indices.front()]);
  }
}

void IconHelper::DidUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& candidate : candidates) {
    if (!candidate->icon_url.is_valid())
      continue;

    switch (candidate->icon_type) {
      case blink::mojom::FaviconIconType::kFavicon:
        if ((listener_ &&
             !listener_->ShouldDownloadFavicon(candidate->icon_url)) ||
            WasUnableToDownloadFavicon(candidate->icon_url)) {
          break;
        }
        web_contents()->DownloadImage(
            candidate->icon_url,
            true,              // Is a favicon
            gfx::Size(),       // No preferred size
            kLargestIconSize,  // Max bitmap size
            false,             // Normal cache policy
            base::BindOnce(&IconHelper::DownloadFaviconCallback,
                           base::Unretained(this)));
        break;
      case blink::mojom::FaviconIconType::kTouchIcon:
        if (listener_)
          listener_->OnReceivedTouchIconUrl(candidate->icon_url.spec(), false);
        break;
      case blink::mojom::FaviconIconType::kTouchPrecomposedIcon:
        if (listener_)
          listener_->OnReceivedTouchIconUrl(candidate->icon_url.spec(), true);
        break;
      case blink::mojom::FaviconIconType::kInvalid:
        // Silently ignore it. Only trigger a callback on valid icons.
        break;
      default:
        NOTREACHED();
    }
  }
}

void IconHelper::DidStartNavigation(content::NavigationHandle* navigation) {
  if (navigation->IsInPrimaryMainFrame() &&
      navigation->GetReloadType() == content::ReloadType::BYPASSING_CACHE) {
    ClearUnableToDownloadFavicons();
  }
}

void IconHelper::MarkUnableToDownloadFavicon(const GURL& icon_url) {
  MissingFaviconURLHash url_hash = base::FastHash(icon_url.spec());
  missing_favicon_urls_.insert(url_hash);
}

bool IconHelper::WasUnableToDownloadFavicon(const GURL& icon_url) const {
  MissingFaviconURLHash url_hash = base::FastHash(icon_url.spec());
  return base::Contains(missing_favicon_urls_, url_hash);
}

void IconHelper::ClearUnableToDownloadFavicons() {
  missing_favicon_urls_.clear();
}

}  // namespace android_webview
