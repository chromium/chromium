// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_screencast.h"

namespace ash {

base::Value::Dict ProjectorScreencastVideo::ToValue() const {
  base::Value::Dict dict;
  dict.Set("fileId", file_id);
  dict.Set("durationMillis", duration_millis);
  return dict;
}

}  // namespace ash
