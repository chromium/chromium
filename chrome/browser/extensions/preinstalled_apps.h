// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PREINSTALLED_APPS_H_
#define CHROME_BROWSER_EXTENSIONS_PREINSTALLED_APPS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Functions and types related to preinstalling apps.
namespace preinstalled_apps {

// These enum values are persisted in the user preferences, so they should not
// be changed.
enum InstallState {
  kUnknown,
  // Now unused, left for backward compatibility.
  kProvideLegacyPreinstalledApps,
  kNeverInstallPreinstalledApps,
  kAlreadyInstalledPreinstalledApps
};

// Register preference properties used by default apps to maintain
// install state.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// A specialization of the ExternalProviderImpl that conditionally installs apps
// from the chrome::DIR_DEFAULT_APPS location based on a preference in the
// profile.
class Provider : public extensions::ExternalProviderImpl {
 public:
  Provider(Profile* profile,
           VisitorInterface* service,
           scoped_refptr<extensions::ExternalLoader> loader,
           extensions::mojom::ManifestLocation crx_location,
           extensions::mojom::ManifestLocation download_location,
           int creation_flags);

  Provider(const Provider&) = delete;
  Provider& operator=(const Provider&) = delete;

  // ExternalProviderImpl overrides:
  void VisitRegisteredExtension() override;
  void SetPrefs(base::Value::Dict prefs) override;

  static bool DidPerformNewInstallationForProfile(Profile* profile);

  // Exposed for testing.
  bool preinstalled_apps_enabled() const { return preinstalled_apps_enabled_; }
  bool is_migration() const { return is_migration_; }
  bool perform_new_installation() const { return perform_new_installation_; }

 private:
  // Initializes state for the Provider based on the profile.
  void InitProfileState();

  // The associated profile.
  raw_ptr<Profile> profile_ = nullptr;
  // Whether default apps are enabled for the profile.
  bool preinstalled_apps_enabled_ = false;
  // Whether this is the first run since a migration from Chrome 22-ish.
  bool is_migration_ = false;
  // Whether this class should perform a new installation, such as for a
  // new profile.
  bool perform_new_installation_ = false;
};

}  // namespace preinstalled_apps

#endif  // CHROME_BROWSER_EXTENSIONS_PREINSTALLED_APPS_H_
