// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"

#include <string>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/os_exchange_data.h"
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
bool IsNullEndpoint(const std::optional<ui::DataTransferEndpoint>& endpoint) {
  return !endpoint.has_value() ||
         endpoint->type() == ui::EndpointType::kDefault;
}

bool IsFilesApp(base::optional_ref<const ui::DataTransferEndpoint> data_dst) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!data_dst.has_value() || !data_dst->IsUrlType()) {
    return false;
  }

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

bool IsClipboardHistory(
    base::optional_ref<const ui::DataTransferEndpoint> data_dst) {
  return data_dst.has_value() &&
         data_dst->type() == ui::EndpointType::kClipboardHistory;
}

bool ShouldNotifyOnPaste(
    base::optional_ref<const ui::DataTransferEndpoint> data_dst) {
  bool notify_on_paste =
      !data_dst.has_value() || data_dst->notify_if_restricted();

  // Files Apps continuously reads the clipboard data which triggers a lot of
  // notifications while the user isn't actually initiating any copy/paste.
  // In BLOCK mode, data access by Files app will be denied silently.
  // In WARN mode, data access by Files app will be allowed silently.
  // TODO(crbug.com/1152475): Find a better way to handle File app.
  // When ClipboardHistory tries to read the clipboard we should allow it
  // silently.
  if (IsFilesApp(data_dst) || IsClipboardHistory(data_dst)) {
    notify_on_paste = false;
  }

  return notify_on_paste;
}

DlpRulesManager::Level IsDataTransferAllowed(
    const DlpRulesManager& dlp_rules_manager,
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    const std::optional<size_t> size,
    std::string* src_pattern,
    std::string* dst_pattern,
    DlpRulesManager::RuleMetadata* out_rule_metadata) {
  if (size.has_value() &&
      size < dlp_rules_manager.GetClipboardCheckSizeLimitInBytes()) {
    return DlpRulesManager::Level::kAllow;
  }

  // Currently we only handle URLs.
  if (!data_src.has_value() || !data_src->IsUrlType()) {
    return DlpRulesManager::Level::kAllow;
  }

  const GURL src_url = *data_src->GetURL();
  ui::EndpointType dst_type =
      data_dst.has_value() ? data_dst->type() : ui::EndpointType::kDefault;

  DlpRulesManager::Level level = DlpRulesManager::Level::kAllow;

  switch (dst_type) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case ui::EndpointType::kCrostini: {
      level = dlp_rules_manager.IsRestrictedComponent(
          src_url, data_controls::Component::kCrostini,
          DlpRulesManager::Restriction::kClipboard, src_pattern,
          out_rule_metadata);
      break;
    }

    case ui::EndpointType::kPluginVm: {
      level = dlp_rules_manager.IsRestrictedComponent(
          src_url, data_controls::Component::kPluginVm,
          DlpRulesManager::Restriction::kClipboard, src_pattern,
          out_rule_metadata);
      break;
    }

    case ui::EndpointType::kArc: {
      level = dlp_rules_manager.IsRestrictedComponent(
          src_url, data_controls::Component::kArc,
          DlpRulesManager::Restriction::kClipboard, src_pattern,
          out_rule_metadata);
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
          src_pattern, dst_pattern, out_rule_metadata);
      break;
    }

    case ui::EndpointType::kUrl: {
      GURL dst_url = *data_dst->GetURL();
      level = dlp_rules_manager.IsRestrictedDestination(
          src_url, dst_url, DlpRulesManager::Restriction::kClipboard,
          src_pattern, dst_pattern, out_rule_metadata);
      break;
    }

    case ui::EndpointType::kClipboardHistory: {
      level = DlpRulesManager::Level::kAllow;
      break;
    }

    default:
      NOTREACHED_IN_MIGRATION();
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

// Returns file paths from the given file infos.
std::vector<base::FilePath> GetFilePathsFromFileInfos(
    const std::vector<ui::FileInfo>& files) {
  std::vector<base::FilePath> paths;
  paths.reserve(files.size());
  for (const auto& file : files) {
    paths.emplace_back(file.path);
  }
  return paths;
}

}  // namespace

// static
void DataTransferDlpController::Init(const DlpRulesManager& dlp_rules_manager) {
  if (!HasInstance()) {
    data_controls::DlpBooleanHistogram(
        data_controls::dlp::kDataTransferControllerStartedUMA, true);
    new DataTransferDlpController(dlp_rules_manager);
  }
}

