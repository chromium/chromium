// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_danger_prompt.h"

#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/download_item_utils.h"

// static
void DownloadDangerPrompt::RecordDownloadWarningEvent(
    Action action,
    download::DownloadItem* download) {
  DownloadItemWarningData::WarningAction warning_action;
  switch (action) {
    case Action::ACCEPT:
      warning_action = DownloadItemWarningData::WarningAction::PROCEED;
      break;
    case Action::CANCEL:
      warning_action = DownloadItemWarningData::WarningAction::CANCEL;
      break;
    case Action::DISMISS:
      warning_action = DownloadItemWarningData::WarningAction::CLOSE;
      break;
  }
  DownloadItemWarningData::AddWarningActionEvent(
      download, DownloadItemWarningData::WarningSurface::DOWNLOAD_PROMPT,
      warning_action);
}
