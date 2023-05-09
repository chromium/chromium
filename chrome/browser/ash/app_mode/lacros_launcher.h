// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_LACROS_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_LACROS_LAUNCHER_H_

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_manager_observer.h"

namespace app_mode {

class LacrosLauncher : public crosapi::BrowserManagerObserver {
 public:
  LacrosLauncher();
  LacrosLauncher(const LacrosLauncher&) = delete;
  LacrosLauncher& operator=(const LacrosLauncher&) = delete;
  ~LacrosLauncher() override;

  void Start(base::OnceClosure callback);

 private:
  // crosapi::BrowserManagerObserver
  void OnStateChanged() override;

  base::OnceClosure on_launched_;

  // Observe the launch state of `BrowserManager`, and launch the
  // lacros-chrome when it is ready. This object is only used when Lacros is
  // enabled.
  base::ScopedObservation<crosapi::BrowserManager,
                          crosapi::BrowserManagerObserver>
      browser_manager_observation_{this};
};

}  // namespace app_mode

#endif  // CHROME_BROWSER_ASH_APP_MODE_LACROS_LAUNCHER_H_
