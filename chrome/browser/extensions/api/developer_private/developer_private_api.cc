// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/browser/extensions/chrome_zipfile_installer.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/error_console/error_console_factory.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/extensions/webstore_reinstaller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/safety_hub/menu_notification_service_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/error_map.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/file_highlighter.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/path_util.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/ui_util.h"
#include "extensions/browser/updater/extension_downloader_types.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_service_factory.h"
#include "extensions/browser/zipfile_installer.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "net/base/filename_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace developer = api::developer_private;

namespace {
const char kCannotUpdateChildAccountProfileSettingsError[] =
    "Cannot change settings for a child account profile.";
const char kNoSuchExtensionError[] = "No such extension.";
const char kRequiresUserGestureError[] =
    "This action requires a user gesture.";
const char kCouldNotShowSelectFileDialogError[] =
    "Could not show a file chooser.";
const char kFileSelectionCanceled[] =
    "File selection was canceled.";
const char kNoSuchRendererError[] = "No such renderer.";
const char kInvalidPathError[] = "Invalid path.";
const char kManifestKeyIsRequiredError[] =
    "The 'manifestKey' argument is required for manifest files.";
const char kCouldNotFindWebContentsError[] =
    "Could not find a valid web contents.";
const char kNoOptionsPageForExtensionError[] =
    "Extension does not have an options page.";
const char kCannotRepairHealthyExtension[] =
    "Cannot repair a healthy extension.";
const char kCannotRepairPolicyExtension[] =
    "Cannot repair a policy-installed extension.";
const char kCannotChangeHostPermissions[] =
    "Cannot change host permissions for the given extension.";
const char kCannotSetPinnedWithoutAction[] =
    "Cannot set pinned action state for an extension with no action.";
const char kInvalidHost[] = "Invalid host.";
const char kInvalidLazyBackgroundPageParameter[] =
    "isServiceWorker can not be set for lazy background page based extensions.";
const char kInvalidRenderProcessId[] =
    "render_process_id can be set to -1 for only lazy background page based or "
    "service-worker based extensions.";
const char kFailToUninstallEnterpriseOrComponentExtensions[] =
    "Cannot uninstall the enterprise or component extensions in your list.";
const char kFailToUninstallNoneExistentExtensions[] =
    "Cannot uninstall non-existent extensions in your list.";
const char kUserCancelledError[] = "User cancelled uninstall";
const char kNoExtensionError[] = "Extension with ID '*' doesn't exist.";
const char kExtensionNotAffectedByMV2Deprecation[] =
    "Extension with ID '*' is not affected by the MV2 deprecation.";
const char kCannotRepairNonWebstoreExtension[] =
    "Cannot repair an extension that is not installed from the Chrome Web "
    "Store.";

const char kUnpackedAppsFolder[] = "apps_target";
const char kManifestFile[] = "manifest.json";

base::FilePath* g_drop_path_for_testing = nullptr;

ExtensionService* GetExtensionService(content::BrowserContext* context) {
  return ExtensionSystem::Get(context)->extension_service();
}

GURL ConvertHostToUrl(const std::string& host) {
  return GURL(base::StrCat(
      {url::kHttpScheme, url::kStandardSchemeSeparator, host, "/"}));
}

std::string ReadFileToString(const base::FilePath& path) {
  std::string data;
  // This call can fail, but it doesn't matter for our purposes. If it fails,
  // we simply return an empty string for the manifest, and ignore it.
  std::ignore = base::ReadFileToString(path, &data);
  return data;
}

using GetManifestErrorCallback =
    base::OnceCallback<void(const base::FilePath& file_path,
                            const std::string& error,
                            size_t line_number,
                            const std::string& manifest)>;
// Takes in an |error| string and tries to parse it as a manifest error (with
// line number), asynchronously calling |callback| with the results.
void GetManifestError(const std::string& error,
                      const base::FilePath& extension_path,
                      GetManifestErrorCallback callback) {
  size_t line = 0u;
  size_t column = 0u;
  std::string regex = base::StringPrintf("%s  Line: (\\d+), column: (\\d+), .*",
                                         manifest_errors::kManifestParseError);
  // If this was a JSON parse error, we can highlight the exact line with the
  // error. Otherwise, we should still display the manifest (for consistency,
  // reference, and so that if we ever make this really fancy and add an editor,
  // it's ready).
  //
  // This regex call can fail, but if it does, we just don't highlight anything.
  re2::RE2::FullMatch(error, regex, &line, &column);

  // This will read the manifest and call AddFailure with the read manifest
  // contents.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFileToString,
                     extension_path.Append(kManifestFilename)),
      base::BindOnce(std::move(callback), extension_path, error, line));
}

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

  if (should_do_verification_check)
    InstallVerifier::Get(context)->VerifyAllExtensions();
}

// Creates a developer::LoadError from the provided data.
developer::LoadError CreateLoadError(
    const base::FilePath& file_path,
    const std::string& error,
    size_t line_number,
    const std::string& manifest,
    const DeveloperPrivateAPI::UnpackedRetryId& retry_guid) {
  base::FilePath prettified_path = path_util::PrettifyPath(file_path);

  SourceHighlighter highlighter(manifest, line_number);
  developer::LoadError response;
  response.error = error;
  response.path = base::UTF16ToUTF8(prettified_path.LossyDisplayName());
  response.retry_guid = retry_guid;

  response.source.emplace();
  response.source->before_highlight = highlighter.GetBeforeFeature();
  response.source->highlight = highlighter.GetFeature();
  response.source->after_highlight = highlighter.GetAfterFeature();

  return response;
}

std::optional<URLPattern> ParseRuntimePermissionsPattern(
    const std::string& pattern_str) {
  constexpr int kValidRuntimePermissionSchemes = URLPattern::SCHEME_HTTP |
                                                 URLPattern::SCHEME_HTTPS |
                                                 URLPattern::SCHEME_FILE;

  URLPattern pattern(kValidRuntimePermissionSchemes);
  if (pattern.Parse(pattern_str) != URLPattern::ParseResult::kSuccess)
    return std::nullopt;

  // We don't allow adding paths for permissions, because they aren't meaningful
  // in terms of origin access. The frontend should validate this, but there's
  // a chance something can slip through, so we should fail gracefully.
  if (pattern.path() != "/*")
    return std::nullopt;

  return pattern;
}

developer::UserSiteSettings ConvertToUserSiteSettings(
    const PermissionsManager::UserPermissionsSettings& settings) {
  api::developer_private::UserSiteSettings user_site_settings;
  user_site_settings.permitted_sites.reserve(settings.permitted_sites.size());
  for (const auto& origin : settings.permitted_sites)
    user_site_settings.permitted_sites.push_back(origin.Serialize());

  user_site_settings.restricted_sites.reserve(settings.restricted_sites.size());
  for (const auto& origin : settings.restricted_sites)
    user_site_settings.restricted_sites.push_back(origin.Serialize());

  return user_site_settings;
}

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

    if (can_run_on_site_group)
      entry.second.num_extensions++;
  }
}

}  // namespace

namespace ChoosePath = api::developer_private::ChoosePath;
namespace PackDirectory = api::developer_private::PackDirectory;
namespace Reload = api::developer_private::Reload;

static base::LazyInstance<BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>>::
    DestructorAtExit g_developer_private_api_factory =
        LAZY_INSTANCE_INITIALIZER;

class DeveloperPrivateAPI::WebContentsTracker
    : public content::WebContentsObserver {
 public:
  WebContentsTracker(base::WeakPtr<DeveloperPrivateAPI> api,
                     content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents), api_(api) {}

  WebContentsTracker(const WebContentsTracker&) = delete;
  WebContentsTracker& operator=(const WebContentsTracker&) = delete;

 private:
  ~WebContentsTracker() override = default;

  void WebContentsDestroyed() override {
    if (api_)
      api_->web_contents_data_.erase(web_contents());
    delete this;
  }

  base::WeakPtr<DeveloperPrivateAPI> api_;
};

DeveloperPrivateAPI::WebContentsData::WebContentsData() = default;
DeveloperPrivateAPI::WebContentsData::~WebContentsData() = default;
DeveloperPrivateAPI::WebContentsData::WebContentsData(WebContentsData&& other) =
    default;

// static
BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>*
DeveloperPrivateAPI::GetFactoryInstance() {
  return g_developer_private_api_factory.Pointer();
}

