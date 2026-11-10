// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/event_initiator_utils.h"

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/glic_enums.mojom.h"
#include "components/actor/core/task_source_info.h"
#include "components/download/public/common/download_item.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/policy/core/common/policy_types.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#endif

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) && \
    BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/enterprise/connectors/reporting/extension_actor_tracker.h"
#include "extensions/common/extension_id.h"
#endif

#if BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)
#include "chrome/browser/enterprise/connectors/reporting/cdp_actor_session_tracker.h"
#endif

namespace enterprise_connectors {

namespace {

using BrowserAgentInfo = chrome::cros::reporting::proto::BrowserAgentInfo;
using EventInitiatorInfo = chrome::cros::reporting::proto::EventInitiatorInfo;
using ExtensionInfo =
    safe_browsing::ExtensionTelemetryReportRequest::ExtensionInfo;

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
using extensions::mojom::ManifestLocation;

// TODO(crbug.com/567524561): This mapper duplicates the private
// GetInstallLocation() helper in extension_telemetry_service.cc. Hoist a
// single shared copy once there is an agreed home for it.
static_assert(static_cast<int>(ManifestLocation::kMaxValue) ==
                  static_cast<int>(ExtensionInfo::InstallLocation_MAX),
              "ExtensionTelemetryReportRequest::ExtensionInfo::InstallLocation "
              "needs to match extensions::mojom::ManifestLocation.");
ExtensionInfo::InstallLocation ToInstallLocation(ManifestLocation location) {
  switch (location) {
    case ManifestLocation::kInvalidLocation:
      return ExtensionInfo::UNKNOWN_LOCATION;
    case ManifestLocation::kInternal:
      return ExtensionInfo::INTERNAL;
    case ManifestLocation::kExternalPref:
      return ExtensionInfo::EXTERNAL_PREF;
    case ManifestLocation::kExternalRegistry:
      return ExtensionInfo::EXTERNAL_REGISTRY;
    case ManifestLocation::kUnpacked:
      return ExtensionInfo::UNPACKED;
    case ManifestLocation::kComponent:
      return ExtensionInfo::COMPONENT;
    case ManifestLocation::kExternalPrefDownload:
      return ExtensionInfo::EXTERNAL_PREF_DOWNLOAD;
    case ManifestLocation::kExternalPolicyDownload:
      return ExtensionInfo::EXTERNAL_POLICY_DOWNLOAD;
    case ManifestLocation::kCommandLine:
      return ExtensionInfo::COMMAND_LINE;
    case ManifestLocation::kExternalPolicy:
      return ExtensionInfo::EXTERNAL_POLICY;
    case ManifestLocation::kExternalComponent:
      return ExtensionInfo::EXTERNAL_COMPONENT;
  }
  NOTREACHED();
}

void PopulateInstalledExtensionInfo(content::BrowserContext* browser_context,
                                    const std::string& extension_id,
                                    ExtensionInfo& extension_info) {
  extensions::ExtensionRegistry* registry =
      browser_context ? extensions::ExtensionRegistry::Get(browser_context)
                      : nullptr;
  const extensions::Extension* extension =
      registry ? registry->GetInstalledExtension(extension_id) : nullptr;
  if (!extension) {
    // Leave version and provenance unset so consumers can tell "unknown" from
    // "false".
    return;
  }

  // Overwrite any fallback with the current name, since the extension is still
  // installed and its name may have changed.
  extension_info.set_name(extension->name());
  extension_info.set_version(extension->version().GetString());

  // Provenance lets consumers tell a Web Store extension apart from a
  // sideloaded one that declares the same ID.
  extension_info.set_install_location(ToInstallLocation(extension->location()));
  extension_info.set_is_from_store(extension->from_webstore());
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

bool CanIncludeDeviceInfoForReporting(
    content::BrowserContext* browser_context) {
  if (!browser_context ||
      !base::FeatureList::IsEnabled(kEventReportingCdpAttribution)) {
    return false;
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ConnectorsService* connectors =
      ConnectorsServiceFactory::GetForBrowserContext(browser_context);
  if (!connectors) {
    return false;
  }
  std::optional<ReportingSettings> reporting =
      connectors->GetReportingSettings();
  return reporting.has_value() &&
         IncludeDeviceInfo(profile, reporting->per_profile);
}

bool CanIncludeDeviceInfoForUrlLookup(
    content::BrowserContext* browser_context) {
  if (!browser_context ||
      !base::FeatureList::IsEnabled(kEventReportingCdpAttribution)) {
    return false;
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto* connectors =
      ConnectorsServiceFactory::GetForBrowserContext(browser_context);
  if (!connectors || !connectors->GetDMTokenForRealTimeUrlCheck().has_value()) {
    return false;
  }
  std::optional<policy::PolicyScope> url_scope =
      connectors->GetRealtimeUrlCheckScope();
  return url_scope.has_value() &&
         IncludeDeviceInfo(profile, *url_scope == policy::POLICY_SCOPE_USER);
}

std::optional<EventInitiatorInfo> GetActiveEventInitiatorInfoImpl(
    content::WebContents* web_contents,
    bool include_device_info) {
  if (!web_contents) {
    return std::nullopt;
  }

  if (std::optional<ExtensionInfo> extension_info =
          GetActiveExtensionInitiatorInfo(web_contents)) {
    EventInitiatorInfo initiator;
    *initiator.mutable_chrome_extension() = std::move(*extension_info);
    return initiator;
  }

#if BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)
  if (include_device_info) {
    if (std::optional<EventInitiatorInfo> cdp_initiator =
            CdpActorSessionTracker::GetEventInitiatorInfo(web_contents)) {
      return cdp_initiator;
    }
  }
#endif  // BUILDFLAG(ENTERPRISE_CDP_ATTRIBUTION)

  // An in-browser agent has the lowest precedence, so it is only reported when
  // no extension or CDP actor is attributed on the tab.
  if (std::optional<BrowserAgentInfo> agent_info =
          GetActiveGlicActorInitiatorInfo(web_contents)) {
    EventInitiatorInfo initiator;
    *initiator.mutable_browser_agent_info() = std::move(*agent_info);
    return initiator;
  }
  return std::nullopt;
}

}  // namespace

std::optional<ExtensionInfo> GetExtensionInitiatorInfo(
    download::DownloadItem* download) {
  // `ExtensionRegistry` is UI-thread only.
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(kEnterpriseExtensionActorAttribution) ||
      !download) {
    return std::nullopt;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)

  // TODO(crbug.com/570065942): Only downloads started via the
  // `chrome.downloads` API are attributed, since `DownloadedByExtension` is
  // only attached there. Downloads an extension triggers by other means (e.g.
  // navigation from an extension page, `<a download>`, content scripts) are not
  // detected yet.
  const extensions::DownloadedByExtension* by_ext =
      extensions::DownloadedByExtension::Get(download);
  if (!by_ext || by_ext->id().empty()) {
    return std::nullopt;
  }

  ExtensionInfo extension_info;

  // Always report the ID, even if the extension has since been uninstalled.
  extension_info.set_id(by_ext->id());

  // Set the name saved when the download started, as a fallback in case the
  // extension is no longer installed.
  if (!by_ext->name().empty()) {
    extension_info.set_name(by_ext->name());
  }

  // The BrowserContext is attached to `download` as user data. It may be null
  // if none was attached.
  PopulateInstalledExtensionInfo(
      content::DownloadItemUtils::GetBrowserContext(download), by_ext->id(),
      extension_info);

  return extension_info;
#else
  return std::nullopt;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

std::optional<ExtensionInfo> GetActiveExtensionInitiatorInfo(
    content::WebContents* web_contents) {
  // `ExtensionRegistry` is UI-thread only.
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(kEnterpriseExtensionActorAttribution) ||
      !web_contents) {
    return std::nullopt;
  }

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) && \
    BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  std::optional<extensions::ExtensionId> actor_id =
      ExtensionActorTracker::GetActiveActor(web_contents);
  if (!actor_id.has_value()) {
    return std::nullopt;
  }

  ExtensionInfo extension_info;
  extension_info.set_id(*actor_id);
  PopulateInstalledExtensionInfo(web_contents->GetBrowserContext(), *actor_id,
                                 extension_info);
  return extension_info;
#else
  return std::nullopt;
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) &&
        // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

std::optional<ExtensionInfo> GetActiveExtensionInitiatorInfo(
    content::BrowserContext* browser_context,
    SessionID tab_id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The feature check and build guard below match the `WebContents` overload
  // and are applied first so the tab walk is skipped whenever attribution is
  // impossible.
  if (!base::FeatureList::IsEnabled(kEnterpriseExtensionActorAttribution) ||
      !browser_context || !tab_id.is_valid()) {
    return std::nullopt;
  }

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) && \
    BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // The enterprise services that call this are shared between a profile and
  // its off-the-record profiles, so search both.
  content::WebContents* web_contents = nullptr;
  if (!extensions::ExtensionTabUtil::GetTabById(tab_id.id(), browser_context,
                                                /*include_incognito=*/true,
                                                &web_contents)) {
    return std::nullopt;
  }
  return GetActiveExtensionInitiatorInfo(web_contents);
#else
  return std::nullopt;
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) &&
        // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

std::optional<BrowserAgentInfo> GetActiveGlicActorInitiatorInfo(
    content::WebContents* web_contents) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(kEnterpriseGlicActorAttribution) ||
      !web_contents) {
    return std::nullopt;
  }

  // The actor service is not created for off-the-record profiles.
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(web_contents->GetBrowserContext());
  if (!actor_service) {
    return std::nullopt;
  }

  // Actor tasks act on tabs, so look up the tab's WebContents when
  // `web_contents` is embedded in it.
  const actor::ActorTask* task =
      actor_service->GetActingActorTaskForWebContents(
          web_contents->GetOutermostWebContents());
  if (!task ||
      task->source_info().type != actor::TaskSourceInfo::Client::kGlic) {
    return std::nullopt;
  }

  BrowserAgentInfo agent_info;
  agent_info.set_agent_type(BrowserAgentInfo::AGENT_TYPE_GEMINI_IN_CHROME);
  if (!task->id().is_null()) {
    agent_info.set_task_id(base::NumberToString(task->id().value()));
  }

  // For tasks started from a Gemini in Chrome conversation, the source ID is
  // the conversation ID. Experimental triggering stores a triggering context ID
  // there instead, which is not reported.
  const std::optional<std::string>& source_id = task->source_info().id;
  if (task->feature_mode() !=
          glic::mojom::FeatureMode::kExperimentalTriggering &&
      source_id.has_value() && !source_id->empty()) {
    agent_info.set_conversation_id(*source_id);
  }
  return agent_info;
}

