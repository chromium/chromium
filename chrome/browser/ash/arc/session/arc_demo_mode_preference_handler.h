// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_DEMO_MODE_PREFERENCE_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_DEMO_MODE_PREFERENCE_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace arc {

class ArcSessionManager;
class ArcDemoModePreferenceHandlerTest;

// Observes Demo Mode preference and stops mini-ARCVM once, if running, via
// ArcSessionManager when the preference is enabled, which only happens after
// Demo Mode enrollment is complete (in
// ash::DemoSetupController::OnDeviceRegistered). ARCVM will automatically
// start again once the Demo session starts. This is necessary because (1) the
// demo apps image must be present at ARCVM boot and (2) mini-ARCVM is started
// once the OOBE is visible, but before Demo Mode setup is completed. As a
// result, that initial instance of mini-ARCVM does not pick up the demo apps
// image, and the first demo session (which otherwise will upgrade and use that
// instance of mini-ARCVM) cannot use the demo apps. This handler stops that
// initial instance of mini-ARCVM if running. After the preference flip, ARCVM
// instances (including mini instances) will load demo apps, which is
// guaranteed by ArcVmClientAdapter.
class ArcDemoModePreferenceHandler {
 public:
  static std::unique_ptr<ArcDemoModePreferenceHandler> Create(
      ArcSessionManager* arc_session_manager);
  ~ArcDemoModePreferenceHandler();
  ArcDemoModePreferenceHandler(const ArcDemoModePreferenceHandler&) = delete;
  ArcDemoModePreferenceHandler& operator=(const ArcDemoModePreferenceHandler&) =
      delete;

 private:
  friend class ArcDemoModePreferenceHandlerTest;
  ArcDemoModePreferenceHandler(base::OnceClosure preference_changed_callback,
                               PrefService* pref_service);

  // Called when Demo Mode preference is set after demo enrollment completes.
  void OnPreferenceChanged();

  base::OnceClosure preference_changed_callback_;
  // Owned by global BrowserProcess instance (g_browser_process);
  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_DEMO_MODE_PREFERENCE_HANDLER_H_
