// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_LINK_HELPER_H_
#define CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_LINK_HELPER_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace content {
class MediaSession;
class WebContents;
}  // namespace content

namespace glic {

// Allow embedded media to be controlled by a link helper.
BASE_DECLARE_FEATURE(kMediaLinkEmbedHelper);

class GlicMediaLinkHelper {
 public:
  explicit GlicMediaLinkHelper(content::WebContents* web_contents);
  virtual ~GlicMediaLinkHelper();

  // Called when the user clicks a link to `target` while `web_contents` is the
  // focused tab.  Returns true if the navigation should be skipped, false if it
  // should be allowed to proceed.  When this returns true, an origin-specific
  // helper can cause side-effects to take the place of the navigation.
  bool MaybeReplaceNavigation(const GURL& target);

  content::WebContents* web_contents() { return web_contents_; }

  // Use this instead of MediaSession::GetIfExists() to make tests easier.
  virtual content::MediaSession* GetMediaSessionIfExists();

  // Return the time from a `t=` parameter if it exists.  Public for testing.
  static std::optional<base::TimeDelta> ExtractTimeFromQueryIfExists(
      const GURL& target);

  // Return the video ID if it's set in `url`.  Public for testing.
  static std::optional<std::string> ExtractVideoNameIfExists(const GURL& url);

 private:
  bool YouTubeHelper(const GURL& target);
  bool YouTubeEmbedHelper(const GURL& target);

  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_MEDIA_GLIC_MEDIA_LINK_HELPER_H_
