// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/data_transfer_dlp_controller.h"

#include <string>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/syslog_logging.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

namespace policy {

namespace {

// Set |kSkipReportingTimeout| to 50 ms because:
// - at 5 ms DataTransferDlpBlinkBrowserTest.Reporting test starts to be flaky
// - 100 ms is approximately the time a human needs to press a key.
// See DataTransferDlpController::LastReportedEndpoints struct for details.
const base::TimeDelta kSkipReportingTimeout =
    base::TimeDelta::FromMilliseconds(50);

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
    const ui::DataTransferEndpoint* const data_dst,
    const absl::optional<size_t> size) {
  DlpRulesManager::Level level =
      IsDataTransferAllowed(data_src, data_dst, size);

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
    const absl::optional<size_t> size,
    content::RenderFrameHost* rfh,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(data_dst);
  DCHECK(data_dst->IsUrlType());

  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    std::move(callback).Run(false);
    return;
  }

  DlpRulesManager::Level level =
      IsDataTransferAllowed(data_src, data_dst, size);

  // If it's blocked, the data should be empty & PasteIfAllowed should not be
  // called.
  DCHECK_NE(level, DlpRulesManager::Level::kBlock);

  if (level == DlpRulesManager::Level::kAllow ||
      level == DlpRulesManager::Level::kReport) {
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
      IsDataTransferAllowed(data_src, data_dst, absl::nullopt);

  if (level == DlpRulesManager::Level::kBlock && is_drop) {
    SYSLOG(INFO) << "DLP blocked drop of dragged data";
    NotifyBlockedDrop(data_src, data_dst);
  }

  const bool is_drop_allowed = (level == DlpRulesManager::Level::kAllow) ||
                               (level == DlpRulesManager::Level::kReport);
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

bool DataTransferDlpController::ShouldSkipReporting(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    base::TimeTicks curr_time) {
  // Skip reporting for destination endpoints that has |notify_if_restricted()|
  // set to false as clipboard API calls for them either aren't triggered by a
  // user action or come from some workarounds. However, allow reporting for
  // unknown destinations (data_dst == nullptr).
  if (data_dst && !data_dst->notify_if_restricted())
    return true;

  // In theory, there is no need to check for data source and destination if
  // |kSkipReportingTimeout| is shorter than human reaction time.
  bool is_same_src = data_src ? *data_src == last_reported_.data_src
                              : !last_reported_.data_src.has_value();
  bool is_same_dst = data_dst ? *data_dst == last_reported_.data_dst
                              : !last_reported_.data_dst.has_value();
  return is_same_src && is_same_dst &&
         curr_time - last_reported_.time < kSkipReportingTimeout;
}

template <typename T>
void DataTransferDlpController::ReportEvent(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    const std::string& src_pattern,
    const T& dst,
    DlpRulesManager::Level level) {
  if (level != DlpRulesManager::Level::kReport &&
      level != DlpRulesManager::Level::kBlock)
    return;

  auto* reporting_manager = dlp_rules_manager_.GetReportingManager();
  if (!reporting_manager)
    return;

  base::TimeTicks curr_time = base::TimeTicks::Now();
  if (ShouldSkipReporting(data_src, data_dst, curr_time))
    return;
  last_reported_.data_src =
      base::OptionalFromPtr<ui::DataTransferEndpoint>(data_src);
  last_reported_.data_dst =
      base::OptionalFromPtr<ui::DataTransferEndpoint>(data_dst);
  last_reported_.time = curr_time;

  reporting_manager->ReportEvent(
      src_pattern, dst, DlpRulesManager::Restriction::kClipboard, level);
}

DlpRulesManager::Level DataTransferDlpController::IsDataTransferAllowed(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    const absl::optional<size_t> size) {
  if (size.has_value() &&
      *size < dlp_rules_manager_.GetClipboardCheckSizeLimitInBytes()) {
    return DlpRulesManager::Level::kAllow;
  }

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
      std::string src_pattern;
      std::string dst_pattern;
      // Passing empty URL will return restricted if there's a rule restricting
      // the src against any dst (*), otherwise it will return ALLOW.
      level = dlp_rules_manager_.IsRestrictedDestination(
          src_url, GURL(), DlpRulesManager::Restriction::kClipboard,
          &src_pattern, &dst_pattern);
      ReportEvent(data_src, data_dst, src_pattern, dst_pattern, level);
      break;
    }

    case ui::EndpointType::kUrl: {
      GURL dst_url = data_dst->origin()->GetURL();
      std::string src_pattern;
      std::string dst_pattern;
      level = dlp_rules_manager_.IsRestrictedDestination(
          src_url, dst_url, DlpRulesManager::Restriction::kClipboard,
          &src_pattern, &dst_pattern);
      if (!IsFilesApp(data_dst))
        ReportEvent(data_src, data_dst, src_pattern, dst_pattern, level);
      break;
    }

    case ui::EndpointType::kCrostini: {
      std::string src_pattern;
      level = dlp_rules_manager_.IsRestrictedComponent(
          src_url, DlpRulesManager::Component::kCrostini,
          DlpRulesManager::Restriction::kClipboard, &src_pattern);
      ReportEvent(data_src, data_dst, src_pattern,
                  DlpRulesManager::Component::kCrostini, level);
      break;
    }

    case ui::EndpointType::kPluginVm: {
      std::string src_pattern;
      level = dlp_rules_manager_.IsRestrictedComponent(
          src_url, DlpRulesManager::Component::kPluginVm,
          DlpRulesManager::Restriction::kClipboard, &src_pattern);
      ReportEvent(data_src, data_dst, src_pattern,
                  DlpRulesManager::Component::kPluginVm, level);
      break;
    }

    case ui::EndpointType::kArc: {
      std::string src_pattern;
      level = dlp_rules_manager_.IsRestrictedComponent(
          src_url, DlpRulesManager::Component::kArc,
          DlpRulesManager::Restriction::kClipboard, &src_pattern);
      ReportEvent(data_src, data_dst, src_pattern,
                  DlpRulesManager::Component::kArc, level);
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

DataTransferDlpController::LastReportedEndpoints::LastReportedEndpoints() =
    default;

DataTransferDlpController::LastReportedEndpoints::~LastReportedEndpoints() =
    default;

}  // namespace policy
