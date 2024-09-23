// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_WEB_APP_PROFILE_SWITCHER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_WEB_APP_PROFILE_SWITCHER_H_

#include "base/scoped_multi_source_observation.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/webapps/common/web_app_id.h"

namespace webapps {
enum class InstallResultCode;
}  // namespace webapps

namespace web_app {
class AppLock;
struct IconBitmaps;
}  // namespace web_app

// A class that can open a web app with the specified |app_id|, that is
// already installed for |active_profile|, for other profiles.
class WebAppProfileSwitcher : public ProfileObserver {
 public:
  WebAppProfileSwitcher(const webapps::AppId& app_id,
                        Profile& active_profile,
                        base::OnceClosure on_completion);
  ~WebAppProfileSwitcher() override;

  // Opens and (if needed) installs the app for |profile_to_open|.
  // If the app is not installed for the profile, it gets installed with
  // basic manifest fields copied over.
  void SwitchToProfile(const base::FilePath& profile_to_open);

 private:
  // ProfileObserver
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Schedules a callback with a lock to check if the app is already installed
  // prior to launching/installation.
  void QueryProfileWebAppRegistryToOpenWebApp(Profile* new_profile);

  // Checks if the app is installed using the obtained |lock| and
  // starts launch or installation.
  void InstallOrOpenWebAppWindowForProfile(web_app::AppLock& lock,
                                           base::Value::Dict& debug_value);

  // Installs web app defined by |app_id_| for a |new_profile| and launches
  // it once installed.
  void InstallAndLaunchWebApp(web_app::IconBitmaps icon_bitmaps);

  // Launches web app defined by |app_id| for a |new_profile|.
  void LaunchAppWithId(const webapps::AppId& app_id,
                       webapps::InstallResultCode install_result);

  // Must be called when the the switcher is no longer needed.
  void RunCompletionCallback();

  // The id of an app to install.
  webapps::AppId app_id_;

  // The profile for which the app is already open.
  raw_ref<Profile> active_profile_;

  // The callback that needs to be invoked when the profile is switched,
  // or when switching is aborted.
  base::OnceClosure on_completion_;

  // The profile, for which the app needs to be installed or/and open.
  raw_ptr<Profile> new_profile_ = nullptr;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profiles_observation_{this};

  base::WeakPtrFactory<WebAppProfileSwitcher> weak_factory_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_WEB_APP_PROFILE_SWITCHER_H_
