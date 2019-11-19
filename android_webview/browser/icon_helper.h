// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_ICON_HELPER_H_
#define ANDROID_WEBVIEW_BROWSER_ICON_HELPER_H_

#include <stdint.h>

#include <string>
#include <unordered_set>

#include "base/macros.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

class SkBitmap;

namespace content {
struct FaviconURL;
}

namespace gfx {
class Size;
}

namespace android_webview {

// A helper that observes favicon changes for Webview.
class IconHelper : public content::WebContentsObserver {
 public:
  class Listener {
   public:
    virtual bool ShouldDownloadFavicon(const GURL& icon_url) = 0;
    virtual void OnReceivedIcon(const GURL& icon_url,
                                const SkBitmap& bitmap) = 0;
    virtual void OnReceivedTouchIconUrl(const std::string& url,
                                        const bool precomposed) = 0;
   protected:
    virtual ~Listener() {}
  };

  explicit IconHelper(content::WebContents* web_contents);
  ~IconHelper() override;

  void SetListener(Listener* listener);

  // From WebContentsObserver
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::ReloadType reload_type) override;

  void DownloadFaviconCallback(
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

 private:
  void MarkUnableToDownloadFavicon(const GURL& icon_url);
  bool WasUnableToDownloadFavicon(const GURL& icon_url) const;
  void ClearUnableToDownloadFavicons();

  Listener* listener_;

  using MissingFaviconURLHash = size_t;
  std::unordered_set<MissingFaviconURLHash> missing_favicon_urls_;

  DISALLOW_COPY_AND_ASSIGN(IconHelper);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_ICON_HELPER_H_
