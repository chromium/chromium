// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_link_helper.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "url/origin.h"

namespace glic {

GlicMediaLinkHelper::GlicMediaLinkHelper(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

GlicMediaLinkHelper::~GlicMediaLinkHelper() = default;

bool GlicMediaLinkHelper::MaybeReplaceNavigation(const GURL& target) {
  auto target_origin = url::Origin::Create(target);

  // Insist that the target and the focused contents are the same origin.
  if (target_origin !=
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin()) {
    return false;
  }

  if (target_origin == url::Origin::Create(GURL("https://www.youtube.com/"))) {
    return YouTubeHelper(target);
  }
  // Add other origins here.

  return false;
}

bool GlicMediaLinkHelper::YouTubeHelper(const GURL& target) {
  // If `target` points to the same video as `web_contents` but contains a `t=`
  // parameter, assume that the goal is to seek to that point in the current
  // video.  This could also do a same-tab navigation instead of a MediaSession
  // seek, but it's not very smooth.
  auto& last_committed_url = web_contents()->GetLastCommittedURL();

  // Make sure there is a v=, and that it is the same non-empty value.
  std::string committed_v;
  if (!net::GetValueForKeyInQuery(last_committed_url, "v", &committed_v)) {
    return false;
  }
  std::string target_v;
  if (!net::GetValueForKeyInQuery(target, "v", &target_v)) {
    return false;
  }
  if (committed_v != target_v || committed_v.length() == 0) {
    return false;
  }

  // Make sure that the target specifies a t=.
  std::string t_string;
  if (!net::GetValueForKeyInQuery(target, "t", &t_string)) {
    return false;
  }

  if (!t_string.length()) {
    return false;
  }

  unsigned int t = 0;
  if (!base::StringToUint(t_string, &t)) {
    return false;
  }

  if (auto* media_session = GetMediaSessionIfExists()) {
    media_session->SeekTo(base::Seconds(t));
    return true;
  }

  return false;
}

content::MediaSession* GlicMediaLinkHelper::GetMediaSessionIfExists() {
  return content::MediaSession::GetIfExists(web_contents());
}

}  // namespace glic
