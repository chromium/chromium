// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/unpacked_installer.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/install_index_helper.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/install_prefs_helper.h"
#include "extensions/browser/path_util.h"
#include "extensions/browser/policy_check.h"
#include "extensions/browser/preload_check_group.h"
#include "extensions/browser/requirements_checker.h"
#include "extensions/browser/ruleset_parse_result.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::SharedModuleInfo;

namespace extensions {

namespace {

const char kUnpackedExtensionsBlocklistedError[] =
    "Loading of unpacked extensions is disabled by the administrator.";

const char kImportMinVersionNewer[] =
    "'import' version requested is newer than what is installed.";
const char kImportMissing[] = "'import' extension is not installed.";
const char kImportNotSharedModule[] = "'import' is not a shared module.";

// Deletes files reserved for use by the Extension system in the kMetadataFolder
// and the kMetadataFolder itself if it is empty.
void MaybeCleanupMetadataFolder(const base::FilePath& extension_path) {
  const std::vector<base::FilePath> reserved_filepaths =
      file_util::GetReservedMetadataFilePaths(extension_path);
  for (const auto& file : reserved_filepaths)
    base::DeletePathRecursively(file);

  const base::FilePath& metadata_dir = extension_path.Append(kMetadataFolder);
  if (base::IsDirectoryEmpty(metadata_dir))
    base::DeletePathRecursively(metadata_dir);
}

}  // namespace

// static
scoped_refptr<UnpackedInstaller> UnpackedInstaller::Create(
    ExtensionService* extension_service) {
  DCHECK(extension_service);
  return scoped_refptr<UnpackedInstaller>(
      new UnpackedInstaller(extension_service));
}

UnpackedInstaller::UnpackedInstaller(ExtensionService* extension_service)
    : service_weak_(extension_service->AsExtensionServiceWeakPtr()),
      profile_(extension_service->profile()),
      require_modern_manifest_version_(true),
      be_noisy_on_failure_(true) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

UnpackedInstaller::~UnpackedInstaller() {
}

void UnpackedInstaller::Load(const base::FilePath& path_in) {
  DCHECK(extension_path_.empty());
  extension_path_ = path_in;
  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&UnpackedInstaller::GetAbsolutePath, this));
}

bool UnpackedInstaller::LoadFromCommandLine(const base::FilePath& path_in,
                                            std::string* extension_id,
                                            bool only_allow_apps) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension_path_.empty());

  if (!service_weak_.get())
    return false;
  // Load extensions from the command line synchronously to avoid a race
  // between extension loading and loading an URL from the command line.
  base::ScopedAllowBlocking allow_blocking;

  extension_path_ =
      base::MakeAbsoluteFilePath(path_util::ResolveHomeDirectory(path_in));

  if (!IsLoadingUnpackedAllowed()) {
    ReportExtensionLoadError(kUnpackedExtensionsBlocklistedError);
    return false;
  }

  std::string error;
  if (!LoadExtension(mojom::ManifestLocation::kCommandLine, GetFlags(),
                     &error)) {
    ReportExtensionLoadError(error);
    return false;
  }

  if (only_allow_apps && !extension()->is_platform_app()) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // Avoid crashing for users with hijacked shortcuts.
    return true;
#else
    // Defined here to avoid unused variable errors in official builds.
    const char extension_instead_of_app_error[] =
        "App loading flags cannot be used to load extensions. Please use "
        "--load-extension instead.";
    ReportExtensionLoadError(extension_instead_of_app_error);
    return false;
#endif
  }

  extension()->permissions_data()->BindToCurrentThread();
  PermissionsUpdater(
      service_weak_->profile(), PermissionsUpdater::INIT_FLAG_TRANSIENT)
      .InitializePermissions(extension());
  StartInstallChecks();

  *extension_id = extension()->id();
  return true;
}

