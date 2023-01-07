// Copyright 2022 The Chromium Authors
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
  std::string file_id;
  std::string duration_millis;
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_SCREENCAST_H_
