// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_BUTTON_BASE_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_BUTTON_BASE_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace views {
class Label;
class View;
}  // namespace views

namespace ash::video_conference {

using MediaApps = std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>;

// The base class for the "return to app" button, showing information of a
// particular running media app. Clicking on this button will take users to the
// app. This class adds the necessary views into the hierarchy without defining
// the layout or padding/spacing (the overriding class needs to take care of
// that).
class ASH_EXPORT ReturnToAppButtonBase : public views::Button {
  METADATA_HEADER(ReturnToAppButtonBase, views::Button)

 public:
  ReturnToAppButtonBase(const ReturnToAppButtonBase&) = delete;
  ReturnToAppButtonBase& operator=(const ReturnToAppButtonBase&) = delete;

  ~ReturnToAppButtonBase() override;

  bool is_capturing_camera() const { return is_capturing_camera_; }
  bool is_capturing_microphone() const { return is_capturing_microphone_; }
  bool is_capturing_screen() const { return is_capturing_screen_; }

  views::Label* label() { return label_; }
  views::View* icons_container() { return icons_container_; }

 protected:
  ReturnToAppButtonBase(const base::UnguessableToken& id,
                        bool is_capturing_camera,
                        bool is_capturing_microphone,
                        bool is_capturing_screen,
                        const std::u16string& display_text,
                        crosapi::mojom::VideoConferenceAppType app_type);

  // Callback for the button.
  virtual void OnButtonClicked(const base::UnguessableToken& id,
                               crosapi::mojom::VideoConferenceAppType app_type);

  // Get the text regarding the peripherals part of the return to app button
  // accessible name.
  std::u16string GetPeripheralsAccessibleName() const;

  // Get the text displayed in `label_`.
  std::u16string GetLabelText() const;

 private:
  // Indicates if the running app is using camera, microphone, or screen
  // sharing.
  const bool is_capturing_camera_;
  const bool is_capturing_microphone_;
  const bool is_capturing_screen_;

  // The pointers below are owned by the views hierarchy.

  // Label showing the url or name of the running app.
  raw_ptr<views::Label> label_ = nullptr;

  // The container of icons showing the state of camera/microphone/screen
  // capturing of the media app.
  raw_ptr<views::View> icons_container_ = nullptr;

  base::WeakPtrFactory<ReturnToAppButtonBase> weak_ptr_factory_{this};
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_BUTTON_BASE_H_
