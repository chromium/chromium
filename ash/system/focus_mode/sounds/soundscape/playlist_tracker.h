// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_PLAYLIST_TRACKER_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_PLAYLIST_TRACKER_H_

#include <string>
#include <vector>

#include "ash/system/focus_mode/sounds/soundscape/soundscape_types.h"
#include "base/memory/raw_ref.h"

namespace ash {

// Tracks the currently played song in a playlist and provides the next track
// chosen at random. Guarantees that all tracks in a playlist are played before
// it repeats.
class PlaylistTracker {
 public:
  // Creates a shuffle of `playlist`. `playlist` must outlive this object.
  PlaylistTracker(const SoundscapePlaylist& playlist);
  PlaylistTracker(const PlaylistTracker&) = delete;
  PlaylistTracker& operator=(const PlaylistTracker&) = delete;
  ~PlaylistTracker();

  const std::string& id() const;
  const SoundscapePlaylist& playlist() const;

  // Returns the next track in the playlist, chosen at random, until the
  // playlist is exhausted. Then, returns a new order of tracks except that
  // the last played track will always be last (before tracks repeat).
  const SoundscapeTrack& NextTrack();

 private:
  raw_ref<const SoundscapePlaylist> playlist_;

  // Contains pointers to the tracks in the playlist which haven't been played
  // in shuffled order. The next track is always at the back.
  std::vector<const SoundscapeTrack*> remaining_tracks_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_SOUNDSCAPE_PLAYLIST_TRACKER_H_
