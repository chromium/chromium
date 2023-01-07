// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

// A device list to be returned when testing.
static base::LazyInstance<scoped_refptr<StorageDeviceList>>::DestructorAtExit
    g_test_device_list = LAZY_INSTANCE_INITIALIZER;

// TODO(haven): Udev code may be duplicated in the Chrome codebase.
// https://code.google.com/p/chromium/issues/detail?id=284898

void RemovableStorageProvider::GetAllDevices(DeviceListReadyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_test_device_list.Get().get() != nullptr) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), g_test_device_list.Get()));
    return;
  }
  // We need to do some file i/o to get the device block size
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RemovableStorageProvider::PopulateDeviceList),
      std::move(callback));
}

// static
void RemovableStorageProvider::SetDeviceListForTesting(
    scoped_refptr<StorageDeviceList> device_list) {
  g_test_device_list.Get() = device_list;
}

// static
void RemovableStorageProvider::ClearDeviceListForTesting() {
  g_test_device_list.Get() = nullptr;
}

}  // namespace extensions
