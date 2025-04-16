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
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "components/crx_file/id_util.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/declarative_net_request/install_index_helper.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
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

}  // namespace

// static
scoped_refptr<UnpackedInstaller> UnpackedInstaller::Create(
    content::BrowserContext* context) {
  CHECK(context);
  return scoped_refptr<UnpackedInstaller>(new UnpackedInstaller(context));
}

UnpackedInstaller::UnpackedInstaller(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      require_modern_manifest_version_(true),
      be_noisy_on_failure_(true) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  profile_observation_.Observe(profile_);

  // Observe for browser shutdown. Unretained is safe because the callback
  // subscription is owned by this object.
  on_browser_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
          &UnpackedInstaller::OnBrowserTerminating, base::Unretained(this)));
}

UnpackedInstaller::~UnpackedInstaller() = default;

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

  if (!profile_) {
    return false;
  }
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
  PermissionsUpdater(profile_, PermissionsUpdater::INIT_FLAG_TRANSIENT)
      .InitializePermissions(extension());
  StartInstallChecks();

  *extension_id = extension()->id();
  return true;
}

void UnpackedInstaller::StartInstallChecks() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!profile_) {
    return;
  }

  // TODO(crbug.com/40388034): Enable these checks all the time.  The reason
  // they are disabled for extensions loaded from the command-line is that
  // installing unpacked extensions is asynchronous, but there can be
  // dependencies between the extensions loaded by the command line.
  if (extension()->manifest()->location() !=
      mojom::ManifestLocation::kCommandLine) {
    if (browser_terminating_) {
      return;
    }

    // TODO(crbug.com/40387578): Move this code to a utility class to avoid
    // duplication of SharedModuleService::CheckImports code.
    if (SharedModuleInfo::ImportsModules(extension())) {
      const std::vector<SharedModuleInfo::ImportInfo>& imports =
          SharedModuleInfo::GetImports(extension());
      std::vector<SharedModuleInfo::ImportInfo>::const_iterator i;
      ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
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
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
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
  file_util::MaybeCleanupMetadataFolder(extension_path_);

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
  if (!profile_) {
    return true;
  }
  // If there is a "*" in the extension blocklist, then no extensions should be
  // allowed at all (except explicitly allowlisted extensions).
  return !ExtensionManagementFactory::GetForBrowserContext(profile_)
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
  if (!profile_) {
    return;
  }

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

  if (profile_) {
    LoadErrorReporter::GetInstance()->ReportLoadError(
        extension_path_, error, profile_, be_noisy_on_failure_);
  }

  if (!callback_.is_null())
    std::move(callback_).Run(nullptr, extension_path_, error);
}

void UnpackedInstaller::InstallExtension() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!profile_) {
    callback_.Reset();
    return;
  }

  // Force file access and/or incognito state and set install param if
  // requested.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile_);
  if (allow_file_access_.has_value()) {
    prefs->SetAllowFileAccess(extension()->id(), *allow_file_access_);
  }
  if (allow_incognito_access_.has_value()) {
    prefs->SetIsIncognitoEnabled(extension()->id(), *allow_incognito_access_);
  }
  if (install_param_.has_value()) {
    SetInstallParam(prefs, extension()->id(), *install_param_);
  }

  PermissionsUpdater perms_updater(profile_);
  perms_updater.InitializePermissions(extension());
  perms_updater.GrantActivePermissions(extension());

  ExtensionRegistrar::Get(profile_)->OnExtensionInstalled(
      extension(), syncer::StringOrdinal(), kInstallFlagInstallImmediately,
      std::move(ruleset_install_prefs_));

  // Record metrics here since the registry would contain the extension by now.
  RecordCommandLineMetrics();

  if (!callback_.is_null())
    std::move(callback_).Run(extension(), extension_path_, std::string());
}

void UnpackedInstaller::RecordCommandLineMetrics() {
  if (!extension()->is_extension() ||
      extension()->location() != mojom::ManifestLocation::kCommandLine) {
    return;
  }

  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  if (!extension_registry->GetInstalledExtension(extension()->id())) {
    return;
  }

  // Manifest settings override metrics.
  base::UmaHistogramCounts100("Extensions.CommandLineInstalled", 1);

  bool new_tab_page_set =
      URLOverrides::GetChromeURLOverrides(extension()).count("newtab");
  bool default_search_engine_set = false;
  // SettingsOverrides are only available on Windows and macOS.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  const SettingsOverrides* settings = SettingsOverrides::Get(extension());
  default_search_engine_set = settings && settings->search_engine &&
                              settings->search_engine->is_default;
#endif

  if (new_tab_page_set && default_search_engine_set) {
    base::UmaHistogramEnumeration(
        "Extensions.CommandLineManifestSettingsOverride",
        kSearchEngineAndNewTabPage);
  } else if (new_tab_page_set) {
    base::UmaHistogramEnumeration(
        "Extensions.CommandLineManifestSettingsOverride", kNewTabPage);
  } else if (default_search_engine_set) {
    base::UmaHistogramEnumeration(
        "Extensions.CommandLineManifestSettingsOverride", kSearchEngine);
  } else {
    base::UmaHistogramEnumeration(
        "Extensions.CommandLineManifestSettingsOverride", kNoOverride);
  }

  // Developer mode metrics.
  bool dev_mode_enabled =
      GetCurrentDeveloperMode(util::GetBrowserContextId(profile_));

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

void UnpackedInstaller::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();
  profile_ = nullptr;
}

void UnpackedInstaller::OnBrowserTerminating() {
  browser_terminating_ = true;
}

}  // namespace extensions
