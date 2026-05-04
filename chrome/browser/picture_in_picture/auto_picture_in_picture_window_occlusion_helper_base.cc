// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_window_occlusion_helper_base.h"

#include "base/no_destructor.h"

namespace {

AutoPictureInPictureWindowOcclusionHelperBase::FactoryCallback&
GetFactoryCallback() {
  static base::NoDestructor<
      AutoPictureInPictureWindowOcclusionHelperBase::FactoryCallback>
      factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<AutoPictureInPictureWindowOcclusionHelperBase>
AutoPictureInPictureWindowOcclusionHelperBase::Create(
    content::WebContents* web_contents,
    OcclusionStateChangedCallback callback) {
  if (GetFactoryCallback()) {
    return GetFactoryCallback().Run(web_contents, std::move(callback));
  }

  // This will be defined in the platform-specific implementation file.
  return CreatePlatformHelper(web_contents, std::move(callback));
}

// static
void AutoPictureInPictureWindowOcclusionHelperBase::
    SetFactoryForTesting(  // IN-TEST
        FactoryCallback factory) {
  GetFactoryCallback() = std::move(factory);
}

AutoPictureInPictureWindowOcclusionHelperBase::
    AutoPictureInPictureWindowOcclusionHelperBase(
        content::WebContents* web_contents,
        OcclusionStateChangedCallback callback)
    : web_contents_(web_contents), callback_(std::move(callback)) {
  CHECK(web_contents);
}

AutoPictureInPictureWindowOcclusionHelperBase::
    ~AutoPictureInPictureWindowOcclusionHelperBase() = default;

void AutoPictureInPictureWindowOcclusionHelperBase::RunCallback(
    OcclusionState occlusion_state) {
  if (callback_) {
    callback_.Run(occlusion_state);
  }
}
