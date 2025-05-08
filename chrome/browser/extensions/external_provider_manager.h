// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_PROVIDER_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_PROVIDER_MANAGER_H_

#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

class CrxInstallError;
class ExtensionErrorController;
class ExtensionPrefs;
class ExtensionRegistry;
class PendingExtensionManager;

// Class ExternalProviderManager manages the set of external extension
// providers, and installs/uninstalls the extensions they provide.
class ExternalProviderManager
    : public KeyedService,
      public ExternalProviderInterface::VisitorInterface {
 public:
  explicit ExternalProviderManager(content::BrowserContext* context);

  ExternalProviderManager(const ExternalProviderManager&) = delete;
  ExternalProviderManager& operator=(const ExternalProviderManager&) = delete;

  ~ExternalProviderManager() override;

  // KeyedService:
  void Shutdown() override;

  // Returns the instance for the given `browser_context`.
  static ExternalProviderManager* Get(content::BrowserContext* browser_context);

  // ExternalProviderInterface::VisitorInterface:
  bool OnExternalExtensionFileFound(
      const ExternalInstallInfoFile& info) override;
  bool OnExternalExtensionUpdateUrlFound(
      const ExternalInstallInfoUpdateUrl& info,
      bool force_update) override;
  void OnExternalProviderReady(
      const ExternalProviderInterface* provider) override;
  void OnExternalProviderUpdateComplete(
      const ExternalProviderInterface* provider,
      const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
      const std::vector<ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override;

  // Creates external extension providers.
  void CreateExternalProviders();

  // Checks for updates (or potentially new extensions from external providers)
  void CheckForExternalUpdates();

  // Ask each external extension provider to call
  // OnExternalExtension(File|UpdateUrl)Found() with their known extensions.
  // This will trigger an update/reinstall of the extensions saved in the
  // provider's prefs.
  void ReinstallProviderExtensions();

  // Clears all ExternalProviders.
  void ClearProvidersForTesting();

  // Adds an ExternalProviderInterface for the service to use during testing.
  void AddProviderForTesting(
      std::unique_ptr<ExternalProviderInterface> test_provider);

  // Sets a callback to be called when all external providers are ready and
  // their extensions have been installed.
  void set_external_updates_finished_callback_for_test(
      base::OnceClosure callback) {
    external_updates_finished_callback_ = std::move(callback);
  }

  // While disabled all calls to CheckForExternalUpdates() will bail out.
  static base::AutoReset<bool> DisableExternalUpdatesForTesting();

 private:
  // Returns true if all the external extension providers are ready.
  bool AreAllExternalProvidersReady() const;

  // Called once all external providers are ready. Checks for unclaimed
  // external extensions.
  void OnAllExternalProvidersReady();

  // For the extension in `version_path` with `id`, check to see if it's an
  // externally managed extension.  If so, uninstall it.
  void CheckExternalUninstall(const std::string& id);

  // Callback for installation finish of an extension from external file, since
  // we need to remove this extension from the pending extension manager in case
  // of installation failure. This is only a need for extensions installed
  // by file, since extensions installed by URL will be intentionally kept in
  // the manager and retried later.
  void InstallationFromExternalFileFinished(
      const std::string& extension_id,
      const std::optional<CrxInstallError>& error);

  // Are we expecting a reinstall of the extension due to corruption?
  bool IsReinstallForCorruptionExpected(const ExtensionId& id) const;

  // The BrowserContext with which the manager is associated.
  raw_ptr<content::BrowserContext> context_;

  // A collection of external extension providers.  Each provider reads
  // a source of external extension information.  Examples include the
  // windows registry and external_extensions.json.
  ProviderCollection external_extension_providers_;

  // Preferences for the owning profile.
  raw_ptr<ExtensionPrefs> extension_prefs_ = nullptr;

  // Sets of enabled/disabled/terminated/blocklisted extensions. Not owned.
  raw_ptr<ExtensionRegistry> registry_ = nullptr;

  // Hold the set of pending extensions. Not owned.
  raw_ptr<PendingExtensionManager> pending_extension_manager_ = nullptr;

  // The controller for the UI that alerts the user about any blocklisted
  // extensions.
  raw_ptr<ExtensionErrorController> error_controller_;

  // Set to true by OnExternalExtensionUpdateUrlFound() when an external
  // extension URL is found, and by CheckForUpdatesSoon() when an update check
  // has to wait for the external providers.  Used in
  // OnAllExternalProvidersReady() to determine if an update check is needed to
  // install pending extensions.
  bool update_once_all_providers_are_ready_ = false;

  // A callback to be called when all external providers are ready and their
  // extensions have been installed. This happens on initial load and whenever
  // a new entry is found. Normally this is a null callback, but is used in
  // external provider related tests.
  base::OnceClosure external_updates_finished_callback_;

  base::WeakPtrFactory<ExternalProviderManager> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_PROVIDER_MANAGER_H_
