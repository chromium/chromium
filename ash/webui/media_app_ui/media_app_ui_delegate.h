// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_
#define ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"

class GURL;

namespace ash {

// A delegate which exposes browser functionality from //chrome to the media app
// ui page handler.
class MediaAppUIDelegate {
 public:
  virtual ~MediaAppUIDelegate() = default;

  // Opens the native chrome feedback dialog scoped to chrome://media-app.
  // Returns an optional error message if unable to open the dialog or nothing
  // if the dialog was determined to have opened successfully.
  virtual std::optional<std::string> OpenFeedbackDialog() = 0;

  // Toggles fullscreen mode on the Browser* hosting this MediaApp instance.
  virtual void ToggleBrowserFullscreenMode() = 0;

  // Indicate that a trigger for displaying the PDF HaTS survey has occurred.
  virtual void MaybeTriggerPdfHats() = 0;

  // Checks whether file represented by the provided transfer token is within a
  // filesystem that ARC is able to write to.
  virtual void IsFileArcWritable(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      base::OnceCallback<void(bool)> is_file_arc_writable_callback) = 0;

  // Launches the file represented by the provided transfer token in the Photos
  // Android app with an intent to edit.
  virtual void EditInPhotos(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      const std::string& mime_type,
      base::OnceCallback<void()> edit_in_photos_callback) = 0;

  // Launches the designated URL with corresponding payload via HTTP post.
  virtual void SubmitForm(const GURL& url,
                          const std::vector<int8_t>& payload,
                          const std::string& header) = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_DELEGATE_H_