bool DataTransferDlpController::IsClipboardReadAllowed(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    const std::optional<size_t> size) {
  // To simplify logic that would have to check OTR in every sub-call of DLP
  // checks, simply null the endpoints so that subsequent code never misuses
  // data.
  base::optional_ref<const ui::DataTransferEndpoint> source =
      data_src.has_value() && !data_src->off_the_record() ? data_src
                                                          : std::nullopt;
  base::optional_ref<const ui::DataTransferEndpoint> destination =
      data_dst.has_value() && !data_dst->off_the_record() ? data_dst
                                                          : std::nullopt;

  std::string src_pattern;
  std::string dst_pattern;
  DlpRulesManager::RuleMetadata rule_metadata;

  DlpRulesManager::Level level = IsDataTransferAllowed(
      *dlp_rules_manager_, source.as_ptr(), destination.as_ptr(), size,
      &src_pattern, &dst_pattern, &rule_metadata);

  MaybeReportEvent(source.as_ptr(), destination.as_ptr(), src_pattern,
                   dst_pattern, level,
                   /*is_clipboard_event=*/true, rule_metadata);

  // Use original destination as OffTheRecord destinations might also have
  // `notify_if_restricted` param to be checked in case of system tools access.
  bool notify_on_paste = ShouldNotifyOnPaste(data_dst.as_ptr());

  bool is_read_allowed = true;

  switch (level) {
    case DlpRulesManager::Level::kBlock:
      if (notify_on_paste) {
        NotifyBlockedPaste(source.as_ptr(), destination.as_ptr());
      }
      is_read_allowed = false;
      break;
    case DlpRulesManager::Level::kWarn:

      if (destination.has_value() && IsVM(destination->type())) {
        if (notify_on_paste) {
          ReportEvent(source.as_ptr(), destination.as_ptr(), src_pattern,
                      dst_pattern, DlpRulesManager::Level::kWarn,
                      /*is_clipboard_event=*/true, rule_metadata);

          auto reporting_cb = base::BindOnce(
              &DataTransferDlpController::ReportWarningProceededEvent,
              weak_ptr_factory_.GetWeakPtr(), source.CopyAsOptional(),
              destination.CopyAsOptional(), src_pattern, dst_pattern,
              /*is_clipboard_event=*/true, rule_metadata);

          WarnOnPaste(source, destination, std::move(reporting_cb));
        }
      } else if (ShouldCancelOnWarn(destination.as_ptr())) {
        is_read_allowed = false;
      } else if (notify_on_paste &&
                 !(destination.has_value() && destination->IsUrlType()) &&
                 !ShouldPasteOnWarn(destination.as_ptr())) {
        ReportEvent(source.as_ptr(), destination.as_ptr(), src_pattern,
                    dst_pattern, DlpRulesManager::Level::kWarn,
                    /*is_clipboard_event=*/true, rule_metadata);

        auto reporting_cb = base::BindOnce(
            &DataTransferDlpController::ReportWarningProceededEvent,
            weak_ptr_factory_.GetWeakPtr(), source.CopyAsOptional(),
            destination.CopyAsOptional(), src_pattern, dst_pattern,
            /*is_clipboard_event=*/true, rule_metadata);

        WarnOnPaste(source, destination, std::move(reporting_cb));
        is_read_allowed = false;
      }
      break;
    default:
      break;
  }
  data_controls::DlpBooleanHistogram(
      data_controls::dlp::kClipboardReadBlockedUMA, !is_read_allowed);
  return is_read_allowed;
}

