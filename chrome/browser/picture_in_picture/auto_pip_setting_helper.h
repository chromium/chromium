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

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

class HostContentSettingsMap;

// Helper class to manage the content setting for AutoPiP, including the
// permissions embargo.
class AutoPipSettingHelper {
 public:
  // Convenience function.
  static std::unique_ptr<AutoPipSettingHelper> CreateForWebContents(
      content::WebContents* web_contents,
      base::OnceClosure close_pip_cb);

  // We'll use `close_pip_cb` to close the pip window as needed.  It should be
  // safe to call at any time.  It is up to our caller to make sure that we are
  // destroyed if `settings_map` is.
  AutoPipSettingHelper(const GURL& origin,
                       HostContentSettingsMap* settings_map,
                       base::OnceClosure close_pip_cb);
  ~AutoPipSettingHelper();

  AutoPipSettingHelper(const AutoPipSettingHelper&) = delete;
  AutoPipSettingHelper(AutoPipSettingHelper&&) = delete;

  // Create a views::View that should be used as the overlay view when the
  // content setting is ASK.  This view will call back to us, so we should
  // outlive it.  Will return nullptr if no UI is needed, and will optionally
  // call `close_pip_cb_` if AutoPiP is blocked.
  std::unique_ptr<views::View> CreateOverlayViewIfNeeded();

 private:
  // Returns the content setting, modified as needed by any embargo.
  ContentSetting GetEffectiveContentSetting();

  // Update the content setting to `new_setting`, and clear any embargo.
  void UpdateContentSetting(ContentSetting new_setting);

  // Notify us that the user has interacted with the content settings UI that's
  // displayed in the pip window.
  void OnUiResult(AutoPipSettingOverlayView::UiResult result);

  GURL origin_;
  const raw_ptr<HostContentSettingsMap> settings_map_ = nullptr;
  base::OnceClosure close_pip_cb_;

  base::WeakPtrFactory<AutoPipSettingHelper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_HELPER_H_
