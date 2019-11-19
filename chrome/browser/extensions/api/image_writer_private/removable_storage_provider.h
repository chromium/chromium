// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_REMOVABLE_STORAGE_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_REMOVABLE_STORAGE_PROVIDER_H_

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "chrome/common/ref_counted_util.h"

namespace extensions {

// TODO(haven): Clean up this class to remove refcounting.  http://crbug/370590

typedef RefCountedVector<api::image_writer_private::RemovableStorageDevice>
    StorageDeviceList;

// Abstraction for platform specific implementations of listing removable
// storage devices
class RemovableStorageProvider {
 public:
  using DeviceListReadyCallback =
      base::OnceCallback<void(scoped_refptr<StorageDeviceList>)>;

  // Gets the list of all available devices and returns it via callback.
  static void GetAllDevices(DeviceListReadyCallback callback);

  // Sets the list of devices that will be returned by GetAllDevices during
  // testing.  All calls to |GetAllDevices| will return this list until
  // |ClearDeviceListForTesting| is called.
  static void SetDeviceListForTesting(
      scoped_refptr<StorageDeviceList> device_list);
  // Clears the list of devices that is used during testing.
  static void ClearDeviceListForTesting();

 private:
  // Returns available list of devices. If there is an error retrieving devices,
  // then returns nullptr.
  static scoped_refptr<StorageDeviceList> PopulateDeviceList();
};

} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_REMOVABLE_STORAGE_PROVIDER_H_
