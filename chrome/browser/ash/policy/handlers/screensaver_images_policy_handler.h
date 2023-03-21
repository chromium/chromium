// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREENSAVER_IMAGES_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREENSAVER_IMAGES_POLICY_HANDLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
namespace policy {

// Observes the policy that provides image sources for the managed screensaver
// feature in order to download and cache the images.
class ASH_EXPORT ScreensaverImagesPolicyHandler : public ash::SessionObserver {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static ScreensaverImagesPolicyHandler*
  GetScreensaverImagesPolicyHandlerInstance();

  ScreensaverImagesPolicyHandler();
  ~ScreensaverImagesPolicyHandler() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

 private:
  void OnAmbientModeManagedScreensaverImagesPrefChanged();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  ash::ScopedSessionObserver scoped_session_observer_{this};
  base::WeakPtrFactory<ScreensaverImagesPolicyHandler> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_SCREENSAVER_IMAGES_POLICY_HANDLER_H_
