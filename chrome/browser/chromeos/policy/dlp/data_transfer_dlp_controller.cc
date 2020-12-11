// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"

#include <vector>

#include "base/notreached.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "extensions/common/constants.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace policy {

namespace {

bool IsFilesApp(const GURL& url) {
  return url.has_scheme() && url.SchemeIs(extensions::kExtensionScheme) &&
         url.has_host() && url.host() == extension_misc::kFilesManagerAppId;
}

DlpRulesManager::Level IsDstRestricted(const GURL& src, const GURL& dst) {
  // Safe to not check for nullptr as DataTransferDlpController is owned by
  // DlpRulesManager.
  return DlpRulesManagerFactory::GetForPrimaryProfile()
      ->IsRestrictedDestination(src, dst,
                                DlpRulesManager::Restriction::kClipboard);
}

DlpRulesManager::Level IsCrostiniRestricted(const GURL& src) {
  // Safe to not check for nullptr as DataTransferDlpController is owned by
  // DlpRulesManager.
  return DlpRulesManagerFactory::GetForPrimaryProfile()->IsRestrictedComponent(
      src, DlpRulesManager::Component::kCrostini,
      DlpRulesManager::Restriction::kClipboard);
}

DlpRulesManager::Level IsPluginVmRestricted(const GURL& src) {
  // Safe to not check for nullptr as DataTransferDlpController is owned by
  // DlpRulesManager.
  return DlpRulesManagerFactory::GetForPrimaryProfile()->IsRestrictedComponent(
      src, DlpRulesManager::Component::kPluginVm,
      DlpRulesManager::Restriction::kClipboard);
}

DlpRulesManager::Level IsArcRestricted(const GURL& src) {
  // Safe to not check for nullptr as DataTransferDlpController is owned by
  // DlpRulesManager.
  return DlpRulesManagerFactory::GetForPrimaryProfile()->IsRestrictedComponent(
      src, DlpRulesManager::Component::kArc,
      DlpRulesManager::Restriction::kClipboard);
}

}  // namespace

// static
void DataTransferDlpController::Init() {
  if (!HasInstance())
    new DataTransferDlpController();
}

bool DataTransferDlpController::IsDataReadAllowed(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  if (!data_src || !data_src->IsUrlType()) {  // Currently we only handle URLs.
    return true;
  }

  const GURL src_url = data_src->origin()->GetURL();
  DlpRulesManager::Level level = DlpRulesManager::Level::kAllow;
  bool notify_on_paste = !data_dst || data_dst->notify_if_restricted();
  ui::EndpointType dst_type =
      data_dst ? data_dst->type() : ui::EndpointType::kDefault;

  switch (dst_type) {
    case ui::EndpointType::kDefault:
    case ui::EndpointType::kUnknownVm:
    case ui::EndpointType::kBorealis:
      // Passing empty URL will return restricted if there's a rule restricting
      // the src against any dst (*), otherwise it will return ALLOW.
      level = IsDstRestricted(src_url, GURL());
      break;

    case ui::EndpointType::kUrl: {
      GURL dst_url = data_dst->origin()->GetURL();
      level = IsDstRestricted(src_url, dst_url);
      // Files Apps continously reads the clipboard data which triggers a lot of
      // notifications while the user isn't actually initiating any copy/paste.
      // TODO(crbug.com/1152475): Find a better way to handle File app.
      if (IsFilesApp(dst_url))
        notify_on_paste = false;
      break;
    }

    case ui::EndpointType::kCrostini:
      level = IsCrostiniRestricted(src_url);
      break;

    case ui::EndpointType::kPluginVm:
      level = IsPluginVmRestricted(src_url);
      break;

    case ui::EndpointType::kArc:
      level = IsArcRestricted(src_url);
      break;

    case ui::EndpointType::kClipboardHistory:
      // When ClipboardHistory tries to read the clipboard we should allow it
      // silently.
      notify_on_paste = false;
      break;

    default:
      NOTREACHED();
  }

  if (level == DlpRulesManager::Level::kBlock && notify_on_paste) {
    DoNotifyBlockedPaste(data_src, data_dst);
  }

  return level == DlpRulesManager::Level::kAllow;
}

DataTransferDlpController::DataTransferDlpController() = default;

DataTransferDlpController::~DataTransferDlpController() = default;

void DataTransferDlpController::DoNotifyBlockedPaste(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  helper_.NotifyBlockedPaste(data_src, data_dst);
}

}  // namespace policy
