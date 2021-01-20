// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_NTP_ICON_SOURCE_H_
#define CHROME_BROWSER_SEARCH_NTP_ICON_SOURCE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "content/public/browser/url_data_source.h"

class GURL;
class Profile;
class SkBitmap;

namespace favicon_base {
struct FaviconRawBitmapResult;
}

namespace gfx {
class Image;
}

// NTP Icon Source is the gateway between network-level chrome: requests for
// NTP icons and the various backends that may serve them.
class NtpIconSource : public content::URLDataSource {
 public:
  explicit NtpIconSource(Profile* profile);
  ~NtpIconSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;

 private:
  struct NtpIconRequest;
  void OnLocalFaviconAvailable(
      NtpIconRequest request,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);
  // Returns whether |url| is in the set of server suggestions.
  bool IsRequestedUrlInServerSuggestions(const GURL& url);
  void RequestServerFavicon(NtpIconRequest request);
  void OnServerFaviconAvailable(NtpIconRequest request,
                                const gfx::Image& fetched_image,
                                const image_fetcher::RequestMetadata& metadata);

  // Will call |request.callback| with the rendered icon. |bitmap| can be empty,
  // in which case the returned icon is a fallback circle with a letter drawn
  // into it.
  void ReturnRenderedIconForRequest(NtpIconRequest request,
                                    const SkBitmap& bitmap);

  base::CancelableTaskTracker cancelable_task_tracker_;
  Profile* profile_;
  std::unique_ptr<image_fetcher::ImageFetcher> const image_fetcher_;

  base::WeakPtrFactory<NtpIconSource> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NtpIconSource);
};

#endif  // CHROME_BROWSER_SEARCH_NTP_ICON_SOURCE_H_
