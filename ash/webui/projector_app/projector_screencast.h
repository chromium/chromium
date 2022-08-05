// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PROJECTOR_SCREENCAST_H_
#define ASH_WEBUI_PROJECTOR_APP_PROJECTOR_SCREENCAST_H_

#include <string>

#include "base/values.h"

namespace ash {

// The video object for screencast.
struct ProjectorScreencastVideo {
  base::Value::Dict ToValue() const;
  // TODO(b/236857019): Add thumbnail link and duration millis.
  std::string file_id;
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_SCREENCAST_H_
