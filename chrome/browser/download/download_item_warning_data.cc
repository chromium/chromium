// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_warning_data.h"

#include "components/download/public/common/download_item.h"

using download::DownloadItem;
using WarningSurface = DownloadItemWarningData::WarningSurface;
using WarningAction = DownloadItemWarningData::WarningAction;
using WarningActionEvent = DownloadItemWarningData::WarningActionEvent;
using ClientSafeBrowsingReportRequest =
    safe_browsing::ClientSafeBrowsingReportRequest;

namespace {
constexpr int kWarningActionEventMaxLength = 20;
}

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
  // TODO(crbug.com/1363368): Add a histogram to log the result before return.
  if (!download) {
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
      data->warning_first_shown_time_ = base::Time::Now();
    }
    return;
  }
  if (data->warning_first_shown_time_.is_null()) {
    return;
  }
  if (data->action_events_.size() >= kWarningActionEventMaxLength) {
    return;
  }
  int64_t action_latency =
      (base::Time::Now() - data->warning_first_shown_time_).InMilliseconds();
  bool is_terminal_action =
      (action == PROCEED || action == DISCARD) ? true : false;
  DCHECK_NE(WarningAction::SHOWN, action);
  data->action_events_.emplace_back(surface, action, action_latency,
                                    is_terminal_action);
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
