// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_APPS_INTENT_PICKER_DELEGATE_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_APPS_INTENT_PICKER_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "chrome/browser/apps/link_capturing/metrics/intent_handling_metrics.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace apps {

using IntentPickerAppsCallback =
    base::OnceCallback<void(std::vector<apps::IntentPickerAppInfo>)>;

using IconLoadedCallback = base::OnceCallback<void(ui::ImageModel)>;

class AppsIntentPickerDelegate {
 public:
  virtual ~AppsIntentPickerDelegate() = default;
  virtual bool ShouldShowIntentPickerWithApps() = 0;
  virtual void FindAllAppsForUrl(const GURL& url,
                                 IntentPickerAppsCallback apps_callback) = 0;
  virtual bool IsPreferredAppForSupportedLinks(const std::string& app_id) = 0;
  virtual void LoadSingleAppIcon(PickerEntryType entry_type,
                                 const std::string& app_id,
                                 int size_in_dep,
                                 IconLoadedCallback icon_loaded_callback) = 0;
  // Records metrics for usage of the intent picker icon which appears in the
  // Omnibox.
  virtual void RecordIntentPickerIconEvent(
      apps::IntentPickerIconEvent event) = 0;
  virtual bool ShouldLaunchAppDirectly(const GURL& url,
                                       const std::string& app_name,
                                       PickerEntryType entry_type) = 0;
  virtual void RecordOutputMetrics(PickerEntryType entry_type,
                                   IntentPickerCloseReason close_reason,
                                   bool should_persist,
                                   bool should_launch_app) = 0;
  virtual void PersistIntentPreferencesForApp(PickerEntryType entry_type,
                                              const std::string& app_id) = 0;
  virtual void LaunchApp(content::WebContents* web_contents,
                         const GURL& url,
                         const std::string& launch_name,
                         PickerEntryType entry_type) = 0;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_APPS_INTENT_PICKER_DELEGATE_H_
