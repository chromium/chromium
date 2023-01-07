// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/history/media_history_contents_observer.h"

#include "base/containers/contains.h"
#include "chrome/browser/media/history/media_history_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_player_watch_time.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"

MediaHistoryContentsObserver::MediaHistoryContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<MediaHistoryContentsObserver>(
          *web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile->IsOffTheRecord()) {
    service_ =
        media_history::MediaHistoryKeyedServiceFactory::GetForProfile(profile);
    DCHECK(service_);
  }

  content::MediaSession::Get(web_contents)
      ->AddObserver(observer_receiver_.BindNewPipeAndPassRemote());
}

MediaHistoryContentsObserver::~MediaHistoryContentsObserver() = default;

void MediaHistoryContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  frozen_ = true;
}

void MediaHistoryContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  MaybeCommitMediaSession();

  cached_position_.reset();
  cached_metadata_.reset();
  cached_artwork_.clear();
  has_been_active_ = false;
  frozen_ = false;
  current_url_ = navigation_handle->GetURL();
}

void MediaHistoryContentsObserver::WebContentsDestroyed() {
  // The web contents is being destroyed so we might want to commit the media
  // session to the database.
  MaybeCommitMediaSession();
}

void MediaHistoryContentsObserver::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  if (frozen_)
    return;

  if (session_info->state ==
      media_session::mojom::MediaSessionInfo::SessionState::kActive) {
    has_been_active_ = true;
  }

  if (base::Contains(*session_info->audio_video_states,
                     media_session::mojom::MediaAudioVideoState::kAudioVideo)) {
    has_audio_and_video_ = true;
  } else {
    has_audio_and_video_ = false;
  }
}

void MediaHistoryContentsObserver::MediaSessionMetadataChanged(
    const absl::optional<media_session::MediaMetadata>& metadata) {
  if (!metadata.has_value() || frozen_)
    return;

  cached_metadata_ = metadata;
}

void MediaHistoryContentsObserver::MediaSessionImagesChanged(
    const base::flat_map<media_session::mojom::MediaSessionImageType,
                         std::vector<media_session::MediaImage>>& images) {
  if (frozen_)
    return;

  if (base::Contains(images,
                     media_session::mojom::MediaSessionImageType::kArtwork)) {
    cached_artwork_ =
        images.at(media_session::mojom::MediaSessionImageType::kArtwork);
  } else {
    cached_artwork_.clear();
  }
}

void MediaHistoryContentsObserver::MediaSessionPositionChanged(
    const absl::optional<media_session::MediaPosition>& position) {
  if (!position.has_value() || frozen_)
    return;

  cached_position_ = position;
}

void MediaHistoryContentsObserver::MaybeCommitMediaSession() {
  // If the media session has never played anything, does not have any metadata
  // or does not have video then we should not commit the media session.
  if (!has_been_active_ || !cached_metadata_ || cached_metadata_->IsEmpty() ||
      !service_ || !has_audio_and_video_) {
    return;
  }

  service_->SavePlaybackSession(current_url_, *cached_metadata_,
                                cached_position_, cached_artwork_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MediaHistoryContentsObserver);
