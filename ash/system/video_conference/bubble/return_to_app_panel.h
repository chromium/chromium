// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_PANEL_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_PANEL_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace views {
class ImageView;
class Label;
class View;
}  // namespace views

namespace ash::video_conference {

class ReturnToAppPanel;

using MediaApps = std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>;

// The "return to app" button that resides within the "return to app" panel,
// showing information of a particular running media app. Clicking on this
// button will take users to the app.
class ASH_EXPORT ReturnToAppButton : public views::Button {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the expanded state is changed.
    virtual void OnExpandedStateChanged(bool expanded) = 0;
  };

  // `is_top_row` specifies if the button is in the top row of `panel`. If the
  // button is in the top row, it might represent the only media app running or
  // the summary row if there are multiple media apps.
  ReturnToAppButton(ReturnToAppPanel* panel,
                    bool is_top_row,
                    const base::UnguessableToken& id,
                    bool is_capturing_camera,
                    bool is_capturing_microphone,
                    bool is_capturing_screen,
                    const std::u16string& display_text);

  ReturnToAppButton(const ReturnToAppButton&) = delete;
  ReturnToAppButton& operator=(const ReturnToAppButton&) = delete;

  ~ReturnToAppButton() override;

  // Observer functions.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool is_capturing_camera() const { return is_capturing_camera_; }
  bool is_capturing_microphone() const { return is_capturing_microphone_; }
  bool is_capturing_screen() const { return is_capturing_screen_; }

  bool expanded() const { return expanded_; }

  views::Label* label() { return label_; }
  views::View* icons_container() { return icons_container_; }
  views::ImageView* expand_indicator() { return expand_indicator_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ReturnToAppPanelTest, ExpandCollapse);

  // Callback for the button.
  void OnButtonClicked(const base::UnguessableToken& id);

  // Indicates if the running app is using camera, microphone, or screen
  // sharing.
  const bool is_capturing_camera_;
  const bool is_capturing_microphone_;
  const bool is_capturing_screen_;

  // Registered observers.
  base::ObserverList<Observer> observer_list_;

  // Indicates if this button (and also the parent panel) is in the expanded
  // state. Note that `expanded_` is only meaningful in the case that the button
  // is in the top row.
  bool expanded_ = false;

  // The pointers below are owned by the views hierarchy.

  // This panel is the parent view of this button.
  ReturnToAppPanel* const panel_;

  // Label showing the url or name of the running app.
  views::Label* label_ = nullptr;

  // The container of icons showing the state of camera/microphone/screen
  // capturing of the media app.
  views::View* icons_container_ = nullptr;

  // The indicator showing if the panel is in expanded or collapsed state. Only
  // available if the button is in the top row.
  views::ImageView* expand_indicator_ = nullptr;

  base::WeakPtrFactory<ReturnToAppButton> weak_ptr_factory_{this};
};

// The "return to app" panel that resides in the video conference bubble. The
// user selects from a list of apps that are actively capturing audio/video
// and/or sharing the screen, and the selected app is brought to the top and
// focused.
class ASH_EXPORT ReturnToAppPanel : public views::View,
                                    ReturnToAppButton::Observer {
 public:
  ReturnToAppPanel();
  ReturnToAppPanel(const ReturnToAppPanel&) = delete;
  ReturnToAppPanel& operator=(const ReturnToAppPanel&) = delete;
  ~ReturnToAppPanel() override;

  int max_capturing_count() { return max_capturing_count_; }

 private:
  friend class ReturnToAppPanelTest;
  friend class VideoConferenceIntegrationTest;

  // ReturnToAppButton::Observer:
  void OnExpandedStateChanged(bool expanded) override;

  // Used by the ctor to add `ReturnToAppButton`(s) to the panel.
  void AddButtonsToPanel(MediaApps apps);

  // The container of the panel, which contains all the views and is used for
  // setting padding and background painting. Owned by the views hierarchy.
  views::View* container_view_ = nullptr;

  // The view at the top of the panel, summarizing the information of all media
  // apps. This pointer will be null when there's one or fewer media apps. Owned
  // by the views hierarchy.
  ReturnToAppButton* summary_row_view_ = nullptr;

  // Keep track the maximum number of capturing that an individual media app
  // has. This number is used to make sure the icons in `ReturnToAppButton` are
  // right aligned with each other.
  int max_capturing_count_ = 0;

  base::WeakPtrFactory<ReturnToAppPanel> weak_ptr_factory_{this};
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_PANEL_H_
