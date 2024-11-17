// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_OVERLAY_VIEW_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_OVERLAY_VIEW_H_

#include "base/check_is_test.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget_observer.h"

// Creates and manages the content setting overlay for autopip.  This is used
// both for video-only and document pip on desktop.  It is not used on Android.
class AutoPipSettingOverlayView : public views::View,
                                  public views::ViewTargeterDelegate,
                                  public views::WidgetObserver {
  METADATA_HEADER(AutoPipSettingOverlayView, views::View)

 public:
  // Represents the Picture-in-Picture window type. Used by the |ShowBubble|
  // method to properly display the bubble according to the PipWindowType.
  enum class PipWindowType {
    kVideoPip,
    kDocumentPip,
  };

  using ResultCb =
      base::OnceCallback<void(AutoPipSettingView::UiResult result)>;

  explicit AutoPipSettingOverlayView(
      ResultCb result_cb,
      const GURL& origin,
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow);
  ~AutoPipSettingOverlayView() override;

  AutoPipSettingOverlayView(const AutoPipSettingOverlayView&) = delete;
  AutoPipSettingOverlayView(AutoPipSettingOverlayView&&) = delete;

  // Create and show the AutoPipSettingView bubble. The parent parameter will be
  // set as the bubble's parent window.
  virtual void ShowBubble(gfx::NativeView parent);

  views::View* get_background_for_testing() const {
    CHECK_IS_TEST();
    return background_;
  }

  views::View* get_blur_view_for_testing() const {
    CHECK_IS_TEST();
    return blur_view_;
  }

  // Returns true if the bubble wants events at this point.  In practice, this
  // means "is over a button".
  virtual bool WantsEvent(const gfx::Point& point_in_screen);

  // Return the size of the bubble.  This does not include the scrim.
  virtual gfx::Size GetBubbleSize() const;

  // views::WidgetObserver
  void OnWidgetDestroying(views::Widget*) override;

  AutoPipSettingView* get_view_for_testing() { return auto_pip_setting_view_; }

  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnAutoPipSettingOverlayViewHidden() = 0;
  };

  void set_delegate(Delegate* delegate) {
    // This should only ever be called to set the initial delegate (i.e.
    // `delegate_` is null and `delegate` is not null) or to clear the delegate
    // (i.e. `delegate_` is not null and `delegate` is null).
    CHECK(!!delegate != !!delegate_);
    delegate_ = delegate;
  }

  // Ignore events on `web_contents` until the user takes an action that hides
  // the UI.  `web_contents` is presumably for the pip window.  Optional; this
  // is intended for document pip, since video pip doesn't have a WebContents.
  void IgnoreInputEvents(content::WebContents* web_contents);

 private:
  // Callback used to hide the semi-opaque background layer.
  void OnHideView();

  // Perform a linear fade in of |layer|.
  void FadeInLayer(ui::Layer* layer);

  // Returns true if |show_timer_| is currently running.
  bool IsShowTimerRunning();

  // We temporarily own the setting view during init, but usually this is null.
  // Keep it separate so one doesn't accidentally use it.  It's likely that you
  // really want `auto_pip_setting_view_`, outside this struct.
  struct {
    std::unique_ptr<AutoPipSettingView> auto_pip_setting_view_;
  } init_;

  raw_ptr<views::View> background_ = nullptr;
  raw_ptr<views::View> blur_view_ = nullptr;
  raw_ptr<AutoPipSettingView> auto_pip_setting_view_ = nullptr;
  raw_ptr<views::Widget> widget_ = nullptr;
  gfx::Size bubble_size_;
  std::unique_ptr<base::OneShotTimer> show_timer_;
  raw_ptr<Delegate> delegate_ = nullptr;

  // Optional closure to re-enable input events, to be run when the user
  // dismisses the UI via any button.  Only used for document pip.
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;

  base::WeakPtrFactory<AutoPipSettingOverlayView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_OVERLAY_VIEW_H_
