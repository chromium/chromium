// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_VIEW_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_VIEW_H_

#include "base/functional/callback.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// Creates and manages the content setting |BubbleDialogDelegateView| for
// autopip.  This view contains the setting optioms and text displayed to the
// user.
class AutoPipSettingView : public views::BubbleDialogDelegateView {
 public:
  enum class UiResult {
    // User selected 'Allow this time'.
    kAllowOnce,

    // User selected 'Allow on every visit'.
    kAllowOnEveryVisit,

    // User selected 'Don't allow'.
    kBlock,

    // UI was dismissed without the user selecting anything.
    // TODO(crbug.com/1465527): Call back with `kDismissed` sometimes.
    kDismissed,
  };
  using ResultCb = base::OnceCallback<void(UiResult result)>;
  // This callback is responsible for hiding the AutoPiP overlay view, after
  // the AutoPiP setting view is closed.
  using HideViewCb = base::OnceCallback<void()>;

  // Constructs an |AutoPipSettingView|. The constructor parameters are
  // explained below:
  //   * result_cb: Callback responsible for updating the content setting,
  //   according to the button pressed.
  //   * hide_view_cb: Callback responsible for hiding the AutoPiP overlay view.
  //   Callback is executed after the |AutoPipSettingView| is closed.
  //   * browser_view_overridden_bounds: These bounds represent the
  //   Picture-in-Picture window bounds. Used to adjust the PiP window size to
  //   accommodate the |AutoPipSettingView|.
  //   * anchor_view: Anchor view for the bubble.
  //   * arrow: The arrow position for the bubble.
  //   * parent: The bubble's parent window.
  explicit AutoPipSettingView(ResultCb result_cb,
                              HideViewCb hide_view_cb,
                              const gfx::Rect& browser_view_overridden_bounds,
                              views::View* anchor_view,
                              views::BubbleBorder::Arrow arrow,
                              gfx::NativeView parent);
  AutoPipSettingView(const AutoPipSettingView&) = delete;
  AutoPipSettingView(AutoPipSettingView&&) = delete;
  AutoPipSettingView& operator=(const AutoPipSettingView&) = delete;
  ~AutoPipSettingView() override;

  // Create the bubble and show the widget.
  void Show();

  // Set the bubble dialog title. Needed to propagate the origin, which is
  // included in the title, from the Picture-in-Picture frame view.
  void SetDialogTitle(const std::u16string& text);

  const views::Label* get_autopip_description_for_testing() const {
    return autopip_description_;
  }
  const views::MdTextButton* get_allow_once_button_for_testing() const {
    return allow_once_button_;
  }
  const views::MdTextButton*
  get_allow_on_every_visit_button_button_for_testing() const {
    return allow_on_every_visit_button_;
  }
  const views::MdTextButton* get_block_button_for_testing() const {
    return block_button_;
  }

  // Override |GetAnchorRect| to allow overlapping the bubble with the window
  // title bar.
  gfx::Rect GetAnchorRect() const override;

 private:
  ResultCb result_cb_;
  raw_ptr<views::Label> autopip_description_ = nullptr;
  raw_ptr<views::MdTextButton> allow_once_button_ = nullptr;
  raw_ptr<views::MdTextButton> allow_on_every_visit_button_ = nullptr;
  raw_ptr<views::MdTextButton> block_button_ = nullptr;

  // Initialize bubble with all it's child views. Called at construction time.
  void InitBubble();

  // Helper method to initialize the bubble views.
  raw_ptr<views::MdTextButton> InitControlViewButton(
      views::BoxLayoutView* controls_view,
      UiResult ui_result,
      const std::u16string& label_text);

  void OnButtonPressed(UiResult result);

  // TODO(crbug.com/1472386): Test through the class public interface, once
  // buttons are added to the view.
  FRIEND_TEST_ALL_PREFIXES(AutoPipSettingViewTest, TestInitControlViewButton);
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_VIEW_H_