void UnpackedInstaller::StartInstallChecks() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ExtensionService* service = service_weak_.get();
  if (!service)
    return;

  // TODO(crbug.com/40388034): Enable these checks all the time.  The reason
  // they are disabled for extensions loaded from the command-line is that
  // installing unpacked extensions is asynchronous, but there can be
  // dependencies between the extensions loaded by the command line.
  if (extension()->manifest()->location() !=
      mojom::ManifestLocation::kCommandLine) {
    if (service->browser_terminating())
      return;

    // TODO(crbug.com/40387578): Move this code to a utility class to avoid
    // duplication of SharedModuleService::CheckImports code.
    if (SharedModuleInfo::ImportsModules(extension())) {
      const std::vector<SharedModuleInfo::ImportInfo>& imports =
          SharedModuleInfo::GetImports(extension());
      std::vector<SharedModuleInfo::ImportInfo>::const_iterator i;
      ExtensionRegistry* registry = ExtensionRegistry::Get(service->profile());
      for (i = imports.begin(); i != imports.end(); ++i) {
        base::Version version_required(i->minimum_version);
        const Extension* imported_module = registry->GetExtensionById(
            i->extension_id, ExtensionRegistry::EVERYTHING);
        if (!imported_module) {
          ReportExtensionLoadError(kImportMissing);
          return;
        } else if (imported_module &&
                   !SharedModuleInfo::IsSharedModule(imported_module)) {
          ReportExtensionLoadError(kImportNotSharedModule);
          return;
        } else if (imported_module && (version_required.IsValid() &&
                                       imported_module->version().CompareTo(
                                           version_required) < 0)) {
          ReportExtensionLoadError(kImportMinVersionNewer);
          return;
        }
      }
    }
  }

  policy_check_ = std::make_unique<PolicyCheck>(profile_, extension_);
  requirements_check_ = std::make_unique<RequirementsChecker>(extension_);

  check_group_ = std::make_unique<PreloadCheckGroup>();
  check_group_->set_stop_on_first_error(true);

  check_group_->AddCheck(policy_check_.get());
  check_group_->AddCheck(requirements_check_.get());
  check_group_->Start(
      base::BindOnce(&UnpackedInstaller::OnInstallChecksComplete, this));
}

void UnpackedInstaller::OnInstallChecksComplete(
    const PreloadCheck::Errors& errors) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (errors.empty()) {
    InstallExtension();
    return;
  }

  std::u16string error_message;
  if (errors.count(PreloadCheck::Error::kDisallowedByPolicy))
    error_message = policy_check_->GetErrorMessage();
  else
    error_message = requirements_check_->GetErrorMessage();

  DCHECK(!error_message.empty());
  ReportExtensionLoadError(base::UTF16ToUTF8(error_message));
}

int UnpackedInstaller::GetFlags() {
  std::string id = crx_file::id_util::GenerateIdForPath(extension_path_);
  bool allow_file_access =
      Manifest::ShouldAlwaysAllowFileAccess(mojom::ManifestLocation::kUnpacked);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(service_weak_->profile());
  if (allow_file_access_.has_value()) {
    allow_file_access = *allow_file_access_;
  } else if (prefs->HasAllowFileAccessSetting(id)) {
    allow_file_access = prefs->AllowFileAccess(id);
  }

  int result = Extension::FOLLOW_SYMLINKS_ANYWHERE;
  if (allow_file_access)
    result |= Extension::ALLOW_FILE_ACCESS;
  if (require_modern_manifest_version_)
    result |= Extension::REQUIRE_MODERN_MANIFEST_VERSION;

  if (base::FeatureList::IsEnabled(
          extensions_features::
              kAllowWithholdingExtensionPermissionsOnInstall)) {
    result |= Extension::WITHHOLD_PERMISSIONS;
  }

  return result;
}

bool UnpackedInstaller::LoadExtension(mojom::ManifestLocation location,
                                      int flags,
                                      std::string* error) {
  // Clean up the kMetadataFolder if necessary. This prevents spurious
  // warnings/errors and ensures we don't treat a user provided file as one by
  // the Extension system.
  MaybeCleanupMetadataFolder(extension_path_);

  // Treat presence of illegal filenames as a hard error for unpacked
  // extensions. Don't do so for command line extensions since this breaks
  // Chrome OS autotests (crbug.com/764787).
  if (location == mojom::ManifestLocation::kUnpacked &&
      !file_util::CheckForIllegalFilenames(extension_path_, error)) {
    return false;
  }

  extension_ =
      file_util::LoadExtension(extension_path_, location, flags, error);

  return extension() &&
         extension_l10n_util::ValidateExtensionLocales(
             extension_path_, *extension()->manifest()->value(), error) &&
         IndexAndPersistRulesIfNeeded(error);
}

bool UnpackedInstaller::IndexAndPersistRulesIfNeeded(std::string* error) {
  DCHECK(extension());

  base::expected<base::Value::Dict, std::string> index_result =
      declarative_net_request::InstallIndexHelper::
          IndexAndPersistRulesOnInstall(*extension_);

  if (!index_result.has_value()) {
    *error = std::move(index_result.error());
    return false;
  }

  ruleset_install_prefs_ = std::move(index_result.value());
  return true;
}

