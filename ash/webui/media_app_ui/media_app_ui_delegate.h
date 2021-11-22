// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_
#define ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// A delegate which exposes browser functionality from //chrome to the media app
// ui page handler.
class MediaAppUIDelegate {
 public:
  virtual ~MediaAppUIDelegate() = default;

  // Opens the native chrome feedback dialog scoped to chrome://media-app.
  // Returns an optional error message if unable to open the dialog or nothing
  // if the dialog was determined to have opened successfully.
  virtual absl::optional<std::string> OpenFeedbackDialog() = 0;

  // Toggles fullscreen mode on the Browser* hosting this MediaApp instance.
  virtual void ToggleBrowserFullscreenMode() = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_
