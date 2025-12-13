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

namespace apps {
class AppServiceProxyBase;
}

namespace apps_util {

// Utility to wait for a change in preferred apps settings to be reflected in a
// PreferredAppsList. This used to be useful for Lacros Crosapi tests where the
// preferred apps settings needed to be synchronized between processes. Now that
// Lacros was sunsetted, this utility is still being used by
// preinstalled_web_app_manager_browsertest.cc and by
// intent_picker_bubble_view_browsertest.cc, and is being tested against
// multiple desktop platforms including but not limited to ChromeOS.
//
//  If this is used in an InteractiveBrowserTest/InteractiveAshTest, the
//  constructor's `run_loop_type` must be set to kNestableTasksAllowed.
class PreferredAppUpdateWaiter
    : public apps::PreferredAppsListHandle::Observer {
 public:
  // Waits for the PreferredAppsList in `handle` to update.
  PreferredAppUpdateWaiter(
      apps::PreferredAppsListHandle& handle,
      std::string app_id,
      bool is_preferred_app = true,
      base::RunLoop::Type run_loop_type = base::RunLoop::Type::kDefault);

  // Waits for the PreferredAppsList owned by `proxy` to update.
  PreferredAppUpdateWaiter(
      apps::AppServiceProxyBase* proxy,
      std::string app_id,
      bool is_preferred_app = true,
      base::RunLoop::Type run_loop_type = base::RunLoop::Type::kDefault);

  // Waits for the PreferredAppsList for `profile` to update.
  PreferredAppUpdateWaiter(
      Profile* profile,
      std::string app_id,
      bool is_preferred_app = true,
      base::RunLoop::Type run_loop_type = base::RunLoop::Type::kDefault);

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
