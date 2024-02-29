// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_SUPPORT_VALIDITY_SERVICE_H_
#define CHROME_BROWSER_TPCD_SUPPORT_VALIDITY_SERVICE_H_

#include "base/functional/callback.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class HostContentSettingsMap;
class GURL;

namespace tpcd::trial {

// ValidityService is responsible for ensuring the Tpcd deprecation trials (for
// third-parties and for top-level sites) is still enabled for the appropriate
// origin of any third-party cookie access that is (or could have been) allowed
// as a result of a |TPCD_TRIAL| or |TOP_LEVEL_TPCD_TRIAL| content setting. This
// is necessary since |content::OriginTrialsControllerDelegate| doesn't notify
// its observers when a trial is disabled for a reason other than all tokens
// being cleared or an origin intentionally disable it (by not supplying the
// token when loaded in the associated context).
class ValidityService : public content::WebContentsObserver,
                        public content::WebContentsUserData<ValidityService> {
 public:
  using ContentSettingUpdateCallback = base::OnceCallback<
      void(const GURL& url, const GURL& first_party_url, bool enabled)>;

  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  ~ValidityService() override;
  ValidityService(const ValidityService&) = delete;
  ValidityService& operator=(const ValidityService&) = delete;

  // Stops the service from removing any trial settings.
  static void DisableForTesting();

 private:
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<ValidityService>;

  explicit ValidityService(content::WebContents* web_contents);

  // Called when a third party cookie access is (or could have been) allowed by
  // a |TPCD_TRIAL| or |TOP_LEVEL_TPCD_TRIAL| content setting. Posts a task to
  // the UI thread to check if the appropriate trial is enabled for the
  // specified context. Upon completion of the task, |update_callback| is run
  // with |url|, |first_party_url|, and the trial enablement status as
  // parameters.
  void CheckTrialStatusAsync(ContentSettingUpdateCallback update_callback,
                             const ContentSettingsType trial_settings_type,
                             const GURL& url,
                             const GURL& first_party_url);

  void CheckTrialStatusOnUiThread(ContentSettingUpdateCallback update_callback,
                                  const ContentSettingsType trial_settings_type,
                                  const GURL& url,
                                  const GURL& first_party_url);
  void UpdateTrialSettings(const ContentSettingsType trial_settings_type,
                           const GURL& url,
                           const GURL& first_party_url,
                           bool enabled);
  void SyncTrialSettingsToNetworkService(
      const ContentSettingsType trial_settings_type,
      const ContentSettingsForOneType& trial_settings);

  // Mostly a wrapper around
  // |content::HostContentSettingsMap::GetContentSetting()|
  // that accounts for the scoping differences between the trials and returns a
  // boolean based on whether a matching setting exists and is
  // |CONTENT_SETTING_ALLOW|.
  bool CheckTrialContentSetting(const GURL& url,
                                const GURL& first_party_url,
                                const ContentSettingsType trial_settings_type,
                                content_settings::SettingInfo* info);

  void OnCookiesAccessedImpl(const content::CookieAccessDetails& details);

  // WebContentsObserver overrides:
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;

  base::WeakPtrFactory<ValidityService> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace tpcd::trial

#endif  // CHROME_BROWSER_TPCD_SUPPORT_VALIDITY_SERVICE_H_
