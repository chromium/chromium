// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_HELPER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_HELPER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"
#include "components/content_settings/core/common/content_settings.h"
#include "url/gurl.h"

namespace views {
class View;
}  // namespace views

// Helper class to manage the content setting for AutoPiP, including the
// permissions embargo.
class AutoPipSettingHelper {
 public:
  // We'll use `close_pip_cb` to close the pip window as needed.  It should be
  // safe to call at any time.
  AutoPipSettingHelper(GURL origin, base::OnceClosure close_pip_cb);
  ~AutoPipSettingHelper();

  AutoPipSettingHelper(const AutoPipSettingHelper&) = delete;
  AutoPipSettingHelper(AutoPipSettingHelper&&) = delete;

  // Create a views::View that should be used as the overlay view when the
  // content setting is ASK.  This view will call back to us, so we should
  // outlive it.  Will return nullptr if no UI is needed, and will optionally
  // call `close_pip_cb_` if AutoPiP is blocked.
  std::unique_ptr<views::View> CreateOverlayViewIfNeeded();

  // If called, pretend that the content setting is `setting`.  This is
  // temporary until we actually check content settings.
  void override_content_setting_for_testing(ContentSetting setting) {
    content_setting_override_ = setting;
  }

 private:
  // Returns the content setting, modified as needed by any embargo.
  ContentSetting GetEffectiveContentSetting();

  // Notify us that the user has interacted with the content settings UI that's
  // displayed in the pip window.
  void OnUiResult(AutoPipSettingOverlayView::UiResult result);

  GURL origin_;
  base::OnceClosure close_pip_cb_;

  // Set for testing.
  absl::optional<ContentSetting> content_setting_override_;

  base::WeakPtrFactory<AutoPipSettingHelper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_HELPER_H_
