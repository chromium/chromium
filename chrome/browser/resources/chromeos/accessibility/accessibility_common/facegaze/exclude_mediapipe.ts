// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Informs FaceGaze whether mediapipe is available. This file will
 * be included in the accessibility_common extension when the gn argument
 * include_mediapipe_task_vision_files_for_facegaze is set to false.
 */

export const MediapipeAvailability = {
  isAvailable() {
    return false;
  },
};
