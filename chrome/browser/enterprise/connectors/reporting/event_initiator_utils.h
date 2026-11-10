// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EVENT_INITIATOR_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EVENT_INITIATOR_UTILS_H_

#include <optional>

#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

class SessionID;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace download {
class DownloadItem;
}  // namespace download

namespace enterprise_connectors {

// Returns an `ExtensionInfo` describing the extension that initiated
// `download`, as recorded by `extensions::DownloadedByExtension`.
//
// Returns `std::nullopt` if `download` is null or was not initiated via the
// `chrome.downloads` API (downloads an extension triggers by other means, such
// as navigation, are not attributed). For an installed extension,
// `ExtensionInfo` is filled with its ID, name, version, install location and
// whether it is from the Chrome Web Store. If `download` has no associated
// `BrowserContext` or the extension is no longer installed, only the ID and
// snapshotted name from `DownloadedByExtension` are set.
//
// Must be called on the UI thread.
std::optional<safe_browsing::ExtensionTelemetryReportRequest::ExtensionInfo>
GetExtensionInitiatorInfo(download::DownloadItem* download);

// Returns an `ExtensionInfo` describing the extension currently acting on
// `web_contents`, as recorded by `ExtensionActorTracker`.
//
// Returns `std::nullopt` if `web_contents` is null, the feature or reporting is
// off, or no extension is active. Name, version and provenance are unset if the
// extension is no longer installed.
//
// Must be called on the UI thread.
std::optional<safe_browsing::ExtensionTelemetryReportRequest::ExtensionInfo>
GetActiveExtensionInitiatorInfo(content::WebContents* web_contents);

// Same as above for the tab identified by `tab_id` in `browser_context`, for
// callers that only know a tab's `SessionID` (e.g. the real-time URL lookup
// service). Off-the-record tabs of `browser_context` are included.
//
// Returns `std::nullopt` if `browser_context` is null, the feature is off, no
// such tab exists, or under the same conditions as the `WebContents` overload.
//
// Must be called on the UI thread.
std::optional<safe_browsing::ExtensionTelemetryReportRequest::ExtensionInfo>
GetActiveExtensionInitiatorInfo(content::BrowserContext* browser_context,
                                SessionID tab_id);

// Returns a `BrowserAgentInfo` describing the Gemini in Chrome (Glic) actor
// task currently acting on the tab that hosts `web_contents`.
//
// Returns `std::nullopt` if `web_contents` is null, the
// `kEnterpriseGlicActorAttribution` feature is off, the profile has no actor
// service (e.g. it is off the record), or no Glic task is acting on the tab.
// Paused and finished tasks are not acting. `conversation_id` is only set for
// tasks started from a Gemini in Chrome conversation.
//
// Must be called on the UI thread.
std::optional<chrome::cros::reporting::proto::BrowserAgentInfo>
GetActiveGlicActorInitiatorInfo(content::WebContents* web_contents);

// Resolves `EventInitiatorInfo` for the entity currently acting on
// `web_contents`, checking `GetActiveExtensionInitiatorInfo()` first, then
// `CdpActorSessionTracker::GetEventInitiatorInfo()`, and falling back to
// `GetActiveGlicActorInitiatorInfo()`. Device-level CDP attribution
// (`ExternalProcessInfo` / `BrowserFeatureInfo`) is included only when
// `web_contents`'s security event reporting policy scope allows device info;
// otherwise only extension and Gemini in Chrome attribution are returned.
// Returns `std::nullopt` if `web_contents` is null or no eligible actor is
// currently attributed.
//
// Must be called on the UI thread.
std::optional<chrome::cros::reporting::proto::EventInitiatorInfo>
GetActiveEventInitiatorInfo(content::WebContents* web_contents);

// Same as above for the tab identified by `tab_id` in `browser_context`, for
// callers that only know a tab's `SessionID` (e.g. the real-time URL lookup
// service). Device-level CDP attribution (`ExternalProcessInfo` /
// `BrowserFeatureInfo`) is included only when `browser_context`'s real-time URL
// lookup policy scope allows device info; otherwise only extension and Gemini
// in Chrome attribution are returned. Returns `std::nullopt` if
// `browser_context` is null, no such tab exists, or no eligible actor is
// currently attributed.
//
// Must be called on the UI thread.
std::optional<chrome::cros::reporting::proto::EventInitiatorInfo>
GetActiveEventInitiatorInfo(content::BrowserContext* browser_context,
                            SessionID tab_id);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EVENT_INITIATOR_UTILS_H_
