// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_DESK_API_DESK_API_EXTENSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_DESK_API_DESK_API_EXTENSION_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

namespace extensions {

class ComponentLoader;

}  // namespace extensions

namespace chromeos {

// `DeskApiExtensionManager` manages the lifecycle of the Desk
// API component extension on ChromeOS devices based on the
// `DeskApiThirdPartyAccessEnabled` user policy.
class DeskApiExtensionManager : public KeyedService {
 public:
  // Delegate used by the manager to check for policy and install/uninstall
  // the Desk API component extension.
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;
    virtual ~Delegate() = default;

    virtual void InstallExtension(
        ::extensions::ComponentLoader* component_loader,
        const std::string& manifest_content);
    virtual void UninstallExtension(
        ::extensions::ComponentLoader* component_loader);
    virtual bool IsProfileAffiliated(Profile* profile) const;
    virtual bool IsExtensionInstalled(
        ::extensions::ComponentLoader* component_loader) const;
  };

  DeskApiExtensionManager(::extensions::ComponentLoader* component_loader,
                          Profile* profile,
                          std::unique_ptr<Delegate> delegate);
  DeskApiExtensionManager(const DeskApiExtensionManager& other) = delete;
  DeskApiExtensionManager& operator=(const DeskApiExtensionManager& other) =
      delete;
  ~DeskApiExtensionManager() override;

  // Boolean helper that decides if the component extension can be installed.
  bool CanInstallExtension() const;

  // Gets component extension's manifest with domain allowlist controlled
  // through enterprise policy. If the policy does not contain any valid
  // domains, this method returns empty string. Otherwise this method returns a
  // valid Chrome extension Manifest V3. See
  // https://developer.chrome.com/docs/extensions/mv3/intro/
  std::string GetManifest() const;

 private:
  // Initializes the extension manager and sets up appropriate observers for
  // the relevant pref.
  void Init();

  // Callback triggered when the value of the relevant pref changes.
  void OnPrefChanged();

  // Loads or unloads the component extension depends whether the component
  // extension can be installed.
  void LoadOrUnloadExtension();

  // Removes the component extension if it is already installed.
  void RemoveExtensionIfInstalled();

  const raw_ptr<::extensions::ComponentLoader, DanglingUntriaged>
      component_loader_;
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  const std::unique_ptr<Delegate> delegate_;
  PrefChangeRegistrar registrar_;

  base::WeakPtrFactory<DeskApiExtensionManager> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_DESK_API_DESK_API_EXTENSION_MANAGER_H_
