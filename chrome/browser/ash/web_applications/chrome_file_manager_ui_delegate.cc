// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/chrome_file_manager_ui_delegate.h"

#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/file_manager_string_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

ChromeFileManagerUIDelegate::ChromeFileManagerUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {
  DCHECK(web_ui_);
}

ChromeFileManagerUIDelegate::~ChromeFileManagerUIDelegate() = default;

base::Value::Dict ChromeFileManagerUIDelegate::GetLoadTimeData() const {
  base::Value::Dict dict = GetFileManagerStrings();

  const std::string locale = g_browser_process->GetApplicationLocale();
  AddFileManagerFeatureStrings(locale, Profile::FromWebUI(web_ui_), &dict);
  return dict;
}

void ChromeFileManagerUIDelegate::ProgressPausedTasks() const {
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(Profile::FromWebUI(web_ui_));

  if (volume_manager && volume_manager->io_task_controller()) {
    volume_manager->io_task_controller()->ProgressPausedTasks();
  }
}

void ChromeFileManagerUIDelegate::ShouldPollDriveHostedPinStates(bool enabled) {
  if (!drive::util::IsDriveFsBulkPinningEnabled(Profile::FromWebUI(web_ui_)) ||
      poll_hosted_pin_states_ == enabled) {
    return;
  }
  poll_hosted_pin_states_ = enabled;
  if (enabled) {
    PollHostedPinStates();
  }
}

void ChromeFileManagerUIDelegate::PollHostedPinStates() {
  if (!poll_hosted_pin_states_) {
    return;
  }
  drive::DriveIntegrationService* const service =
      drive::util::GetIntegrationServiceByProfile(Profile::FromWebUI(web_ui_));
  if (service && service->IsMounted()) {
    service->PollHostedFilePinStates();
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ChromeFileManagerUIDelegate::PollHostedPinStates,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(30));
}
