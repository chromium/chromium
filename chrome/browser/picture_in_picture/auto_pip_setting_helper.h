// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_HELPER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_HELPER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace permissions {
class PermissionDecisionAutoBlockerBase;
}  // namespace permissions

namespace views {
class View;
}  // namespace views

class HostContentSettingsMap;

// Helper class to manage the content setting for AutoPiP, including the
// permissions embargo.  It's intended to be kept around for the duration of the
// visit to the site, so that 'allow once' can be sticky until navigation.  It
// does not detect navigation; somebody else should get a new instance.
class AutoPipSettingHelper {
 public:
  using ResultCb =
      base::OnceCallback<void(AutoPipSettingView::UiResult result)>;
  // Convenience function.
  static std::unique_ptr<AutoPipSettingHelper> CreateForWebContents(
      content::WebContents* web_contents,
      HostContentSettingsMap* settings_map,
      permissions::PermissionDecisionAutoBlockerBase* auto_blocker);

  // We'll use `close_pip_cb` to close the pip window as needed.  It should be
  // safe to call at any time.  It is up to our caller to make sure that we are
  // destroyed if `settings_map` is.
  AutoPipSettingHelper(
      const GURL& origin,
      HostContentSettingsMap* settings_map,
      permissions::PermissionDecisionAutoBlockerBase* auto_blocker);
  ~AutoPipSettingHelper();

  AutoPipSettingHelper(const AutoPipSettingHelper&) = delete;
  AutoPipSettingHelper(AutoPipSettingHelper&&) = delete;

  // Notify us that the user has closed the window.  This will cause the embargo
  // to be updated if needed.
  void OnUserClosedWindow();

  // Create an AutoPipSettingOverlayView that should be used as the overlay view
  // when the content setting is ASK.  This view will call back to us, so we
  // should outlive it.  Will return nullptr if no UI is needed, and will
  // optionally call `close_pip_cb_` if AutoPiP is blocked.
  std::unique_ptr<AutoPipSettingOverlayView> CreateOverlayViewIfNeeded(
      base::OnceClosure close_pip_cb,
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow);

  // Called by the AutoPictureInPictureTabHelper when automatic
  // picture-in-picture has been preemptively blocked. Used to record associated
  // `Media.AutoPictureInPicture.PromptResultV2` metrics.
  void OnAutoPipBlockedByPermission();
  void OnAutoPipBlockedByIncognito();

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PromptResult {
    // The user closed the PiP window before selecting a choice. Note that this
    // will not be recorded when the PiP window is closed automatically by the
    // user focusing the original tab.
    kIgnored = 0,

    // The user chose to block automatic picture-in-picture.
    kBlock = 1,

    // The user chose to allow automatic picture-in-picture on every visit.
    kAllowOnEveryVisit = 2,

    // The user chose to allow automatic picture-in-picture this time.
    kAllowOnce = 3,

    // The prompt was not shown because the user has already allowed automatic
    // picture-in-picture for every visit.
    kNotShownAllowedOnEveryVisit = 4,

    // The prompt was not shown because the user has already allowed automatic
    // picture-in-picture for this visit.
    kNotShownAllowedOnce = 5,

    // The prompt was not shown because the user has already blocked automatic
    // picture-in-picture (and therefore no picture-in-picture was shown at
    // all).
    kNotShownBlocked = 6,

    // The prompt was not shown because automatic picture-in-picture was blocked
    // due to the user being in incognito mode.
    kNotShownIncognito = 7,

    kMaxValue = kNotShownIncognito,
  };

  // Returns the content setting, modified as needed by any embargo.
  ContentSetting GetEffectiveContentSetting();

  // Update the content setting to `new_setting`, and clear any embargo.
  void UpdateContentSetting(ContentSetting new_setting);

  // Notify us that the user has interacted with the content settings UI that's
  // displayed in the pip window.  `close_pip_cb` will be called if the result
  // is 'block'.
  void OnUiResult(base::OnceClosure close_pip_cb,
                  AutoPipSettingView::UiResult result);

  // Return a new ResultCb, and invalidate any previous ones.
  ResultCb CreateResultCb(base::OnceClosure close_pip_cb);

  // Record metrics for the result of the prompt.
  void RecordResult(PromptResult result);

  GURL origin_;
  const raw_ptr<HostContentSettingsMap> settings_map_ = nullptr;
  base::OnceClosure close_pip_cb_;
  const raw_ptr<permissions::PermissionDecisionAutoBlockerBase> auto_blocker_ =
      nullptr;

  // If true, then we've shown the UI but the user hasn't picked an option yet.
  bool ui_was_shown_but_not_acknowledged_ = false;

  // Has the user clicked 'allow once' on any permission UI we've created?
  bool already_selected_allow_once_ = false;

  base::WeakPtrFactory<AutoPipSettingHelper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PIP_SETTING_HELPER_H_
