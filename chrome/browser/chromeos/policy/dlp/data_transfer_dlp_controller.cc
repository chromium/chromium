// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"

#include <string>

#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/file_manager/url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace policy {

namespace {

// Set |kSkipReportingTimeout| to 75 ms because:
// - at 5 ms DataTransferDlpBlinkBrowserTest.Reporting test starts to be flaky
// - 100 ms is approximately the time a human needs to press a key.
// See DataTransferDlpController::LastReportedEndpoints struct for details.
// TODO(crbug.com/1381095): Improve timeout tuning since duplicate warning
// proceed events delay is very variable.
const base::TimeDelta kSkipReportingTimeout = base::Milliseconds(75);

// In case the clipboard data is in warning mode, it will be allowed to
// be shared with Arc, Crostini, and Plugin VM without waiting for the
// user decision.
bool IsVM(const ui::EndpointType type) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return type == ui::EndpointType::kArc ||
         type == ui::EndpointType::kPluginVm ||
         type == ui::EndpointType::kCrostini;
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Returns true if `endpoint` has no value or its type is kDefault.
bool IsNullEndpoint(const absl::optional<ui::DataTransferEndpoint>& endpoint) {
  return !endpoint.has_value() ||
         endpoint->type() == ui::EndpointType::kDefault;
}

bool IsFilesApp(const ui::DataTransferEndpoint* const data_dst) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!data_dst || !data_dst->IsUrlType())
    return false;

  GURL url = *data_dst->GetURL();
  // TODO(b/207576430): Once Files Extension is removed, remove this condition.
  bool is_files_extension =
      url.has_scheme() && url.SchemeIs(extensions::kExtensionScheme) &&
      url.has_host() && url.host() == extension_misc::kFilesManagerAppId;
  bool is_files_swa = url.has_scheme() &&
                      url.SchemeIs(content::kChromeUIScheme) &&
                      url.has_host() &&
                      url.host() == ash::file_manager::kChromeUIFileManagerHost;

  return is_files_extension || is_files_swa;
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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

