// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/ambient_video_albums.h"

#include <utility>

#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace ash::personalization_app {
namespace {

struct VideoAlbumInfo {
  AmbientVideo video;
  base::StringPiece id;
  base::StringPiece url;
  int title_resource_id;
};

constexpr VideoAlbumInfo kAllVideoAlbumInfo[] = {
    {AmbientVideo::kClouds, kCloudsAlbumId,
     /*url=*/"chrome://personalization/time_of_day/thumbnails/clouds.jpg",
     IDS_PERSONALIZATION_APP_TIME_OF_DAY_VIDEO_CLOUDS_ALBUM_TITLE},
    {AmbientVideo::kNewMexico, kNewMexicoAlbumId,
     /*url=*/"chrome://personalization/time_of_day/thumbnails/new_mexico.jpg",
     IDS_PERSONALIZATION_APP_TIME_OF_DAY_VIDEO_NEW_MEXICO_ALBUM_TITLE}};

}  // namespace

void AppendAmbientVideoAlbums(AmbientVideo currently_selected_video,
                              std::vector<mojom::AmbientModeAlbumPtr>& output) {
  for (const VideoAlbumInfo& video_album_info : kAllVideoAlbumInfo) {
    mojom::AmbientModeAlbumPtr album = mojom::AmbientModeAlbum::New();
    album->id = video_album_info.id.data();
    album->checked = currently_selected_video == video_album_info.video;
    album->title = l10n_util::GetStringUTF8(video_album_info.title_resource_id);
    album->description = l10n_util::GetStringUTF8(
        IDS_PERSONALIZATION_APP_TIME_OF_DAY_VIDEO_ALBUM_DESCRIPTION);
    album->url = GURL(video_album_info.url);
    album->topic_source = AmbientModeTopicSource::kVideo;
    output.emplace_back(std::move(album));
  }
}

absl::optional<AmbientVideo> FindAmbientVideoByAlbumId(base::StringPiece id) {
  for (const VideoAlbumInfo& album_info : kAllVideoAlbumInfo) {
    if (album_info.id == id) {
      return album_info.video;
    }
  }
  return absl::nullopt;
}

}  // namespace ash::personalization_app
