// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_VIEW_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_VIEW_H_

#include "base/functional/callback.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// Creates and manages the content setting |BubbleDialogDelegate| for
// autopip.  This view contains the setting optioms and text displayed to the
// user.
class AutoPipSettingView : public views::BubbleDialogDelegate {
 public:
  enum class UiResult {
    // User selected 'Allow this time'.
    // These values are also used as the AutoPiP setting view button ID's,
    // starting from 1 because 0 is the default view ID.
    kAllowOnce = 1,

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
  //   * origin: GURL from which the origin will be extracted and added to the
  //   bubble title.
  //   * browser_view_overridden_bounds: These bounds represent the
  //   Picture-in-Picture window bounds. Used to adjust the PiP window size to
  //   accommodate the |AutoPipSettingView|.
  //   * anchor_view: Anchor view for the bubble.
  //   * arrow: The arrow position for the bubble.
  explicit AutoPipSettingView(ResultCb result_cb,
                              HideViewCb hide_view_cb,
                              const GURL& origin,
                              const gfx::Rect& browser_view_overridden_bounds,
                              views::View* anchor_view,
                              views::BubbleBorder::Arrow arrow);
  AutoPipSettingView(const AutoPipSettingView&) = delete;
  AutoPipSettingView(AutoPipSettingView&&) = delete;
  AutoPipSettingView& operator=(const AutoPipSettingView&) = delete;
  ~AutoPipSettingView() override;

  // Override |GetAnchorRect| to allow overlapping the bubble with the window
  // title bar.
  gfx::Rect GetAnchorRect() const override;

  // views::WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  void OnWidgetInitialized() override;

  const std::u16string& get_origin_text_for_testing() const {
    return origin_text_;
  }

 private:
  // AnchorViewObserver observes the anchor widget view. When the anchor view is
  // removed from from the widget or the view is being deleted, the observer
  // will close the bubble widget (if the widget exists and is visible). This
  // guarantees that a single bubble widget exists for the cases mentioned
  // above.
  //
  // As a side effect, this also guarantees that a single bubble widgte exists
  // for the cases where the PiP browser frame view is re-created.
  class AnchorViewObserver : public views::ViewObserver {
   public:
    explicit AnchorViewObserver(views::View* anchor_view,
                                AutoPipSettingView* bubble);
    AnchorViewObserver(const AnchorViewObserver&) = delete;
    AnchorViewObserver& operator=(const AnchorViewObserver&) = delete;
    ~AnchorViewObserver() override;

   private:
    // views::ViewObserver:
    void OnViewRemovedFromWidget(views::View*) override;
    void OnViewIsDeleting(views::View*) override;

    void CloseWidget();

    const raw_ptr<AutoPipSettingView> bubble_;
    base::ScopedObservation<views::View, views::ViewObserver> observation_{
        this};
  };

  ResultCb result_cb_;
  raw_ptr<views::Label> autopip_description_ = nullptr;
  raw_ptr<views::MdTextButton> allow_once_button_ = nullptr;
  raw_ptr<views::MdTextButton> allow_on_every_visit_button_ = nullptr;
  raw_ptr<views::MdTextButton> block_button_ = nullptr;
  std::unique_ptr<views::View> dialog_title_view_;
  std::unique_ptr<AutoPipSettingView::AnchorViewObserver> anchor_view_observer_;
  std::u16string origin_text_;

  // Initialize bubble with all it's child views. Called at construction time.
  void InitBubble();

  // Helper method to initialize the bubble views.
  raw_ptr<views::MdTextButton> InitControlViewButton(
      views::BoxLayoutView* controls_view,
      UiResult ui_result,
      const std::u16string& label_text);

  void OnButtonPressed(UiResult result);

  // Initializes the bubble dialog title view.
  void InitBubbleTitleView(const GURL& origin);
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_VIEW_H_