void DataTransferDlpController::PasteIfAllowed(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    absl::variant<size_t, std::vector<base::FilePath>> pasted_content,
    content::RenderFrameHost* rfh,
    base::OnceCallback<void(bool)> paste_cb) {
  // To simplify logic that would have to check OTR in every sub-call of DLP
  // checks, simply null the endpoints so that subsequent code never misuses
  // data.
  base::optional_ref<const ui::DataTransferEndpoint> source =
      data_src.has_value() && !data_src->off_the_record() ? data_src
                                                          : std::nullopt;
  base::optional_ref<const ui::DataTransferEndpoint> destination =
      data_dst.has_value() && !data_dst->off_the_record() ? data_dst
                                                          : std::nullopt;

  if (absl::holds_alternative<std::vector<base::FilePath>>(pasted_content) &&
      !IsFilesApp(destination)) {
    auto pasted_files =
        std::move(absl::get<std::vector<base::FilePath>>(pasted_content));
    auto* files_controller = dlp_rules_manager_->GetDlpFilesController();
    if (files_controller) {
      files_controller->CheckIfPasteOrDropIsAllowed(
          pasted_files, destination.as_ptr(), std::move(paste_cb));
    }
    return;
  }

  if (absl::holds_alternative<size_t>(pasted_content) &&
      absl::get<size_t>(pasted_content) > 0) {
    ContinuePasteIfClipboardRestrictionsAllow(source, destination,
                                              absl::get<size_t>(pasted_content),
                                              rfh, std::move(paste_cb));
    return;
  }

  std::move(paste_cb).Run(true);
}

void DataTransferDlpController::DropIfAllowed(
    std::optional<ui::DataTransferEndpoint> data_src,
    std::optional<ui::DataTransferEndpoint> data_dst,
    std::optional<std::vector<ui::FileInfo>> filenames,
    base::OnceClosure drop_cb) {
  // To simplify logic that would have to check OTR in every sub-call of DLP
  // checks, simply null the endpoints so that subsequent code never misuses
  // data.
  std::optional<ui::DataTransferEndpoint> source =
      data_src.has_value() && !data_src->off_the_record() ? data_src
                                                          : std::nullopt;
  std::optional<ui::DataTransferEndpoint> destination =
      data_dst.has_value() && !data_dst->off_the_record() ? data_dst
                                                          : std::nullopt;

  if (filenames.has_value() && !filenames->empty() &&
      !IsFilesApp(destination)) {
    auto* files_controller = dlp_rules_manager_->GetDlpFilesController();
    if (files_controller) {
      CHECK(destination.has_value());
      files_controller->CheckIfPasteOrDropIsAllowed(
          GetFilePathsFromFileInfos(filenames.value()), &destination.value(),
          base::BindOnce(
              [](base::OnceClosure drop_cb, bool is_allowed) {
                if (is_allowed) {
                  std::move(drop_cb).Run();
                }
              },
              std::move(drop_cb)));
      return;
    }
  }
  ContinueDropIfAllowed(source, destination, std::move(drop_cb));
}

DataTransferDlpController::DataTransferDlpController(
    const DlpRulesManager& dlp_rules_manager)
    : dlp_rules_manager_(dlp_rules_manager) {}

DataTransferDlpController::~DataTransferDlpController() = default;

base::TimeDelta DataTransferDlpController::GetSkipReportingTimeout() {
  return kSkipReportingTimeout;
}

void DataTransferDlpController::ReportWarningProceededEvent(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    const std::string& src_pattern,
    const std::string& dst_pattern,
    bool is_clipboard_event,
    const DlpRulesManager::RuleMetadata& rule_metadata) {
  auto* reporting_manager = dlp_rules_manager_->GetReportingManager();

  if (!reporting_manager) {
    return;
  }

  if (is_clipboard_event) {
    base::TimeTicks curr_time = base::TimeTicks::Now();

    if (ShouldSkipReporting(data_src, data_dst, /*is_warning_proceeded=*/true,
                            curr_time)) {
      return;
    }
    last_reported_.data_src = data_src.CopyAsOptional();
    last_reported_.data_dst = data_dst.CopyAsOptional();
    last_reported_.time = curr_time;
    last_reported_.is_warning_proceeded = true;
  }

  if (data_dst.has_value() && IsVM(data_dst->type())) {
    NOTREACHED_IN_MIGRATION();
  } else {
    const std::string src_url = (data_src.has_value() && data_src->IsUrlType())
                                    ? data_src->GetURL()->spec()
                                    : src_pattern;
    const std::string dst_url = (data_dst.has_value() && data_dst->IsUrlType())
                                    ? data_dst->GetURL()->spec()
                                    : dst_pattern;
    reporting_manager->ReportWarningProceededEvent(
        src_url, dst_url, DlpRulesManager::Restriction::kClipboard,
        rule_metadata.name, rule_metadata.obfuscated_id);
  }
}

void DataTransferDlpController::NotifyBlockedPaste(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst) {
  clipboard_notifier_.NotifyBlockedAction(data_src, data_dst);
}

