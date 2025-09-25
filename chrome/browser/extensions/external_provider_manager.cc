// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_manager.h"

#include <cstddef>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/corrupted_extension_reinstaller.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_error_controller.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/external_provider_manager_factory.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/installed_loader.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/pending_extension_manager.h"
#include "extensions/browser/updater/extension_cache.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/verifier_formats.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/extensions/install_limiter.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace {
bool g_external_updates_disabled_for_test_ = false;
}  // namespace

using extensions::mojom::ManifestLocation;

namespace extensions {

using content::BrowserThread;

ExternalProviderManager::ExternalProviderManager(
    content::BrowserContext* context)
    : context_(context),
      extension_prefs_(ExtensionPrefs::Get(context)),
      registry_(ExtensionRegistry::Get(context)),
      pending_extension_manager_(PendingExtensionManager::Get(context)),
      error_controller_(ExtensionErrorController::Get(context)) {}

ExternalProviderManager::~ExternalProviderManager() = default;

// static
ExternalProviderManager* ExternalProviderManager::Get(
    content::BrowserContext* context) {
  return ExternalProviderManagerFactory::GetForBrowserContext(context);
}

void ExternalProviderManager::Shutdown() {
  // No need to unload extensions here because they are profile-scoped, and the
  // profile is in the process of being deleted.
  for (const auto& provider : external_extension_providers_) {
    provider->ServiceShutdown();
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ExternalProviderManager::CreateExternalProviders() {
  ExternalProviderImpl::CreateExternalProviders(
      this, Profile::FromBrowserContext(context_.get()),
      &external_extension_providers_);
}

// Some extensions will autoupdate themselves externally from Chrome.  These
// are typically part of some larger client application package. To support
// these, the extension will register its location in the preferences file
// (and also, on Windows, in the registry) and this code will periodically
// check that location for a .crx file, which it will then install locally if
// a new version is available.
// Errors are reported through LoadErrorReporter. Success is not reported.
void ExternalProviderManager::CheckForExternalUpdates() {
  if (g_external_updates_disabled_for_test_) {
    return;
  }

  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("browser,startup",
               "ExternalProviderManager::CheckForExternalUpdates");

  // Note that this installation is intentionally silent (since it didn't
  // go through the front-end).  Extensions that are registered in this
  // way are effectively considered 'pre-bundled', and so implicitly
  // trusted.  In general, if something has HKLM or filesystem access,
  // they could install an extension manually themselves anyway.

  // Ask each external extension provider to give us a call back for each
  // extension they know about. See OnExternalExtension(File|UpdateUrl)Found.
  for (const auto& provider : external_extension_providers_) {
    provider->VisitRegisteredExtension();
  }

  // Do any required work that we would have done after completion of all
  // providers.
  if (external_extension_providers_.empty()) {
    OnAllExternalProvidersReady();
  }
}

void ExternalProviderManager::OnAllExternalProvidersReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(context_.get());
#if BUILDFLAG(IS_CHROMEOS)
  auto* install_limiter = InstallLimiter::Get(profile);
  if (install_limiter) {
    install_limiter->OnAllExternalProvidersReady();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Install any pending extensions.
  ExtensionUpdater* updater = ExtensionUpdater::Get(profile);

  if (update_once_all_providers_are_ready_ && updater->enabled()) {
    update_once_all_providers_are_ready_ = false;
    ExtensionUpdater::CheckParams params;
    params.callback = external_updates_finished_callback_
                          ? std::move(external_updates_finished_callback_)
                          : base::OnceClosure();
    updater->CheckNow(std::move(params));
  } else if (external_updates_finished_callback_) {
    std::move(external_updates_finished_callback_).Run();
  }

  // Uninstall all the unclaimed extensions.
  ExtensionPrefs::ExtensionsInfo extensions_info =
      extension_prefs_->GetInstalledExtensionsInfo();
  for (const auto& info : extensions_info) {
    if (Manifest::IsExternalLocation(info.extension_location)) {
      CheckExternalUninstall(info.extension_id);
    }
  }

  ExtensionErrorController::Get(context_)->ShowErrorIfNeeded();

  ExternalInstallManager::Get(context_)->UpdateExternalExtensionAlert();
}

void ExternalProviderManager::CheckExternalUninstall(const std::string& id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We get the list of external extensions to check from preferences.
  // It is possible that an extension has preferences but is not loaded.
  // For example, an extension that requires experimental permissions
  // will not be loaded if the experimental command line flag is not used.
  // In this case, do not uninstall.
  const Extension* extension = registry_->GetInstalledExtension(id);
  if (!extension) {
    // We can't call UninstallExtension with an unloaded/invalid
    // extension ID.
    LOG(WARNING) << "Checking uninstallation of unloaded/invalid extension "
                 << "with id: " << id;
    return;
  }

  // Check if the providers know about this extension.
  for (const auto& provider : external_extension_providers_) {
    DCHECK(provider->IsReady());
    if (provider->HasExtensionWithLocation(id, extension->location())) {
      // Yup, known extension, don't uninstall.
      return;
    }
  }

  ExtensionRegistrar::Get(context_)->UninstallExtension(
      id, UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION, nullptr,
      base::NullCallback());
}

void ExternalProviderManager::ReinstallProviderExtensions() {
  for (const auto& provider : external_extension_providers_) {
    provider->TriggerOnExternalExtensionFound();
  }
}
bool ExternalProviderManager::AreAllExternalProvidersReady() const {
  for (const auto& provider : external_extension_providers_) {
    if (!provider->IsReady()) {
      return false;
    }
  }
  return true;
}

void ExternalProviderManager::ClearProvidersForTesting() {
  external_extension_providers_.clear();
}

void ExternalProviderManager::AddProviderForTesting(
    std::unique_ptr<ExternalProviderInterface> test_provider) {
  CHECK(test_provider);
  external_extension_providers_.push_back(std::move(test_provider));
}

base::AutoReset<bool>
ExternalProviderManager::DisableExternalUpdatesForTesting() {
  return base::AutoReset<bool>(&g_external_updates_disabled_for_test_, true);
}

bool ExternalProviderManager::OnExternalExtensionFileFound(
    const ExternalInstallInfoFile& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(crx_file::id_util::IdIsValid(info.extension_id));
  if (extension_prefs_->IsExternalExtensionUninstalled(info.extension_id)) {
    return false;
  }

  // Before even bothering to unpack, check and see if we already have this
  // version. This is important because these extensions are going to get
  // installed on every startup.
  const Extension* existing = registry_->GetExtensionById(
      info.extension_id, ExtensionRegistry::EVERYTHING);

  if (existing) {
    // The pre-installed apps will have the location set as INTERNAL. Since
    // older pre-installed apps are installed as EXTERNAL, we override them.
    // However, if the app is already installed as internal, then do the version
    // check.
    // TODO(grv) : Remove after Q1-2013.
    bool is_preinstalled_apps_migration =
        (info.crx_location == mojom::ManifestLocation::kInternal &&
         Manifest::IsExternalLocation(existing->location()));

    if (!is_preinstalled_apps_migration) {
      switch (existing->version().CompareTo(info.version)) {
        case -1:  // existing version is older, we should upgrade
          break;
        case 0:  // existing version is same, do nothing
          return false;
        case 1:  // existing version is newer, uh-oh
          LOG(WARNING) << "Found external version of extension "
                       << info.extension_id
                       << " that is older than current version. Current version"
                       << " is: " << existing->VersionString() << ". New "
                       << "version is: " << info.version.GetString()
                       << ". Keeping current version.";
          return false;
      }
    }
  }

  // If the extension is already pending, don't start an install.
  if (!pending_extension_manager_->AddFromExternalFile(
          info.extension_id, info.crx_location, info.version,
          info.creation_flags, info.mark_acknowledged)) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (extension_misc::IsDemoModeChromeApp(info.extension_id)) {
    pending_extension_manager_->Remove(info.extension_id);
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // no client (silent install)
  scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(context_));
  installer->AddInstallerCallback(base::BindOnce(
      &ExternalProviderManager::InstallationFromExternalFileFinished,
      weak_ptr_factory_.GetWeakPtr(), info.extension_id));
  installer->set_install_source(info.crx_location);
  installer->set_expected_id(info.extension_id);
  installer->set_expected_version(info.version,
                                  true /* fail_install_if_unexpected */);
  installer->set_install_immediately(info.install_immediately);
  installer->set_creation_flags(info.creation_flags);

  CRXFileInfo file_info(
      info.path, info.crx_location == mojom::ManifestLocation::kExternalPolicy
                     ? GetPolicyVerifierFormat()
                     : GetExternalVerifierFormat());
#if BUILDFLAG(IS_CHROMEOS)
  auto* install_limiter =
      InstallLimiter::Get(Profile::FromBrowserContext(context_.get()));
  if (install_limiter) {
    install_limiter->Add(installer, file_info);
  } else {
    installer->InstallCrxFile(file_info);
  }
#else
  installer->InstallCrxFile(file_info);
#endif

  // Depending on the source, a new external extension might not need a user
  // notification on installation. For such extensions, mark them acknowledged
  // now to suppress the notification.
  if (info.mark_acknowledged) {
    ExternalInstallManager::Get(context_)->AcknowledgeExternalExtension(
        info.extension_id);
  }

  return true;
}

bool ExternalProviderManager::OnExternalExtensionUpdateUrlFound(
    const ExternalInstallInfoUpdateUrl& info,
    bool force_update) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(crx_file::id_util::IdIsValid(info.extension_id));

  if (Manifest::IsExternalLocation(info.download_location)) {
    // All extensions that are not user specific can be cached.
    ExtensionsBrowserClient::Get()->GetExtensionCache()->AllowCaching(
        info.extension_id);
  }

  InstallStageTracker* install_stage_tracker =
      InstallStageTracker::Get(context_);

  const Extension* extension = registry_->GetExtensionById(
      info.extension_id, ExtensionRegistry::EVERYTHING);
  if (extension) {
    // Already installed. Skip this install if the current location has higher
    // priority than |info.download_location|, and we aren't doing a
    // reinstall of a corrupt policy force-installed extension.
    ManifestLocation current = extension->location();
    if (!IsReinstallForCorruptionExpected(info.extension_id) &&
        current == Manifest::GetHigherPriorityLocation(
                       current, info.download_location)) {
      install_stage_tracker->ReportFailure(
          info.extension_id,
          InstallStageTracker::FailureReason::ALREADY_INSTALLED);
      return false;
    }
    // If the installation is requested from a higher priority source, update
    // its install location.
    ExtensionRegistrar* registrar = ExtensionRegistrar::Get(context_);
    if (current !=
        Manifest::GetHigherPriorityLocation(current, info.download_location)) {
      registrar->RemoveExtension(info.extension_id,
                                 UnloadedExtensionReason::UPDATE);

      // Fetch the installation info from the prefs, and reload the extension
      // with a modified install location.
      std::optional<ExtensionInfo> installed_extension(
          extension_prefs_->GetInstalledExtensionInfo(info.extension_id));
      installed_extension->extension_location = info.download_location;

      // Load the extension with the new install location
      Profile* profile = Profile::FromBrowserContext(context_);
      InstalledLoader(profile).Load(*installed_extension, false);
      // Update the install location in the prefs.
      extension_prefs_->SetInstallLocation(info.extension_id,
                                           info.download_location);

      // If the extension was due to any of the following reasons, and it must
      // remain enabled, remove those reasons:
      // - Disabled by the user.
      // - User hasn't accepted a permissions increase.
      // - User hasn't accepted an external extension's prompt.
      if (registry_->disabled_extensions().GetByID(info.extension_id) &&
          ExtensionSystem::Get(context_)
              ->management_policy()
              ->MustRemainEnabled(
                  registry_->GetExtensionById(info.extension_id,
                                              ExtensionRegistry::EVERYTHING),
                  nullptr)) {
        const DisableReasonSet to_remove = {
            disable_reason::DISABLE_USER_ACTION,
            disable_reason::DISABLE_EXTERNAL_EXTENSION,
            disable_reason::DISABLE_PERMISSIONS_INCREASE};
        extension_prefs_->RemoveDisableReasons(info.extension_id, to_remove);

        // Only re-enable the extension if there are no other disable reasons.
        if (extension_prefs_->GetDisableReasons(info.extension_id).empty()) {
          registrar->EnableExtension(info.extension_id);
        }
      }
      // If the extension is not corrupted, it is already installed with the
      // correct install location, so there is no need to add it to the pending
      // set of extensions. If the extension is corrupted, it should be
      // reinstalled, thus it should be added to the pending extensions for
      // installation.
      if (!IsReinstallForCorruptionExpected(info.extension_id)) {
        return false;
      }
    }
    // Otherwise, overwrite the current installation.
  }

  // Add |info.extension_id| to the set of pending extensions.  If it can not
  // be added, then there is already a pending record from a higher-priority
  // install source.  In this case, signal that this extension will not be
  // installed by returning false.
  install_stage_tracker->ReportInstallationStage(
      info.extension_id, InstallStageTracker::Stage::PENDING);
  if (!pending_extension_manager_->AddFromExternalUpdateUrl(
          info.extension_id, info.install_parameter, info.update_url,
          info.download_location, info.creation_flags,
          info.mark_acknowledged)) {
    // We can reach here if the extension from an equal or higher priority
    // source is already present in the |pending_extension_list_|. No need to
    // report the failure in this case.
    if (!pending_extension_manager_->IsIdPending(info.extension_id)) {
      install_stage_tracker->ReportFailure(
          info.extension_id,
          InstallStageTracker::FailureReason::PENDING_ADD_FAILED);
    }
    return false;
  }

  if (force_update) {
    update_once_all_providers_are_ready_ = true;
  }
  return true;
}

void ExternalProviderManager::OnExternalProviderReady(
    const ExternalProviderInterface* provider) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(provider->IsReady());

  // An external provider has finished loading.  We only take action
  // if all of them are finished. So we check them first.
  if (AreAllExternalProvidersReady()) {
    OnAllExternalProvidersReady();
  }
}

void ExternalProviderManager::OnExternalProviderUpdateComplete(
    const ExternalProviderInterface* provider,
    const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
    const std::vector<ExternalInstallInfoFile>& file_extensions,
    const std::set<std::string>& removed_extensions) {
  // Update pending_extension_manager_ with the new extensions first.
  for (const auto& extension : update_url_extensions) {
    OnExternalExtensionUpdateUrlFound(extension, false);
  }
  for (const auto& extension : file_extensions) {
    OnExternalExtensionFileFound(extension);
  }

#if DCHECK_IS_ON()
  for (const std::string& id : removed_extensions) {
    for (const auto& extension : update_url_extensions) {
      DCHECK_NE(id, extension.extension_id);
    }
    for (const auto& extension : file_extensions) {
      DCHECK_NE(id, extension.extension_id);
    }
  }
#endif

  // Then uninstall before running |updater_|.
  for (const std::string& id : removed_extensions) {
    CheckExternalUninstall(id);
  }

  Profile* profile = Profile::FromBrowserContext(context_);
  ExtensionUpdater* updater = ExtensionUpdater::Get(profile);
  if (!update_url_extensions.empty() && updater->enabled()) {
    // Empty params will cause pending extensions to be updated.
    updater->CheckNow(ExtensionUpdater::CheckParams());
  }

  error_controller_->ShowErrorIfNeeded();
  ExternalInstallManager::Get(context_)->UpdateExternalExtensionAlert();
}

void ExternalProviderManager::InstallationFromExternalFileFinished(
    const std::string& extension_id,
    const std::optional<CrxInstallError>& error) {
  if (error != std::nullopt) {
    // When installation is finished, the extension should not remain in the
    // pending extension manager. For successful installations this is done
    // in OnExtensionInstalled handler.
    pending_extension_manager_->Remove(extension_id);
  }
}

bool ExternalProviderManager::IsReinstallForCorruptionExpected(
    const ExtensionId& id) const {
  auto* reinstaller = CorruptedExtensionReinstaller::Get(context_);
  return reinstaller->IsReinstallForCorruptionExpected(id);
}

}  // namespace extensions
