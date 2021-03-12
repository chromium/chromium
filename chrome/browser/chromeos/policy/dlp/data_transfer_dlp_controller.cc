// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/syslog_logging.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/web_contents.h"
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

bool ShouldNotifyOnPaste(const ui::DataTransferEndpoint* const data_dst) {
  bool notify_on_paste = !data_dst || data_dst->notify_if_restricted();

  // Files Apps continuously reads the clipboard data which triggers a lot of
  // notifications while the user isn't actually initiating any copy/paste.
  // In BLOCK mode, data access by Files app will be denied silently.
  // In WARN mode, data access by Files app will be allowed silently.
  // TODO(crbug.com/1152475): Find a better way to handle File app.
  // When ClipboardHistory tries to read the clipboard we should allow it
  // silently.
  if (IsFilesApp(data_dst) || IsClipboardHistory(data_dst))
    notify_on_paste = false;

  return notify_on_paste;
}
}  // namespace

// static
void DataTransferDlpController::Init(const DlpRulesManager& dlp_rules_manager) {
  if (!HasInstance()) {
    DlpBooleanHistogram(dlp::kDataTransferControllerStartedUMA, true);
    new DataTransferDlpController(dlp_rules_manager);
  }
}

bool DataTransferDlpController::IsClipboardReadAllowed(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  DlpRulesManager::Level level =
      IsDataTransferAllowed(dlp_rules_manager_, data_src, data_dst);

  bool notify_on_paste = ShouldNotifyOnPaste(data_dst);

  bool is_read_allowed = true;

  switch (level) {
    case DlpRulesManager::Level::kBlock:
      if (notify_on_paste) {
        SYSLOG(INFO) << "DLP blocked paste from clipboard";
        NotifyBlockedPaste(data_src, data_dst);
      }
      is_read_allowed = false;
      break;

    case DlpRulesManager::Level::kWarn:
      if (notify_on_paste) {
        // In case the clipboard data is in warning mode, it will be allowed to
        // be shared with Arc, Crostini, and Plugin VM without waiting for the
        // user decision.
        if (data_dst && (data_dst->type() == ui::EndpointType::kArc ||
                         data_dst->type() == ui::EndpointType::kPluginVm ||
                         data_dst->type() == ui::EndpointType::kCrostini)) {
          WarnOnPaste(data_src, data_dst);
        } else if (ShouldCancelOnWarn(data_dst)) {
          is_read_allowed = false;
        } else if (!(data_dst && data_dst->IsUrlType()) &&
                   !ShouldPasteOnWarn(data_dst)) {
          SYSLOG(INFO) << "DLP warned on paste from clipboard";
          WarnOnPaste(data_src, data_dst);
          is_read_allowed = false;
        }
      }
      break;

    default:
      break;
  }
  DlpBooleanHistogram(dlp::kClipboardReadBlockedUMA, !is_read_allowed);
  return is_read_allowed;
}

void DataTransferDlpController::PasteIfAllowed(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(data_dst);
  DCHECK(data_dst->IsUrlType());

  if (!web_contents) {
    std::move(callback).Run(false);
    return;
  }

  DlpRulesManager::Level level =
      IsDataTransferAllowed(dlp_rules_manager_, data_src, data_dst);

  // If it's blocked, the data should be empty & PasteIfAllowed should not be
  // called.
  DCHECK_NE(level, DlpRulesManager::Level::kBlock);

  if (level == DlpRulesManager::Level::kAllow) {
    std::move(callback).Run(true);
    return;
  }

  DCHECK_EQ(level, DlpRulesManager::Level::kWarn);

  if (ShouldNotifyOnPaste(data_dst)) {
    if (ShouldPasteOnWarn(data_dst))
      std::move(callback).Run(true);
    else if (ShouldCancelOnWarn(data_dst))
      std::move(callback).Run(false);
    else
      WarnOnBlinkPaste(data_src, data_dst, web_contents, std::move(callback));
  } else {
    std::move(callback).Run(true);
  }
}

bool DataTransferDlpController::IsDragDropAllowed(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    const bool is_drop) {
  DlpRulesManager::Level level =
      IsDataTransferAllowed(dlp_rules_manager_, data_src, data_dst);

  if (level == DlpRulesManager::Level::kBlock && is_drop) {
    SYSLOG(INFO) << "DLP blocked drop of dragged data";
    NotifyBlockedPaste(data_src, data_dst);
  }

  const bool is_drop_allowed = level == DlpRulesManager::Level::kAllow;
  DlpBooleanHistogram(dlp::kDragDropBlockedUMA, !is_drop_allowed);
  return is_drop_allowed;
}

DataTransferDlpController::DataTransferDlpController(
    const DlpRulesManager& dlp_rules_manager)
    : dlp_rules_manager_(dlp_rules_manager) {}

DataTransferDlpController::~DataTransferDlpController() = default;

void DataTransferDlpController::NotifyBlockedPaste(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  clipboard_notifier_.NotifyBlockedAction(data_src, data_dst);
}

void DataTransferDlpController::WarnOnPaste(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  DCHECK(!(data_dst && data_dst->IsUrlType()));
  clipboard_notifier_.WarnOnPaste(data_src, data_dst);
}

void DataTransferDlpController::WarnOnBlinkPaste(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> paste_cb) {
  clipboard_notifier_.WarnOnBlinkPaste(data_src, data_dst, web_contents,
                                       std::move(paste_cb));
}

bool DataTransferDlpController::ShouldPasteOnWarn(
    const ui::DataTransferEndpoint* const data_dst) {
  return clipboard_notifier_.DidUserApproveDst(data_dst);
}

bool DataTransferDlpController::ShouldCancelOnWarn(
    const ui::DataTransferEndpoint* const data_dst) {
  return clipboard_notifier_.DidUserCancelDst(data_dst);
}

void DataTransferDlpController::NotifyBlockedDrop(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  drag_drop_notifier_.NotifyBlockedAction(data_src, data_dst);
}

}  // namespace policy
