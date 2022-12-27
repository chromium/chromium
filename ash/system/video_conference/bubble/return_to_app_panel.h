// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_PANEL_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_PANEL_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash::video_conference {

using MediaApps = std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>;

// The "return to app" button that resides within the "return to app" panel,
// showing information of a particular running media app. Clicking on this
// button will take users to the app.
class ASH_EXPORT ReturnToAppButton : public views::View {
 public:
  ReturnToAppButton(bool is_capturing_camera,
                    bool is_capturing_microphone,
                    bool is_capturing_screen,
                    const std::u16string& display_text);

  ReturnToAppButton(const ReturnToAppButton&) = delete;
  ReturnToAppButton& operator=(const ReturnToAppButton&) = delete;

  ~ReturnToAppButton() override = default;

  bool is_capturing_camera() const { return is_capturing_camera_; }
  bool is_capturing_microphone() const { return is_capturing_microphone_; }
  bool is_capturing_screen() const { return is_capturing_screen_; }

  views::Label* label() { return label_; }

 private:
  // Indicates if the running app is using camera, microphone, or screen
  // sharing.
  const bool is_capturing_camera_;
  const bool is_capturing_microphone_;
  const bool is_capturing_screen_;

  // Label showing the url or name of the running app. Owned by the views
  // hierarchy.
  views::Label* label_ = nullptr;
};

// The "return to app" panel that resides in the video conference bubble. The
// user selects from a list of apps that are actively capturing audio/video
// and/or sharing the screen, and the selected app is brought to the top and
// focused.
class ASH_EXPORT ReturnToAppPanel : public views::View {
 public:
  ReturnToAppPanel();
  ReturnToAppPanel(const ReturnToAppPanel&) = delete;
  ReturnToAppPanel& operator=(const ReturnToAppPanel&) = delete;
  ~ReturnToAppPanel() override;

 private:
  // Used by the ctor to add `ReturnToAppButton`(s) to the panel.
  void AddButtonsToPanel(MediaApps apps);

  base::WeakPtrFactory<ReturnToAppPanel> weak_ptr_factory_{this};
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_PANEL_H_