void DataTransferDlpController::WarnOnPaste(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    base::OnceClosure reporting_cb) {
  DCHECK(!(data_dst.has_value() && data_dst->IsUrlType()));
  clipboard_notifier_.WarnOnPaste(data_src, data_dst, std::move(reporting_cb));
}

void DataTransferDlpController::WarnOnBlinkPaste(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> paste_cb) {
  clipboard_notifier_.WarnOnBlinkPaste(data_src, data_dst, web_contents,
                                       std::move(paste_cb));
}

bool DataTransferDlpController::ShouldPasteOnWarn(
    base::optional_ref<const ui::DataTransferEndpoint> data_dst) {
  return clipboard_notifier_.DidUserApproveDst(data_dst);
}

bool DataTransferDlpController::ShouldCancelOnWarn(
    base::optional_ref<const ui::DataTransferEndpoint> data_dst) {
  return clipboard_notifier_.DidUserCancelDst(data_dst);
}

void DataTransferDlpController::NotifyBlockedDrop(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst) {
  drag_drop_notifier_.NotifyBlockedAction(data_src, data_dst);
}

void DataTransferDlpController::WarnOnDrop(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    base::OnceClosure drop_cb) {
  drag_drop_notifier_.WarnOnDrop(data_src, data_dst, std::move(drop_cb));
}

bool DataTransferDlpController::ShouldSkipReporting(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    bool is_warning_proceeded,
    base::TimeTicks curr_time) {
  // Skip reporting for destination endpoints which don't notify the user
  // because it's not originating from a user action.
  if (!ShouldNotifyOnPaste(data_dst)) {
    return true;
  }

  // In theory, there is no need to check for data source and destination if
  // |kSkipReportingTimeout| is shorter than human reaction time.
  bool is_same_src = data_src.has_value()
                         ? *data_src == last_reported_.data_src
                         : IsNullEndpoint(last_reported_.data_src);
  bool is_same_dst = data_dst.has_value()
                         ? *data_dst == last_reported_.data_dst
                         : IsNullEndpoint(last_reported_.data_dst);
  bool is_same_mode =
      last_reported_.is_warning_proceeded.has_value() &&
      is_warning_proceeded == last_reported_.is_warning_proceeded.value();

  if (is_same_src && is_same_dst && is_same_mode) {
    base::TimeDelta time_diff = curr_time - last_reported_.time;
    base::UmaHistogramTimes(
        data_controls::GetDlpHistogramPrefix() +
            data_controls::dlp::kDataTransferReportingTimeDiffUMA,
        time_diff);

    return time_diff < GetSkipReportingTimeout();
  }
  return false;
}

void DataTransferDlpController::ReportEvent(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    const std::string& src_pattern,
    const std::string& dst_pattern,
    DlpRulesManager::Level level,
    bool is_clipboard_event,
    const DlpRulesManager::RuleMetadata& rule_metadata) {
  auto* reporting_manager = dlp_rules_manager_->GetReportingManager();
  if (!reporting_manager) {
    return;
  }

  if (is_clipboard_event) {
    base::TimeTicks curr_time = base::TimeTicks::Now();
    if (ShouldSkipReporting(data_src, data_dst, /*is_warning_proceeded=*/false,
                            curr_time)) {
      return;
    }
    last_reported_.data_src = data_src.CopyAsOptional();
    last_reported_.data_dst = data_dst.CopyAsOptional();
    last_reported_.time = curr_time;
    last_reported_.is_warning_proceeded = false;
  }

  const std::string src_url = (data_src.has_value() && data_src->IsUrlType())
                                  ? data_src->GetURL()->spec()
                                  : src_pattern;
  ui::EndpointType dst_type =
      data_dst.has_value() ? data_dst->type() : ui::EndpointType::kDefault;
  switch (dst_type) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case ui::EndpointType::kCrostini:
      reporting_manager->ReportEvent(
          src_url, data_controls::Component::kCrostini,
          DlpRulesManager::Restriction::kClipboard, level, rule_metadata.name,
          rule_metadata.obfuscated_id);
      break;

    case ui::EndpointType::kPluginVm:
      reporting_manager->ReportEvent(
          src_url, data_controls::Component::kPluginVm,
          DlpRulesManager::Restriction::kClipboard, level, rule_metadata.name,
          rule_metadata.obfuscated_id);
      break;

    case ui::EndpointType::kArc:
      reporting_manager->ReportEvent(src_url, data_controls::Component::kArc,
                                     DlpRulesManager::Restriction::kClipboard,
                                     level, rule_metadata.name,
                                     rule_metadata.obfuscated_id);
      break;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    default:
      const std::string dst_url =
          (data_dst.has_value() && data_dst->IsUrlType())
              ? data_dst->GetURL()->spec()
              : dst_pattern;
      reporting_manager->ReportEvent(
          src_url, dst_url, DlpRulesManager::Restriction::kClipboard, level,
          rule_metadata.name, rule_metadata.obfuscated_id);
      break;
  }
}

