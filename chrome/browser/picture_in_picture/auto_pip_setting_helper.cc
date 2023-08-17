// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"

#include "components/content_settings/core/common/content_settings.h"

AutoPipSettingHelper::AutoPipSettingHelper(GURL origin,
                                           base::OnceClosure close_pip_cb)
    : origin_(origin), close_pip_cb_(std::move(close_pip_cb)) {}

AutoPipSettingHelper::~AutoPipSettingHelper() = default;

ContentSetting AutoPipSettingHelper::GetEffectiveContentSetting() {
  // TODO(crbug.com/1464065): Fetch the content setting.
  return content_setting_override_.value_or(CONTENT_SETTING_ASK);
}

std::unique_ptr<views::View> AutoPipSettingHelper::CreateOverlayViewIfNeeded() {
  switch (GetEffectiveContentSetting()) {
    case CONTENT_SETTING_ASK:
      // Create and return the UI to ask the user.
      return std::make_unique<AutoPipSettingOverlayView>(base::BindOnce(
          &AutoPipSettingHelper::OnUiResult, weak_factory_.GetWeakPtr()));
    case CONTENT_SETTING_ALLOW:
      // Nothing to do -- allow the auto pip to proceed.
      return nullptr;
    case CONTENT_SETTING_BLOCK:
      // Auto-pip is not allowed.  Close the window.
      std::move(close_pip_cb_).Run();
      return nullptr;
    default:
      NOTREACHED() << " AutoPiP unknown effective content setting";
      std::move(close_pip_cb_).Run();
      return nullptr;
  }
}

void AutoPipSettingHelper::OnUiResult(
    AutoPipSettingOverlayView::UiResult result) {
  // TODO(crbug.com/1464065): Update the content setting.

  // If the user selected 'Block', then also close the pip window.
  if (result == AutoPipSettingOverlayView::UiResult::kBlock) {
    std::move(close_pip_cb_).Run();
  }
}
