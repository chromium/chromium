// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/media_galleries/media_galleries_api_util.h"

#include <memory>

#include "base/check.h"
#include "chrome/common/apps/platform_apps/api/media_galleries.h"

namespace chrome_apps {
namespace api {

template <typename T>
void SetValueOptional(T value, std::optional<T>& destination) {
  if (value >= 0)
    destination = value;
}

template <>
void SetValueOptional(std::string value,
                      std::optional<std::string>& destination) {
  if (!value.empty())
    destination = std::move(value);
}

base::Value::Dict SerializeMediaMetadata(
    chrome::mojom::MediaMetadataPtr metadata) {
  DCHECK(metadata);
  media_galleries::MediaMetadata extension_metadata;
  extension_metadata.mime_type = std::move(metadata->mime_type);
  if (metadata->height >= 0 && metadata->width >= 0) {
    extension_metadata.height = metadata->height;
    extension_metadata.width = metadata->width;
  }

  SetValueOptional(metadata->duration, extension_metadata.duration);
  SetValueOptional(std::move(metadata->artist), extension_metadata.artist);
  SetValueOptional(std::move(metadata->album), extension_metadata.album);
  SetValueOptional(std::move(metadata->comment), extension_metadata.comment);
  SetValueOptional(std::move(metadata->copyright),
                   extension_metadata.copyright);
  SetValueOptional(metadata->disc, extension_metadata.disc);
  SetValueOptional(std::move(metadata->genre), extension_metadata.genre);
  SetValueOptional(std::move(metadata->language), extension_metadata.language);
  SetValueOptional(metadata->rotation, extension_metadata.rotation);
  SetValueOptional(std::move(metadata->title), extension_metadata.title);
  SetValueOptional(metadata->track, extension_metadata.track);

  for (const chrome::mojom::MediaStreamInfoPtr& info : metadata->raw_tags) {
    media_galleries::StreamInfo stream_info;
    stream_info.type = std::move(info->type);
    stream_info.tags.additional_properties =
        std::move(info->additional_properties);
    extension_metadata.raw_tags.push_back(std::move(stream_info));
  }

  return extension_metadata.ToValue();
}

}  // namespace api
}  // namespace chrome_apps
