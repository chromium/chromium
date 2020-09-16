// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/storage_notification_service_impl.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/storage_pressure_bubble.h"
#endif

namespace {

// Minimum interval between consecutive storage pressure notifications.
const base::TimeDelta kDiskPressureNotificationInterval =
    base::TimeDelta::FromDays(1);

const base::TimeDelta GetThrottlingInterval() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  int int_value;
  if (command_line->HasSwitch(switches::kStoragePressureNotificationInterval)) {
    const std::string string_value = command_line->GetSwitchValueASCII(
        switches::kStoragePressureNotificationInterval);
    if (base::StringToInt(string_value, &int_value) && int_value >= 0) {
      return base::TimeDelta::FromMinutes(int_value);
    }
  }
  return kDiskPressureNotificationInterval;
}

}  // namespace

void StorageNotificationServiceImpl::MaybeShowStoragePressureNotification(
    const url::Origin origin) {
  if (!disk_pressure_notification_last_sent_at_.is_null() &&
      base::TimeTicks::Now() - disk_pressure_notification_last_sent_at_ <
          GetThrottlingInterval()) {
    return;
  }

  chrome::ShowStoragePressureBubble(origin);
  disk_pressure_notification_last_sent_at_ = base::TimeTicks::Now();
}

StorageNotificationServiceImpl::StorageNotificationServiceImpl() = default;

StorageNotificationServiceImpl::~StorageNotificationServiceImpl() = default;
