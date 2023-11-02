// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_WINDOW_CLOSER_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_WINDOW_CLOSER_H_

#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/instance_registry.h"

// Used to close the GMSCore dialog window which disrupts the attract
// loop during demo mode sessions.
class DemoModeWindowCloser : public apps::InstanceRegistry::Observer {
 public:
  DemoModeWindowCloser();
  DemoModeWindowCloser(const DemoModeWindowCloser&) = delete;
  DemoModeWindowCloser& operator=(const DemoModeWindowCloser&) = delete;
  ~DemoModeWindowCloser() override;

  // apps::InstanceRegistry::Observer:
  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

 private:
  std::string gms_core_app_id_;
  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      scoped_observation_{this};
};

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_MODE_WINDOW_CLOSER_H_
