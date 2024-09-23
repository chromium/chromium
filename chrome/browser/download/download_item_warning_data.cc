// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_warning_data.h"

#include <functional>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/download/public/common/download_item.h"

using download::DownloadItem;
using WarningSurface = DownloadItemWarningData::WarningSurface;
using WarningAction = DownloadItemWarningData::WarningAction;
using WarningActionEvent = DownloadItemWarningData::WarningActionEvent;
using ClientSafeBrowsingReportRequest =
    safe_browsing::ClientSafeBrowsingReportRequest;
using DeepScanTrigger = DownloadItemWarningData::DeepScanTrigger;

namespace {
constexpr int kWarningActionEventMaxLength = 20;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AddWarningActionEventOutcome {
  // `download` was nullptr. This should never happen.
  NOT_ADDED_MISSING_DOWNLOAD = 0,
  // The first warning shown event is already logged so it is not logged this
  // time.
  NOT_ADDED_WARNING_SHOWN_ALREADY_LOGGED = 1,
  // The warning action event is not added because the first warning shown event
  // was not logged before.
  NOT_ADDED_MISSING_FIRST_WARNING = 2,
  // The warning action event is not added because it exceeds the max length.
  NOT_ADDED_EXCEED_MAX_LENGTH = 3,
  // The first warning shown event is successfully added.
  ADDED_WARNING_FIRST_SHOWN = 4,
  // The warning action event is successfully added.
  ADDED_WARNING_ACTION = 5,
  kMaxValue = ADDED_WARNING_ACTION
};

void RecordAddWarningActionEventOutcome(AddWarningActionEventOutcome outcome) {
  base::UmaHistogramEnumeration(
      "Download.WarningData.AddWarningActionEventOutcome", outcome);
}

void RecordSurfaceWithoutWarningShown(WarningSurface surface) {
  base::UmaHistogramEnumeration(
      "Download.WarningData.SurfaceWithoutWarningShown", surface);
}

void RecordWarningActionAdded(WarningAction action) {
  base::UmaHistogramEnumeration("Download.WarningData.ActionAdded", action);
}

}  // namespace

// static
const char DownloadItemWarningData::kKey[] = "DownloadItemWarningData key";

// static
template <typename F, typename V>
V DownloadItemWarningData::GetWithDefault(const DownloadItem* download,
                                          F&& f,
                                          V&& default_value) {
  if (!download) {
    return default_value;
  }
  DownloadItemWarningData* data =
      static_cast<DownloadItemWarningData*>(download->GetUserData(kKey));
  if (!data) {
    return default_value;
  }
  return std::invoke(std::forward<F>(f), *data);
}

// static
DownloadItemWarningData* DownloadItemWarningData::GetOrCreate(
    DownloadItem* download) {
  DownloadItemWarningData* data =
      static_cast<DownloadItemWarningData*>(download->GetUserData(kKey));
  if (!data) {
    data = new DownloadItemWarningData();
    download->SetUserData(kKey, base::WrapUnique(data));
  }

  return data;
}

// static
std::vector<WarningActionEvent> DownloadItemWarningData::GetWarningActionEvents(
    const DownloadItem* download) {
  return GetWithDefault(download, &DownloadItemWarningData::ActionEvents,
                        std::vector<WarningActionEvent>());
}

// static
void DownloadItemWarningData::AddWarningActionEvent(DownloadItem* download,
                                                    WarningSurface surface,
                                                    WarningAction action) {
  if (!download) {
    RecordAddWarningActionEventOutcome(
        AddWarningActionEventOutcome::NOT_ADDED_MISSING_DOWNLOAD);
    return;
  }
  DownloadItemWarningData* data = GetOrCreate(download);
  if (action == WarningAction::SHOWN) {
    if (!data->logged_downloads_page_shown_ &&
        surface == WarningSurface::DOWNLOADS_PAGE) {
      base::UmaHistogramEnumeration(
          "Download.ShowedDownloadWarning.DownloadsPage",
          download->GetDangerType(), download::DOWNLOAD_DANGER_TYPE_MAX);
      data->logged_downloads_page_shown_ = true;
    }
    if (data->warning_first_shown_time_.is_null()) {
      RecordAddWarningActionEventOutcome(
          AddWarningActionEventOutcome::ADDED_WARNING_FIRST_SHOWN);
      RecordWarningActionAdded(action);
      data->warning_first_shown_time_ = base::Time::Now();
      data->warning_first_shown_surface_ = surface;
    } else {
      RecordAddWarningActionEventOutcome(
          AddWarningActionEventOutcome::NOT_ADDED_WARNING_SHOWN_ALREADY_LOGGED);
    }
    return;
  }
  if (data->warning_first_shown_time_.is_null()) {
    RecordAddWarningActionEventOutcome(
        AddWarningActionEventOutcome::NOT_ADDED_MISSING_FIRST_WARNING);
    RecordSurfaceWithoutWarningShown(surface);
    return;
  }
  if (data->action_events_.size() >= kWarningActionEventMaxLength) {
    RecordAddWarningActionEventOutcome(
        AddWarningActionEventOutcome::NOT_ADDED_EXCEED_MAX_LENGTH);
    return;
  }
  int64_t action_latency =
      (base::Time::Now() - data->warning_first_shown_time_).InMilliseconds();
  bool is_terminal_action = action == WarningAction::PROCEED ||
                            action == WarningAction::DISCARD ||
                            action == WarningAction::PROCEED_DEEP_SCAN;
  DCHECK_NE(WarningAction::SHOWN, action);
  data->action_events_.emplace_back(surface, action, action_latency,
                                    is_terminal_action);
  RecordAddWarningActionEventOutcome(
      AddWarningActionEventOutcome::ADDED_WARNING_ACTION);
  RecordWarningActionAdded(action);
}

