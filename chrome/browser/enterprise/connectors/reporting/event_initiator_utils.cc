// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/event_initiator_utils.h"

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_actor_attribution_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/glic_enums.mojom.h"
#include "components/actor/core/task_source_info.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_source.h"
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
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"
#endif

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) && \
    BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/enterprise/connectors/reporting/extension_actor_tracker.h"
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

// Returns the `ExtensionInfo` to report for `extension_id`. The ID is always
// set, even if the extension is no longer installed. `fallback_name`, if not
// empty, is reported as the name when the extension is not found in
// `ExtensionRegistry`.
ExtensionInfo BuildExtensionInfo(
    content::BrowserContext* browser_context,
    const extensions::ExtensionId& extension_id,
    const std::string& fallback_name = std::string()) {
  ExtensionInfo extension_info;
  extension_info.set_id(extension_id);
  if (!fallback_name.empty()) {
    extension_info.set_name(fallback_name);
  }
  PopulateInstalledExtensionInfo(browser_context, extension_id, extension_info);
  return extension_info;
}

// Returns the ID of the extension whose content requested `download`, if the
// request initiator identifies one. `download` must not be null.
//
// Only navigation and renderer-initiated downloads are considered. A download
// started from browser UI (e.g. "Save as", "Save link as", retry) reuses the
// frame's origin or replays the initiator of an earlier request, so there the
// initiator does not say who started the download. The same holds for a reload
// or back/forward navigation, and for the built-in PDF Viewer, which navigates
// the links a user clicks in a PDF.
std::optional<extensions::ExtensionId> GetExtensionIdFromRequestInitiator(
    download::DownloadItem* download) {
  const download::DownloadSource source = download->GetDownloadSource();
  if (source != download::DownloadSource::NAVIGATION &&
      source != download::DownloadSource::FROM_RENDERER) {
    return std::nullopt;
  }

  // A reload or back/forward navigation keeps the original initiator, even
  // when the user starts it (e.g. by bypassing an interstitial). This matches
  // `ExtensionActorTracker::DidFinishNavigation()`.
  const ui::PageTransition transition = download->GetTransitionType();
  if (source == download::DownloadSource::NAVIGATION &&
      (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD) ||
       (transition & ui::PAGE_TRANSITION_FORWARD_BACK))) {
    return std::nullopt;
  }

  // TODO(crbug.com/570065942): A user clicking a download link inside an
  // extension page also produces `NAVIGATION` or `FROM_RENDERER` with the
  // extension's origin, so it is attributed to the extension here. Revisit if
  // user-clicked links on extension pages need to be excluded.

  // An opaque origin, such as a sandboxed extension page's, has an empty scheme
  // and does not match.
  const std::optional<url::Origin>& initiator = download->GetRequestInitiator();
  if (!initiator.has_value() ||
      initiator->scheme() != extensions::kExtensionScheme) {
    return std::nullopt;
  }

  // The built-in PDF Viewer navigates the links a user clicks in a PDF with
  // `chrome.tabs.update()` and `chrome.tabs.create()`, so it is the request
  // initiator of downloads the user started.
  // TODO(crbug.com/570065942): Other extensions that show content in a tab
  // and navigate for the user (e.g. third-party document viewers) are still
  // attributed here. Track these false positives and exclude them if needed.
  if (initiator->host() == extension_misc::kPdfExtensionId) {
    return std::nullopt;
  }
  return initiator->host();
}

// Returns the strongest extension signal on a download that is not attributed
// to an extension. Used for metrics only: the tab's actor does not attribute
// the download yet. `download` must not be null.
ExtensionDownloadAttributionSignal ClassifyUnattributedDownload(
    download::DownloadItem* download) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  // TODO(crbug.com/570065942): Prefer the actor at download start once it is
  // snapshotted onto the `DownloadItem`. The tab's current actor can differ
  // from it: the actor may have been set, cleared or replaced since the
  // download started.
  // Use the original WebContents: the actor is per-tab state and can outlive
  // the primary page that started the download.
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetOriginalWebContents(download);
  if (web_contents &&
      ExtensionActorTracker::GetActiveActor(web_contents).has_value()) {
    return ExtensionDownloadAttributionSignal::kTabActor;
  }
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

  return ExtensionDownloadAttributionSignal::kNone;
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

  // The `BrowserContext` is attached to `download` as user data. It may be null
  // if none was attached.
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);

  // A download started via the `chrome.downloads` API.
  const extensions::DownloadedByExtension* by_ext =
      extensions::DownloadedByExtension::Get(download);
  if (by_ext && !by_ext->id().empty()) {
    base::UmaHistogramEnumeration(
        kExtensionActorLookupDownloadResultHistogram,
        ExtensionDownloadAttributionSignal::kDownloadedByExtension);
    // The name saved when the download started is the fallback, in case the
    // extension is no longer installed.
    return BuildExtensionInfo(browser_context, by_ext->id(), by_ext->name());
  }

  // A download that extension content requested another way, e.g. by
  // navigating to the file. No name was saved when it started.
  // TODO(crbug.com/554183135): Download warning bypass events also look up the
  // initiator here, so they name the extension that started the download (for
  // both `DownloadedByExtension` and this fallback). Revisit whether
  // user-bypassed events should omit the initiator.
  if (std::optional<extensions::ExtensionId> extension_id =
          GetExtensionIdFromRequestInitiator(download)) {
    base::UmaHistogramEnumeration(
        kExtensionActorLookupDownloadResultHistogram,
        ExtensionDownloadAttributionSignal::kExtensionRequestInitiator);
    return BuildExtensionInfo(browser_context, *extension_id);
  }

  // TODO(crbug.com/570065942): A download an extension triggers by acting on a
  // web page (e.g. clicking a link from a content script or through
  // `chrome.debugger`) has a web or empty request initiator and is not
  // attributed yet.
  base::UmaHistogramEnumeration(kExtensionActorLookupDownloadResultHistogram,
                                ClassifyUnattributedDownload(download));
  return std::nullopt;
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
    base::UmaHistogramEnumeration(
        kExtensionActorLookupWebContentsResultHistogram,
        ExtensionActorAttributionResult::kNoActor);
    return std::nullopt;
  }
  base::UmaHistogramEnumeration(kExtensionActorLookupWebContentsResultHistogram,
                                ExtensionActorAttributionResult::kAttributed);

  return BuildExtensionInfo(web_contents->GetBrowserContext(), *actor_id);
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
