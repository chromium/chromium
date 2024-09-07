// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_DELEGATE_H_
#define ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "url/gurl.h"

namespace ash {

// An interface for APIs which provide sound data for Focus Mode.
class ASH_EXPORT FocusModeSoundsDelegate {
 public:
  struct Playlist {
    Playlist(const std::string& id,
             const std::string& title,
             const GURL& thumbnail_url);
    Playlist(const Playlist&);
    Playlist& operator=(const Playlist&);
    ~Playlist();

    std::string ToString() const;

    std::string id;
    std::string title;
    GURL thumbnail_url;
  };

  struct Track {
    Track(const std::string& title,
          const std::string& artist,
          const std::string& source,
          const GURL& thumbnail_url,
          const GURL& source_url,
          const bool enable_playback_reporting);
    Track(const Track&);
    Track& operator=(const Track&);
    ~Track();

    std::string ToString() const;

    bool operator==(const Track& other) const = default;

    std::string title;
    std::string artist;
    std::string source;
    GURL thumbnail_url;
    GURL source_url;
    bool enable_playback_reporting;
  };

  virtual ~FocusModeSoundsDelegate() = default;

  // Request the next track in `playlist_id`. The next track in a playlist is
  // implementation dependent. `callback` is called when the next track is ready
  // and is expected to be playable. If the next `Track` is unavailable due to a
  // service failure, the value will be nullopt.
  using TrackCallback = base::OnceCallback<void(const std::optional<Track>&)>;
  virtual void GetNextTrack(const std::string& playlist_id,
                            TrackCallback callback) = 0;

  // Request the list of playlists for Focus Mode. In the event of request
  // failure, `playlists` will be empty.
  using PlaylistsCallback =
      base::OnceCallback<void(const std::vector<Playlist>& playlists)>;
  virtual void GetPlaylists(PlaylistsCallback callback) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_SOUNDS_FOCUS_MODE_SOUNDS_DELEGATE_H_
