// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EVENT_INITIATOR_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EVENT_INITIATOR_UTILS_H_

#include <optional>

#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace content {
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

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EVENT_INITIATOR_UTILS_H_
