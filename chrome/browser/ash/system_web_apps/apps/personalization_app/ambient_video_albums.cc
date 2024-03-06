// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/ambient_video_albums.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "url/gurl.h"

namespace ash::personalization_app {
namespace {

struct VideoAlbumInfo {
  AmbientVideo video;
  std::string_view id;
  std::string_view url;
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
    album->id = std::string(video_album_info.id);
    album->checked = currently_selected_video == video_album_info.video;
    album->title = l10n_util::GetStringUTF8(video_album_info.title_resource_id);
    // Product name does not need to be translated.
    auto product_name =
        l10n_util::GetStringUTF16(ui::GetChromeOSDeviceTypeResourceId());
    if (features::IsTimeOfDayScreenSaverEnabled()) {
      product_name = base::UTF8ToUTF16(
          AmbientBackendController::Get()->GetTimeOfDayProductName());
    }
    album->description = l10n_util::GetStringFUTF8(
        IDS_PERSONALIZATION_APP_TIME_OF_DAY_VIDEO_ALBUM_DESCRIPTION,
        product_name);
    album->url = GURL(video_album_info.url);
    album->topic_source = mojom::TopicSource::kVideo;
    output.emplace_back(std::move(album));
  }
}

std::optional<AmbientVideo> FindAmbientVideoByAlbumId(std::string_view id) {
  for (const VideoAlbumInfo& album_info : kAllVideoAlbumInfo) {
    if (album_info.id == id) {
      return album_info.video;
    }
  }
  return std::nullopt;
}

}  // namespace ash::personalization_app
