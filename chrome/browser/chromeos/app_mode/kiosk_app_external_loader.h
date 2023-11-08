// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_EXTERNAL_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_EXTERNAL_LOADER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_external_loader_broker.h"
#include "chrome/browser/extensions/external_loader.h"

namespace chromeos {

// An extensions::ExternalLoader that loads apps and extensions for a kiosk
// session. It is used to either load primary kiosk app, or secondary kiosk
// apps (or extensions). Single instance cannot load both primary and secondary
// apps.
// It registers a callback with KioskChromeAppManager that is called when the
// kiosk app information is updated - when run, the callback will finish
// extension load handled by `this`. Note that `this` might call
// extensions::ExternalLoader::OnUpdated(), because in certain cases kiosk app
// properties might get updated. This lives on the UI thread, even though it's
// ref counted as a subclass of `extensions::ExternalLoader`.
class KioskAppExternalLoader : public extensions::ExternalLoader {
 public:
  enum class AppClass { kPrimary, kSecondary };

  explicit KioskAppExternalLoader(AppClass app_class);
  KioskAppExternalLoader(const KioskAppExternalLoader&) = delete;
  KioskAppExternalLoader& operator=(const KioskAppExternalLoader&) = delete;

  // extensions::ExternalLoader:
  void StartLoading() override;

 protected:
  ~KioskAppExternalLoader() override;

 private:
  enum class State { kInitial, kLoading, kLoaded };

  // Registers callback for handling kiosk apps prefs changes.
  void SetPrefsChangedHandler(
      ChromeKioskExternalLoaderBroker::InstallDataChangeCallback handler);

  // Sends `prefs` through to the external loader owner (using
  // extensions::ExternalLoader interface).
  void SendPrefs(base::Value::Dict prefs);

  // The class of kiosk apps this external extensions loader handles.
  const AppClass app_class_;

  State state_ = State::kInitial;

  base::WeakPtrFactory<KioskAppExternalLoader> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_EXTERNAL_LOADER_H_
