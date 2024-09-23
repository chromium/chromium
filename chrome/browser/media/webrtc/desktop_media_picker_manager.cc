// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"

// static
DesktopMediaPickerManager* DesktopMediaPickerManager::Get() {
  static base::NoDestructor<DesktopMediaPickerManager> instance;
  return instance.get();
}

DesktopMediaPickerManager::DesktopMediaPickerManager() = default;
DesktopMediaPickerManager::~DesktopMediaPickerManager() = default;

void DesktopMediaPickerManager::AddObserver(DialogObserver* observer) {
  observers_.AddObserver(observer);
}

void DesktopMediaPickerManager::RemoveObserver(DialogObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DesktopMediaPickerManager::OnShowDialog(
    const DesktopMediaPicker::Params& params) {
  for (auto& observer : observers_) {
    observer.OnDialogOpened(params);
  }
}

void DesktopMediaPickerManager::OnHideDialog() {
  for (auto& observer : observers_)
    observer.OnDialogClosed();
}