// static
std::unique_ptr<developer::ProfileInfo> DeveloperPrivateAPI::CreateProfileInfo(
    Profile* profile) {
  std::unique_ptr<developer::ProfileInfo> info(new developer::ProfileInfo());
  info->is_child_account =
      supervised_user::AreExtensionsPermissionsEnabled(profile);
  PrefService* prefs = profile->GetPrefs();
  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kExtensionsUIDeveloperMode);
  info->is_incognito_available = IncognitoModePrefs::GetAvailability(prefs) !=
                                 policy::IncognitoModeAvailability::kDisabled;
  info->is_developer_mode_controlled_by_policy = pref->IsManaged();
  info->in_developer_mode =
      !info->is_child_account &&
      prefs->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  info->can_load_unpacked =
      ExtensionManagementFactory::GetForBrowserContext(profile)
          ->HasAllowlistedExtension();
  info->is_mv2_deprecation_notice_dismissed =
      ManifestV2ExperimentManager::Get(profile)
          ->DidUserAcknowledgeNoticeGlobally();

  return info;
}

template <>
void BrowserContextKeyedAPIFactory<
    DeveloperPrivateAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ErrorConsoleFactory::GetInstance());
  DependsOn(ProcessManagerFactory::GetInstance());
  DependsOn(AppWindowRegistry::Factory::GetInstance());
  DependsOn(WarningServiceFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionManagementFactory::GetInstance());
  DependsOn(CommandService::GetFactoryInstance());
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(ExtensionSystemFactory::GetInstance());
  DependsOn(PermissionsManager::GetFactory());
  DependsOn(ToolbarActionsModelFactory::GetInstance());
}

// static
DeveloperPrivateAPI* DeveloperPrivateAPI::Get(
    content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

DeveloperPrivateAPI::DeveloperPrivateAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  RegisterNotifications();
}

