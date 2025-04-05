// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_WEB_APPS_INTENT_PICKER_DELEGATE_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_WEB_APPS_INTENT_PICKER_DELEGATE_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/link_capturing/apps_intent_picker_delegate.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/apps/link_capturing/mac_intent_picker_helpers.h"
#endif

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {
class WebAppProvider;
}  // namespace web_app

namespace apps {

class WebAppsIntentPickerDelegate : public AppsIntentPickerDelegate {
 public:
  // Since this class sometimes pre-caches icons for returned applications, all
  // the icon sizes that might be passed to calls to `LoadSingleAppIcon` should
  // be included in the `icon_sizes_in_dep` parameter.
  WebAppsIntentPickerDelegate(Profile* profile,
                              std::vector<int> icon_sizes_in_dep);
  ~WebAppsIntentPickerDelegate() override;

  WebAppsIntentPickerDelegate(const WebAppsIntentPickerDelegate&) = delete;
  WebAppsIntentPickerDelegate& operator=(const WebAppsIntentPickerDelegate&) =
      delete;

  bool ShouldShowIntentPickerWithApps() override;
  void FindAllAppsForUrl(const GURL& url,
                         IntentPickerAppsCallback apps_callback) override;
  bool IsPreferredAppForSupportedLinks(const std::string& app_id) override;
  void LoadSingleAppIcon(PickerEntryType entry_type,
                         const std::string& app_id,
                         int size_in_dep,
                         IconLoadedCallback icon_loaded_callback) override;
  void RecordIntentPickerIconEvent(apps::IntentPickerIconEvent event) override;
  bool ShouldLaunchAppDirectly(const GURL& url,
                               const std::string& app_id,
                               PickerEntryType entry_type) override;
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
#if BUILDFLAG(IS_MAC)
  // If any app with universal links are found, cache that for reuse later (like
  // getting icons) instead of going through the universal links API again.
  void CacheMacAppInfoAndPostFinalCallback(
      IntentPickerAppsCallback apps_callback,
      std::vector<IntentPickerAppInfo> apps,
      std::optional<MacAppInfo> mac_app_info);
#endif  // BUILDFLAG(IS_MAC)

  raw_ref<Profile> profile_;
  raw_ptr<web_app::WebAppProvider> provider_;
  std::vector<int> icon_sizes_in_dep_;
#if BUILDFLAG(IS_MAC)
  std::optional<MacAppInfo> mac_app_info_;
#endif  // BUILDFLAG(IS_MAC)
  base::WeakPtrFactory<WebAppsIntentPickerDelegate> weak_ptr_factory{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_WEB_APPS_INTENT_PICKER_DELEGATE_H_
