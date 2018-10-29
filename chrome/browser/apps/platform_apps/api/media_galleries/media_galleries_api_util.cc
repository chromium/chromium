// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/media_galleries/media_galleries_api_util.h"

#include "base/logging.h"
#include "chrome/common/apps/platform_apps/api/media_galleries.h"

namespace chrome_apps {
namespace api {

template <class T>
void SetValueScopedPtr(T value, std::unique_ptr<T>* destination) {
  DCHECK(destination);
  if (value >= 0)
    destination->reset(new T(value));
}

template <>
void SetValueScopedPtr(std::string value,
                       std::unique_ptr<std::string>* destination) {
  DCHECK(destination);
  if (!value.empty())
    destination->reset(new std::string(std::move(value)));
}

std::unique_ptr<base::DictionaryValue> SerializeMediaMetadata(
    chrome::mojom::MediaMetadataPtr metadata) {
  DCHECK(metadata);
  media_galleries::MediaMetadata extension_metadata;
  extension_metadata.mime_type = std::move(metadata->mime_type);
  if (metadata->height >= 0 && metadata->width >= 0) {
    extension_metadata.height.reset(new int(metadata->height));
    extension_metadata.width.reset(new int(metadata->width));
  }

  SetValueScopedPtr(metadata->duration, &extension_metadata.duration);
  SetValueScopedPtr(std::move(metadata->artist), &extension_metadata.artist);
  SetValueScopedPtr(std::move(metadata->album), &extension_metadata.album);
  SetValueScopedPtr(std::move(metadata->comment), &extension_metadata.comment);
  SetValueScopedPtr(std::move(metadata->copyright),
                    &extension_metadata.copyright);
  SetValueScopedPtr(metadata->disc, &extension_metadata.disc);
  SetValueScopedPtr(std::move(metadata->genre), &extension_metadata.genre);
  SetValueScopedPtr(std::move(metadata->language),
                    &extension_metadata.language);
  SetValueScopedPtr(metadata->rotation, &extension_metadata.rotation);
  SetValueScopedPtr(std::move(metadata->title), &extension_metadata.title);
  SetValueScopedPtr(metadata->track, &extension_metadata.track);

  for (const chrome::mojom::MediaStreamInfoPtr& info : metadata->raw_tags) {
    media_galleries::StreamInfo stream_info;
    stream_info.type = std::move(info->type);
    base::DictionaryValue* dict_value;
    info->additional_properties.GetAsDictionary(&dict_value);
    stream_info.tags.additional_properties.Swap(dict_value);
    extension_metadata.raw_tags.push_back(std::move(stream_info));
  }

  return extension_metadata.ToValue();
}

}  // namespace api
}  // namespace chrome_apps
