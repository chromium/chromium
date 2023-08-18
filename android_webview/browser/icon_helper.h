// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_ICON_HELPER_H_
#define ANDROID_WEBVIEW_BROWSER_ICON_HELPER_H_

#include <stdint.h>

#include <string>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"
#include "url/gurl.h"

class SkBitmap;

namespace gfx {
class Size;
}

namespace android_webview {

// A helper that observes favicon changes for Webview.
// Lifetime: WebView
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

  IconHelper(const IconHelper&) = delete;
  IconHelper& operator=(const IconHelper&) = delete;

  ~IconHelper() override;

  void SetListener(Listener* listener);

  // From WebContentsObserver
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;
  void DidStartNavigation(content::NavigationHandle* navigation) override;

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

  raw_ptr<Listener> listener_;

  using MissingFaviconURLHash = size_t;
  std::unordered_set<MissingFaviconURLHash> missing_favicon_urls_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_ICON_HELPER_H_
