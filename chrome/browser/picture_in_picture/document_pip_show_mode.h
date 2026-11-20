// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PIP_SHOW_MODE_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PIP_SHOW_MODE_H_

// How a standalone Document Picture-in-Picture window is shown. Mirrors what
// the Browser-backed path derives from `NavigateParams::window_action` and
// `user_gesture`. A separate header so callers that only pick a mode do not
// need to include DocumentPipHost.
enum class DocumentPipShowMode {
  // Shows the window without activating it or focusing its contents, e.g.
  // when opened by the AutoPictureInPictureTabHelper. The contents are focused
  // the first time the window is activated.
  kInactive,
  // Shows and activates the window.
  kActivate,
  // Shows and activates the window, then explicitly focuses its contents. Used
  // for user-gesture opens.
  kActivateAndFocus,
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PIP_SHOW_MODE_H_
