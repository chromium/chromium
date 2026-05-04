// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_window_occlusion_helper_base.h"

// static
std::unique_ptr<AutoPictureInPictureWindowOcclusionHelperBase>
AutoPictureInPictureWindowOcclusionHelperBase::CreatePlatformHelper(
    content::WebContents* web_contents,
    OcclusionStateChangedCallback callback) {
  return nullptr;
}
