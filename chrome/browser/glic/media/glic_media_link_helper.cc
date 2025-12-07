// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_link_helper.h"

#include "base/strings/string_number_conversions.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "net/base/url_util.h"
#include "url/origin.h"

namespace glic {

// Allows embedded media to be controlled by a link helper.
BASE_FEATURE(kMediaLinkEmbedHelper, base::FEATURE_ENABLED_BY_DEFAULT);

GlicMediaLinkHelper::GlicMediaLinkHelper(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

GlicMediaLinkHelper::~GlicMediaLinkHelper() = default;

bool GlicMediaLinkHelper::MaybeReplaceNavigation(const GURL& target) {
  const std::string youtube_host("www.youtube.com");

  // Handle embedded YT first, since it's experimental.  For any YT target, let
  // the embed helper figure out if it applies to any frame.
  if (base::FeatureList::IsEnabled(kMediaLinkEmbedHelper)) {
    if (target.GetHost() == youtube_host && YouTubeEmbedHelper(target)) {
      return true;
    }
  }

  const GURL& committed_url =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL();

  // Insist that the target and the focused contents have the same host.
  if (target.GetHost() != committed_url.GetHost()) {
    return false;
  }

  if (target.GetHost() == youtube_host) {
    return YouTubeHelper(target);
  }

  return false;
}

// static
std::optional<base::TimeDelta>
GlicMediaLinkHelper::ExtractTimeFromQueryIfExists(const GURL& target) {
  // Make sure that the target specifies a t=.
  std::string t_string;
  if (!net::GetValueForKeyInQuery(target, "t", &t_string)) {
    return {};
  }

  if (!t_string.length()) {
    return {};
  }

  unsigned int t = 0;
  if (!base::StringToUint(t_string, &t)) {
    return {};
  }

  return base::Seconds(t);
}

// static
std::optional<std::string> GlicMediaLinkHelper::ExtractVideoNameIfExists(
    const GURL& url) {
  // `url` is a link to www.youtube.com.  The video name is either the value of
  // the `v=` query param if the format is "...youtube.com/watch", or the last
  // part of the path if it's "...youtube.com/embed/video name here".
  // Extract it and return it, or else {} if there's no match.
  std::string video_name;
  if (url.GetPath() == "/watch") {
    if (net::GetValueForKeyInQuery(url, "v", &video_name) &&
        !video_name.empty()) {
      return video_name;
    }
  } else if (base::StartsWith(url.GetPath(), "/embed/")) {
    video_name = url.GetPath().substr(strlen("/embed/"));
    if (!video_name.empty()) {
      return video_name;
    }
  }
  return {};
}

bool GlicMediaLinkHelper::YouTubeEmbedHelper(const GURL& target) {
  // `target` might be `www.youtube.com/watch` with `v=videoname`, while the
  // video we're looking for might be in a subframe.  Use the media session's
  // routed frame, since that's the one we can control.
  auto* media_session = GetMediaSessionIfExists();
  if (!media_session) {
    return false;
  }
  auto* media_session_rfh = media_session->GetRoutedFrame();
  if (!media_session_rfh) {
    return false;
  }

  // Unlike normal helpers, this hasn't been checked yet.  We just figured out
  // the frame now.
  const auto& last_committed_url = media_session_rfh->GetLastCommittedURL();
  if (last_committed_url.GetHost() != target.GetHost()) {
    // Mediasession is not controlling YT.
    return false;
  }

  // Make sure the video names exist and match.
  auto committed_v = ExtractVideoNameIfExists(last_committed_url);
  auto target_v = ExtractVideoNameIfExists(target);
  if (!committed_v || !target_v || *committed_v != *target_v) {
    return false;
  }

  // If there is a `t=` parameter in `target`, then use it.
  if (auto maybe_time = ExtractTimeFromQueryIfExists(target)) {
    media_session->SeekTo(*maybe_time);
    return true;
  }

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

  // This should be refactored to use the `IfExists` methods, but for now we
  // don't want to break working code.

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
