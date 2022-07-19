// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PROJECTOR_SCREENCAST_H_
#define ASH_WEBUI_PROJECTOR_APP_PROJECTOR_SCREENCAST_H_

#include <string>

namespace base {
class Value;
}  // namespace base

namespace ash {

// The video object for screencast.
struct ProjectorScreencastVideo {
  base::Value ToValue() const;
  // TODO(b/236857019): Add thumbnail link and video file id.
  std::string src_url;
};

// Struct of screencast model.
struct ProjectorScreencast {
  ProjectorScreencast();
  ProjectorScreencast(const ProjectorScreencast&);
  ProjectorScreencast& operator=(const ProjectorScreencast&);
  ~ProjectorScreencast();

  base::Value ToValue() const;

  // Only available for screencasts locate in DriveFs.
  std::string container_folder_id;

  std::string name;

  ProjectorScreencastVideo video;

  // TODO(b/236857019): 1 Implement following fields: status, metadata_file_id,
  // metadata, is_editable, upload_progress.
  //  2 Replace the PendingScreencast struct defined in ProjectorAppClient with
  //  this struct.
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_SCREENCAST_H_