// static
bool DownloadItemWarningData::IsTopLevelEncryptedArchive(
    const download::DownloadItem* download) {
  return GetWithDefault(
      download, &DownloadItemWarningData::is_top_level_encrypted_archive_,
      false);
}

// static
void DownloadItemWarningData::SetIsTopLevelEncryptedArchive(
    download::DownloadItem* download,
    bool is_top_level_encrypted_archive) {
  if (!download) {
    return;
  }

  GetOrCreate(download)->is_top_level_encrypted_archive_ =
      is_top_level_encrypted_archive;
}

// static
bool DownloadItemWarningData::HasIncorrectPassword(
    const download::DownloadItem* download) {
  return GetWithDefault(
      download, &DownloadItemWarningData::has_incorrect_password_, false);
}

// static
void DownloadItemWarningData::SetHasIncorrectPassword(
    download::DownloadItem* download,
    bool has_incorrect_password) {
  if (!download) {
    return;
  }

  GetOrCreate(download)->has_incorrect_password_ = has_incorrect_password;
}

// static
ClientSafeBrowsingReportRequest::DownloadWarningAction
DownloadItemWarningData::ConstructCsbrrDownloadWarningAction(
    const WarningActionEvent& event) {
  ClientSafeBrowsingReportRequest::DownloadWarningAction action;
  switch (event.surface) {
    case DownloadItemWarningData::WarningSurface::BUBBLE_MAINPAGE:
      action.set_surface(ClientSafeBrowsingReportRequest::
                             DownloadWarningAction::BUBBLE_MAINPAGE);
      break;
    case DownloadItemWarningData::WarningSurface::BUBBLE_SUBPAGE:
      action.set_surface(ClientSafeBrowsingReportRequest::
                             DownloadWarningAction::BUBBLE_SUBPAGE);
      break;
    case DownloadItemWarningData::WarningSurface::DOWNLOADS_PAGE:
      action.set_surface(ClientSafeBrowsingReportRequest::
                             DownloadWarningAction::DOWNLOADS_PAGE);
      break;
    case DownloadItemWarningData::WarningSurface::DOWNLOAD_PROMPT:
      action.set_surface(ClientSafeBrowsingReportRequest::
                             DownloadWarningAction::DOWNLOAD_PROMPT);
      break;
    case DownloadItemWarningData::WarningSurface::DOWNLOAD_NOTIFICATION:
      action.set_surface(ClientSafeBrowsingReportRequest::
                             DownloadWarningAction::DOWNLOAD_NOTIFICATION);
      break;
  }
  switch (event.action) {
    case DownloadItemWarningData::WarningAction::PROCEED:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::PROCEED);
      break;
    case DownloadItemWarningData::WarningAction::DISCARD:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::DISCARD);
      break;
    case DownloadItemWarningData::WarningAction::KEEP:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::KEEP);
      break;
    case DownloadItemWarningData::WarningAction::CLOSE:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::CLOSE);
      break;
    case DownloadItemWarningData::WarningAction::CANCEL:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::CANCEL);
      break;
    case DownloadItemWarningData::WarningAction::DISMISS:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::DISMISS);
      break;
    case DownloadItemWarningData::WarningAction::BACK:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::BACK);
      break;
    case DownloadItemWarningData::WarningAction::OPEN_SUBPAGE:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::OPEN_SUBPAGE);
      break;
    case DownloadItemWarningData::WarningAction::PROCEED_DEEP_SCAN:
      action.set_action(ClientSafeBrowsingReportRequest::DownloadWarningAction::
                            PROCEED_DEEP_SCAN);
      break;
    case DownloadItemWarningData::WarningAction::OPEN_LEARN_MORE_LINK:
      action.set_action(ClientSafeBrowsingReportRequest::DownloadWarningAction::
                            OPEN_LEARN_MORE_LINK);
      break;
    case DownloadItemWarningData::WarningAction::SHOWN:
      NOTREACHED_IN_MIGRATION();
      break;
    case DownloadItemWarningData::WarningAction::ACCEPT_DEEP_SCAN:
      action.set_action(ClientSafeBrowsingReportRequest::DownloadWarningAction::
                            ACCEPT_DEEP_SCAN);
  }
  action.set_is_terminal_action(event.is_terminal_action);
  action.set_interval_msec(event.action_latency_msec);
  return action;
}

