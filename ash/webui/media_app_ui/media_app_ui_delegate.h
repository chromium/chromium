// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_
#define ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// A delegate which exposes browser functionality from //chrome to the media app
// ui page handler.
class MediaAppUIDelegate {
 public:
  virtual ~MediaAppUIDelegate() = default;

  virtual base::WeakPtr<MediaAppUIDelegate> GetWeakPtr() = 0;

  // Opens the native chrome feedback dialog scoped to chrome://media-app.
  // Returns an optional error message if unable to open the dialog or nothing
  // if the dialog was determined to have opened successfully.
  virtual absl::optional<std::string> OpenFeedbackDialog() = 0;

  // Toggles fullscreen mode on the Browser* hosting this MediaApp instance.
  virtual void ToggleBrowserFullscreenMode() = 0;

  // Launches the file at |url| in the Photos Android app with an intent to
  // edit.
  virtual void EditFileInPhotos(
      absl::optional<storage::FileSystemURL> url,
      const std::string& mime_type,
      base::OnceCallback<void()> edit_in_photos_callback) = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_
