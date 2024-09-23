// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/caption_settings_dialog.h"

#include <windows.h>

#include <shellapi.h>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"

namespace {

// A helper callback that opens the caption settings dialog.
void CaptionSettingsDialogCallback() {
  ShellExecute(nullptr, L"open", L"ms-settings:easeofaccess-closedcaptioning",
               nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace

namespace captions {

void CaptionSettingsDialog::ShowCaptionSettingsDialog() {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(CaptionSettingsDialogCallback));
}

}  // namespace captions
