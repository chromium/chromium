// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_MANAGER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_MANAGER_H_

#include "base/no_destructor.h"
#include "base/observer_list.h"

// A singleton that acts as a rendezvous for dialog observers to register and
// the dialog managers/delegates to post their activities.
// TODO(crbug/953495): Merge this into DesktopMediaPickerFactoryImpl.
class DesktopMediaPickerManager {
 public:
  class DialogObserver : public base::CheckedObserver {
   public:
    // Called when a media dialog is opened/shown.
    virtual void OnDialogOpened() = 0;

    // Called when a media dialog is closed/hidden.
    virtual void OnDialogClosed() = 0;
  };

  static DesktopMediaPickerManager* Get();

  DesktopMediaPickerManager(const DesktopMediaPickerManager&) = delete;
  DesktopMediaPickerManager& operator=(const DesktopMediaPickerManager&) =
      delete;

  // For the observers
  void AddObserver(DialogObserver* observer);
  void RemoveObserver(DialogObserver* observer);

  // For the notifiers
  void OnShowDialog();
  void OnHideDialog();

 private:
  friend base::NoDestructor<DesktopMediaPickerManager>;

  DesktopMediaPickerManager();
  ~DesktopMediaPickerManager();  // Never called.

  base::ObserverList<DesktopMediaPickerManager::DialogObserver> observers_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_MANAGER_H_
