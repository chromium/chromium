// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_STORAGE_NOTIFICATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_STORAGE_STORAGE_NOTIFICATION_SERVICE_IMPL_H_

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/storage_notification_service.h"

class StorageNotificationServiceImpl
    : public content::StorageNotificationService,
      public KeyedService {
 public:
  StorageNotificationServiceImpl();
  ~StorageNotificationServiceImpl() override;
  base::RepeatingClosure GetStoragePressureNotificationClosure() override;

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_STORAGE_STORAGE_NOTIFICATION_SERVICE_IMPL_H_
