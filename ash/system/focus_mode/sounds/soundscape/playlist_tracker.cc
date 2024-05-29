// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/sounds/soundscape/playlist_tracker.h"

#include "base/rand_util.h"
#include "base/uuid.h"

namespace ash {

PlaylistTracker::PlaylistTracker(const SoundscapePlaylist& playlist)
    : playlist_(playlist) {
  CHECK(!playlist_->tracks.empty());

  remaining_tracks_.reserve(playlist_->tracks.size());
  for (const auto& track : playlist_->tracks) {
    remaining_tracks_.push_back(&track);
  }

  base::RandomShuffle(remaining_tracks_.begin(), remaining_tracks_.end());
}

PlaylistTracker::~PlaylistTracker() = default;

const std::string& PlaylistTracker::id() const {
  return playlist_->uuid.AsLowercaseString();
}

const SoundscapePlaylist& PlaylistTracker::playlist() const {
  return playlist_.get();
}

const SoundscapeTrack& PlaylistTracker::NextTrack() {
  const SoundscapeTrack& track = *remaining_tracks_.back();
  remaining_tracks_.pop_back();

  if (remaining_tracks_.empty()) {
    remaining_tracks_.push_back(&track);

    // Refill the sequence with everything except the last track so it's
    // guaranteed not to repeat immediately.
    for (const SoundscapeTrack& entry : playlist_->tracks) {
      if (entry.path != track.path) {
        remaining_tracks_.push_back(&entry);
      }
    }
    base::RandomShuffle(remaining_tracks_.begin() + 1, remaining_tracks_.end());
  }

  return track;
}

}  // namespace ash