std::optional<EventInitiatorInfo> GetActiveEventInitiatorInfo(
    content::WebContents* web_contents) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents) {
    return std::nullopt;
  }
  return GetActiveEventInitiatorInfoImpl(
      web_contents,
      CanIncludeDeviceInfoForReporting(web_contents->GetBrowserContext()));
}

std::optional<EventInitiatorInfo> GetActiveEventInitiatorInfo(
    content::BrowserContext* browser_context,
    SessionID tab_id) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!browser_context || !tab_id.is_valid()) {
    return std::nullopt;
  }

  const bool include_device_info =
      CanIncludeDeviceInfoForUrlLookup(browser_context);
  if (!base::FeatureList::IsEnabled(kEnterpriseExtensionActorAttribution) &&
      !base::FeatureList::IsEnabled(kEnterpriseGlicActorAttribution) &&
      !include_device_info) {
    return std::nullopt;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  content::WebContents* web_contents = nullptr;
  if (!extensions::ExtensionTabUtil::GetTabById(tab_id.id(), browser_context,
                                                /*include_incognito=*/true,
                                                &web_contents)) {
    return std::nullopt;
  }
  return GetActiveEventInitiatorInfoImpl(web_contents, include_device_info);
#else
  return std::nullopt;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

}  // namespace enterprise_connectors