DeveloperPrivateEventRouter::DeveloperPrivateEventRouter(Profile* profile)
    : profile_(profile), event_router_(EventRouter::Get(profile_)) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
  error_console_observation_.Observe(ErrorConsole::Get(profile));
  process_manager_observation_.Observe(ProcessManager::Get(profile));
  app_window_registry_observation_.Observe(AppWindowRegistry::Get(profile));
  warning_service_observation_.Observe(WarningService::Get(profile));
  extension_prefs_observation_.Observe(ExtensionPrefs::Get(profile));
  extension_management_observation_.Observe(
      ExtensionManagementFactory::GetForBrowserContext(profile));
  command_service_observation_.Observe(CommandService::Get(profile));
  extension_allowlist_observer_.Observe(
      ExtensionSystem::Get(profile)->extension_service()->allowlist());
  permissions_manager_observation_.Observe(PermissionsManager::Get(profile));
  toolbar_actions_model_observation_.Observe(ToolbarActionsModel::Get(profile));
  pref_change_registrar_.Init(profile->GetPrefs());
  // The unretained is safe, since the PrefChangeRegistrar unregisters the
  // callback on destruction.
  pref_change_registrar_.Add(
      prefs::kExtensionsUIDeveloperMode,
      base::BindRepeating(&DeveloperPrivateEventRouter::OnProfilePrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      kMV2DeprecationWarningAcknowledgedGloballyPref.name,
      base::BindRepeating(&DeveloperPrivateEventRouter::OnProfilePrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      kMV2DeprecationDisabledAcknowledgedGloballyPref.name,
      base::BindRepeating(&DeveloperPrivateEventRouter::OnProfilePrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      kMV2DeprecationUnsupportedAcknowledgedGloballyPref.name,
      base::BindRepeating(&DeveloperPrivateEventRouter::OnProfilePrefChanged,
                          base::Unretained(this)));
}

DeveloperPrivateEventRouter::~DeveloperPrivateEventRouter() {
}

void DeveloperPrivateEventRouter::AddExtensionId(
    const ExtensionId& extension_id) {
  extension_ids_.insert(extension_id);
}

void DeveloperPrivateEventRouter::RemoveExtensionId(
    const ExtensionId& extension_id) {
  extension_ids_.erase(extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK(
      profile_->IsSameOrParent(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EventType::kLoaded, extension->id());
}

void DeveloperPrivateEventRouter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK(
      profile_->IsSameOrParent(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EventType::kUnloaded, extension->id());
}

void DeveloperPrivateEventRouter::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  DCHECK(
      profile_->IsSameOrParent(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EventType::kInstalled, extension->id());
}

void DeveloperPrivateEventRouter::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  DCHECK(
      profile_->IsSameOrParent(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EventType::kUninstalled,
                            extension->id());
}

void DeveloperPrivateEventRouter::OnErrorAdded(const ExtensionError* error) {
  // We don't want to handle errors thrown by extensions subscribed to these
  // events (currently only the Apps Developer Tool), because doing so risks
  // entering a loop.
  if (extension_ids_.count(error->extension_id()))
    return;

  BroadcastItemStateChanged(developer::EventType::kErrorAdded,
                            error->extension_id());
}

void DeveloperPrivateEventRouter::OnExtensionConfigurationChanged(
    const ExtensionId& extension_id) {
  BroadcastItemStateChanged(developer::EventType::kConfigurationChanged,
                            extension_id);
}

void DeveloperPrivateEventRouter::OnErrorsRemoved(
    const std::set<ExtensionId>& removed_ids) {
  for (const ExtensionId& id : removed_ids) {
    if (!extension_ids_.count(id))
      BroadcastItemStateChanged(developer::EventType::kErrorsRemoved, id);
  }
}

void DeveloperPrivateEventRouter::OnExtensionFrameRegistered(
    const ExtensionId& extension_id,
    content::RenderFrameHost* render_frame_host) {
  BroadcastItemStateChanged(developer::EventType::kViewRegistered,
                            extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionFrameUnregistered(
    const ExtensionId& extension_id,
    content::RenderFrameHost* render_frame_host) {
  BroadcastItemStateChanged(developer::EventType::kViewUnregistered,
                            extension_id);
}

void DeveloperPrivateEventRouter::OnStartedTrackingServiceWorkerInstance(
    const WorkerId& worker_id) {
  BroadcastItemStateChanged(developer::EventType::kServiceWorkerStarted,
                            worker_id.extension_id);
}

void DeveloperPrivateEventRouter::OnStoppedTrackingServiceWorkerInstance(
    const WorkerId& worker_id) {
  BroadcastItemStateChanged(developer::EventType::kServiceWorkerStopped,
                            worker_id.extension_id);
}

void DeveloperPrivateEventRouter::OnAppWindowAdded(AppWindow* window) {
  BroadcastItemStateChanged(developer::EventType::kViewRegistered,
                            window->extension_id());
}

void DeveloperPrivateEventRouter::OnAppWindowRemoved(AppWindow* window) {
  BroadcastItemStateChanged(developer::EventType::kViewUnregistered,
                            window->extension_id());
}

void DeveloperPrivateEventRouter::OnExtensionCommandAdded(
    const ExtensionId& extension_id,
    const Command& added_command) {
  BroadcastItemStateChanged(developer::EventType::kCommandAdded, extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionCommandRemoved(
    const ExtensionId& extension_id,
    const Command& removed_command) {
  BroadcastItemStateChanged(developer::EventType::kCommandRemoved,
                            extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionDisableReasonsChanged(
    const ExtensionId& extension_id,
    int disable_reasons) {
  BroadcastItemStateChanged(developer::EventType::kPrefsChanged, extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionRuntimePermissionsChanged(
    const ExtensionId& extension_id) {
  BroadcastItemStateChanged(developer::EventType::kPermissionsChanged,
                            extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionAllowlistWarningStateChanged(
    const ExtensionId& extension_id,
    bool show_warning) {
  BroadcastItemStateChanged(developer::EventType::kPrefsChanged, extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionManagementSettingsChanged() {
  base::Value::List args;
  args.Append(DeveloperPrivateAPI::CreateProfileInfo(profile_)->ToValue());

  auto event = std::make_unique<Event>(
      events::DEVELOPER_PRIVATE_ON_PROFILE_STATE_CHANGED,
      developer::OnProfileStateChanged::kEventName, std::move(args));
  event_router_->BroadcastEvent(std::move(event));
}

void DeveloperPrivateEventRouter::ExtensionWarningsChanged(
    const ExtensionIdSet& affected_extensions) {
  for (const ExtensionId& id : affected_extensions)
    BroadcastItemStateChanged(developer::EventType::kWarningsChanged, id);
}

void DeveloperPrivateEventRouter::OnUserPermissionsSettingsChanged(
    const PermissionsManager::UserPermissionsSettings& settings) {
  developer::UserSiteSettings user_site_settings =
      ConvertToUserSiteSettings(settings);
  base::Value::List args;
  args.Append(user_site_settings.ToValue());

  auto event = std::make_unique<Event>(
      events::DEVELOPER_PRIVATE_ON_USER_SITE_SETTINGS_CHANGED,
      developer::OnUserSiteSettingsChanged::kEventName, std::move(args));
  event_router_->BroadcastEvent(std::move(event));
}

void DeveloperPrivateEventRouter::OnExtensionPermissionsUpdated(
    const Extension& extension,
    const PermissionSet& permissions,
    PermissionsManager::UpdateReason reason) {
  BroadcastItemStateChanged(developer::EventType::kPermissionsChanged,
                            extension.id());
}

void DeveloperPrivateEventRouter::OnToolbarPinnedActionsChanged() {
  // Currently, only enabled extensions are considered since they are the only
  // ones that have extension actions.
  // TODO(crbug.com/40280426): Since pinned info is stored as a pref, include
  // disabled extensions in this event as well.
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  for (const auto& extension : extensions) {
    if (ui_util::ShouldDisplayInExtensionSettings(*extension)) {
      BroadcastItemStateChanged(developer::EventType::kPinnedActionsChanged,
                                extension->id());
    }
  }
}

void DeveloperPrivateEventRouter::OnProfilePrefChanged() {
  base::Value::List args;
  args.Append(DeveloperPrivateAPI::CreateProfileInfo(profile_)->ToValue());
  auto event = std::make_unique<Event>(
      events::DEVELOPER_PRIVATE_ON_PROFILE_STATE_CHANGED,
      developer::OnProfileStateChanged::kEventName, std::move(args));
  event_router_->BroadcastEvent(std::move(event));

  // The following properties are updated when dev mode is toggled.
  //   - error_collection.is_enabled
  //   - error_collection.is_active
  //   - runtime_errors
  //   - manifest_errors
  //   - install_warnings
  // An alternative approach would be to factor out the dev mode state from the
  // above properties and allow the UI control what happens when dev mode
  // changes. If the UI rendering performance is an issue, instead of replacing
  // the entire extension info, a diff of the old and new extension info can be
  // made by the UI and only perform a partial update of the extension info.
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(profile_)->enabled_extensions();
  for (const auto& extension : extensions)
    BroadcastItemStateChanged(developer::EventType::kPrefsChanged,
                              extension->id());
}

void DeveloperPrivateEventRouter::BroadcastItemStateChanged(
    developer::EventType event_type,
    const ExtensionId& extension_id) {
  std::unique_ptr<ExtensionInfoGenerator> info_generator(
      new ExtensionInfoGenerator(profile_));
  ExtensionInfoGenerator* info_generator_weak = info_generator.get();
  info_generator_weak->CreateExtensionInfo(
      extension_id,
      base::BindOnce(
          &DeveloperPrivateEventRouter::BroadcastItemStateChangedHelper,
          weak_factory_.GetWeakPtr(), event_type, extension_id,
          std::move(info_generator)));
}

void DeveloperPrivateEventRouter::BroadcastItemStateChangedHelper(
    developer::EventType event_type,
    const ExtensionId& extension_id,
    std::unique_ptr<ExtensionInfoGenerator> info_generator,
    ExtensionInfoGenerator::ExtensionInfoList infos) {
  DCHECK_LE(infos.size(), 1u);

  developer::EventData event_data;
  event_data.event_type = event_type;
  event_data.item_id = extension_id;
  if (!infos.empty()) {
    event_data.extension_info = std::move(infos[0]);
  }

  base::Value::List args;
  args.Append(event_data.ToValue());
  std::unique_ptr<Event> event(
      new Event(events::DEVELOPER_PRIVATE_ON_ITEM_STATE_CHANGED,
                developer::OnItemStateChanged::kEventName, std::move(args)));
  event_router_->BroadcastEvent(std::move(event));
}

DeveloperPrivateAPI::UnpackedRetryId DeveloperPrivateAPI::AddUnpackedPath(
    content::WebContents* web_contents,
    const base::FilePath& path) {
  DCHECK(web_contents);
  last_unpacked_directory_ = path;
  WebContentsData* data = GetOrCreateWebContentsData(web_contents);
  IdToPathMap& paths = data->allowed_unpacked_paths;
  auto existing =
      base::ranges::find(paths, path, &IdToPathMap::value_type::second);
  if (existing != paths.end())
    return existing->first;

  UnpackedRetryId id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  paths[id] = path;
  return id;
}

base::FilePath DeveloperPrivateAPI::GetUnpackedPath(
    content::WebContents* web_contents,
    const UnpackedRetryId& id) const {
  const WebContentsData* data = GetWebContentsData(web_contents);
  if (!data)
    return base::FilePath();
  const IdToPathMap& paths = data->allowed_unpacked_paths;
  auto path_iter = paths.find(id);
  if (path_iter == paths.end())
    return base::FilePath();
  return path_iter->second;
}

void DeveloperPrivateAPI::SetDraggedPath(content::WebContents* web_contents,
                                         const base::FilePath& dragged_path) {
  WebContentsData* data = GetOrCreateWebContentsData(web_contents);
  data->dragged_path = dragged_path;
}

base::FilePath DeveloperPrivateAPI::GetDraggedPath(
    content::WebContents* web_contents) const {
  const WebContentsData* data = GetWebContentsData(web_contents);
  return data ? data->dragged_path : base::FilePath();
}

void DeveloperPrivateAPI::RegisterNotifications() {
  EventRouter::Get(profile_)->RegisterObserver(
      this, developer::OnItemStateChanged::kEventName);
  EventRouter::Get(profile_)->RegisterObserver(
      this, developer::OnUserSiteSettingsChanged::kEventName);
}

const DeveloperPrivateAPI::WebContentsData*
DeveloperPrivateAPI::GetWebContentsData(
    content::WebContents* web_contents) const {
  auto iter = web_contents_data_.find(web_contents);
  return iter == web_contents_data_.end() ? nullptr : &iter->second;
}

DeveloperPrivateAPI::WebContentsData*
DeveloperPrivateAPI::GetOrCreateWebContentsData(
    content::WebContents* web_contents) {
  auto iter = web_contents_data_.find(web_contents);
  if (iter != web_contents_data_.end())
    return &iter->second;

  // This is the first we've added this WebContents. Track its lifetime so we
  // can clean up the paths when it is destroyed.
  // WebContentsTracker manages its own lifetime.
  new WebContentsTracker(weak_factory_.GetWeakPtr(), web_contents);
  return &web_contents_data_[web_contents];
}

DeveloperPrivateAPI::~DeveloperPrivateAPI() {}

void DeveloperPrivateAPI::Shutdown() {}

void DeveloperPrivateAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  if (!developer_private_event_router_) {
    developer_private_event_router_ =
        std::make_unique<DeveloperPrivateEventRouter>(profile_);
  }

  developer_private_event_router_->AddExtensionId(details.extension_id);
}

void DeveloperPrivateAPI::OnListenerRemoved(
    const EventListenerInfo& details) {
  if (!EventRouter::Get(profile_)->HasEventListener(
          developer::OnItemStateChanged::kEventName) &&
      !EventRouter::Get(profile_)->HasEventListener(
          developer::OnUserSiteSettingsChanged::kEventName)) {
    developer_private_event_router_.reset(nullptr);
  } else {
    developer_private_event_router_->RemoveExtensionId(details.extension_id);
  }
}

namespace api {

DeveloperPrivateAPIFunction::~DeveloperPrivateAPIFunction() {
}

const Extension* DeveloperPrivateAPIFunction::GetExtensionById(
    const ExtensionId& id) {
  return ExtensionRegistry::Get(browser_context())->GetExtensionById(
      id, ExtensionRegistry::EVERYTHING);
}

const Extension* DeveloperPrivateAPIFunction::GetEnabledExtensionById(
    const ExtensionId& id) {
  return ExtensionRegistry::Get(browser_context())->enabled_extensions().
      GetByID(id);
}

DeveloperPrivateAutoUpdateFunction::~DeveloperPrivateAutoUpdateFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateAutoUpdateFunction::Run() {
  ExtensionUpdater* updater =
      ExtensionSystem::Get(browser_context())->extension_service()->updater();
  if (updater) {
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
DeveloperPrivateGetExtensionsInfoFunction() {
}

DeveloperPrivateGetExtensionsInfoFunction::
~DeveloperPrivateGetExtensionsInfoFunction() {
}

ExtensionFunction::ResponseAction
DeveloperPrivateGetExtensionsInfoFunction::Run() {
  std::optional<developer::GetExtensionsInfo::Params> params =
      developer::GetExtensionsInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  bool include_disabled = true;
  bool include_terminated = true;
  if (params->options) {
    if (params->options->include_disabled)
      include_disabled = *params->options->include_disabled;
    if (params->options->include_terminated)
      include_terminated = *params->options->include_terminated;
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
DeveloperPrivateGetExtensionInfoFunction() {
}

DeveloperPrivateGetExtensionInfoFunction::
~DeveloperPrivateGetExtensionInfoFunction() {
}

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
    DeveloperPrivateGetExtensionSizeFunction() {}

DeveloperPrivateGetExtensionSizeFunction::
    ~DeveloperPrivateGetExtensionSizeFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateGetExtensionSizeFunction::Run() {
  std::optional<developer::GetExtensionSize::Params> params =
      developer::GetExtensionSize::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const Extension* extension = GetExtensionById(params->id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

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
~DeveloperPrivateGetProfileConfigurationFunction() {
}

ExtensionFunction::ResponseAction
DeveloperPrivateGetProfileConfigurationFunction::Run() {
  std::unique_ptr<developer::ProfileInfo> info =
      DeveloperPrivateAPI::CreateProfileInfo(
          Profile::FromBrowserContext(browser_context()));

  // If this is called from the chrome://extensions page, we use this as a
  // heuristic that it's a good time to verify installs. We do this on startup,
  // but there's a chance that it failed erroneously, so it's good to double-
  // check.
  if (source_context_type() == mojom::ContextType::kWebUi) {
    PerformVerificationCheck(browser_context());
  }

  return RespondNow(WithArguments(info->ToValue()));
}

DeveloperPrivateUpdateProfileConfigurationFunction::
~DeveloperPrivateUpdateProfileConfigurationFunction() {
}

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

  if (update.is_mv2_deprecation_notice_dismissed.value_or(false)) {
    ManifestV2ExperimentManager::Get(browser_context())
        ->MarkNoticeAsAcknowledgedGlobally();
  }

  return RespondNow(NoArguments());
}

DeveloperPrivateUpdateExtensionConfigurationFunction::
~DeveloperPrivateUpdateExtensionConfigurationFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateUpdateExtensionConfigurationFunction::Run() {
  std::optional<developer::UpdateExtensionConfiguration::Params> params =
      developer::UpdateExtensionConfiguration::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const developer::ExtensionConfigurationUpdate& update = params->update;

  const Extension* extension = GetExtensionById(update.extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  // The chrome://extensions page uses toggles which, when dragged, do not
  // invoke a user gesture. Work around this for the chrome://extensions page.
  // TODO(dpapad): Remove this exemption when sliding a toggle counts as a
  // gesture.
  bool allowed =
      source_context_type() == mojom::ContextType::kWebUi || user_gesture();
  if (!allowed)
    return RespondNow(Error(kRequiresUserGestureError));

  if (update.file_access) {
    util::SetAllowFileAccess(
        extension->id(), browser_context(), *update.file_access);
  }
  if (update.incognito_access) {
    util::SetIsIncognitoEnabled(
        extension->id(), browser_context(), *update.incognito_access);
  }
  if (update.error_collection) {
    ErrorConsole::Get(browser_context())->SetReportingAllForExtension(
        extension->id(), *update.error_collection);
  }
  if (update.host_access != developer::HostAccess::kNone) {
    PermissionsManager* manager = PermissionsManager::Get(browser_context());
    if (!manager->CanAffectExtension(*extension))
      return RespondNow(Error(kCannotChangeHostPermissions));

    ScriptingPermissionsModifier modifier(browser_context(), extension);
    switch (update.host_access) {
      case developer::HostAccess::kOnClick:
        modifier.SetWithholdHostPermissions(true);
        modifier.RemoveAllGrantedHostPermissions();
        break;
      case developer::HostAccess::kOnSpecificSites:
        if (manager->HasBroadGrantedHostPermissions(*extension))
          modifier.RemoveBroadGrantedHostPermissions();
        modifier.SetWithholdHostPermissions(true);
        break;
      case developer::HostAccess::kOnAllSites:
        modifier.SetWithholdHostPermissions(false);
        break;
      case developer::HostAccess::kNone:
        NOTREACHED_IN_MIGRATION();
    }
  }
  if (update.show_access_requests_in_toolbar) {
    SitePermissionsHelper(Profile::FromBrowserContext(browser_context()))
        .SetShowAccessRequestsInToolbar(
            extension->id(), *update.show_access_requests_in_toolbar);
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

  return RespondNow(NoArguments());
}

DeveloperPrivateReloadFunction::DeveloperPrivateReloadFunction() = default;
DeveloperPrivateReloadFunction::~DeveloperPrivateReloadFunction() = default;

ExtensionFunction::ResponseAction DeveloperPrivateReloadFunction::Run() {
  std::optional<Reload::Params> params = Reload::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  reloading_extension_path_ = extension->path();

  bool fail_quietly = false;
  bool wait_for_completion = false;
  if (params->options) {
    fail_quietly =
        params->options->fail_quietly && *params->options->fail_quietly;
    // We only wait for completion for unpacked extensions, since they are the
    // only extensions for which we can show actionable feedback to the user.
    wait_for_completion = params->options->populate_error_for_unpacked &&
                          *params->options->populate_error_for_unpacked &&
                          Manifest::IsUnpackedLocation(extension->location());
  }

  ExtensionService* service = GetExtensionService(browser_context());
  if (fail_quietly)
    service->ReloadExtensionWithQuietFailure(params->extension_id);
  else
    service->ReloadExtension(params->extension_id);

  if (!wait_for_completion)
    return RespondNow(NoArguments());

  // Balanced in ClearObservers(), which is called from the first observer
  // method to be called with the appropriate extension (or shutdown).
  AddRef();
  error_reporter_observation_.Observe(LoadErrorReporter::GetInstance());
  registry_observation_.Observe(ExtensionRegistry::Get(browser_context()));

  return RespondLater();
}

void DeveloperPrivateReloadFunction::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension->path() == reloading_extension_path_) {
    // Reload succeeded!
    Respond(NoArguments());
    ClearObservers();
  }
}

void DeveloperPrivateReloadFunction::OnShutdown(ExtensionRegistry* registry) {
  Respond(Error("Shutting down."));
  ClearObservers();
}

void DeveloperPrivateReloadFunction::OnLoadFailure(
    content::BrowserContext* browser_context,
    const base::FilePath& file_path,
    const std::string& error) {
  if (file_path == reloading_extension_path_) {
    // Reload failed - create an error to pass back to the extension.
    GetManifestError(
        error, file_path,
        base::BindOnce(&DeveloperPrivateReloadFunction::OnGotManifestError,
                       this));  // Creates a reference.
    ClearObservers();
  }
}

void DeveloperPrivateReloadFunction::OnGotManifestError(
    const base::FilePath& file_path,
    const std::string& error,
    size_t line_number,
    const std::string& manifest) {
  DeveloperPrivateAPI::UnpackedRetryId retry_guid =
      DeveloperPrivateAPI::Get(browser_context())
          ->AddUnpackedPath(GetSenderWebContents(), reloading_extension_path_);
  // Respond to the caller with the load error, which allows the caller to retry
  // reloading through developerPrivate.loadUnpacked().
  // TODO(devlin): This is weird. Really, we should allow retrying through this
  // function instead of through loadUnpacked(), but
  // ExtensionService::ReloadExtension doesn't behave well with an extension
  // that failed to reload, and untangling that mess is quite significant.
  // See https://crbug.com/792277.
  Respond(WithArguments(
      CreateLoadError(file_path, error, line_number, manifest, retry_guid)
          .ToValue()));
}

void DeveloperPrivateReloadFunction::ClearObservers() {
  registry_observation_.Reset();
  error_reporter_observation_.Reset();

  Release();  // Balanced in Run().
}

DeveloperPrivateLoadUnpackedFunction::DeveloperPrivateLoadUnpackedFunction() {}

DeveloperPrivateLoadUnpackedFunction::~DeveloperPrivateLoadUnpackedFunction() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get()) {
    select_file_dialog_->ListenerDestroyed();
  }
}

ExtensionFunction::ResponseAction DeveloperPrivateLoadUnpackedFunction::Run() {
  std::optional<developer::LoadUnpacked::Params> params =
      developer::LoadUnpacked::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return RespondNow(Error(kCouldNotFindWebContentsError));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile && supervised_user::AreExtensionsPermissionsEnabled(profile)) {
    return RespondNow(
        Error("Child account users cannot load unpacked extensions."));
  }
  PrefService* prefs = profile->GetPrefs();
  if (!prefs->GetBoolean(prefs::kExtensionsUIDeveloperMode)) {
    return RespondNow(
        Error("Must be in developer mode to load unpacked extensions."));
  }
  if (ExtensionManagementFactory::GetForBrowserContext(browser_context())
          ->BlocklistedByDefault()) {
    return RespondNow(Error("Extension installation is blocked by policy."));
  }

  fail_quietly_ = params->options &&
                  params->options->fail_quietly &&
                  *params->options->fail_quietly;

  populate_error_ = params->options && params->options->populate_error &&
                    *params->options->populate_error;

  if (params->options && params->options->retry_guid) {
    DeveloperPrivateAPI* api = DeveloperPrivateAPI::Get(browser_context());
    base::FilePath path =
        api->GetUnpackedPath(web_contents, *params->options->retry_guid);
    if (path.empty()) {
      return RespondNow(Error("Invalid retry id"));
    }

    AddRef();  // Balanced in Finish.
    StartFileLoad(path);
    return RespondLater();
  }

  if (params->options && params->options->use_dragged_path &&
      *params->options->use_dragged_path) {
    DeveloperPrivateAPI* api = DeveloperPrivateAPI::Get(browser_context());
    base::FilePath path = api->GetDraggedPath(web_contents);
    if (path.empty()) {
      return RespondNow(Error("No dragged path"));
    }

    AddRef();  // Balanced in Finish.
    StartFileLoad(path);
    return RespondLater();
  }

  ShowSelectFileDialog();
  AddRef();  // Balanced in Finish.
  return RespondLater();
}

void DeveloperPrivateLoadUnpackedFunction::ShowSelectFileDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Start or cancel the file load without showing the select file dialog for
  // tests that require it.
  if (accept_dialog_for_testing_.has_value()) {
    if (accept_dialog_for_testing_.value()) {
      CHECK(selected_file_for_testing_.has_value());
      FileSelected(selected_file_for_testing_.value(), /*index=*/0);
    } else {
      FileSelectionCanceled();
    }
    return;
  }

  content::WebContents* web_contents = GetSenderWebContents();
  CHECK(web_contents);
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  ui::SelectFileDialog::Type file_type =
      ui::SelectFileDialog::SELECT_EXISTING_FOLDER;
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY);
  const base::FilePath last_directory =
      DeveloperPrivateAPI::Get(browser_context())->last_unpacked_directory();
  auto file_type_info = ui::SelectFileDialog::FileTypeInfo();
  int file_type_index = 0;
  gfx::NativeWindow owning_window =
      platform_util::GetTopLevel(web_contents->GetNativeView());

  select_file_dialog_->SelectFile(file_type, title, last_directory,
                                  &file_type_info, file_type_index,
                                  base::FilePath::StringType(), owning_window);
}

void DeveloperPrivateLoadUnpackedFunction::FileSelected(
    const ui::SelectedFileInfo& file,
    int index) {
  select_file_dialog_.reset();
  StartFileLoad(file.path());
}

void DeveloperPrivateLoadUnpackedFunction::FileSelectionCanceled() {
  select_file_dialog_.reset();
  // This isn't really an error, but we should keep it like this for
  // backward compatibility.
  Finish(Error(kFileSelectionCanceled));
}

void DeveloperPrivateLoadUnpackedFunction::StartFileLoad(
    const base::FilePath file_path) {
  scoped_refptr<UnpackedInstaller> installer(
      UnpackedInstaller::Create(GetExtensionService(browser_context())));
  installer->set_be_noisy_on_failure(!fail_quietly_);
  installer->set_completion_callback(base::BindOnce(
      &DeveloperPrivateLoadUnpackedFunction::OnLoadComplete, this));
  installer->Load(file_path);

  retry_guid_ = DeveloperPrivateAPI::Get(browser_context())
                    ->AddUnpackedPath(GetSenderWebContents(), file_path);
}

void DeveloperPrivateLoadUnpackedFunction::OnLoadComplete(
    const Extension* extension,
    const base::FilePath& file_path,
    const std::string& error) {
  if (extension) {
    Finish(NoArguments());
    return;
  }

  if (!populate_error_) {
    Finish(Error(error));
    return;
  }

  GetManifestError(
      error, file_path,
      base::BindOnce(&DeveloperPrivateLoadUnpackedFunction::OnGotManifestError,
                     this));
}

void DeveloperPrivateLoadUnpackedFunction::OnGotManifestError(
    const base::FilePath& file_path,
    const std::string& error,
    size_t line_number,
    const std::string& manifest) {
  DCHECK(!retry_guid_.empty());
  Finish(WithArguments(
      CreateLoadError(file_path, error, line_number, manifest, retry_guid_)
          .ToValue()));
}

void DeveloperPrivateLoadUnpackedFunction::Finish(
    ResponseValue response_value) {
  Respond(std::move(response_value));
  Release();  // Balanced in Run().
}

DeveloperPrivateInstallDroppedFileFunction::
    DeveloperPrivateInstallDroppedFileFunction() = default;
DeveloperPrivateInstallDroppedFileFunction::
    ~DeveloperPrivateInstallDroppedFileFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateInstallDroppedFileFunction::Run() {
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return RespondNow(Error(kCouldNotFindWebContentsError));

  DeveloperPrivateAPI* api = DeveloperPrivateAPI::Get(browser_context());
  base::FilePath path = api->GetDraggedPath(web_contents);
  if (path.empty())
    return RespondNow(Error("No dragged path"));

  ExtensionService* service = GetExtensionService(browser_context());
  if (path.MatchesExtension(FILE_PATH_LITERAL(".zip"))) {
      ZipFileInstaller::Create(GetExtensionFileTaskRunner(),
                               MakeRegisterInExtensionServiceCallback(service))
          ->InstallZipFileToUnpackedExtensionsDir(
              path, service->unpacked_install_directory());
  } else {
    auto prompt = std::make_unique<ExtensionInstallPrompt>(web_contents);
    scoped_refptr<CrxInstaller> crx_installer =
        CrxInstaller::Create(service, std::move(prompt));
    crx_installer->set_error_on_unsupported_requirements(true);
    crx_installer->set_off_store_install_allow_reason(
        CrxInstaller::OffStoreInstallAllowedFromSettingsPage);
    crx_installer->set_install_immediately(true);

    if (path.MatchesExtension(FILE_PATH_LITERAL(".user.js"))) {
      crx_installer->InstallUserScript(path, net::FilePathToFileURL(path));
    } else if (path.MatchesExtension(FILE_PATH_LITERAL(".crx"))) {
      crx_installer->InstallCrx(path);
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
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return RespondNow(Error(kCouldNotFindWebContentsError));

  const base::FilePath* file_path = nullptr;
  if (g_drop_path_for_testing) {
    file_path = g_drop_path_for_testing;
  } else {
    content::DropData* drop_data = web_contents->GetDropData();
    if (!drop_data)
      return RespondNow(Error("No current drop data."));

    if (drop_data->filenames.empty())
      return RespondNow(Error("No files being dragged."));

    const ui::FileInfo& file_info = drop_data->filenames.front();
    file_path = &file_info.path;
  }

  DCHECK(file_path);
  // Note(devlin): we don't do further validation that the file is a directory
  // here. This is validated in the JS, but if that fails, then trying to load
  // the file as an unpacked extension will also fail (reasonably gracefully).
  DeveloperPrivateAPI::Get(browser_context())
      ->SetDraggedPath(web_contents, *file_path);
  return RespondNow(NoArguments());
}

// static
void DeveloperPrivateNotifyDragInstallInProgressFunction::SetDropPathForTesting(
    base::FilePath* file_path) {
  g_drop_path_for_testing = file_path;
}

void DeveloperPrivatePackDirectoryFunction::OnPackSuccess(
    const base::FilePath& crx_file,
    const base::FilePath& pem_file) {
  developer::PackDirectoryResponse response;
  response.message = base::UTF16ToUTF8(
      PackExtensionJob::StandardSuccessMessage(crx_file, pem_file));
  response.status = developer::PackStatus::kSuccess;
  Respond(WithArguments(response.ToValue()));
  pack_job_.reset();
  Release();  // Balanced in Run().
}

void DeveloperPrivatePackDirectoryFunction::OnPackFailure(
    const std::string& error,
    ExtensionCreator::ErrorType error_type) {
  developer::PackDirectoryResponse response;
  response.message = error;
  if (error_type == ExtensionCreator::kCRXExists) {
    response.item_path = item_path_str_;
    response.pem_path = key_path_str_;
    response.override_flags = ExtensionCreator::kOverwriteCRX;
    response.status = developer::PackStatus::kWarning;
  } else {
    response.status = developer::PackStatus::kError;
  }
  Respond(WithArguments(response.ToValue()));
  pack_job_.reset();
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction DeveloperPrivatePackDirectoryFunction::Run() {
  std::optional<PackDirectory::Params> params =
      PackDirectory::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int flags = params->flags ? *params->flags : 0;
  item_path_str_ = params->path;
  if (params->private_key_path)
    key_path_str_ = *params->private_key_path;

  base::FilePath root_directory =
      base::FilePath::FromUTF8Unsafe(item_path_str_);
  base::FilePath key_file = base::FilePath::FromUTF8Unsafe(key_path_str_);

  developer::PackDirectoryResponse response;
  if (root_directory.empty()) {
    if (item_path_str_.empty())
      response.message = l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_REQUIRED);
    else
      response.message = l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_INVALID);

    response.status = developer::PackStatus::kError;
    return RespondNow(WithArguments(response.ToValue()));
  }

  if (!key_path_str_.empty() && key_file.empty()) {
    response.message = l10n_util::GetStringUTF8(
        IDS_EXTENSION_PACK_DIALOG_ERROR_KEY_INVALID);
    response.status = developer::PackStatus::kError;
    return RespondNow(WithArguments(response.ToValue()));
  }

  AddRef();  // Balanced in OnPackSuccess / OnPackFailure.

  pack_job_ =
      std::make_unique<PackExtensionJob>(this, root_directory, key_file, flags);
  pack_job_->Start();
  return RespondLater();
}

DeveloperPrivatePackDirectoryFunction::DeveloperPrivatePackDirectoryFunction() {
}

DeveloperPrivatePackDirectoryFunction::
~DeveloperPrivatePackDirectoryFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateLoadDirectoryFunction::Run() {
  // In theory `extension()` can be null when an ExtensionFunction is invoked
  // from WebUI, but this should never be the case for this particular API.
  DCHECK(extension());

  // TODO(grv) : add unittests.
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 3);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[2].is_string());

  const std::string& filesystem_name = args()[0].GetString();
  const std::string& filesystem_path = args()[1].GetString();
  const std::string& directory_url_str = args()[2].GetString();

  context_ = browser_context()
                 ->GetStoragePartition(render_frame_host()->GetSiteInstance())
                 ->GetFileSystemContext();

  // Directory url is non empty only for syncfilesystem.
  if (!directory_url_str.empty()) {
    storage::FileSystemURL directory_url =
        context_->CrackURLInFirstPartyContext(GURL(directory_url_str));
    if (!directory_url.is_valid() ||
        directory_url.type() != storage::kFileSystemTypeSyncable) {
      return RespondNow(Error("DirectoryEntry of unsupported filesystem."));
    }
    return LoadByFileSystemAPI(directory_url);
  }

  std::string unused_error;
  // Check if the DirectoryEntry is the instance of chrome filesystem.
  if (!app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path, source_process_id(),
          &project_base_path_, &unused_error)) {
    return RespondNow(Error("DirectoryEntry of unsupported filesystem."));
  }

  // Try to load using the FileSystem API backend, in case the filesystem
  // points to a non-native local directory.
  std::string filesystem_id;
  bool cracked =
      storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id);
  CHECK(cracked);
  base::FilePath virtual_path =
      storage::IsolatedContext::GetInstance()
          ->CreateVirtualRootPath(filesystem_id)
          .Append(base::FilePath::FromUTF8Unsafe(filesystem_path));
  storage::FileSystemURL directory_url = context_->CreateCrackedFileSystemURL(
      blink::StorageKey::CreateFirstParty(extension()->origin()),
      storage::kFileSystemTypeIsolated, virtual_path);

  if (directory_url.is_valid() &&
      directory_url.type() != storage::kFileSystemTypeLocal &&
      directory_url.type() != storage::kFileSystemTypeDragged) {
    return LoadByFileSystemAPI(directory_url);
  }

  Load();
  return AlreadyResponded();
}

ExtensionFunction::ResponseAction
DeveloperPrivateLoadDirectoryFunction::LoadByFileSystemAPI(
    const storage::FileSystemURL& directory_url) {
  std::string directory_url_str = directory_url.ToGURL().spec();

  size_t pos = 0;
  // Parse the project directory name from the project url. The project url is
  // expected to have project name as the suffix.
  if ((pos = directory_url_str.rfind("/")) == std::string::npos) {
    return RespondNow(Error("Invalid Directory entry."));
  }

  std::string project_name;
  project_name = directory_url_str.substr(pos + 1);
  project_base_url_ = directory_url_str.substr(0, pos + 1);

  base::FilePath project_path(browser_context()->GetPath());
  project_path = project_path.AppendASCII(kUnpackedAppsFolder);
  project_path = project_path.Append(
      base::FilePath::FromUTF8Unsafe(project_name));

  project_base_path_ = project_path;

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &DeveloperPrivateLoadDirectoryFunction::ClearExistingDirectoryContent,
          this, project_base_path_));
  return RespondLater();
}

void DeveloperPrivateLoadDirectoryFunction::Load() {
  ExtensionService* service = GetExtensionService(browser_context());
  UnpackedInstaller::Create(service)->Load(project_base_path_);

  // TODO(grv) : The unpacked installer should fire an event when complete
  // and return the extension_id.
  Respond(WithArguments("-1"));
}

void DeveloperPrivateLoadDirectoryFunction::ClearExistingDirectoryContent(
    const base::FilePath& project_path) {
  // Clear the project directory before copying new files.
  base::DeletePathRecursively(project_path);

  pending_copy_operations_count_ = 1;

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DeveloperPrivateLoadDirectoryFunction::ReadDirectoryByFileSystemAPI,
          this, project_path, project_path.BaseName()));
}

void DeveloperPrivateLoadDirectoryFunction::ReadDirectoryByFileSystemAPI(
    const base::FilePath& project_path,
    const base::FilePath& destination_path) {
  GURL project_url = GURL(project_base_url_ + destination_path.AsUTF8Unsafe());
  storage::FileSystemURL url =
      context_->CrackURLInFirstPartyContext(project_url);

  context_->operation_runner()->ReadDirectory(
      url, base::BindRepeating(&DeveloperPrivateLoadDirectoryFunction::
                                   ReadDirectoryByFileSystemAPICb,
                               this, project_path, destination_path));
}

void DeveloperPrivateLoadDirectoryFunction::ReadDirectoryByFileSystemAPICb(
    const base::FilePath& project_path,
    const base::FilePath& destination_path,
    base::File::Error status,
    storage::FileSystemOperation::FileEntryList file_list,
    bool has_more) {
  if (status != base::File::FILE_OK) {
    DLOG(ERROR) << "Error in copying files from sync filesystem.";
    return;
  }

  // We add 1 to the pending copy operations for both files and directories. We
  // release the directory copy operation once all the files under the directory
  // are added for copying. We do that to ensure that pendingCopyOperationsCount
  // does not become zero before all copy operations are finished.
  // In case the directory happens to be executing the last copy operation it
  // will call Respond to send the response to the API. The pending copy
  // operations of files are released by the CopyFile function.
  pending_copy_operations_count_ += file_list.size();

  for (auto& file : file_list) {
    if (file.type == filesystem::mojom::FsFileType::DIRECTORY) {
      ReadDirectoryByFileSystemAPI(project_path.Append(file.name),
                                   destination_path.Append(file.name));
      continue;
    }

    GURL project_url = GURL(project_base_url_ +
                            destination_path.Append(file.name).AsUTF8Unsafe());
    storage::FileSystemURL url =
        context_->CrackURLInFirstPartyContext(project_url);

    base::FilePath target_path = project_path;
    target_path = target_path.Append(file.name);

    context_->operation_runner()->CreateSnapshotFile(
        url, base::BindOnce(
                 &DeveloperPrivateLoadDirectoryFunction::SnapshotFileCallback,
                 this, target_path));
  }

  if (!has_more) {
    // Directory copy operation released here.
    pending_copy_operations_count_--;

    if (!pending_copy_operations_count_) {
      ExtensionFunction::ResponseValue response =
          success_ ? NoArguments() : Error(error_);
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&DeveloperPrivateLoadDirectoryFunction::Respond, this,
                         std::move(response)));
    }
  }
}

void DeveloperPrivateLoadDirectoryFunction::SnapshotFileCallback(
    const base::FilePath& target_path,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& src_path,
    scoped_refptr<storage::ShareableFileReference> file_ref) {
  if (result != base::File::FILE_OK) {
    error_ = "Error in copying files from sync filesystem.";
    success_ = false;
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DeveloperPrivateLoadDirectoryFunction::CopyFile, this,
                     src_path, target_path));
}

void DeveloperPrivateLoadDirectoryFunction::CopyFile(
    const base::FilePath& src_path,
    const base::FilePath& target_path) {
  if (!base::CreateDirectory(target_path.DirName())) {
    error_ = "Error in copying files from sync filesystem.";
    success_ = false;
  }

  if (success_)
    base::CopyFile(src_path, target_path);

  CHECK(pending_copy_operations_count_ > 0);
  pending_copy_operations_count_--;

  if (!pending_copy_operations_count_) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DeveloperPrivateLoadDirectoryFunction::Load, this));
  }
}

DeveloperPrivateLoadDirectoryFunction::DeveloperPrivateLoadDirectoryFunction()
    : pending_copy_operations_count_(0), success_(true) {}

DeveloperPrivateLoadDirectoryFunction::~DeveloperPrivateLoadDirectoryFunction()
    {}

DeveloperPrivateChoosePathFunction::DeveloperPrivateChoosePathFunction() {}

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
    select_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_PACK_DIALOG_SELECT_KEY);
    file_type_info.extensions.emplace_back(1, FILE_PATH_LITERAL("pem"));
    file_type_info.extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PACK_DIALOG_KEY_FILE_TYPE_DESCRIPTION));
    file_type_info.include_all_files = true;
    file_type_index = 1;
  } else {
    NOTREACHED_IN_MIGRATION();
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

ExtensionFunction::ResponseAction
DeveloperPrivateIsProfileManagedFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  return RespondNow(WithArguments(
      profile && supervised_user::AreExtensionsPermissionsEnabled(profile)));
}

