// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_EXTENSION_INSTALLER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_EXTENSION_INSTALLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

// A helper class to help mimic the timing of installs of synced extensions
// from the app store.
//
// Listens for attempts to contact the web store.  When it detects such an
// attempt (which will fail in tests), it posts a task to fake the installation
// of synced extensions.
//
// Also installs any pending synced extensions when this class is instantiated.
// This is to avoid a potential races between the emitting of notifications and
// the instantation of this class.
class SyncedExtensionInstaller : public content::NotificationObserver {
 public:
  explicit SyncedExtensionInstaller(Profile* profile);
  ~SyncedExtensionInstaller() override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  void DoInstallSyncedExtensions();

  Profile* profile_;
  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<SyncedExtensionInstaller> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncedExtensionInstaller);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_EXTENSION_INSTALLER_H_