// static
bool DownloadItemWarningData::HasShownLocalDecryptionPrompt(
    const download::DownloadItem* download) {
  return GetWithDefault(
      download, &DownloadItemWarningData::has_shown_local_decryption_prompt_,
      false);
}

// static
void DownloadItemWarningData::SetHasShownLocalDecryptionPrompt(
    download::DownloadItem* download,
    bool has_shown) {
  if (!download) {
    return;
  }

  GetOrCreate(download)->has_shown_local_decryption_prompt_ = has_shown;
}

// static
bool DownloadItemWarningData::IsFullyExtractedArchive(
    const download::DownloadItem* download) {
  return GetWithDefault(
      download, &DownloadItemWarningData::fully_extracted_archive_, false);
}

// static
void DownloadItemWarningData::SetIsFullyExtractedArchive(
    download::DownloadItem* download,
    bool extracted) {
  if (!download) {
    return;
  }

  GetOrCreate(download)->fully_extracted_archive_ = extracted;
}

// static
DeepScanTrigger DownloadItemWarningData::DownloadDeepScanTrigger(
    const download::DownloadItem* download) {
  return GetWithDefault(download, &DownloadItemWarningData::deep_scan_trigger_,
                        DeepScanTrigger::TRIGGER_UNKNOWN);
}

// static
void DownloadItemWarningData::SetDeepScanTrigger(
    download::DownloadItem* download,
    DeepScanTrigger trigger) {
  if (!download) {
    return;
  }

  GetOrCreate(download)->deep_scan_trigger_ = trigger;
}

// static
base::Time DownloadItemWarningData::WarningFirstShownTime(
    const download::DownloadItem* download) {
  return GetWithDefault(download,
                        &DownloadItemWarningData::warning_first_shown_time_,
                        base::Time());
}

// static
std::optional<DownloadItemWarningData::WarningSurface>
DownloadItemWarningData::WarningFirstShownSurface(
    const download::DownloadItem* download) {
  return GetWithDefault(download,
                        &DownloadItemWarningData::warning_first_shown_surface_,
                        std::optional<WarningSurface>());
}

DownloadItemWarningData::DownloadItemWarningData() = default;

DownloadItemWarningData::~DownloadItemWarningData() = default;

std::vector<WarningActionEvent> DownloadItemWarningData::ActionEvents() const {
  if (warning_first_shown_time_.is_null()) {
    return {};
  }
  return action_events_;
}

WarningActionEvent::WarningActionEvent(WarningSurface surface,
                                       WarningAction action,
                                       int64_t action_latency_msec,
                                       bool is_terminal_action)
    : surface(surface),
      action(action),
      action_latency_msec(action_latency_msec),
      is_terminal_action(is_terminal_action) {}

std::string DownloadItemWarningData::WarningActionEvent::ToString() const {
  std::string surface_string, action_string;
  switch (surface) {
    case WarningSurface::BUBBLE_MAINPAGE:
      surface_string = "BUBBLE_MAINPAGE";
      break;
    case WarningSurface::BUBBLE_SUBPAGE:
      surface_string = "BUBBLE_SUBPAGE";
      break;
    case WarningSurface::DOWNLOADS_PAGE:
      surface_string = "DOWNLOADS_PAGE";
      break;
    case WarningSurface::DOWNLOAD_PROMPT:
      surface_string = "DOWNLOAD_PROMPT";
      break;
    case WarningSurface::DOWNLOAD_NOTIFICATION:
      surface_string = "DOWNLOAD_NOTIFICATION";
      break;
  }
  switch (action) {
    case WarningAction::SHOWN:
      action_string = "SHOWN";
      break;
    case WarningAction::PROCEED:
      action_string = "PROCEED";
      break;
    case WarningAction::DISCARD:
      action_string = "DISCARD";
      break;
    case WarningAction::KEEP:
      action_string = "KEEP";
      break;
    case WarningAction::CLOSE:
      action_string = "CLOSE";
      break;
    case WarningAction::CANCEL:
      action_string = "CANCEL";
      break;
    case WarningAction::DISMISS:
      action_string = "DISMISS";
      break;
    case WarningAction::BACK:
      action_string = "BACK";
      break;
    case WarningAction::OPEN_SUBPAGE:
      action_string = "OPEN_SUBPAGE";
      break;
    case WarningAction::PROCEED_DEEP_SCAN:
      action_string = "PROCEED_DEEP_SCAN";
      break;
    case WarningAction::OPEN_LEARN_MORE_LINK:
      action_string = "OPEN_LEARN_MORE_LINK";
      break;
    case WarningAction::ACCEPT_DEEP_SCAN:
      action_string = "ACCEPT_DEEP_SCAN";
      break;
  }
  return base::JoinString({surface_string, action_string,
                           base::NumberToString(action_latency_msec)},
                          ":");
}
