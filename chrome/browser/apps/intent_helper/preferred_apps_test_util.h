// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_PREFERRED_APPS_TEST_UTIL_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_PREFERRED_APPS_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"

class Profile;

namespace apps_util {

// Utility to wait for a change in preferred apps settings to be reflected in a
// PreferredAppsList. This is useful for Lacros Crosapi tests where the
// preferred apps settings need to be synchronized between processes.
class PreferredAppUpdateWaiter
    : public apps::PreferredAppsListHandle::Observer {
 public:
  explicit PreferredAppUpdateWaiter(apps::PreferredAppsListHandle& handle,
                                    std::string app_id,
                                    bool is_preferred_app = true);
  ~PreferredAppUpdateWaiter() override;

  void Wait();

  // apps::PreferredAppsListHandle::Observer:
  void OnPreferredAppChanged(const std::string& app_id,
                             bool is_preferred_app) override;
  void OnPreferredAppsListWillBeDestroyed(
      apps::PreferredAppsListHandle* handle) override;

 private:
  std::string waiting_app_id_;
  bool waiting_is_preferred_app_;
  raw_ref<apps::PreferredAppsListHandle> preferred_apps_;
  base::RunLoop run_loop_;

  base::ScopedObservation<apps::PreferredAppsListHandle,
                          apps::PreferredAppsListHandle::Observer>
      observation_{this};
};

// Utility to set an app to be the preferred app for its supported links
// and wait for the change to propagate through to the current process.
void SetSupportedLinksPreferenceAndWait(Profile* profile,
                                        const std::string& app_id);

// Utility to remove an app as the preferred app for its supported links
// and wait for the change to propagate through to the current process.
void RemoveSupportedLinksPreferenceAndWait(Profile* profile,
                                           const std::string& app_id);

}  // namespace apps_util

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_PREFERRED_APPS_TEST_UTIL_H_
