// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"

#include "base/notreached.h"
#include "base/syslog_logging.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "extensions/common/constants.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace policy {

namespace {

bool IsFilesApp(const ui::DataTransferEndpoint* const data_dst) {
  if (!data_dst || !data_dst->IsUrlType())
    return false;

  GURL url = data_dst->origin()->GetURL();
  return url.has_scheme() && url.SchemeIs(extensions::kExtensionScheme) &&
         url.has_host() && url.host() == extension_misc::kFilesManagerAppId;
}

bool IsClipboardHistory(const ui::DataTransferEndpoint* const data_dst) {
  return data_dst && data_dst->type() == ui::EndpointType::kClipboardHistory;
}

DlpRulesManager::Level IsDataTransferAllowed(
    const DlpRulesManager& dlp_rules_manager,
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  if (!data_src || !data_src->IsUrlType()) {  // Currently we only handle URLs.
    return DlpRulesManager::Level::kAllow;
  }

  const GURL src_url = data_src->origin()->GetURL();
  ui::EndpointType dst_type =
      data_dst ? data_dst->type() : ui::EndpointType::kDefault;

  DlpRulesManager::Level level = DlpRulesManager::Level::kAllow;

  switch (dst_type) {
    case ui::EndpointType::kDefault:
    case ui::EndpointType::kUnknownVm:
    case ui::EndpointType::kBorealis: {
      // Passing empty URL will return restricted if there's a rule restricting
      // the src against any dst (*), otherwise it will return ALLOW.
      level = dlp_rules_manager.IsRestrictedDestination(
          src_url, GURL(), DlpRulesManager::Restriction::kClipboard);
      break;
    }

    case ui::EndpointType::kUrl: {
      GURL dst_url = data_dst->origin()->GetURL();
      level = dlp_rules_manager.IsRestrictedDestination(
          src_url, dst_url, DlpRulesManager::Restriction::kClipboard);
      break;
    }

    case ui::EndpointType::kCrostini: {
      level = dlp_rules_manager.IsRestrictedComponent(
          src_url, DlpRulesManager::Component::kCrostini,
          DlpRulesManager::Restriction::kClipboard);
      break;
    }

    case ui::EndpointType::kPluginVm: {
      level = dlp_rules_manager.IsRestrictedComponent(
          src_url, DlpRulesManager::Component::kPluginVm,
          DlpRulesManager::Restriction::kClipboard);
      break;
    }

    case ui::EndpointType::kArc: {
      level = dlp_rules_manager.IsRestrictedComponent(
          src_url, DlpRulesManager::Component::kArc,
          DlpRulesManager::Restriction::kClipboard);
      break;
    }

    case ui::EndpointType::kClipboardHistory: {
      level = DlpRulesManager::Level::kAllow;
      break;
    }

    default:
      NOTREACHED();
  }

  return level;
}

}  // namespace

// static
void DataTransferDlpController::Init(const DlpRulesManager& dlp_rules_manager) {
  if (!HasInstance())
    new DataTransferDlpController(dlp_rules_manager);
}

bool DataTransferDlpController::IsClipboardReadAllowed(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  DlpRulesManager::Level level =
      IsDataTransferAllowed(dlp_rules_manager_, data_src, data_dst);

  bool notify_on_paste = !data_dst || data_dst->notify_if_restricted();
  // Files Apps continuously reads the clipboard data which triggers a lot of
  // notifications while the user isn't actually initiating any copy/paste.
  // TODO(crbug.com/1152475): Find a better way to handle File app.
  // When ClipboardHistory tries to read the clipboard we should allow it
  // silently.
  if (IsFilesApp(data_dst) || IsClipboardHistory(data_dst))
    notify_on_paste = false;

  if (level == DlpRulesManager::Level::kBlock && notify_on_paste) {
    SYSLOG(INFO) << "DLP blocked paste from clipboard";
    DoNotifyBlockedPaste(data_src, data_dst);
  }

  return level == DlpRulesManager::Level::kAllow;
}

bool DataTransferDlpController::IsDragDropAllowed(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    const bool is_drop) {
  DlpRulesManager::Level level =
      IsDataTransferAllowed(dlp_rules_manager_, data_src, data_dst);

  if (level == DlpRulesManager::Level::kBlock && is_drop) {
    SYSLOG(INFO) << "DLP blocked drop of dragged data";
    DoNotifyBlockedPaste(data_src, data_dst);
  }

  return level == DlpRulesManager::Level::kAllow;
}

DataTransferDlpController::DataTransferDlpController(
    const DlpRulesManager& dlp_rules_manager)
    : dlp_rules_manager_(dlp_rules_manager) {}

DataTransferDlpController::~DataTransferDlpController() = default;

void DataTransferDlpController::DoNotifyBlockedPaste(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  helper_.NotifyBlockedPaste(data_src, data_dst);
}

}  // namespace policy