DeveloperPrivateIsProfileManagedFunction::
    ~DeveloperPrivateIsProfileManagedFunction() {
}

DeveloperPrivateRequestFileSourceFunction::
    DeveloperPrivateRequestFileSourceFunction() {}

DeveloperPrivateRequestFileSourceFunction::
    ~DeveloperPrivateRequestFileSourceFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateRequestFileSourceFunction::Run() {
  params_ = developer::RequestFileSource::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  const developer::RequestFileSourceProperties& properties =
      params_->properties;
  const Extension* extension = GetExtensionById(properties.extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  // Under no circumstances should we ever need to reference a file outside of
  // the extension's directory. If it tries to, abort.
  base::FilePath path_suffix =
      base::FilePath::FromUTF8Unsafe(properties.path_suffix);
  if (path_suffix.empty() || path_suffix.ReferencesParent())
    return RespondNow(Error(kInvalidPathError));

  if (properties.path_suffix == kManifestFile && !properties.manifest_key)
    return RespondNow(Error(kManifestKeyIsRequiredError));

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
  response.title = base::StringPrintf("%s: %s",
                                      extension->name().c_str(),
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

DeveloperPrivateOpenDevToolsFunction::DeveloperPrivateOpenDevToolsFunction() {}
DeveloperPrivateOpenDevToolsFunction::~DeveloperPrivateOpenDevToolsFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateOpenDevToolsFunction::Run() {
  std::optional<developer::OpenDevTools::Params> params =
      developer::OpenDevTools::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const developer::OpenDevToolsProperties& properties = params->properties;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (properties.incognito && *properties.incognito)
    profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  const Extension* extension =
      properties.extension_id
          ? GetEnabledExtensionById(*properties.extension_id)
          : nullptr;

  const bool is_service_worker =
      properties.is_service_worker && *properties.is_service_worker;
  if (is_service_worker) {
    if (!extension)
      return RespondNow(Error(kNoSuchExtensionError));
    if (!BackgroundInfo::IsServiceWorkerBased(extension))
      return RespondNow(Error(kInvalidLazyBackgroundPageParameter));
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
    if (!extension)
      return RespondNow(Error(kNoSuchExtensionError));
    if (!BackgroundInfo::HasLazyBackgroundPage(extension))
      return RespondNow(Error(kInvalidRenderProcessId));
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
  if (!web_contents)
    return RespondNow(Error(kNoSuchRendererError));

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

  // Once we open the inspector, we focus on the appropriate tab...
  Browser* browser = chrome::FindBrowserWithTab(web_contents);

  // ... but some pages (popups and apps) don't have tabs, and some (background
  // pages) don't have an associated browser. For these, the inspector opens in
  // a new window, and our work is done.
  if (!browser || !browser->is_type_normal())
    return RespondNow(NoArguments());

  TabStripModel* tab_strip = browser->tab_strip_model();
  tab_strip->ActivateTabAt(tab_strip->GetIndexOfWebContents(
      web_contents));  // Not through direct user gesture.
  return RespondNow(NoArguments());
}

DeveloperPrivateDeleteExtensionErrorsFunction::
~DeveloperPrivateDeleteExtensionErrorsFunction() {}

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
  error_console->RemoveErrors(ErrorMap::Filter(
      properties.extension_id, type, error_ids, false));

  return RespondNow(NoArguments());
}

DeveloperPrivateRepairExtensionFunction::
~DeveloperPrivateRepairExtensionFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateRepairExtensionFunction::Run() {
  std::optional<developer::RepairExtension::Params> params =
      developer::RepairExtension::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

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
  if (management_policy->ShouldRepairIfCorrupted(extension))
    return RespondNow(Error(kCannotRepairPolicyExtension));

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return RespondNow(Error(kCouldNotFindWebContentsError));

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

DeveloperPrivateShowOptionsFunction::~DeveloperPrivateShowOptionsFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateShowOptionsFunction::Run() {
  std::optional<developer::ShowOptions::Params> params =
      developer::ShowOptions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const Extension* extension = GetEnabledExtensionById(params->extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  if (OptionsPageInfo::GetOptionsPage(extension).is_empty())
    return RespondNow(Error(kNoOptionsPageForExtensionError));

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return RespondNow(Error(kCouldNotFindWebContentsError));

  ExtensionTabUtil::OpenOptionsPage(extension,
                                    chrome::FindBrowserWithTab(web_contents));
  return RespondNow(NoArguments());
}

DeveloperPrivateShowPathFunction::~DeveloperPrivateShowPathFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateShowPathFunction::Run() {
  std::optional<developer::ShowPath::Params> params =
      developer::ShowPath::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  // We explicitly show manifest.json in order to work around an issue in OSX
  // where opening the directory doesn't focus the Finder.
  platform_util::ShowItemInFolder(
      Profile::FromBrowserContext(browser_context()),
      extension->path().Append(kManifestFilename));
  return RespondNow(NoArguments());
}

DeveloperPrivateSetShortcutHandlingSuspendedFunction::
~DeveloperPrivateSetShortcutHandlingSuspendedFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateSetShortcutHandlingSuspendedFunction::Run() {
  std::optional<developer::SetShortcutHandlingSuspended::Params> params =
      developer::SetShortcutHandlingSuspended::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  ExtensionCommandsGlobalRegistry::Get(browser_context())
      ->SetShortcutHandlingSuspended(params->is_suspended);
  return RespondNow(NoArguments());
}

DeveloperPrivateUpdateExtensionCommandFunction::
~DeveloperPrivateUpdateExtensionCommandFunction() {}

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
  if (!pattern)
    return RespondNow(Error(kInvalidHost));

  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

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
  if (!pattern)
    return RespondNow(Error(kInvalidHost));

  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  if (!manager->CanAffectExtension(*extension))
    return RespondNow(Error(kCannotChangeHostPermissions));

  URLPatternSet host_permissions_to_remove({*pattern});
  std::unique_ptr<const PermissionSet> permissions_to_remove =
      PermissionSet::CreateIntersection(
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        host_permissions_to_remove.Clone(),
                        host_permissions_to_remove.Clone()),
          *manager->GetRevokablePermissions(*extension),
          URLPatternSet::IntersectionBehavior::kDetailed);
  if (permissions_to_remove->IsEmpty())
    return RespondNow(Error("Cannot remove a host that hasn't been granted."));

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
  developer::UserSiteSettings user_site_settings = ConvertToUserSiteSettings(
      PermissionsManager::Get(browser_context())->GetUserPermissionsSettings());

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
    if (!url.is_valid())
      return RespondNow(Error("Invalid host: " + host));
    origins.insert(url::Origin::Create(url));
  }

  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  switch (params->options.site_set) {
    case developer::SiteSet::kUserPermitted:
      for (const auto& origin : origins)
        manager->AddUserPermittedSite(origin);
      break;
    case developer::SiteSet::kUserRestricted:
      for (const auto& origin : origins)
        manager->AddUserRestrictedSite(origin);
      break;
    case developer::SiteSet::kExtensionSpecified:
      return RespondNow(
          Error("Site set must be USER_PERMITTED or USER_RESTRICTED"));
    case developer::SiteSet::kNone:
      NOTREACHED_IN_MIGRATION();
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
    if (!url.is_valid())
      return RespondNow(Error("Invalid host: " + host));
    origins.insert(url::Origin::Create(url));
  }

  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  switch (params->options.site_set) {
    case developer::SiteSet::kUserPermitted:
      for (const auto& origin : origins)
        manager->RemoveUserPermittedSite(origin);
      break;
    case developer::SiteSet::kUserRestricted:
      for (const auto& origin : origins)
        manager->RemoveUserRestrictedSite(origin);
      break;
    case developer::SiteSet::kExtensionSpecified:
      return RespondNow(
          Error("Site set must be USER_PERMITTED or USER_RESTRICTED"));
    case developer::SiteSet::kNone:
      NOTREACHED_IN_MIGRATION();
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
  if (parsed_site.Parse(params->site) != URLPattern::ParseResult::kSuccess)
    return RespondNow(Error("Invalid site: " + params->site));

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

    if (granted_intersection.is_empty() && withheld_intersection.is_empty())
      continue;

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
  if (parsed_site.Parse(params->site) != URLPattern::ParseResult::kSuccess)
    return RespondNow(Error("Invalid site: " + params->site));

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
    if (!extension)
      return RespondNow(Error(kNoSuchExtensionError));
    if (!permissions_manager->CanAffectExtension(*extension))
      return RespondNow(Error(kCannotChangeHostPermissions));

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
        NOTREACHED_IN_MIGRATION();
    }
  }

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void DeveloperPrivateUpdateSiteAccessFunction::OnSiteSettingsUpdated() {
  Respond(NoArguments());
}

DeveloperPrivateRemoveMultipleExtensionsFunction::
    DeveloperPrivateRemoveMultipleExtensionsFunction() = default;
DeveloperPrivateRemoveMultipleExtensionsFunction::
    ~DeveloperPrivateRemoveMultipleExtensionsFunction() = default;

ExtensionFunction::ResponseAction
DeveloperPrivateRemoveMultipleExtensionsFunction::Run() {
  std::optional<developer::RemoveMultipleExtensions::Params> params =
      developer::RemoveMultipleExtensions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  profile_ = Profile::FromBrowserContext(browser_context());
  extension_ids_ = std::move(params->extension_ids);

  // Verify the input extension list.
  for (const auto& extension_id : extension_ids_) {
    CHECK(profile_);
    const Extension* current_extension =
        ExtensionRegistry::Get(profile_)->GetExtensionById(
            extension_id, ExtensionRegistry::EVERYTHING);
    if (!current_extension) {
      // Return early if the extension is a non-existent extension.
      return RespondNow(Error(kFailToUninstallNoneExistentExtensions));
    }
    // If enterprise or component extensions are found, do nothing and respond
    // with an error.
    if (Manifest::IsComponentLocation(current_extension->location()) ||
        Manifest::IsPolicyLocation(current_extension->location())) {
      return RespondNow(Error(kFailToUninstallEnterpriseOrComponentExtensions));
    }
  }

  if (accept_bubble_for_testing_.has_value()) {
    if (*accept_bubble_for_testing_) {
      OnDialogAccepted();
    } else {
      OnDialogCancelled();
    }
    return AlreadyResponded();
  }

  gfx::NativeWindow parent;
  if (!GetSenderWebContents()) {
    CHECK_IS_TEST();
    parent = nullptr;
  } else {
    parent = chrome::FindBrowserWithTab(GetSenderWebContents())
                 ->window()
                 ->GetNativeWindow();
  }

  ShowExtensionMultipleUninstallDialog(
      profile_, parent, extension_ids_,
      base::BindOnce(
          &DeveloperPrivateRemoveMultipleExtensionsFunction::OnDialogAccepted,
          this),
      base::BindOnce(
          &DeveloperPrivateRemoveMultipleExtensionsFunction::OnDialogCancelled,
          this));
  return RespondLater();
}

void DeveloperPrivateRemoveMultipleExtensionsFunction::OnDialogCancelled() {
  // Let the consumer end know that the Close button was clicked.
  Respond(Error(kUserCancelledError));
}

void DeveloperPrivateRemoveMultipleExtensionsFunction::OnDialogAccepted() {
  for (const auto& extension_id : extension_ids_) {
    if (!browser_context()) {
      return;
    }
    const Extension* current_extension =
        ExtensionRegistry::Get(profile_)->GetExtensionById(
            extension_id, ExtensionRegistry::EVERYTHING);
    // Extensions can be uninstalled externally while the dialog is open. Only
    // uninstall extensions that are still existent.
    if (!current_extension) {
      continue;
    }
    // If an extension fails to be uninstalled, it will not pause the
    // uninstall of the other extensions on the list.
    ExtensionSystem::Get(profile_)->extension_service()->UninstallExtension(
        extension_id, UNINSTALL_REASON_USER_INITIATED, nullptr);
  }
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

DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
    DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction() = default;
DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
    ~DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction() =
        default;

ExtensionFunction::ResponseAction
DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::Run() {
  std::optional<developer::DismissMv2DeprecationNoticeForExtension::Params>
      params =
          developer::DismissMv2DeprecationNoticeForExtension::Params::Create(
              args());
  EXTENSION_FUNCTION_VALIDATE(params);
  extension_id_ = std::move(params->extension_id);

  ManifestV2ExperimentManager* experiment_manager =
      ManifestV2ExperimentManager::Get(browser_context());

  // Extension must be affected by the MV2 deprecation.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    return RespondNow(Error(
        ErrorUtils::FormatErrorMessage(kNoExtensionError, extension_id_)));
  }
  if (!experiment_manager->IsExtensionAffected(*extension)) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        kExtensionNotAffectedByMV2Deprecation, extension_id_)));
  }

  MV2ExperimentStage experiment_stage =
      experiment_manager->GetCurrentExperimentStage();
  switch (experiment_stage) {
    case MV2ExperimentStage::kNone:
      NOTREACHED();

    case MV2ExperimentStage::kWarning: {
      // Immediately dismiss the notice.
      DismissExtensionNotice();
      return RespondNow(NoArguments());
    }

    case MV2ExperimentStage::kDisableWithReEnable: {
      // Prompt for user confirmation before dismissing the notice.
      if (accept_bubble_for_testing_.has_value()) {
        if (*accept_bubble_for_testing_) {
          OnDialogAccepted();
        } else {
          OnDialogCancelled();
        }
        return AlreadyResponded();
      }

      Browser* browser = chrome::FindLastActiveWithProfile(
          Profile::FromBrowserContext(browser_context()));
      if (!browser) {
        return RespondNow(Error(kCouldNotFindWebContentsError));
      }

      ShowMv2DeprecationKeepDialog(
          browser, *extension,
          base::BindOnce(
              &DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
                  OnDialogAccepted,
              this),
          base::BindOnce(
              &DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
                  OnDialogCancelled,
              this));

      return RespondLater();
    }

    case MV2ExperimentStage::kUnsupported:
      // TODO(https://crbug.com/367395349): Add handling for the kUnsupported
      // experiment stage.
      NOTREACHED();
  }
}

void DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
    DismissExtensionNotice() {
  ManifestV2ExperimentManager* experiment_manager =
      ManifestV2ExperimentManager::Get(browser_context());
  experiment_manager->MarkNoticeAsAcknowledged(extension_id_);

  // There isn't a separate observer for the MV2 acknowledged state changing,
  // but this is the only place it's changed. Just fire the event directly.
  DeveloperPrivateEventRouter* event_router =
      DeveloperPrivateAPI::Get(browser_context())
          ->developer_private_event_router();
  if (event_router) {
    event_router->OnExtensionConfigurationChanged(extension_id_);
  }
}

void DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
    OnDialogAccepted() {
  if (!browser_context()) {
    return;
  }

  DismissExtensionNotice();
  Respond(NoArguments());
}

void DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction::
    OnDialogCancelled() {
  if (!browser_context()) {
    return;
  }

  Respond(NoArguments());
}

}  // namespace api

}  // namespace extensions
