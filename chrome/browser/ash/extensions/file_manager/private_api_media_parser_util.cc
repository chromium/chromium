// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_media_parser_util.h"

#include <memory>

#include "base/check.h"
#include "base/values.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom.h"
#include "net/base/mime_util.h"

namespace {

template <class T>
void SetValueOptional(T value, std::optional<T>* destination) {
  DCHECK(destination);
  if (value >= 0) {
    *destination = value;
  }
}

template <>
void SetValueOptional(std::string value,
                      std::optional<std::string>* destination) {
  DCHECK(destination);
  if (!value.empty()) {
    *destination = std::move(value);
  }
}

void ChangeAudioMimePrefixToVideo(std::string* mime_type) {
  const std::string audio_type("audio/*");
  if (net::MatchesMimeType(audio_type, *mime_type)) {
    mime_type->replace(0, audio_type.length() - 1, "video/");
  }
}

}  // namespace

namespace extensions {

namespace api {

namespace file_manager_private {

base::Value::Dict MojoMediaMetadataToValue(
    chrome::mojom::MediaMetadataPtr metadata) {
  DCHECK(metadata);

  file_manager_private::MediaMetadata media_metadata;
  media_metadata.mime_type = std::move(metadata->mime_type);

  // Video files have dimensions.
  if (metadata->height >= 0 && metadata->width >= 0) {
    ChangeAudioMimePrefixToVideo(&media_metadata.mime_type);
    SetValueOptional(metadata->height, &media_metadata.height);
    SetValueOptional(metadata->width, &media_metadata.width);
  }

  SetValueOptional(metadata->duration, &media_metadata.duration);
  SetValueOptional(metadata->rotation, &media_metadata.rotation);
  SetValueOptional(std::move(metadata->artist), &media_metadata.artist);
  SetValueOptional(std::move(metadata->album), &media_metadata.album);
  SetValueOptional(std::move(metadata->comment), &media_metadata.comment);
  SetValueOptional(std::move(metadata->copyright), &media_metadata.copyright);
  SetValueOptional(metadata->disc, &media_metadata.disc);
  SetValueOptional(std::move(metadata->genre), &media_metadata.genre);
  SetValueOptional(std::move(metadata->language), &media_metadata.language);
  SetValueOptional(std::move(metadata->title), &media_metadata.title);
  SetValueOptional(metadata->track, &media_metadata.track);

  for (const chrome::mojom::MediaStreamInfoPtr& info : metadata->raw_tags) {
    file_manager_private::StreamInfo stream_info;
    stream_info.type = std::move(info->type);
    std::swap(stream_info.tags.additional_properties,
              info->additional_properties);
    media_metadata.raw_tags.push_back(std::move(stream_info));
  }

  return media_metadata.ToValue();
}

}  // namespace file_manager_private

}  // namespace api

}  // namespace extensions
