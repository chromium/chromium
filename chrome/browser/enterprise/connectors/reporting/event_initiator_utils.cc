// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/event_initiator_utils.h"

#include <optional>
#include <string>

#include "base/notreached.h"
#include "base/version.h"
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace enterprise_connectors {

namespace {

using ExtensionInfo =
    safe_browsing::ExtensionTelemetryReportRequest::ExtensionInfo;
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

}  // namespace

std::optional<ExtensionInfo> GetExtensionInitiatorInfo(
    download::DownloadItem* download) {
  // ExtensionRegistry is UI-thread only.
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!download) {
    return std::nullopt;
  }

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
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download);
  extensions::ExtensionRegistry* registry =
      browser_context ? extensions::ExtensionRegistry::Get(browser_context)
                      : nullptr;
  const extensions::Extension* extension =
      registry ? registry->GetInstalledExtension(by_ext->id()) : nullptr;
  if (!extension) {
    // Leave version and provenance unset so consumers can tell "unknown" from
    // "false".
    return extension_info;
  }

  // Overwrite the fallback with the current name, since the extension is still
  // installed and its name may have changed since the download started.
  extension_info.set_name(extension->name());
  extension_info.set_version(extension->version().GetString());

  // Provenance lets consumers tell a Web Store extension apart from a
  // sideloaded one that declares the same ID.
  extension_info.set_install_location(
      ToInstallLocation(extension->location()));
  extension_info.set_is_from_store(extension->from_webstore());

  return extension_info;
}

}  // namespace enterprise_connectors