DlpRulesManager::Level IsDataTransferAllowed(
    const DlpRulesManager& dlp_rules_manager,
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    const absl::optional<size_t> size,
    std::string* src_pattern,
    std::string* dst_pattern) {
  if (size.has_value() &&
      *size < dlp_rules_manager.GetClipboardCheckSizeLimitInBytes()) {
    return DlpRulesManager::Level::kAllow;
  }

  if (!data_src || !data_src->IsUrlType()) {  // Currently we only handle URLs.
    return DlpRulesManager::Level::kAllow;
  }

  const GURL src_url = *data_src->GetURL();
  ui::EndpointType dst_type =
      data_dst ? data_dst->type() : ui::EndpointType::kDefault;

  DlpRulesManager::Level level = DlpRulesManager::Level::kAllow;

  switch (dst_type) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case ui::EndpointType::kCrostini: {
      level = dlp_rules_manager.IsRestrictedComponent(
          src_url, DlpRulesManager::Component::kCrostini,
          DlpRulesManager::Restriction::kClipboard, src_pattern);
      break;
    }

    case ui::EndpointType::kPluginVm: {
      level = dlp_rules_manager.IsRestrictedComponent(
          src_url, DlpRulesManager::Component::kPluginVm,
          DlpRulesManager::Restriction::kClipboard, src_pattern);
      break;
    }

    case ui::EndpointType::kArc: {
      level = dlp_rules_manager.IsRestrictedComponent(
          src_url, DlpRulesManager::Component::kArc,
          DlpRulesManager::Restriction::kClipboard, src_pattern);
      break;
    }

    case ui::EndpointType::kLacros: {
      // Return ALLOW for Lacros destinations, as Lacros itself will make DLP
      // checks.
      level = DlpRulesManager::Level::kAllow;
      break;
    }

    case ui::EndpointType::kUnknownVm:
    case ui::EndpointType::kBorealis:
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    case ui::EndpointType::kDefault: {
      // Passing empty URL will return restricted if there's a rule restricting
      // the src against any dst (*), otherwise it will return ALLOW.
      level = dlp_rules_manager.IsRestrictedDestination(
          src_url, GURL(), DlpRulesManager::Restriction::kClipboard,
          src_pattern, dst_pattern);
      break;
    }

    case ui::EndpointType::kUrl: {
      GURL dst_url = *data_dst->GetURL();
      level = dlp_rules_manager.IsRestrictedDestination(
          src_url, dst_url, DlpRulesManager::Restriction::kClipboard,
          src_pattern, dst_pattern);
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

// Reports warning proceeded events and paste the copied text according to
// `should_proceed`. It is used as a callback in `PasteIfAllowed` to handle
// warning proceeded clipboard events.
void MaybeReportWarningProceededEventAndPaste(
    base::OnceCallback<void(void)> reporting_cb,
    base::OnceCallback<void(bool)> paste_cb,
    bool should_proceed) {
  if (should_proceed) {
    std::move(reporting_cb).Run();
  }
  std::move(paste_cb).Run(should_proceed);
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
  std::string src_pattern;
  std::string dst_pattern;
  DlpRulesManager::Level level = IsDataTransferAllowed(
      dlp_rules_manager_, data_src, data_dst, size, &src_pattern, &dst_pattern);

  MaybeReportEvent(data_src, data_dst, src_pattern, dst_pattern, level,
                   /*is_clipboard_event=*/true);

  bool notify_on_paste = ShouldNotifyOnPaste(data_dst);

  bool is_read_allowed = true;

  switch (level) {
    case DlpRulesManager::Level::kBlock:
      if (notify_on_paste) {
        NotifyBlockedPaste(data_src, data_dst);
      }
      is_read_allowed = false;
      break;
    case DlpRulesManager::Level::kWarn:

      if (data_dst && IsVM(data_dst->type())) {
        if (notify_on_paste) {
          ReportEvent(data_src, data_dst, src_pattern, dst_pattern,
                      DlpRulesManager::Level::kWarn,
                      /*is_clipboard_event=*/true);

          auto reporting_cb = base::BindRepeating(
              &DataTransferDlpController::ReportWarningProceededEvent,
              weak_ptr_factory_.GetWeakPtr(), base::OptionalFromPtr(data_src),
              base::OptionalFromPtr(data_dst), src_pattern, dst_pattern,
              /*is_clipboard_event=*/true);

          WarnOnPaste(data_src, data_dst, std::move(reporting_cb));
        }
      } else if (ShouldCancelOnWarn(data_dst)) {
        is_read_allowed = false;
      } else if (notify_on_paste && !(data_dst && data_dst->IsUrlType()) &&
                 !ShouldPasteOnWarn(data_dst)) {
        ReportEvent(data_src, data_dst, src_pattern, dst_pattern,
                    DlpRulesManager::Level::kWarn,
                    /*is_clipboard_event=*/true);

        auto reporting_cb = base::BindRepeating(
            &DataTransferDlpController::ReportWarningProceededEvent,
            weak_ptr_factory_.GetWeakPtr(), base::OptionalFromPtr(data_src),
            base::OptionalFromPtr(data_dst), src_pattern, dst_pattern,
            /*is_clipboard_event=*/true);

        WarnOnPaste(data_src, data_dst, std::move(reporting_cb));
        is_read_allowed = false;
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
    base::OnceCallback<void(bool)> paste_cb) {
  DCHECK(data_dst);
  DCHECK(data_dst->IsUrlType());

  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    std::move(paste_cb).Run(false);
    return;
  }

  std::string src_pattern;
  std::string dst_pattern;
  DlpRulesManager::Level level = IsDataTransferAllowed(
      dlp_rules_manager_, data_src, data_dst, size, &src_pattern, &dst_pattern);
  // Reporting doesn't need to be added here because PasteIfAllowed is called
  // after IsClipboardReadAllowed

  // If it's blocked, the data should be empty & PasteIfAllowed should not be
  // called.
  DCHECK_NE(level, DlpRulesManager::Level::kBlock);

  if (level == DlpRulesManager::Level::kAllow ||
      level == DlpRulesManager::Level::kReport) {
    std::move(paste_cb).Run(true);
    return;
  }

  DCHECK_EQ(level, DlpRulesManager::Level::kWarn);

  if (ShouldPasteOnWarn(data_dst)) {
    if (ShouldNotifyOnPaste(data_dst)) {
      ReportWarningProceededEvent(base::OptionalFromPtr(data_src),
                                  base::OptionalFromPtr(data_dst), src_pattern,
                                  dst_pattern,
                                  /*is_clipboard_event=*/true);
    }
    std::move(paste_cb).Run(true);
  } else if (ShouldCancelOnWarn(data_dst)) {
    std::move(paste_cb).Run(false);
  } else {
    if (ShouldNotifyOnPaste(data_dst)) {
      ReportEvent(data_src, data_dst, src_pattern, dst_pattern,
                  DlpRulesManager::Level::kWarn, /*is_clipboard_event=*/true);

      auto reporting_cb = base::BindOnce(
          &DataTransferDlpController::ReportWarningProceededEvent,
          weak_ptr_factory_.GetWeakPtr(), base::OptionalFromPtr(data_src),
          base::OptionalFromPtr(data_dst), src_pattern, dst_pattern,
          /*is_clipboard_event=*/true);

      auto report_and_paste_cb =
          base::BindOnce(&MaybeReportWarningProceededEventAndPaste,
                         std::move(reporting_cb), std::move(paste_cb));

      WarnOnBlinkPaste(data_src, data_dst, web_contents,
                       std::move(report_and_paste_cb));
    } else {
      std::move(paste_cb).Run(true);
    }
  }
}

void DataTransferDlpController::DropIfAllowed(
    const ui::DataTransferEndpoint* data_src,
    const ui::DataTransferEndpoint* data_dst,
    base::OnceClosure drop_cb) {
  std::string src_pattern;
  std::string dst_pattern;
  DlpRulesManager::Level level =
      IsDataTransferAllowed(dlp_rules_manager_, data_src, data_dst,
                            absl::nullopt, &src_pattern, &dst_pattern);

  MaybeReportEvent(data_src, data_dst, src_pattern, dst_pattern, level,
                   /*is_clipboard_event*/ false);

  switch (level) {
    case DlpRulesManager::Level::kBlock:
      NotifyBlockedDrop(data_src, data_dst);
      break;

    case DlpRulesManager::Level::kWarn:
      WarnOnDrop(data_src, data_dst, std::move(drop_cb));
      break;

    case DlpRulesManager::Level::kAllow:
      [[fallthrough]];
    case DlpRulesManager::Level::kReport:
      std::move(drop_cb).Run();
      break;

    case DlpRulesManager::Level::kNotSet:
      NOTREACHED();
  }

  const bool is_drop_allowed = (level == DlpRulesManager::Level::kAllow) ||
                               (level == DlpRulesManager::Level::kReport);
  DlpBooleanHistogram(dlp::kDragDropBlockedUMA, !is_drop_allowed);
}

DataTransferDlpController::DataTransferDlpController(
    const DlpRulesManager& dlp_rules_manager)
    : dlp_rules_manager_(dlp_rules_manager) {}

DataTransferDlpController::~DataTransferDlpController() = default;

base::TimeDelta DataTransferDlpController::GetSkipReportingTimeout() {
  return kSkipReportingTimeout;
}

void DataTransferDlpController::NotifyBlockedPaste(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst) {
  clipboard_notifier_.NotifyBlockedAction(data_src, data_dst);
}

void DataTransferDlpController::WarnOnPaste(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    base::RepeatingCallback<void()> reporting_cb) {
  DCHECK(!(data_dst && data_dst->IsUrlType()));
  clipboard_notifier_.WarnOnPaste(data_src, data_dst, std::move(reporting_cb));
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

void DataTransferDlpController::WarnOnDrop(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    base::OnceClosure drop_cb) {
  drag_drop_notifier_.WarnOnDrop(data_src, data_dst, std::move(drop_cb));
}

bool DataTransferDlpController::ShouldSkipReporting(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    bool is_warning_proceeded,
    base::TimeTicks curr_time) {
  // Skip reporting for destination endpoints which don't notify the user
  // because it's not originating from a user action.
  if (!ShouldNotifyOnPaste(data_dst))
    return true;

  // In theory, there is no need to check for data source and destination if
  // |kSkipReportingTimeout| is shorter than human reaction time.
  bool is_same_src = data_src ? *data_src == last_reported_.data_src
                              : IsNullEndpoint(last_reported_.data_src);
  bool is_same_dst = data_dst ? *data_dst == last_reported_.data_dst
                              : IsNullEndpoint(last_reported_.data_dst);
  bool is_same_mode =
      last_reported_.is_warning_proceeded.has_value() &&
      is_warning_proceeded == last_reported_.is_warning_proceeded.value();

  if (is_same_src && is_same_dst && is_same_mode) {
    base::TimeDelta time_diff = curr_time - last_reported_.time;
    base::UmaHistogramTimes(
        GetDlpHistogramPrefix() + dlp::kDataTransferReportingTimeDiffUMA,
        time_diff);

    return time_diff < GetSkipReportingTimeout();
  }
  return false;
}

void DataTransferDlpController::ReportEvent(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    const std::string& src_pattern,
    const std::string& dst_pattern,
    DlpRulesManager::Level level,
    bool is_clipboard_event) {
  auto* reporting_manager = dlp_rules_manager_.GetReportingManager();
  if (!reporting_manager)
    return;

  if (is_clipboard_event) {
    base::TimeTicks curr_time = base::TimeTicks::Now();
    if (ShouldSkipReporting(data_src, data_dst, /*is_warning_proceeded=*/false,
                            curr_time))
      return;
    last_reported_.data_src =
        base::OptionalFromPtr<ui::DataTransferEndpoint>(data_src);
    last_reported_.data_dst =
        base::OptionalFromPtr<ui::DataTransferEndpoint>(data_dst);
    last_reported_.time = curr_time;
    last_reported_.is_warning_proceeded = false;
  }

  ui::EndpointType dst_type =
      data_dst ? data_dst->type() : ui::EndpointType::kDefault;
  switch (dst_type) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case ui::EndpointType::kCrostini:
      reporting_manager->ReportEvent(
          src_pattern, DlpRulesManager::Component::kCrostini,
          DlpRulesManager::Restriction::kClipboard, level);
      break;

    case ui::EndpointType::kPluginVm:
      reporting_manager->ReportEvent(
          src_pattern, DlpRulesManager::Component::kPluginVm,
          DlpRulesManager::Restriction::kClipboard, level);
      break;

    case ui::EndpointType::kArc:
      reporting_manager->ReportEvent(
          src_pattern, DlpRulesManager::Component::kArc,
          DlpRulesManager::Restriction::kClipboard, level);
      break;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    default:
      reporting_manager->ReportEvent(src_pattern, dst_pattern,
                                     DlpRulesManager::Restriction::kClipboard,
                                     level);
      break;
  }
}

void DataTransferDlpController::MaybeReportEvent(
    const ui::DataTransferEndpoint* const data_src,
    const ui::DataTransferEndpoint* const data_dst,
    const std::string& src_pattern,
    const std::string& dst_pattern,
    DlpRulesManager::Level level,
    bool is_clipboard_event) {
  if (level == DlpRulesManager::Level::kReport ||
      level == DlpRulesManager::Level::kBlock) {
    ReportEvent(data_src, data_dst, src_pattern, dst_pattern, level,
                is_clipboard_event);
  }
}

void DataTransferDlpController::ReportWarningProceededEvent(
    const absl::optional<ui::DataTransferEndpoint> maybe_data_src,
    const absl::optional<ui::DataTransferEndpoint> maybe_data_dst,
    const std::string& src_pattern,
    const std::string& dst_pattern,
    bool is_clipboard_event) {
  auto* reporting_manager = dlp_rules_manager_.GetReportingManager();

  if (!reporting_manager)
    return;

  const ui::DataTransferEndpoint* data_dst =
      maybe_data_dst.has_value() ? &maybe_data_dst.value() : nullptr;

  if (is_clipboard_event) {
    base::TimeTicks curr_time = base::TimeTicks::Now();

    const ui::DataTransferEndpoint* data_src =
        maybe_data_src.has_value() ? &maybe_data_src.value() : nullptr;

    if (ShouldSkipReporting(data_src, data_dst, /*is_warning_proceeded=*/true,
                            curr_time))
      return;
    last_reported_.data_src = maybe_data_src;
    last_reported_.data_dst = maybe_data_dst;
    last_reported_.time = curr_time;
    last_reported_.is_warning_proceeded = true;
  }

  if (data_dst && IsVM(data_dst->type())) {
    NOTREACHED();
  } else {
    reporting_manager->ReportWarningProceededEvent(
        src_pattern, dst_pattern, DlpRulesManager::Restriction::kClipboard);
  }
}

DataTransferDlpController::LastReportedEndpoints::LastReportedEndpoints() =
    default;

DataTransferDlpController::LastReportedEndpoints::~LastReportedEndpoints() =
    default;

}  // namespace policy
