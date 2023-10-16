// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_APPS_INTENT_PICKER_DELEGATE_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_APPS_INTENT_PICKER_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/link_capturing/apps_intent_picker_delegate.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "chrome/browser/apps/link_capturing/metrics/intent_handling_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

namespace apps {

class ChromeOsAppsIntentPickerDelegate : public AppsIntentPickerDelegate {
 public:
  explicit ChromeOsAppsIntentPickerDelegate(Profile* profile);
  ~ChromeOsAppsIntentPickerDelegate() override;

  ChromeOsAppsIntentPickerDelegate(const ChromeOsAppsIntentPickerDelegate&) =
      delete;
  ChromeOsAppsIntentPickerDelegate& operator=(
      const ChromeOsAppsIntentPickerDelegate&) = delete;

  bool ShouldShowIntentPickerWithApps() override;
  void FindAllAppsForUrl(const GURL& url,
                         IntentPickerAppsCallback apps_callback) override;
  bool IsPreferredAppForSupportedLinks(const webapps::AppId& app_id) override;
  void LoadSingleAppIcon(
      apps::AppType app_type,
      const webapps::AppId& app_id,
      int size_in_dep,
      base::OnceCallback<void(apps::IconValuePtr)> callback) override;
  void RecordIntentPickerIconEvent(
      IntentHandlingMetrics::IntentPickerIconEvent event) override;
  bool ShouldLaunchAppDirectly(const GURL& url,
                               const std::string& app_name) override;
  void RecordOutputMetrics(PickerEntryType entry_type,
                           IntentPickerCloseReason close_reason,
                           bool should_persist,
                           bool should_launch_app) override;
  void PersistIntentPreferencesForApp(PickerEntryType entry_type,
                                      const std::string& app_id) override;
  void LaunchApp(content::WebContents* web_contents,
                 const GURL& url,
                 const std::string& launch_name,
                 PickerEntryType entry_type) override;

 private:
  raw_ref<Profile> profile_;
  raw_ptr<apps::AppServiceProxy> proxy_ = nullptr;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_APPS_INTENT_PICKER_DELEGATE_H_
