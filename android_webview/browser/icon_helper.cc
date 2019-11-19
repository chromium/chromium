// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/icon_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/favicon_url.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

using content::BrowserThread;
using content::WebContents;

namespace android_webview {

IconHelper::IconHelper(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      listener_(NULL),
      missing_favicon_urls_() {
}

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

  // TODO(acleung): Pick the best icon to return based on size.
  if (listener_)
    listener_->OnReceivedIcon(image_url, bitmaps[0]);
}

void IconHelper::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (std::vector<content::FaviconURL>::const_iterator i = candidates.begin();
       i != candidates.end(); ++i) {
    if (!i->icon_url.is_valid())
      continue;

    switch(i->icon_type) {
      case content::FaviconURL::IconType::kFavicon:
        if ((listener_ && !listener_->ShouldDownloadFavicon(i->icon_url)) ||
            WasUnableToDownloadFavicon(i->icon_url)) {
          break;
        }
        web_contents()->DownloadImage(
            i->icon_url,
            true,   // Is a favicon
            0,      // No preferred size
            0,      // No maximum size
            false,  // Normal cache policy
            base::BindOnce(&IconHelper::DownloadFaviconCallback,
                           base::Unretained(this)));
        break;
      case content::FaviconURL::IconType::kTouchIcon:
        if (listener_)
          listener_->OnReceivedTouchIconUrl(i->icon_url.spec(), false);
        break;
      case content::FaviconURL::IconType::kTouchPrecomposedIcon:
        if (listener_)
          listener_->OnReceivedTouchIconUrl(i->icon_url.spec(), true);
        break;
      case content::FaviconURL::IconType::kInvalid:
        // Silently ignore it. Only trigger a callback on valid icons.
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

void IconHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::ReloadType reload_type) {
  if (reload_type == content::ReloadType::BYPASSING_CACHE)
    ClearUnableToDownloadFavicons();
}

void IconHelper::MarkUnableToDownloadFavicon(const GURL& icon_url) {
  MissingFaviconURLHash url_hash = base::FastHash(icon_url.spec());
  missing_favicon_urls_.insert(url_hash);
}

bool IconHelper::WasUnableToDownloadFavicon(const GURL& icon_url) const {
  MissingFaviconURLHash url_hash = base::FastHash(icon_url.spec());
  return missing_favicon_urls_.find(url_hash) != missing_favicon_urls_.end();
}

void IconHelper::ClearUnableToDownloadFavicons() {
  missing_favicon_urls_.clear();
}

}  // namespace android_webview
