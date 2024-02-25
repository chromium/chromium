// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_SIGNIN_SCREEN_EXTENSIONS_EXTERNAL_LOADER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_SIGNIN_SCREEN_EXTENSIONS_EXTERNAL_LOADER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/external_cache_delegate.h"
#include "chrome/browser/ash/extensions/external_cache_impl.h"
#include "chrome/browser/extensions/external_loader.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace chromeos {

// Loader of extensions force-installed into the sign-in profile using the
// DeviceLoginScreenExtensions policy.
//
// Overview of the initialization flow:
//   StartLoading()
//   => SubscribeAndInitializeFromPrefs()
//   => UpdateStateFromPrefs()
//   => OnExtensionListsUpdated()
//   => {LoadFinished()|OnUpdated()}.
class SigninScreenExtensionsExternalLoader : public extensions::ExternalLoader,
                                             public ExternalCacheDelegate {
 public:
  explicit SigninScreenExtensionsExternalLoader(Profile* profile);
  SigninScreenExtensionsExternalLoader(
      const SigninScreenExtensionsExternalLoader&) = delete;
  SigninScreenExtensionsExternalLoader& operator=(
      const SigninScreenExtensionsExternalLoader&) = delete;

  // extensions::ExternalLoader:
  void StartLoading() override;

  // ExternalCacheDelegate:
  void OnExtensionListsUpdated(const base::Value::Dict& prefs) override;
  bool IsRollbackAllowed() const override;

 private:
  friend class base::RefCounted<SigninScreenExtensionsExternalLoader>;

  ~SigninScreenExtensionsExternalLoader() override;

  // Called when the pref service gets initialized asynchronously.
  void OnPrefsInitialized(bool success);
  // Starts loading the force-installed extensions specified via prefs and
  // observing the dynamic changes of the prefs.
  void SubscribeAndInitializeFromPrefs();
  // Starts loading the force-installed extensions specified via prefs.
  void UpdateStateFromPrefs();

  const raw_ptr<Profile> profile_;
  // Owned by ExtensionService, outlives |this|.
  ExternalCacheImpl external_cache_;
  PrefChangeRegistrar pref_change_registrar_;
  // Whether the list of extensions was already passed via LoadFinished().
  bool initial_load_finished_ = false;

  // Must be the last member.
  base::WeakPtrFactory<SigninScreenExtensionsExternalLoader> weak_factory_{
      this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_SIGNIN_SCREEN_EXTENSIONS_EXTERNAL_LOADER_H_
