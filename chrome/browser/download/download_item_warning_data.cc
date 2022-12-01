// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_warning_data.h"

#include "base/metrics/histogram_functions.h"
#include "components/download/public/common/download_item.h"

using download::DownloadItem;
using WarningSurface = DownloadItemWarningData::WarningSurface;
using WarningAction = DownloadItemWarningData::WarningAction;
using WarningActionEvent = DownloadItemWarningData::WarningActionEvent;
using ClientSafeBrowsingReportRequest =
    safe_browsing::ClientSafeBrowsingReportRequest;

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
}  // namespace

// static
const char DownloadItemWarningData::kKey[] = "DownloadItemWarningData key";

// static
std::vector<WarningActionEvent> DownloadItemWarningData::GetWarningActionEvents(
    const DownloadItem* download) {
  DownloadItemWarningData* data =
      static_cast<DownloadItemWarningData*>(download->GetUserData(kKey));
  if (!data || data->warning_first_shown_time_.is_null()) {
    return {};
  }
  return data->action_events_;
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
  DownloadItemWarningData* data =
      static_cast<DownloadItemWarningData*>(download->GetUserData(kKey));
  if (!data) {
    data = new DownloadItemWarningData();
    download->SetUserData(kKey, base::WrapUnique(data));
  }
  if (action == WarningAction::SHOWN) {
    if (data->warning_first_shown_time_.is_null()) {
      RecordAddWarningActionEventOutcome(
          AddWarningActionEventOutcome::ADDED_WARNING_FIRST_SHOWN);
      data->warning_first_shown_time_ = base::Time::Now();
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
  bool is_terminal_action =
      (action == PROCEED || action == DISCARD) ? true : false;
  DCHECK_NE(WarningAction::SHOWN, action);
  data->action_events_.emplace_back(surface, action, action_latency,
                                    is_terminal_action);
  RecordAddWarningActionEventOutcome(
      AddWarningActionEventOutcome::ADDED_WARNING_ACTION);
}

// static
ClientSafeBrowsingReportRequest::DownloadWarningAction
DownloadItemWarningData::ConstructCsbrrDownloadWarningAction(
    const WarningActionEvent& event) {
  ClientSafeBrowsingReportRequest::DownloadWarningAction action;
  switch (event.surface) {
    case DownloadItemWarningData::BUBBLE_MAINPAGE:
      action.set_surface(ClientSafeBrowsingReportRequest::
                             DownloadWarningAction::BUBBLE_MAINPAGE);
      break;
    case DownloadItemWarningData::BUBBLE_SUBPAGE:
      action.set_surface(ClientSafeBrowsingReportRequest::
                             DownloadWarningAction::BUBBLE_SUBPAGE);
      break;
    case DownloadItemWarningData::DOWNLOADS_PAGE:
      action.set_surface(ClientSafeBrowsingReportRequest::
                             DownloadWarningAction::DOWNLOADS_PAGE);
      break;
    case DownloadItemWarningData::DOWNLOAD_PROMPT:
      action.set_surface(ClientSafeBrowsingReportRequest::
                             DownloadWarningAction::DOWNLOAD_PROMPT);
      break;
  }
  switch (event.action) {
    case DownloadItemWarningData::PROCEED:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::PROCEED);
      break;
    case DownloadItemWarningData::DISCARD:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::DISCARD);
      break;
    case DownloadItemWarningData::KEEP:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::KEEP);
      break;
    case DownloadItemWarningData::CLOSE:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::CLOSE);
      break;
    case DownloadItemWarningData::CANCEL:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::CANCEL);
      break;
    case DownloadItemWarningData::DISMISS:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::DISMISS);
      break;
    case DownloadItemWarningData::BACK:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::BACK);
      break;
    case DownloadItemWarningData::OPEN_SUBPAGE:
      action.set_action(
          ClientSafeBrowsingReportRequest::DownloadWarningAction::OPEN_SUBPAGE);
      break;
    case DownloadItemWarningData::SHOWN:
      NOTREACHED();
      break;
  }
  action.set_is_terminal_action(event.is_terminal_action);
  action.set_interval_msec(event.action_latency_msec);
  return action;
}

DownloadItemWarningData::DownloadItemWarningData() = default;

DownloadItemWarningData::~DownloadItemWarningData() = default;

WarningActionEvent::WarningActionEvent(WarningSurface surface,
                                       WarningAction action,
                                       int64_t action_latency_msec,
                                       bool is_terminal_action)
    : surface(surface),
      action(action),
      action_latency_msec(action_latency_msec),
      is_terminal_action(is_terminal_action) {}
