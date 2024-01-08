// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_UNINSTALLER_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_UNINSTALLER_NOTIFICATION_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/message_center/public/cpp/notification.h"

class Profile;

class PluginVmUninstallerNotification {
 public:
  enum class FailedReason {
    kUnknown,
    kStopVmFailed,
  };

  explicit PluginVmUninstallerNotification(Profile* profile);
  virtual ~PluginVmUninstallerNotification();

  void SetFailed(FailedReason reason);
  void SetCompleted();
  // Will show the notification, even if it has been closed.
  void ForceRedisplay();

 private:
  raw_ptr<Profile> profile_;
  std::unique_ptr<message_center::Notification> notification_;
  base::WeakPtrFactory<PluginVmUninstallerNotification> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_UNINSTALLER_NOTIFICATION_H_
