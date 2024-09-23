// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_PANEL_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_PANEL_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/system/video_conference/bubble/return_to_app_button_base.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace gfx {
class Animation;
class LinearAnimation;
}  // namespace gfx

namespace views {
class FlexLayout;
class ImageView;
class View;
}  // namespace views

namespace ash::video_conference {

class ReturnToAppExpandButton;
class ReturnToAppPanel;

using MediaApps = std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>;

// The "return to app" button that resides within the "return to app" panel.
class ASH_EXPORT ReturnToAppButton : public ReturnToAppButtonBase {
  METADATA_HEADER(ReturnToAppButton, ReturnToAppButtonBase)

 public:
  // `is_top_row` specifies if the button is in the top row of `panel`. If the
  // button is in the top row, it might represent the only media app running or
  // the summary row if there are multiple media apps.
  ReturnToAppButton(ReturnToAppPanel* panel,
                    bool is_top_row,
                    const base::UnguessableToken& id,
                    bool is_capturing_camera,
                    bool is_capturing_microphone,
                    bool is_capturing_screen,
                    const std::u16string& display_text,
                    crosapi::mojom::VideoConferenceAppType app_type);

  ReturnToAppButton(const ReturnToAppButton&) = delete;
  ReturnToAppButton& operator=(const ReturnToAppButton&) = delete;

  ~ReturnToAppButton() override;

  // ReturnToAppButtonBase:
  void OnButtonClicked(
      const base::UnguessableToken& id,
      crosapi::mojom::VideoConferenceAppType app_type) override;

  bool expanded() const { return expanded_; }
  void HideExpandIndicator();
  const views::ImageView* expand_indicator_for_testing() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ReturnToAppPanelTest, ExpandCollapse);

  // Creates an expand button for the top row and adds it as a child view.
  ReturnToAppExpandButton* CreateExpandIndicator();

  void UpdateAccessibleName();

  bool is_top_row() const { return expand_indicator_ != nullptr; }

  // Indicates if this button (and also the parent panel) is in the expanded
  // state. Note that `expanded_` is only meaningful in the case that the button
  // is in the top row.
  bool expanded_ = false;

  // The pointers below are owned by the views hierarchy.

  // This panel is the parent view of this button.
  const raw_ptr<ReturnToAppPanel> panel_;

  // The indicator showing if the panel is in expanded or collapsed state. Only
  // available if there are multiple rows and `this` is the top row.
  const raw_ptr<ReturnToAppExpandButton> expand_indicator_;
};

// The "return to app" panel that resides in the video conference bubble. The
// user selects from a list of apps that are actively capturing audio/video
// and/or sharing the screen, and the selected app is brought to the top and
// focused.
class ASH_EXPORT ReturnToAppPanel : public views::FlexLayoutView {
  METADATA_HEADER(ReturnToAppPanel, views::FlexLayoutView)

 public:
  explicit ReturnToAppPanel(const MediaApps& apps);
  ReturnToAppPanel(const ReturnToAppPanel&) = delete;
  ReturnToAppPanel& operator=(const ReturnToAppPanel&) = delete;
  ~ReturnToAppPanel() override;

  // True if the container is running its expand/collapse animation.
  bool IsExpandCollapseAnimationRunning();

  int max_capturing_count() const { return max_capturing_count_; }

  // Updates the expanded state and repaint the view if needed.
  void OnExpandedStateChanged(bool expanded);

 private:
  friend class ReturnToAppPanelTest;
  friend class VideoConferenceIntegrationTest;
  friend class BubbleViewPixelTest;

  // The container that is the parent of all the buttons inside this panel.
  // Mainly used to handle expand/collapse animation.
  class ReturnToAppContainer : public views::View,
                               public views::AnimationDelegateViews {
    METADATA_HEADER(ReturnToAppContainer, views::View)

   public:
    ReturnToAppContainer();
    ReturnToAppContainer(const ReturnToAppContainer&) = delete;
    ReturnToAppContainer& operator=(const ReturnToAppContainer&) = delete;
    ~ReturnToAppContainer() override;

    // Starts the expand/collapse animation.
    void StartExpandCollapseAnimation();

    // We use different layout padding for expand and collapse state.
    void AdjustLayoutForExpandCollapseState(bool expanded);

    gfx::LinearAnimation* animation() { return animation_.get(); }

    void set_height_before_animation(int height_before_animation) {
      height_before_animation_ = height_before_animation;
    }

    void set_expanded_target(bool expanded_target) {
      expanded_target_ = expanded_target;
    }

   private:
    friend class ReturnToAppPanelTest;

    // views::AnimationDelegateViews:
    void AnimationProgressed(const gfx::Animation* animation) override;
    void AnimationEnded(const gfx::Animation* animation) override;
    void AnimationCanceled(const gfx::Animation* animation) override;

    // views::View:
    gfx::Size CalculatePreferredSize(
        const views::SizeBounds& available_size) const override;

    // Layout manager of this view. Owned by the views hierarchy.
    raw_ptr<views::FlexLayout> layout_manager_ = nullptr;

    // Animation used for the expand/collapse animation.
    std::unique_ptr<gfx::LinearAnimation> animation_;

    // Keeps track of the height of the panel before animation starts. This is
    // used for the expand/collapse animation.
    int height_before_animation_ = 0;

    // Target expand state of the panel after the animation is completed.
    bool expanded_target_ = false;

    // Measure animation smoothness metrics for all the animations.
    std::optional<ui::ThroughputTracker> throughput_tracker_;
  };

  // views::View:
  void ChildPreferredSizeChanged(View* child) override;

  // The container of the panel, which contains all the views and is used for
  // setting padding and background painting. Owned by the views hierarchy.
  raw_ptr<ReturnToAppContainer> container_view_ = nullptr;

  // The view at the top of the panel, summarizing the information of all media
  // apps. This pointer will be null when there's one or fewer media apps. Owned
  // by the views hierarchy.
  raw_ptr<ReturnToAppButton> summary_row_view_ = nullptr;

  // Keep track the maximum number of capturing that an individual media app
  // has. This number is used to make sure the icons in `ReturnToAppButton` are
  // right aligned with each other.
  int max_capturing_count_ = 0;

  base::WeakPtrFactory<ReturnToAppPanel> weak_ptr_factory_{this};
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_RETURN_TO_APP_PANEL_H_
