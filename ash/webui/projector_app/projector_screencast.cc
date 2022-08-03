// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_screencast.h"

namespace ash {

base::Value::Dict ProjectorScreencastVideo::ToValue() const {
  base::Value::Dict dict;
  dict.Set("srcUrl", src_url);
  dict.Set("fileId", file_id);
  return dict;
}

ProjectorScreencast::ProjectorScreencast() = default;

ProjectorScreencast::ProjectorScreencast(const ProjectorScreencast&) = default;

ProjectorScreencast& ProjectorScreencast::operator=(
    const ProjectorScreencast&) = default;

ProjectorScreencast::~ProjectorScreencast() = default;

base::Value::Dict ProjectorScreencast::ToValue() const {
  base::Value::Dict dict;
  dict.Set("containerFolderId", container_folder_id);
  dict.Set("name", name);
  dict.Set("metadataFileId", metadata_file_id);
  dict.Set("video", video.ToValue());

  return dict;
}

}  // namespace ash
