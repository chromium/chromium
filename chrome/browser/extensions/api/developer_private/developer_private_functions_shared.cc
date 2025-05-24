// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_functions_shared.h"

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_event_router.h"
#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/browser/extensions/api/developer_private/profile_info_generator.h"
#include "chrome/browser/extensions/chrome_zipfile_installer.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/extensions/webstore_reinstaller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/common/drop_data.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/file_highlighter.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/path_util.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/ui_util.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "extensions/browser/ui_util.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace extensions {

namespace developer = api::developer_private;

namespace {
const char kCannotUpdateChildAccountProfileSettingsError[] =
    "Cannot change settings for a child account profile.";

ui::FileInfo* g_drop_file_for_testing = nullptr;

std::string ReadFileToString(const base::FilePath& path) {
  std::string data;
  // This call can fail, but it doesn't matter for our purposes. If it fails,
  // we simply return an empty string for the manifest, and ignore it.
  std::ignore = base::ReadFileToString(path, &data);
  return data;
}

std::optional<URLPattern> ParseRuntimePermissionsPattern(
    const std::string& pattern_str) {
  constexpr int kValidRuntimePermissionSchemes = URLPattern::SCHEME_HTTP |
                                                 URLPattern::SCHEME_HTTPS |
                                                 URLPattern::SCHEME_FILE;

  URLPattern pattern(kValidRuntimePermissionSchemes);
  if (pattern.Parse(pattern_str) != URLPattern::ParseResult::kSuccess) {
    return std::nullopt;
  }

  // We don't allow adding paths for permissions, because they aren't meaningful
  // in terms of origin access. The frontend should validate this, but there's
  // a chance something can slip through, so we should fail gracefully.
  if (pattern.path() != "/*") {
    return std::nullopt;
  }

  return pattern;
}

GURL ConvertHostToUrl(const std::string& host) {
  return GURL(base::StrCat(
      {url::kHttpScheme, url::kStandardSchemeSeparator, host, "/"}));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Runs the install verifier for all extensions that are enabled, disabled, or
// terminated.
void PerformVerificationCheck(content::BrowserContext* context) {
  const ExtensionSet extensions =
      ExtensionRegistry::Get(context)->GenerateInstalledExtensionsSet(
          ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
          ExtensionRegistry::TERMINATED);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  bool should_do_verification_check = false;
  for (const scoped_refptr<const Extension>& extension : extensions) {
    if (ui_util::ShouldDisplayInExtensionSettings(*extension) &&
        prefs->HasDisableReason(extension->id(),
                                disable_reason::DISABLE_NOT_VERIFIED)) {
      should_do_verification_check = true;
      break;
    }
  }

  if (should_do_verification_check) {
    InstallVerifier::Get(context)->VerifyAllExtensions();
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

std::string GetETldPlusOne(const GURL& site) {
  DCHECK(site.is_valid());
  std::string etld_plus_one =
      net::registry_controlled_domains::GetDomainAndRegistry(
          site, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return etld_plus_one.empty() ? site.host() : etld_plus_one;
}

developer::SiteInfo CreateSiteInfo(const std::string& site,
                                   developer::SiteSet site_set,
                                   size_t num_extensions = 0u) {
  developer::SiteInfo site_info;
  site_info.site = site;
  site_info.site_set = site_set;
  site_info.num_extensions = num_extensions;
  return site_info;
}

// Adds `site` grouped under `etld_plus_one` into `site_groups`. This function
// is a no-op if `site` already exists inside the SiteGroup for `etld_plus_one`.
void AddSiteToSiteGroups(
    std::map<std::string, developer::SiteGroup>* site_groups,
    const std::string& site,
    const std::string& etld_plus_one,
    developer::SiteSet site_set) {
  auto [it, inserted] = site_groups->try_emplace(etld_plus_one);
  if (inserted) {
    it->second.etld_plus_one = etld_plus_one;
    it->second.sites.push_back(CreateSiteInfo(site, site_set));
  } else if (!base::Contains(it->second.sites, site,
                             &developer::SiteInfo::site)) {
    it->second.sites.push_back(CreateSiteInfo(site, site_set));
  }
}

// Adds an extension's granted host permissions in `distinct_hosts` to
// `site_groups`,
void ProcessSitesForRuntimeHostPermissions(
    std::map<std::string, developer::SiteGroup>* site_groups,
    const std::vector<URLPattern>& distinct_hosts) {
  for (const auto& pattern : distinct_hosts) {
    // Do not add the pattern if it matches an overly broad set of urls (all
    // urls under one or all top level domains).
    if (pattern.match_all_urls() || pattern.host().empty() ||
        pattern.MatchesEffectiveTld()) {
      continue;
    }

    std::string etld_plus_one =
        GetETldPlusOne(ConvertHostToUrl(pattern.host()));
    // Process the site if:
    // 1) It does not match any subdomains, or:
    // 2) It matches subdomains but the host portion does not equal
    //    `etld_plus_one`. This treats patterns such as "*.sub.etldplusone.com"
    //    as just "sub.etldplusone.com" and prevents "*.etldplusone.com" from
    //    being processed as "etldplusone.com", since such patterns will be
    //    processed separately.
    if (!pattern.match_subdomains() || pattern.host() != etld_plus_one) {
      AddSiteToSiteGroups(site_groups, pattern.host(), etld_plus_one,
                          developer::SiteSet::kExtensionSpecified);
    }
  }
}

// Updates num_extensions counts in `site_groups` for `granted_hosts` from one
// extension.
void UpdateSiteGroupCountsForExtensionHosts(
    std::map<std::string, developer::SiteGroup>* site_groups,
    std::map<std::string, size_t>* match_subdomains_count,
    const URLPatternSet& granted_hosts) {
  for (auto& entry : *site_groups) {
    bool can_run_on_site_group = false;
    // For each site under the eTLD+1, increment num_extensions if the extension
    // can access the site.
    for (developer::SiteInfo& site_info : entry.second.sites) {
      // When updating num_extensions counts, only look at extension specified
      // hosts as num_extensions is not useful for user specified hosts. (i.e.
      // user permitted sites can be accessed to any extensions that specify the
      // site in their host permissions, user restricted sites cannot be
      // accessed by any extensions.)
      if (site_info.site_set != developer::SiteSet::kExtensionSpecified) {
        continue;
      }

      if (granted_hosts.MatchesHost(ConvertHostToUrl(site_info.site),
                                    /*require_match_subdomains=*/false)) {
        can_run_on_site_group = true;
        site_info.num_extensions++;
      }
    }

    // Check if the extension can run on all sites under this eTLD+1 and
    // update `match_subdomains_count` for this eTLD+1. The SiteInfo entry will
    // be created later if at least one extension can run on all subdomains.
    if (granted_hosts.MatchesHost(ConvertHostToUrl(entry.first),
                                  /*require_match_subdomains=*/true)) {
      (*match_subdomains_count)[entry.first]++;
      can_run_on_site_group = true;
    }

    if (can_run_on_site_group) {
      entry.second.num_extensions++;
    }
  }
}

// Sets the dropped path to DeveloperPrivateAPI.
base::expected<void, std::string> SetDroppedPath(
    content::WebContents* web_contents,
    content::BrowserContext* context) {
  const ui::FileInfo* file_info = nullptr;
  if (g_drop_file_for_testing) {
    file_info = g_drop_file_for_testing;
  } else {
    content::DropData* drop_data = web_contents->GetDropData();
    if (!drop_data) {
      return base::unexpected{"No current drop data."};
    }

    if (drop_data->filenames.empty()) {
      return base::unexpected{"No files being dropped."};
    }

    file_info = &drop_data->filenames.front();
  }

  DCHECK(file_info);
  // Note(devlin): we don't do further validation that the file is a directory
  // here. This is validated in the JS, but if that fails, then trying to load
  // the file as an unpacked extension will also fail (reasonably gracefully).

  DeveloperPrivateAPI::Get(context)->SetDraggedFile(web_contents, *file_info);
  return base::ok();
}

// Returns whether the filename perceived to the user ends with the extension.
// Just using `path` is not enough because it might be a content URI that
// doesn't have the `display_name` as a suffix.
bool MatchesExtension(ui::FileInfo& file_info,
                      base::FilePath::StringViewType extension) {
  return file_info.display_name.empty()
             ? file_info.path.MatchesExtension(extension)
             : file_info.display_name.MatchesExtension(extension);
}

}  // namespace

namespace api {

DeveloperPrivateAPIFunction::~DeveloperPrivateAPIFunction() = default;

const Extension* DeveloperPrivateAPIFunction::GetExtensionById(
    const ExtensionId& id) {
  return ExtensionRegistry::Get(browser_context())
      ->GetExtensionById(id, ExtensionRegistry::EVERYTHING);
}

const Extension* DeveloperPrivateAPIFunction::GetEnabledExtensionById(
    const ExtensionId& id) {
  return ExtensionRegistry::Get(browser_context())
      ->enabled_extensions()
      .GetByID(id);
}

DeveloperPrivateAutoUpdateFunction::~DeveloperPrivateAutoUpdateFunction() =
    default;

ExtensionFunction::ResponseAction DeveloperPrivateAutoUpdateFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  ExtensionUpdater* updater = ExtensionUpdater::Get(profile);
  if (updater->enabled()) {
    ExtensionUpdater::CheckParams params;
    params.fetch_priority = DownloadFetchPriority::kForeground;
    params.install_immediately = true;
    params.callback =
        base::BindOnce(&DeveloperPrivateAutoUpdateFunction::OnComplete, this);
    updater->CheckNow(std::move(params));
  }
  return RespondLater();
}

void DeveloperPrivateAutoUpdateFunction::OnComplete() {
  Respond(NoArguments());
}

DeveloperPrivateGetExtensionsInfoFunction::
    DeveloperPrivateGetExtensionsInfoFunction() = default;

DeveloperPrivateGetExtensionsInfoFunction::
    ~DeveloperPrivateGetExtensionsInfoFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetExtensionsInfoFunction::Run() {
  std::optional<developer::GetExtensionsInfo::Params> params =
      developer::GetExtensionsInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool include_disabled = true;
  bool include_terminated = true;
  if (params->options) {
    if (params->options->include_disabled) {
      include_disabled = *params->options->include_disabled;
    }
    if (params->options->include_terminated) {
      include_terminated = *params->options->include_terminated;
    }
  }

  info_generator_ = std::make_unique<ExtensionInfoGenerator>(browser_context());
  info_generator_->CreateExtensionsInfo(
      include_disabled, include_terminated,
      base::BindOnce(
          &DeveloperPrivateGetExtensionsInfoFunction::OnInfosGenerated, this));

  return RespondLater();
}

void DeveloperPrivateGetExtensionsInfoFunction::OnInfosGenerated(
    ExtensionInfoGenerator::ExtensionInfoList list) {
  Respond(ArgumentList(developer::GetExtensionsInfo::Results::Create(list)));
}

DeveloperPrivateGetExtensionInfoFunction::
    DeveloperPrivateGetExtensionInfoFunction() = default;

DeveloperPrivateGetExtensionInfoFunction::
    ~DeveloperPrivateGetExtensionInfoFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetExtensionInfoFunction::Run() {
  std::optional<developer::GetExtensionInfo::Params> params =
      developer::GetExtensionInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  info_generator_ = std::make_unique<ExtensionInfoGenerator>(browser_context());
  info_generator_->CreateExtensionInfo(
      params->id,
      base::BindOnce(
          &DeveloperPrivateGetExtensionInfoFunction::OnInfosGenerated, this));

  return RespondLater();
}

void DeveloperPrivateGetExtensionInfoFunction::OnInfosGenerated(
    ExtensionInfoGenerator::ExtensionInfoList list) {
  DCHECK_LE(1u, list.size());
  Respond(list.empty() ? Error(kNoSuchExtensionError)
                       : WithArguments(list[0].ToValue()));
}

DeveloperPrivateGetExtensionSizeFunction::
    DeveloperPrivateGetExtensionSizeFunction() = default;

DeveloperPrivateGetExtensionSizeFunction::
    ~DeveloperPrivateGetExtensionSizeFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetExtensionSizeFunction::Run() {
  std::optional<developer::GetExtensionSize::Params> params =
      developer::GetExtensionSize::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const Extension* extension = GetExtensionById(params->id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  extensions::path_util::CalculateAndFormatExtensionDirectorySize(
      extension->path(), IDS_APPLICATION_INFO_SIZE_SMALL_LABEL,
      base::BindOnce(
          &DeveloperPrivateGetExtensionSizeFunction::OnSizeCalculated, this));

  return RespondLater();
}

void DeveloperPrivateGetExtensionSizeFunction::OnSizeCalculated(
    const std::u16string& size) {
  Respond(WithArguments(size));
}

DeveloperPrivateGetProfileConfigurationFunction::
    ~DeveloperPrivateGetProfileConfigurationFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetProfileConfigurationFunction::Run() {
  developer::ProfileInfo info =
      CreateProfileInfo(Profile::FromBrowserContext(browser_context()));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If this is called from the chrome://extensions page, we use this as a
  // heuristic that it's a good time to verify installs. We do this on startup,
  // but there's a chance that it failed erroneously, so it's good to double-
  // check.
  if (source_context_type() == mojom::ContextType::kWebUi) {
    PerformVerificationCheck(browser_context());
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return RespondNow(WithArguments(info.ToValue()));
}

DeveloperPrivateUpdateProfileConfigurationFunction::
    ~DeveloperPrivateUpdateProfileConfigurationFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateUpdateProfileConfigurationFunction::Run() {
  std::optional<developer::UpdateProfileConfiguration::Params> params =
      developer::UpdateProfileConfiguration::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const developer::ProfileConfigurationUpdate& update = params->update;

  if (update.in_developer_mode) {
    Profile* profile = Profile::FromBrowserContext(browser_context());
    CHECK(profile);
    if (supervised_user::AreExtensionsPermissionsEnabled(profile)) {
      return RespondNow(Error(kCannotUpdateChildAccountProfileSettingsError));
    }
    util::SetDeveloperModeForProfile(profile, *update.in_developer_mode);
  }

// Consider the deprecation notice already dismissed on Android.
#if !BUILDFLAG(IS_ANDROID)
  if (update.is_mv2_deprecation_notice_dismissed.value_or(false)) {
    ManifestV2ExperimentManager::Get(browser_context())
        ->MarkNoticeAsAcknowledgedGlobally();
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return RespondNow(NoArguments());
}

DeveloperPrivateUpdateExtensionConfigurationFunction::
    ~DeveloperPrivateUpdateExtensionConfigurationFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateUpdateExtensionConfigurationFunction::Run() {
  std::optional<developer::UpdateExtensionConfiguration::Params> params =
      developer::UpdateExtensionConfiguration::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const developer::ExtensionConfigurationUpdate& update = params->update;

  const Extension* extension = GetExtensionById(update.extension_id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  // The chrome://extensions page uses toggles which, when dragged, do not
  // invoke a user gesture. Work around this for the chrome://extensions page.
  // TODO(dpapad): Remove this exemption when sliding a toggle counts as a
  // gesture.
  bool allowed =
      source_context_type() == mojom::ContextType::kWebUi || user_gesture();
  if (!allowed) {
    return RespondNow(Error(kRequiresUserGestureError));
  }

  if (update.file_access) {
    util::SetAllowFileAccess(extension->id(), browser_context(),
                             *update.file_access);
  }
  if (update.incognito_access) {
    util::SetIsIncognitoEnabled(extension->id(), browser_context(),
                                *update.incognito_access);
  }
  if (update.user_scripts_access) {
    ExtensionSystem::Get(browser_context())
        ->user_script_manager()
        ->SetUserScriptPrefEnabled(extension->id(),
                                   *update.user_scripts_access);
  }
  if (update.error_collection) {
    ErrorConsole::Get(browser_context())
        ->SetReportingAllForExtension(extension->id(),
                                      *update.error_collection);
  }
  if (update.host_access != developer::HostAccess::kNone) {
    PermissionsManager* manager = PermissionsManager::Get(browser_context());
    if (!manager->CanAffectExtension(*extension)) {
      return RespondNow(Error(kCannotChangeHostPermissions));
    }

    ScriptingPermissionsModifier modifier(browser_context(), extension);
    switch (update.host_access) {
      case developer::HostAccess::kOnClick:
        modifier.SetWithholdHostPermissions(true);
        modifier.RemoveAllGrantedHostPermissions();
        break;
      case developer::HostAccess::kOnSpecificSites:
        if (manager->HasBroadGrantedHostPermissions(*extension)) {
          modifier.RemoveBroadGrantedHostPermissions();
        }
        modifier.SetWithholdHostPermissions(true);
        break;
      case developer::HostAccess::kOnAllSites:
        modifier.SetWithholdHostPermissions(false);
        break;
      case developer::HostAccess::kNone:
        NOTREACHED();
    }
  }
  if (update.acknowledge_safety_check_warning_reason !=
      developer_private::SafetyCheckWarningReason::kNone) {
    ExtensionPrefs::Get(browser_context())
        ->SetIntegerPref(
            extension->id(), kPrefAcknowledgeSafetyCheckWarningReason,
            static_cast<int>(update.acknowledge_safety_check_warning_reason));
    DeveloperPrivateEventRouter* event_router =
        DeveloperPrivateAPI::Get(browser_context())
            ->developer_private_event_router();
    if (event_router) {
      event_router->OnExtensionConfigurationChanged(extension->id());
    }
  }
// TODO(crbug.com/392777363): Enable this code when toolbars are supported on
// desktop Android.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (update.show_access_requests_in_toolbar) {
    SitePermissionsHelper(Profile::FromBrowserContext(browser_context()))
        .SetShowAccessRequestsInToolbar(
            extension->id(), *update.show_access_requests_in_toolbar);
  }
  if (update.pinned_to_toolbar) {
    ToolbarActionsModel* toolbar_actions_model = ToolbarActionsModel::Get(
        Profile::FromBrowserContext(browser_context()));
    if (!toolbar_actions_model->HasAction(extension->id())) {
      return RespondNow(Error(kCannotSetPinnedWithoutAction));
    }

    bool is_action_pinned =
        toolbar_actions_model->IsActionPinned(extension->id());
    if (is_action_pinned != *update.pinned_to_toolbar) {
      toolbar_actions_model->SetActionVisibility(extension->id(),
                                                 !is_action_pinned);
    }
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return RespondNow(NoArguments());
}

DeveloperPrivateInstallDroppedFileFunction::
    DeveloperPrivateInstallDroppedFileFunction() = default;
DeveloperPrivateInstallDroppedFileFunction::
    ~DeveloperPrivateInstallDroppedFileFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateInstallDroppedFileFunction::Run() {
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(Error(kCouldNotFindWebContentsError));
  }

#if BUILDFLAG(IS_ANDROID)
  base::expected<void, std::string> result =
      SetDroppedPath(web_contents, browser_context());
  if (!result.has_value()) {
    return RespondNow(Error(result.error()));
  }
#endif  // BUILDFLAG(IS_ANDROID)

  DeveloperPrivateAPI* api = DeveloperPrivateAPI::Get(browser_context());
  ui::FileInfo file = api->GetDraggedFile(web_contents);
  if (file.path.empty()) {
    return RespondNow(Error("No dragged path"));
  }

  if (MatchesExtension(file, FILE_PATH_LITERAL(".zip"))) {
    ExtensionRegistrar* registrar = ExtensionRegistrar::Get(browser_context());
    ZipFileInstaller::Create(
        GetExtensionFileTaskRunner(),
        MakeRegisterInExtensionServiceCallback(browser_context()))
        ->InstallZipFileToUnpackedExtensionsDir(
            file.path, registrar->unpacked_install_directory());
  } else {
    auto prompt = std::make_unique<ExtensionInstallPrompt>(web_contents);
    scoped_refptr<CrxInstaller> crx_installer =
        CrxInstaller::Create(browser_context(), std::move(prompt));
    crx_installer->set_error_on_unsupported_requirements(true);
    crx_installer->set_off_store_install_allow_reason(
        CrxInstaller::OffStoreInstallAllowedFromSettingsPage);
    crx_installer->set_install_immediately(true);

    if (MatchesExtension(file, FILE_PATH_LITERAL(".user.js"))) {
      crx_installer->InstallUserScript(file.path,
                                       net::FilePathToFileURL(file.path));
    } else if (MatchesExtension(file, FILE_PATH_LITERAL(".crx"))) {
      crx_installer->InstallCrx(file.path);
    } else {
      EXTENSION_FUNCTION_VALIDATE(false);
    }
  }

  // TODO(devlin): We could optionally wait to return until we validate whether
  // the load succeeded or failed. For now, that's unnecessary, and just adds
  // complexity.
  return RespondNow(NoArguments());
}

DeveloperPrivateNotifyDragInstallInProgressFunction::
    DeveloperPrivateNotifyDragInstallInProgressFunction() = default;
DeveloperPrivateNotifyDragInstallInProgressFunction::
    ~DeveloperPrivateNotifyDragInstallInProgressFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateNotifyDragInstallInProgressFunction::Run() {
// Drop data is available only on drop on android.
#if !BUILDFLAG(IS_ANDROID)
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(Error(kCouldNotFindWebContentsError));
  }

  const base::expected<void, std::string> result =
      SetDroppedPath(web_contents, browser_context());
  if (!result.has_value()) {
    return RespondNow(Error(result.error()));
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  return RespondNow(NoArguments());
}

// static
void DeveloperPrivateNotifyDragInstallInProgressFunction::SetDropFileForTesting(
    ui::FileInfo* file_info) {
  g_drop_file_for_testing = file_info;
}

DeveloperPrivateDeleteExtensionErrorsFunction::
    ~DeveloperPrivateDeleteExtensionErrorsFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateDeleteExtensionErrorsFunction::Run() {
  std::optional<developer::DeleteExtensionErrors::Params> params =
      developer::DeleteExtensionErrors::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const developer::DeleteExtensionErrorsProperties& properties =
      params->properties;

  ErrorConsole* error_console = ErrorConsole::Get(browser_context());
  int type = -1;
  if (properties.type != developer::ErrorType::kNone) {
    type = properties.type == developer::ErrorType::kManifest
               ? static_cast<int>(ExtensionError::Type::kManifestError)
               : static_cast<int>(ExtensionError::Type::kRuntimeError);
  }
  std::set<int> error_ids;
  if (properties.error_ids) {
    error_ids.insert(properties.error_ids->begin(),
                     properties.error_ids->end());
  }
  error_console->RemoveErrors(
      ErrorMap::Filter(properties.extension_id, type, error_ids, false));

  return RespondNow(NoArguments());
}

DeveloperPrivateUpdateExtensionCommandFunction::
    ~DeveloperPrivateUpdateExtensionCommandFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateUpdateExtensionCommandFunction::Run() {
  std::optional<developer::UpdateExtensionCommand::Params> params =
      developer::UpdateExtensionCommand::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const developer::ExtensionCommandUpdate& update = params->update;

  CommandService* command_service = CommandService::Get(browser_context());

  if (update.scope != developer::CommandScope::kNone) {
    command_service->SetScope(update.extension_id, update.command_name,
                              update.scope == developer::CommandScope::kGlobal);
  }

  if (update.keybinding) {
    command_service->UpdateKeybindingPrefs(
        update.extension_id, update.command_name, *update.keybinding);
  }

  return RespondNow(NoArguments());
}

DeveloperPrivateAddHostPermissionFunction::
    DeveloperPrivateAddHostPermissionFunction() = default;
DeveloperPrivateAddHostPermissionFunction::
    ~DeveloperPrivateAddHostPermissionFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateAddHostPermissionFunction::Run() {
  std::optional<developer::AddHostPermission::Params> params =
      developer::AddHostPermission::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::optional<URLPattern> pattern =
      ParseRuntimePermissionsPattern(params->host);
  if (!pattern) {
    return RespondNow(Error(kInvalidHost));
  }

  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  if (!PermissionsManager::Get(browser_context())
           ->CanAffectExtension(*extension)) {
    return RespondNow(Error(kCannotChangeHostPermissions));
  }

  URLPatternSet new_host_permissions({*pattern});
  PermissionsUpdater(browser_context())
      .GrantRuntimePermissions(
          *extension,
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        new_host_permissions.Clone(),
                        new_host_permissions.Clone()),
          base::BindOnce(&DeveloperPrivateAddHostPermissionFunction::
                             OnRuntimePermissionsGranted,
                         this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DeveloperPrivateAddHostPermissionFunction::OnRuntimePermissionsGranted() {
  Respond(NoArguments());
}

DeveloperPrivateRemoveHostPermissionFunction::
    DeveloperPrivateRemoveHostPermissionFunction() = default;
DeveloperPrivateRemoveHostPermissionFunction::
    ~DeveloperPrivateRemoveHostPermissionFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateRemoveHostPermissionFunction::Run() {
  std::optional<developer::RemoveHostPermission::Params> params =
      developer::RemoveHostPermission::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::optional<URLPattern> pattern =
      ParseRuntimePermissionsPattern(params->host);
  if (!pattern) {
    return RespondNow(Error(kInvalidHost));
  }

  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  if (!manager->CanAffectExtension(*extension)) {
    return RespondNow(Error(kCannotChangeHostPermissions));
  }

  URLPatternSet host_permissions_to_remove({*pattern});
  std::unique_ptr<const PermissionSet> permissions_to_remove =
      PermissionSet::CreateIntersection(
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        host_permissions_to_remove.Clone(),
                        host_permissions_to_remove.Clone()),
          *manager->GetRevokablePermissions(*extension),
          URLPatternSet::IntersectionBehavior::kDetailed);
  if (permissions_to_remove->IsEmpty()) {
    return RespondNow(Error("Cannot remove a host that hasn't been granted."));
  }

  PermissionsUpdater(browser_context())
      .RevokeRuntimePermissions(
          *extension, *permissions_to_remove,
          base::BindOnce(&DeveloperPrivateRemoveHostPermissionFunction::
                             OnRuntimePermissionsRevoked,
                         this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DeveloperPrivateRemoveHostPermissionFunction::
    OnRuntimePermissionsRevoked() {
  Respond(NoArguments());
}

DeveloperPrivateGetUserSiteSettingsFunction::
    DeveloperPrivateGetUserSiteSettingsFunction() = default;
DeveloperPrivateGetUserSiteSettingsFunction::
    ~DeveloperPrivateGetUserSiteSettingsFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetUserSiteSettingsFunction::Run() {
  developer::UserSiteSettings user_site_settings =
      DeveloperPrivateEventRouter::ConvertToUserSiteSettings(
          PermissionsManager::Get(browser_context())
              ->GetUserPermissionsSettings());

  return RespondNow(WithArguments(user_site_settings.ToValue()));
}

DeveloperPrivateAddUserSpecifiedSitesFunction::
    DeveloperPrivateAddUserSpecifiedSitesFunction() = default;
DeveloperPrivateAddUserSpecifiedSitesFunction::
    ~DeveloperPrivateAddUserSpecifiedSitesFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateAddUserSpecifiedSitesFunction::Run() {
  std::optional<developer::AddUserSpecifiedSites::Params> params =
      developer::AddUserSpecifiedSites::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::set<url::Origin> origins;
  for (const auto& host : params->options.hosts) {
    GURL url(host);
    if (!url.is_valid()) {
      return RespondNow(Error("Invalid host: " + host));
    }
    origins.insert(url::Origin::Create(url));
  }

  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  switch (params->options.site_set) {
    case developer::SiteSet::kUserPermitted:
      for (const auto& origin : origins) {
        manager->AddUserPermittedSite(origin);
      }
      break;
    case developer::SiteSet::kUserRestricted:
      for (const auto& origin : origins) {
        manager->AddUserRestrictedSite(origin);
      }
      break;
    case developer::SiteSet::kExtensionSpecified:
      return RespondNow(
          Error("Site set must be USER_PERMITTED or USER_RESTRICTED"));
    case developer::SiteSet::kNone:
      NOTREACHED();
  }

  return RespondNow(NoArguments());
}

DeveloperPrivateRemoveUserSpecifiedSitesFunction::
    DeveloperPrivateRemoveUserSpecifiedSitesFunction() = default;
DeveloperPrivateRemoveUserSpecifiedSitesFunction::
    ~DeveloperPrivateRemoveUserSpecifiedSitesFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateRemoveUserSpecifiedSitesFunction::Run() {
  std::optional<developer::RemoveUserSpecifiedSites::Params> params =
      developer::RemoveUserSpecifiedSites::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::set<url::Origin> origins;
  for (const auto& host : params->options.hosts) {
    GURL url(host);
    if (!url.is_valid()) {
      return RespondNow(Error("Invalid host: " + host));
    }
    origins.insert(url::Origin::Create(url));
  }

  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  switch (params->options.site_set) {
    case developer::SiteSet::kUserPermitted:
      for (const auto& origin : origins) {
        manager->RemoveUserPermittedSite(origin);
      }
      break;
    case developer::SiteSet::kUserRestricted:
      for (const auto& origin : origins) {
        manager->RemoveUserRestrictedSite(origin);
      }
      break;
    case developer::SiteSet::kExtensionSpecified:
      return RespondNow(
          Error("Site set must be USER_PERMITTED or USER_RESTRICTED"));
    case developer::SiteSet::kNone:
      NOTREACHED();
  }

  return RespondNow(NoArguments());
}

DeveloperPrivateGetUserAndExtensionSitesByEtldFunction::
    DeveloperPrivateGetUserAndExtensionSitesByEtldFunction() = default;
DeveloperPrivateGetUserAndExtensionSitesByEtldFunction::
    ~DeveloperPrivateGetUserAndExtensionSitesByEtldFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetUserAndExtensionSitesByEtldFunction::Run() {
  std::map<std::string, developer::SiteGroup> site_groups;
  const PermissionsManager::UserPermissionsSettings& settings =
      PermissionsManager::Get(browser_context())->GetUserPermissionsSettings();
  for (const url::Origin& site : settings.permitted_sites) {
    AddSiteToSiteGroups(&site_groups, site.host(),
                        GetETldPlusOne(site.GetURL()),
                        developer::SiteSet::kUserPermitted);
  }

  for (const url::Origin& site : settings.restricted_sites) {
    AddSiteToSiteGroups(&site_groups, site.host(),
                        GetETldPlusOne(site.GetURL()),
                        developer::SiteSet::kUserRestricted);
  }

  std::vector<scoped_refptr<const Extension>> extensions_to_check;
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser_context());

  // Note: we are only counting enabled extensions as the returned extension
  // counts will reflect how many extensions can actually run on each site at
  // the current moment.
  for (const auto& extension : registry->enabled_extensions()) {
    if (!ui_util::ShouldDisplayInExtensionSettings(*extension)) {
      continue;
    }

    std::unique_ptr<const PermissionSet> granted_permissions =
        permissions_manager->GetExtensionGrantedPermissions(*extension);
    std::vector<URLPattern> distinct_hosts =
        ExtensionInfoGenerator::GetDistinctHosts(
            granted_permissions->effective_hosts());

    ProcessSitesForRuntimeHostPermissions(&site_groups, distinct_hosts);
    extensions_to_check.push_back(extension);
  }

  // Maps an eTLD+1 to the number of extensions that can run on all subdomains
  // of that eTLD+1.
  std::map<std::string, size_t> match_subdomains_count;

  // Iterate over `site_groups` again and count the number of extensions that
  // can run on each site. This is in a separate loop as `site_groups` needs to
  // be fully populated before these checks can be made, so the num_extensions
  // counts are accurate.
  for (const auto& extension : extensions_to_check) {
    std::unique_ptr<const PermissionSet> granted_permissions =
        permissions_manager->GetExtensionGrantedPermissions(*extension);
    UpdateSiteGroupCountsForExtensionHosts(
        &site_groups, &match_subdomains_count,
        granted_permissions->effective_hosts());
  }

  std::vector<developer::SiteGroup> site_group_list;
  site_group_list.reserve(site_groups.size());
  for (auto& entry : site_groups) {
    // Sort the sites in each SiteGroup in ascending order by site.
    std::sort(entry.second.sites.begin(), entry.second.sites.end(),
              [](const developer::SiteInfo& a, const developer::SiteInfo& b) {
                return b.site > a.site;
              });

    size_t subdomains_count_for_site = match_subdomains_count[entry.first];
    if (subdomains_count_for_site > 0u) {
      // Append the all subdomains info to the end of the list.
      developer::SiteInfo all_subdomains_info = CreateSiteInfo(
          base::StrCat({"*.", entry.first}),
          developer::SiteSet::kExtensionSpecified, subdomains_count_for_site);

      entry.second.sites.push_back(std::move(all_subdomains_info));
    }
    site_group_list.push_back(std::move(entry.second));
  }

  return RespondNow(
      ArgumentList(developer::GetUserAndExtensionSitesByEtld::Results::Create(
          site_group_list)));
}

DeveloperPrivateGetMatchingExtensionsForSiteFunction::
    DeveloperPrivateGetMatchingExtensionsForSiteFunction() = default;
DeveloperPrivateGetMatchingExtensionsForSiteFunction::
    ~DeveloperPrivateGetMatchingExtensionsForSiteFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateGetMatchingExtensionsForSiteFunction::Run() {
  std::optional<developer::GetMatchingExtensionsForSite::Params> params =
      developer::GetMatchingExtensionsForSite::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  URLPattern parsed_site(Extension::kValidHostPermissionSchemes);
  if (parsed_site.Parse(params->site) != URLPattern::ParseResult::kSuccess) {
    return RespondNow(Error("Invalid site: " + params->site));
  }

  constexpr bool kIncludeApiPermissions = false;

  std::vector<developer::MatchingExtensionInfo> matching_extensions;
  URLPatternSet site_pattern({parsed_site});
  const ExtensionSet& enabled_extensions =
      ExtensionRegistry::Get(browser_context())->enabled_extensions();
  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser_context());
  for (const auto& extension : enabled_extensions) {
    std::unique_ptr<const PermissionSet> granted_permissions =
        permissions_manager->GetExtensionGrantedPermissions(*extension);
    const URLPatternSet& extension_withheld_sites =
        extension->permissions_data()->withheld_permissions().effective_hosts();
    const URLPatternSet granted_intersection =
        URLPatternSet::CreateIntersection(
            site_pattern, granted_permissions->effective_hosts(),
            URLPatternSet::IntersectionBehavior::kDetailed);
    const URLPatternSet withheld_intersection =
        URLPatternSet::CreateIntersection(
            site_pattern, extension_withheld_sites,
            URLPatternSet::IntersectionBehavior::kDetailed);

    if (granted_intersection.is_empty() && withheld_intersection.is_empty()) {
      continue;
    }

    // By default, return ON_CLICK if the extension has requested but does not
    // have access to any sites that match `site_pattern`.
    developer::HostAccess host_access = developer::HostAccess::kOnClick;

    // TODO(crbug.com/40278776): Add a version of CanUserSelectSiteAccess to
    // PermissionsManager which takes in a URLPattern.
    bool can_request_all_sites =
        granted_permissions->ShouldWarnAllHosts(kIncludeApiPermissions) ||
        extension->permissions_data()
            ->withheld_permissions()
            .ShouldWarnAllHosts(kIncludeApiPermissions);

    // If the extension has access to at least one site that matches
    // `site_pattern`, return ON_ALL_SITES if the extension can request all
    // sites and has no withheld sites, or ON_SPECIFIC_SITES otherwise.
    if (!granted_intersection.is_empty()) {
      host_access = can_request_all_sites && extension_withheld_sites.is_empty()
                        ? developer::HostAccess::kOnAllSites
                        : developer::HostAccess::kOnSpecificSites;
    }

    developer::MatchingExtensionInfo matching_info;
    matching_info.can_request_all_sites = can_request_all_sites;
    matching_info.site_access = host_access;
    matching_info.id = extension->id();
    matching_extensions.push_back(std::move(matching_info));
  }

  return RespondNow(
      ArgumentList(developer::GetMatchingExtensionsForSite::Results::Create(
          matching_extensions)));
}

DeveloperPrivateUpdateSiteAccessFunction::
    DeveloperPrivateUpdateSiteAccessFunction() = default;
DeveloperPrivateUpdateSiteAccessFunction::
    ~DeveloperPrivateUpdateSiteAccessFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateUpdateSiteAccessFunction::Run() {
  std::optional<developer::UpdateSiteAccess::Params> params =
      developer::UpdateSiteAccess::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  URLPattern parsed_site(Extension::kValidHostPermissionSchemes);
  if (parsed_site.Parse(params->site) != URLPattern::ParseResult::kSuccess) {
    return RespondNow(Error("Invalid site: " + params->site));
  }

  base::RepeatingClosure done_callback = base::BarrierClosure(
      params->updates.size(),
      base::BindOnce(
          &DeveloperPrivateUpdateSiteAccessFunction::OnSiteSettingsUpdated,
          base::RetainedRef(this)));

  // To ensure that this function is atomic, return with an error if any
  // extension specified does not exist or cannot have its host permissions
  // changed.
  PermissionsManager* const permissions_manager =
      PermissionsManager::Get(browser_context());
  std::vector<std::pair<const Extension&, developer::HostAccess>>
      extensions_to_modify;
  extensions_to_modify.reserve(params->updates.size());
  for (const auto& update : params->updates) {
    const Extension* extension = GetExtensionById(update.id);
    if (!extension) {
      return RespondNow(Error(kNoSuchExtensionError));
    }
    if (!permissions_manager->CanAffectExtension(*extension)) {
      return RespondNow(Error(kCannotChangeHostPermissions));
    }

    extensions_to_modify.emplace_back(*extension, update.site_access);
  }

  for (const auto& update : extensions_to_modify) {
    const Extension& extension = update.first;

    std::unique_ptr<const PermissionSet> permissions;
    ScriptingPermissionsModifier modifier(browser_context(), &extension);
    bool has_withheld_permissions =
        permissions_manager->HasWithheldHostPermissions(extension);
    switch (update.second) {
      case developer::HostAccess::kOnClick:
        // If the extension has no withheld permissions and can run on all of
        // its requested hosts, withhold all of its host permissions as a
        // blocklist based model for runtime host permissions (i.e. run on all
        // sites except these) is not currently supported.
        if (!has_withheld_permissions) {
          modifier.SetWithholdHostPermissions(true);
          modifier.RemoveAllGrantedHostPermissions();
          done_callback.Run();
        } else {
          modifier.RemoveHostPermissions(parsed_site, done_callback);
        }
        break;
      case developer::HostAccess::kOnSpecificSites:
        // If the extension has no withheld host permissions and can run on
        // all of its requested hosts, withhold all of its permissions
        // before granting `site`.
        if (!has_withheld_permissions) {
          modifier.SetWithholdHostPermissions(true);
          modifier.RemoveAllGrantedHostPermissions();
        }
        modifier.GrantHostPermission(parsed_site, done_callback);
        break;
      case developer::HostAccess::kOnAllSites:
        modifier.SetWithholdHostPermissions(false);
        done_callback.Run();
        break;
      case developer::HostAccess::kNone:
        NOTREACHED();
    }
  }

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DeveloperPrivateUpdateSiteAccessFunction::OnSiteSettingsUpdated() {
  Respond(NoArguments());
}

DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction::
    DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction() =
        default;
DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction::
    ~DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction() =
        default;

ExtensionFunction::ResponseAction
DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction::Run() {
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(Error(kCouldNotFindWebContentsError));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  SafetyHubMenuNotificationServiceFactory::GetForProfile(profile)
      ->DismissActiveNotificationOfModule(
          safety_hub::SafetyHubModuleType::EXTENSIONS);
  return RespondNow(NoArguments());
}

DeveloperPrivateChoosePathFunction::DeveloperPrivateChoosePathFunction() =
    default;

DeveloperPrivateChoosePathFunction::~DeveloperPrivateChoosePathFunction() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get()) {
    select_file_dialog_->ListenerDestroyed();
  }
}

ExtensionFunction::ResponseAction DeveloperPrivateChoosePathFunction::Run() {
  std::optional<developer::ChoosePath::Params> params =
      developer::ChoosePath::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(Error(kCouldNotShowSelectFileDialogError));
  }

  // Start or cancel the file selection without showing the select file dialog
  // for tests that require it.
  if (accept_dialog_for_testing_.has_value()) {
    AddRef();  // Balanced in FileSelected() / FileSelectionCanceled().
    if (accept_dialog_for_testing_.value()) {
      CHECK(selected_file_for_testing_.has_value());
      FileSelected(selected_file_for_testing_.value(), /*index=*/0);
    } else {
      FileSelectionCanceled();
    }
    CHECK(did_respond());
    return AlreadyResponded();
  }

  ui::SelectFileDialog::Type file_type = ui::SelectFileDialog::SELECT_FOLDER;
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  std::u16string select_title;

  if (params->select_type == developer::SelectType::kFile) {
    file_type = ui::SelectFileDialog::SELECT_OPEN_FILE;
  }

  int file_type_index = 0;
  if (params->file_type == developer::FileType::kLoad) {
    select_title = l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY);
  } else if (params->file_type == developer::FileType::kPem) {
    select_title =
        l10n_util::GetStringUTF16(IDS_EXTENSION_PACK_DIALOG_SELECT_KEY);
    file_type_info.extensions.emplace_back(1, FILE_PATH_LITERAL("pem"));
    file_type_info.extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PACK_DIALOG_KEY_FILE_TYPE_DESCRIPTION));
    file_type_info.include_all_files = true;
    file_type_index = 1;
  } else {
    NOTREACHED();
  }

  const base::FilePath last_directory =
      DeveloperPrivateAPI::Get(browser_context())->last_unpacked_directory();
  gfx::NativeWindow owning_window =
      platform_util::GetTopLevel(web_contents->GetNativeView());

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  select_file_dialog_->SelectFile(file_type, select_title, last_directory,
                                  &file_type_info, file_type_index,
                                  base::FilePath::StringType(), owning_window);

  AddRef();  // Balanced in FileSelected() / FileSelectionCanceled().
  return RespondLater();
}

void DeveloperPrivateChoosePathFunction::FileSelected(
    const ui::SelectedFileInfo& file,
    int index) {
  Respond(WithArguments(file.path().LossyDisplayName()));
  Release();
}

void DeveloperPrivateChoosePathFunction::FileSelectionCanceled() {
  // This isn't really an error, but we should keep it like this for
  // backward compatability.
  Respond(Error(kFileSelectionCanceled));
  Release();
}

DeveloperPrivateRequestFileSourceFunction::
    DeveloperPrivateRequestFileSourceFunction() = default;

DeveloperPrivateRequestFileSourceFunction::
    ~DeveloperPrivateRequestFileSourceFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateRequestFileSourceFunction::Run() {
  params_ = developer::RequestFileSource::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  const developer::RequestFileSourceProperties& properties =
      params_->properties;
  const Extension* extension = GetExtensionById(properties.extension_id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  // Under no circumstances should we ever need to reference a file outside of
  // the extension's directory. If it tries to, abort.
  base::FilePath path_suffix =
      base::FilePath::FromUTF8Unsafe(properties.path_suffix);
  if (path_suffix.empty() || path_suffix.ReferencesParent()) {
    return RespondNow(Error(kInvalidPathError));
  }

  if (properties.path_suffix == kManifestFile && !properties.manifest_key) {
    return RespondNow(Error(kManifestKeyIsRequiredError));
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadFileToString, extension->path().Append(path_suffix)),
      base::BindOnce(&DeveloperPrivateRequestFileSourceFunction::Finish, this));

  return RespondLater();
}

void DeveloperPrivateRequestFileSourceFunction::Finish(
    const std::string& file_contents) {
  const developer::RequestFileSourceProperties& properties =
      params_->properties;
  const Extension* extension = GetExtensionById(properties.extension_id);
  if (!extension) {
    Respond(Error(kNoSuchExtensionError));
    return;
  }

  developer::RequestFileSourceResponse response;
  base::FilePath path_suffix =
      base::FilePath::FromUTF8Unsafe(properties.path_suffix);
  base::FilePath path = extension->path().Append(path_suffix);
  response.title = base::StringPrintf("%s: %s", extension->name().c_str(),
                                      path.BaseName().AsUTF8Unsafe().c_str());
  response.message = properties.message;

  std::unique_ptr<FileHighlighter> highlighter;
  if (properties.path_suffix == kManifestFile) {
    highlighter = std::make_unique<ManifestHighlighter>(
        file_contents, *properties.manifest_key,
        properties.manifest_specific ? *properties.manifest_specific
                                     : std::string());
  } else {
    highlighter = std::make_unique<SourceHighlighter>(
        file_contents, properties.line_number ? *properties.line_number : 0);
  }

  response.before_highlight = highlighter->GetBeforeFeature();
  response.highlight = highlighter->GetFeature();
  response.after_highlight = highlighter->GetAfterFeature();

  Respond(WithArguments(response.ToValue()));
}

DeveloperPrivateOpenDevToolsFunction::DeveloperPrivateOpenDevToolsFunction() =
    default;
DeveloperPrivateOpenDevToolsFunction::~DeveloperPrivateOpenDevToolsFunction() =
    default;

ExtensionFunction::ResponseAction DeveloperPrivateOpenDevToolsFunction::Run() {
  std::optional<developer::OpenDevTools::Params> params =
      developer::OpenDevTools::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const developer::OpenDevToolsProperties& properties = params->properties;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (properties.incognito && *properties.incognito) {
    profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  const Extension* extension =
      properties.extension_id
          ? GetEnabledExtensionById(*properties.extension_id)
          : nullptr;

  const bool is_service_worker =
      properties.is_service_worker && *properties.is_service_worker;
  if (is_service_worker) {
    if (!extension) {
      return RespondNow(Error(kNoSuchExtensionError));
    }
    if (!BackgroundInfo::IsServiceWorkerBased(extension)) {
      return RespondNow(Error(kInvalidLazyBackgroundPageParameter));
    }
    if (properties.render_process_id == -1) {
      // Start the service worker and open the inspect window.
      devtools_util::InspectInactiveServiceWorkerBackground(
          extension, profile, DevToolsOpenedByAction::kInspectLink);
      return RespondNow(NoArguments());
    }
    devtools_util::InspectServiceWorkerBackground(
        extension, profile, DevToolsOpenedByAction::kInspectLink);
    return RespondNow(NoArguments());
  }

  if (properties.render_process_id == -1) {
    // This is for a lazy background page.
    if (!extension) {
      return RespondNow(Error(kNoSuchExtensionError));
    }
    if (!BackgroundInfo::HasLazyBackgroundPage(extension)) {
      return RespondNow(Error(kInvalidRenderProcessId));
    }
    // Wakes up the background page and opens the inspect window.
    devtools_util::InspectBackgroundPage(extension, profile,
                                         DevToolsOpenedByAction::kInspectLink);
    return RespondNow(NoArguments());
  }

  // NOTE(devlin): Even though the properties use "render_view_id", this
  // actually refers to a render frame.
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(properties.render_process_id,
                                       properties.render_view_id);

  content::WebContents* web_contents =
      render_frame_host
          ? content::WebContents::FromRenderFrameHost(render_frame_host)
          : nullptr;
  // It's possible that the render frame was closed since we last updated the
  // links. Handle this gracefully.
  if (!web_contents) {
    return RespondNow(Error(kNoSuchRendererError));
  }

  // If we include a url, we should inspect it specifically (and not just the
  // render frame).
  if (properties.url) {
    // Line/column numbers are reported in display-friendly 1-based numbers,
    // but are inspected in zero-based numbers.
    // Default to the first line/column.
    DevToolsWindow::OpenDevToolsWindow(
        web_contents,
        DevToolsToggleAction::Reveal(
            base::UTF8ToUTF16(*properties.url),
            properties.line_number ? *properties.line_number - 1 : 0,
            properties.column_number ? *properties.column_number - 1 : 0),
        DevToolsOpenedByAction::kInspectLink);
  } else {
    DevToolsWindow::OpenDevToolsWindow(web_contents,
                                       DevToolsOpenedByAction::kInspectLink);
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Once we open the inspector, we focus on the appropriate tab...
  Browser* browser = chrome::FindBrowserWithTab(web_contents);

  // ... but some pages (popups and apps) don't have tabs, and some (background
  // pages) don't have an associated browser. For these, the inspector opens in
  // a new window, and our work is done.
  if (!browser || !browser->is_type_normal()) {
    return RespondNow(NoArguments());
  }

  TabStripModel* tab_strip = browser->tab_strip_model();
  tab_strip->ActivateTabAt(tab_strip->GetIndexOfWebContents(
      web_contents));  // Not through direct user gesture.
#endif                 // BUILDFLAG(ENABLE_EXTENSIONS)
  return RespondNow(NoArguments());
}

DeveloperPrivateRepairExtensionFunction::
    ~DeveloperPrivateRepairExtensionFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateRepairExtensionFunction::Run() {
  std::optional<developer::RepairExtension::Params> params =
      developer::RepairExtension::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension) {
    return RespondNow(Error(kNoSuchExtensionError));
  }

  if (!ExtensionPrefs::Get(browser_context())
           ->HasDisableReason(extension->id(),
                              disable_reason::DISABLE_CORRUPTED)) {
    return RespondNow(Error(kCannotRepairHealthyExtension));
  }

  ManagementPolicy* management_policy =
      ExtensionSystem::Get(browser_context())->management_policy();
  // If content verifier would repair this extension independently, then don't
  // allow repair from here. This applies to policy extensions.
  // Also note that if we let |reinstaller| continue with the repair, this would
  // have uninstalled the extension but then we would have failed to reinstall
  // it for policy check (see PolicyCheck::Start()).
  if (management_policy->ShouldRepairIfCorrupted(extension)) {
    return RespondNow(Error(kCannotRepairPolicyExtension));
  }

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(Error(kCouldNotFindWebContentsError));
  }

  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(browser_context());
  if (!extension_management->UpdatesFromWebstore(*extension)) {
    return RespondNow(Error(kCannotRepairNonWebstoreExtension));
  }

  auto reinstaller = base::MakeRefCounted<WebstoreReinstaller>(
      web_contents, params->extension_id,
      base::BindOnce(
          &DeveloperPrivateRepairExtensionFunction::OnReinstallComplete, this));
  reinstaller->BeginReinstall();

  return RespondLater();
}

void DeveloperPrivateRepairExtensionFunction::OnReinstallComplete(
    bool success,
    const std::string& error,
    webstore_install::Result result) {
  Respond(success ? NoArguments() : Error(error));
}

}  // namespace api
}  // namespace extensions
