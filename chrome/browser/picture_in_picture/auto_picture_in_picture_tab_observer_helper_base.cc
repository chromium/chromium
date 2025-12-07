// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_observer_helper_base.h"

AutoPictureInPictureTabObserverHelperBase::
    AutoPictureInPictureTabObserverHelperBase(
        content::WebContents* web_contents,
        ActivatedChangedCallback callback)
    : web_contents_(web_contents), callback_(std::move(callback)) {}

AutoPictureInPictureTabObserverHelperBase::
    ~AutoPictureInPictureTabObserverHelperBase() = default;

// Runs the callback_ if it's set.
void AutoPictureInPictureTabObserverHelperBase::RunCallback(
    bool is_tab_activated) {
  if (callback_) {
    callback_.Run(is_tab_activated);
  }
}
