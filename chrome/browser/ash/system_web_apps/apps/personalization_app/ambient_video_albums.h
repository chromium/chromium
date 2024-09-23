// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_AMBIENT_VIDEO_ALBUMS_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_AMBIENT_VIDEO_ALBUMS_H_

#include <optional>
#include <string_view>
#include <vector>

#include "ash/constants/ambient_video.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"

namespace ash::personalization_app {

// All content here is specific to |TopicSource::kVideo|.
//
// The ambient video theme is special. When selected, the |TopicSource|
// automatically becomes |kVideo|; the others (google photos, art gallery) are
// not possible because the ambient video UI does not use photos from the
// ambient backend at all. The ambient backend is not even aware of the |kVideo|
// topic source.
//
// That being said, the personalization app does see the video topic source as
// any other topic source in that is has a set of "albums" to pick from (one for
// each possible video) and a preview image for each "album" (a snapshot from
// each video). As such, these methods play the bare minimum part of the
// "backend" for this topic source in the same way that
// |AmbientBackendController| serves data about the gphotos and art gallery
// topic sources.

// Appends all possible video albums (one for each possible |AmbientVideo|
// value) to the |output|. The |currently_selected_video| is used for telling
// personalization app which album should be checked in the UI.
void AppendAmbientVideoAlbums(AmbientVideo currently_selected_video,
                              std::vector<mojom::AmbientModeAlbumPtr>& output);

// Returns the |AmbientVideo| for the corresponding |id|. |id| must match that
// of one of the |AmbientModeAlbum|s returned by |AppendAmbientVideoAlbums()|.
// Returns nullopt if not.
std::optional<AmbientVideo> FindAmbientVideoByAlbumId(std::string_view id);

inline constexpr std::string_view kCloudsAlbumId = "AmbientCloudsVideoAlbumId";
inline constexpr std::string_view kNewMexicoAlbumId =
    "AmbientNewMexicoVideoAlbumId";

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_AMBIENT_VIDEO_ALBUMS_H_
