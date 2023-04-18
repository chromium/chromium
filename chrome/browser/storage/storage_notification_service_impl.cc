// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/storage_notification_service_impl.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/storage_pressure_bubble.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace {

// Minimum interval between consecutive storage pressure notifications.
const base::TimeDelta kDiskPressureNotificationInterval = base::Days(1);

const base::TimeDelta GetThrottlingInterval() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  int int_value;
  if (command_line->HasSwitch(switches::kStoragePressureNotificationInterval)) {
    const std::string string_value = command_line->GetSwitchValueASCII(
        switches::kStoragePressureNotificationInterval);
    if (base::StringToInt(string_value, &int_value) && int_value >= 0) {
      return base::Minutes(int_value);
    }
  }
  return kDiskPressureNotificationInterval;
}

}  // namespace

StoragePressureNotificationCallback
StorageNotificationServiceImpl::CreateThreadSafePressureNotificationCallback() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  auto thread_unsafe_callback = base::BindRepeating(
      &StorageNotificationServiceImpl::MaybeShowStoragePressureNotification,
      weak_ptr_factory_.GetWeakPtr());
  return base::BindRepeating(
      [](StoragePressureNotificationCallback cb, const blink::StorageKey& key) {
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce([](StoragePressureNotificationCallback callback,
                              blink::StorageKey key) { callback.Run(key); },
                           std::move(cb), key));
      },
      std::move(thread_unsafe_callback));
}

void StorageNotificationServiceImpl::MaybeShowStoragePressureNotification(
    const blink::StorageKey& storage_key) {
  auto origin = storage_key.origin();
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
