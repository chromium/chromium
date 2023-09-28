// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_OVERLAY_VIEW_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_OVERLAY_VIEW_H_

#include "base/check_is_test.h"
#include "base/functional/callback.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"

// Creates and manages the content setting overlay for autopip.  This is used
// both for video-only and document pip on desktop.  It is not used on Android.
class AutoPipSettingOverlayView : public views::View,
                                  public views::ViewTargeterDelegate {
 public:
  METADATA_HEADER(AutoPipSettingOverlayView);
  using ResultCb =
      base::OnceCallback<void(AutoPipSettingView::UiResult result)>;

  explicit AutoPipSettingOverlayView(
      ResultCb result_cb,
      const GURL& origin,
      const gfx::Rect& browser_view_overridden_bounds,
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

 private:
  std::unique_ptr<AutoPipSettingView> auto_pip_setting_view_;
  raw_ptr<views::View> background_ = nullptr;
  base::WeakPtrFactory<AutoPipSettingOverlayView> weak_factory_{this};

  // Callback used to hide the semi-opaque background layer.
  void OnHideView();

  // Perform a linear fade in of |layer|.
  void FadeInLayer(ui::Layer* layer);
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_OVERLAY_VIEW_H_