bool UnpackedInstaller::IsLoadingUnpackedAllowed() const {
  if (!service_weak_.get())
    return true;
  // If there is a "*" in the extension blocklist, then no extensions should be
  // allowed at all (except explicitly allowlisted extensions).
  return !ExtensionManagementFactory::GetForBrowserContext(
              service_weak_->profile())
              ->BlocklistedByDefault();
}

void UnpackedInstaller::GetAbsolutePath() {
  extension_path_ = base::MakeAbsoluteFilePath(extension_path_);

  // Set priority explicitly to avoid unwanted task priority inheritance.
  content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&UnpackedInstaller::CheckExtensionFileAccess, this));
}

void UnpackedInstaller::CheckExtensionFileAccess() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_weak_.get())
    return;

  if (!IsLoadingUnpackedAllowed()) {
    ReportExtensionLoadError(kUnpackedExtensionsBlocklistedError);
    return;
  }

  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&UnpackedInstaller::LoadWithFileAccess, this, GetFlags()));
}

void UnpackedInstaller::LoadWithFileAccess(int flags) {
  std::string error;
  if (!LoadExtension(mojom::ManifestLocation::kUnpacked, flags, &error)) {
    // Set priority explicitly to avoid unwanted task priority inheritance.
    content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
        ->PostTask(FROM_HERE,
                   base::BindOnce(&UnpackedInstaller::ReportExtensionLoadError,
                                  this, error));
    return;
  }

  // Set priority explicitly to avoid unwanted task priority inheritance.
  content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&UnpackedInstaller::StartInstallChecks, this));
}

void UnpackedInstaller::ReportExtensionLoadError(const std::string &error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (service_weak_.get()) {
    LoadErrorReporter::GetInstance()->ReportLoadError(
        extension_path_, error, service_weak_->profile(), be_noisy_on_failure_);
  }

  if (!callback_.is_null())
    std::move(callback_).Run(nullptr, extension_path_, error);
}

void UnpackedInstaller::InstallExtension() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!service_weak_.get()) {
    callback_.Reset();
    return;
  }

  // Force file access and/or incognito state and set install param if
  // requested.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(service_weak_->profile());
  if (allow_file_access_.has_value()) {
    prefs->SetAllowFileAccess(extension()->id(), *allow_file_access_);
  }
  if (allow_incognito_access_.has_value()) {
    prefs->SetIsIncognitoEnabled(extension()->id(), *allow_incognito_access_);
  }
  if (install_param_.has_value()) {
    SetInstallParam(prefs, extension()->id(), *install_param_);
  }

  PermissionsUpdater perms_updater(service_weak_->profile());
  perms_updater.InitializePermissions(extension());
  perms_updater.GrantActivePermissions(extension());

  service_weak_->OnExtensionInstalled(extension(), syncer::StringOrdinal(),
                                      kInstallFlagInstallImmediately,
                                      std::move(ruleset_install_prefs_));

  // Record metrics here since the registry would contain the extension by now.
  RecordCommandLineDeveloperModeMetrics();

  if (!callback_.is_null())
    std::move(callback_).Run(extension(), extension_path_, std::string());
}

void UnpackedInstaller::RecordCommandLineDeveloperModeMetrics() {
  if (!extension()->is_extension() ||
      extension()->location() != mojom::ManifestLocation::kCommandLine) {
    return;
  }

  bool dev_mode_enabled =
      GetCurrentDeveloperMode(util::GetBrowserContextId(profile_));

  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  if (extension_registry->enabled_extensions().Contains(extension()->id())) {
    if (dev_mode_enabled) {
      base::UmaHistogramCounts100(
          "Extensions.CommandLineWithDeveloperModeOn.Enabled", 1);
    } else {
      base::UmaHistogramCounts100(
          "Extensions.CommandLineWithDeveloperModeOff.Enabled", 1);
    }
  }

  if (extension_registry->disabled_extensions().Contains(extension()->id())) {
    if (dev_mode_enabled) {
      base::UmaHistogramCounts100(
          "Extensions.CommandLineWithDeveloperModeOn.Disabled", 1);
    } else {
      base::UmaHistogramCounts100(
          "Extensions.CommandLineWithDeveloperModeOff.Disabled", 1);
    }
  }
}

}  // namespace extensions
