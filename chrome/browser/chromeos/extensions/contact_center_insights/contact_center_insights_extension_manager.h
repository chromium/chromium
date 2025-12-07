// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTACT_CENTER_INSIGHTS_CONTACT_CENTER_INSIGHTS_EXTENSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTACT_CENTER_INSIGHTS_CONTACT_CENTER_INSIGHTS_EXTENSION_MANAGER_H_

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

// `ContactCenterInsightsExtensionManager` manages the lifecycle of the Contact
// Center Insights Chrome component extension on managed ChromeOS
// devices based on the `InsightsExtensionEnabled` user policy.
class ContactCenterInsightsExtensionManager : public KeyedService {
 public:
  // Delegate used by the manager to check for profile
  // affiliation and install/uninstall the Contact Center Insights Chrome
  // component extension. Some functionality is stubbed out in tests to simplify
  // testing.
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;
    virtual ~Delegate() = default;

    virtual void InstallExtension(
        ::extensions::ComponentLoader* component_loader);
    virtual void UninstallExtension(
        ::extensions::ComponentLoader* component_loader);
    virtual bool IsProfileAffiliated(Profile* profile) const;
    virtual bool IsExtensionInstalled(
        ::extensions::ComponentLoader* component_loader) const;
  };

  ContactCenterInsightsExtensionManager(
      ::extensions::ComponentLoader* component_loader,
      Profile* profile,
      std::unique_ptr<Delegate> delegate);
  ContactCenterInsightsExtensionManager(
      const ContactCenterInsightsExtensionManager& other) = delete;
  ContactCenterInsightsExtensionManager& operator=(
      const ContactCenterInsightsExtensionManager& other) = delete;
  ~ContactCenterInsightsExtensionManager() override;

  // Boolean helper that decides if the component extension can be installed.
  bool CanInstallExtension() const;

 private:
  // Initializes the extension manager and sets up appropriate observers for
  // the relevant pref.
  void Init();

  // Callback triggered when the value of the relevant pref changes.
  void OnPrefChanged();

  // Removes the component extension if it is already installed.
  void RemoveExtensionIfInstalled();

  const raw_ptr<::extensions::ComponentLoader, DanglingUntriaged>
      component_loader_;
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  const std::unique_ptr<Delegate> delegate_;
  PrefChangeRegistrar registrar_;

  base::WeakPtrFactory<ContactCenterInsightsExtensionManager> weak_ptr_factory_{
      this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTACT_CENTER_INSIGHTS_CONTACT_CENTER_INSIGHTS_EXTENSION_MANAGER_H_
