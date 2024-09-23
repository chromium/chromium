// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include <objbase.h>

#include <shobjidl.h>

#include <wrl/client.h>

#include <string>

#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/views/win/hwnd_util.h"

namespace {

void UpdateTaskbarProgressBar(int download_count,
                              bool progress_known,
                              float progress) {
  Microsoft::WRL::ComPtr<ITaskbarList3> taskbar;
  HRESULT result = ::CoCreateInstance(
      CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar));
  if (FAILED(result)) {
    DVLOG(1) << "Failed creating a TaskbarList object: " << result;
    return;
  }

  result = taskbar->HrInit();
  if (FAILED(result)) {
    LOG(ERROR) << "Failed initializing an ITaskbarList3 interface.";
    return;
  }

  // Iterate through all the browser windows, and draw the progress bar.
  for (Browser* browser : *BrowserList::GetInstance()) {
    BrowserWindow* window = browser->window();
    if (!window)
      continue;
    HWND frame = views::HWNDForNativeWindow(window->GetNativeWindow());
    if (download_count == 0 || progress == 1.0f)
      taskbar->SetProgressState(frame, TBPF_NOPROGRESS);
    else if (!progress_known)
      taskbar->SetProgressState(frame, TBPF_INDETERMINATE);
    else
      taskbar->SetProgressValue(frame, static_cast<int>(progress * 100), 100);
  }
}

}  // namespace

void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    download::DownloadItem* download) {
  // Always update overall progress.
  float progress = 0;
  int download_count = 0;
  bool progress_known = GetProgress(&progress, &download_count);
  UpdateTaskbarProgressBar(download_count, progress_known, progress);
}