void DataTransferDlpController::MaybeReportEvent(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    const std::string& src_pattern,
    const std::string& dst_pattern,
    DlpRulesManager::Level level,
    bool is_clipboard_event,
    const DlpRulesManager::RuleMetadata& rule_metadata) {
  if (level == DlpRulesManager::Level::kReport ||
      level == DlpRulesManager::Level::kBlock) {
    ReportEvent(data_src, data_dst, src_pattern, dst_pattern, level,
                is_clipboard_event, rule_metadata);
  }
}

void DataTransferDlpController::ContinueDropIfAllowed(
    std::optional<ui::DataTransferEndpoint> data_src,
    std::optional<ui::DataTransferEndpoint> data_dst,
    base::OnceClosure drop_cb) {
  std::string src_pattern;
  std::string dst_pattern;
  DlpRulesManager::RuleMetadata rule_metadata;
  DlpRulesManager::Level level = IsDataTransferAllowed(
      *dlp_rules_manager_, data_src, data_dst, std::nullopt, &src_pattern,
      &dst_pattern, &rule_metadata);

  MaybeReportEvent(data_src, data_dst, src_pattern, dst_pattern, level,
                   /*is_clipboard_event=*/false, rule_metadata);

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
      NOTREACHED_IN_MIGRATION();
  }

  const bool is_drop_allowed = (level == DlpRulesManager::Level::kAllow) ||
                               (level == DlpRulesManager::Level::kReport);
  data_controls::DlpBooleanHistogram(data_controls::dlp::kDragDropBlockedUMA,
                                     !is_drop_allowed);
}

void DataTransferDlpController::ContinuePasteIfClipboardRestrictionsAllow(
    base::optional_ref<const ui::DataTransferEndpoint> data_src,
    base::optional_ref<const ui::DataTransferEndpoint> data_dst,
    size_t size,
    content::RenderFrameHost* rfh,
    base::OnceCallback<void(bool)> paste_cb) {
  DCHECK(data_dst.has_value());
  DCHECK(data_dst->IsUrlType());

  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    std::move(paste_cb).Run(false);
    return;
  }

  std::string src_pattern;
  std::string dst_pattern;
  DlpRulesManager::RuleMetadata rule_metadata;

  DlpRulesManager::Level level =
      IsDataTransferAllowed(*dlp_rules_manager_, data_src, data_dst, size,
                            &src_pattern, &dst_pattern, &rule_metadata);
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
      ReportWarningProceededEvent(data_src, data_dst, src_pattern, dst_pattern,
                                  /*is_clipboard_event=*/true, rule_metadata);
    }
    std::move(paste_cb).Run(true);
  } else if (ShouldCancelOnWarn(data_dst)) {
    std::move(paste_cb).Run(false);
  } else {
    if (ShouldNotifyOnPaste(data_dst)) {
      ReportEvent(data_src, data_dst, src_pattern, dst_pattern,
                  DlpRulesManager::Level::kWarn, /*is_clipboard_event=*/true,
                  rule_metadata);

      auto reporting_cb = base::BindOnce(
          &DataTransferDlpController::ReportWarningProceededEvent,
          weak_ptr_factory_.GetWeakPtr(), data_src.CopyAsOptional(),
          data_dst.CopyAsOptional(), src_pattern, dst_pattern,
          /*is_clipboard_event=*/true, rule_metadata);

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

DataTransferDlpController::LastReportedEndpoints::LastReportedEndpoints() =
    default;

DataTransferDlpController::LastReportedEndpoints::~LastReportedEndpoints() =
    default;

}  // namespace policy
