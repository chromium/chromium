// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_ui_model_utils.h"

#include "base/time/time.h"
#include "chrome/browser/download/download_ui_model.h"

bool DownloadUIModelIsRecent(const DownloadUIModel* model,
                             base::Time cutoff_time) {
  return ((model->GetStartTime().is_null() && !model->IsDone()) ||
          model->GetStartTime() > cutoff_time);
}

bool IsPendingDeepScanning(const DownloadUIModel* model) {
  return model->GetState() == download::DownloadItem::IN_PROGRESS &&
         model->GetDangerType() ==
             download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING;
}

bool IsModelInProgress(const DownloadUIModel* model) {
  if (model->IsDangerous() && !IsPendingDeepScanning(model)) {
    return false;
  }
  return model->GetState() == download::DownloadItem::IN_PROGRESS;
}
