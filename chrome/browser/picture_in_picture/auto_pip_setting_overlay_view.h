// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_OVERLAY_VIEW_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_OVERLAY_VIEW_H_

#include "base/functional/callback.h"
#include "ui/views/view.h"

// Creates and manages the content setting overlay for autopip.  This is used
// both for video-only and document pip on desktop.  It is not used on Android.
class AutoPipSettingOverlayView : public views::View {
 public:
  enum class UiResult {
    // User selected 'Allow'.
    kAllow,

    // User selected 'Block'.
    kBlock,

    // UI was dismissed without the user selecting anything.
    // TODO(crbug.com/1465527): Call back with `kDismissed` sometimes.
    kDismissed,
  };
  using ResultCb = base::OnceCallback<void(UiResult result)>;

  explicit AutoPipSettingOverlayView(ResultCb result_cb);
  ~AutoPipSettingOverlayView() override;

  AutoPipSettingOverlayView(const AutoPipSettingOverlayView&) = delete;
  AutoPipSettingOverlayView(AutoPipSettingOverlayView&&) = delete;

  const views::View* get_block_button_for_testing() const {
    return block_button_;
  }
  const views::View* get_allow_button_for_testing() const {
    return allow_button_;
  }

 private:
  void OnButtonPressed(UiResult result);

  ResultCb result_cb_;
  raw_ptr<views::View> block_button_ = nullptr;
  raw_ptr<views::View> allow_button_ = nullptr;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_OVERLAY_VIEW_H_
