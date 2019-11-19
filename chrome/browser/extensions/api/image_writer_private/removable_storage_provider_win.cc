// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// devguid requires Windows.h be imported first.
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider.h"

#include <windows.h>
#include <setupapi.h>

// LogSeverity is both a macro in setupapi.h and a typedef in base/logging.h
#undef LogSeverity

#include <winioctl.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"

namespace extensions {

namespace {

bool AddDeviceInfo(HANDLE interface_enumerator,
                   SP_DEVICE_INTERFACE_DATA* interface_data,
                   scoped_refptr<StorageDeviceList> device_list) {
  // Get the required buffer size by calling with a null output buffer.
  DWORD interface_detail_data_size;
  BOOL status = SetupDiGetDeviceInterfaceDetail(
      interface_enumerator,
      interface_data,
      NULL,                         // Output buffer.
      0,                            // Output buffer size.
      &interface_detail_data_size,  // Receives the buffer size.
      NULL);                        // Optional DEVINFO_DATA.

  if (status == FALSE && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    PLOG(ERROR) << "SetupDiGetDeviceInterfaceDetail failed";
    return false;
  }

  std::unique_ptr<char[]> interface_detail_data_buffer(
      new char[interface_detail_data_size]);

  SP_DEVICE_INTERFACE_DETAIL_DATA* interface_detail_data =
      reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(
          interface_detail_data_buffer.get());

  interface_detail_data->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);

  status = SetupDiGetDeviceInterfaceDetail(
      interface_enumerator,
      interface_data,
      interface_detail_data, // Output struct.
      interface_detail_data_size,  // Output struct size.
      NULL,                        // Receives required size, unneeded.
      NULL);                       // Optional DEVINFO_Data.

  if (status == FALSE) {
    PLOG(ERROR) << "SetupDiGetDeviceInterfaceDetail failed";
    return false;
  }

  // Open a handle to the device to send DeviceIoControl messages.
  base::win::ScopedHandle device_handle(CreateFile(
      interface_detail_data->DevicePath,
      // Desired access, which is none as we only need metadata.
      0,
      // Required to be read + write for devices.
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,           // Optional security attributes.
      OPEN_EXISTING,  // Devices already exist.
      0,              // No optional flags.
      NULL));          // No template file.

  if (!device_handle.IsValid()) {
    PLOG(ERROR) << "Opening device handle failed.";
    return false;
  }

  DISK_GEOMETRY geometry;
  DWORD bytes_returned;
  status = DeviceIoControl(
      device_handle.Get(),           // Device handle.
      IOCTL_DISK_GET_DRIVE_GEOMETRY, // Flag to request disk size.
      NULL,                          // Optional additional parameters.
      0,                             // Optional parameter size.
      &geometry,                     // output buffer.
      sizeof(DISK_GEOMETRY),         // output size.
      &bytes_returned,               // Must be non-null. If overlapped is null,
                                     // then value is meaningless.
      NULL);                         // Optional unused overlapped parameter.

  if (status == FALSE) {
    PLOG(ERROR) << "DeviceIoControl";
    return false;
  }

  ULONGLONG disk_capacity = geometry.Cylinders.QuadPart *
    geometry.TracksPerCylinder *
    geometry.SectorsPerTrack *
    geometry.BytesPerSector;

  STORAGE_PROPERTY_QUERY query = STORAGE_PROPERTY_QUERY();
  query.PropertyId = StorageDeviceProperty;
  query.QueryType = PropertyStandardQuery;

  std::unique_ptr<char[]> output_buf(new char[1024]);
  status = DeviceIoControl(
      device_handle.Get(),            // Device handle.
      IOCTL_STORAGE_QUERY_PROPERTY,   // Flag to request device properties.
      &query,                         // Query parameters.
      sizeof(STORAGE_PROPERTY_QUERY), // query parameters size.
      output_buf.get(),               // output buffer.
      1024,                           // Size of buffer.
      &bytes_returned,                // Number of bytes returned.
                                      // Must not be null.
      NULL);                          // Optional unused overlapped perameter.

  if (status == FALSE) {
    PLOG(ERROR) << "Storage property query failed.";
    return false;
  }

  STORAGE_DEVICE_DESCRIPTOR* device_descriptor =
      reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(output_buf.get());

  if (!device_descriptor->RemovableMedia &&
      !(device_descriptor->BusType == BusTypeUsb)) {
    // Reject non-removable and non-USB devices.
    // Return true to indicate success but not add anything to the device list.
    return true;
  }

  // Create a drive identifier from the drive number.
  STORAGE_DEVICE_NUMBER device_number = {0};
  status = DeviceIoControl(
      device_handle.Get(),            // Device handle.
      IOCTL_STORAGE_GET_DEVICE_NUMBER,// Flag to request device number.
      NULL,                           // Query parameters, should be NULL.
      0,                              // Query parameters size, should be 0.
      &device_number,                 // output buffer.
      sizeof(device_number),          // Size of buffer.
      &bytes_returned,                // Number of bytes returned.
      NULL);                          // Optional unused overlapped perameter.

  if (status == FALSE) {
    PLOG(ERROR) << "Storage device number query failed.";
    return false;
  }

  std::string drive_id = "\\\\.\\PhysicalDrive";
  drive_id.append(base::NumberToString(device_number.DeviceNumber));

  api::image_writer_private::RemovableStorageDevice device;
  device.capacity = disk_capacity;
  device.storage_unit_id = drive_id;
  device.removable = device_descriptor->RemovableMedia == TRUE;

  if (device_descriptor->VendorIdOffset &&
      output_buf[device_descriptor->VendorIdOffset]) {
    device.vendor.assign(output_buf.get() + device_descriptor->VendorIdOffset);
  }

  std::string product_id;
  if (device_descriptor->ProductIdOffset &&
      output_buf[device_descriptor->ProductIdOffset]) {
    device.model.assign(output_buf.get() + device_descriptor->ProductIdOffset);
  }

  device_list->data.push_back(std::move(device));

  return true;
}

}  // namespace

// static
scoped_refptr<StorageDeviceList>
RemovableStorageProvider::PopulateDeviceList() {
  HDEVINFO interface_enumerator = SetupDiGetClassDevs(
      &DiskClassGuid,
      NULL, // Enumerator.
      NULL, // Parent window.
      // Only devices present & interface class.
      (DIGCF_PRESENT | DIGCF_INTERFACEDEVICE));

  if (interface_enumerator == INVALID_HANDLE_VALUE) {
    DPLOG(ERROR) << "SetupDiGetClassDevs failed.";
    return nullptr;
  }

  DWORD index = 0;
  SP_DEVICE_INTERFACE_DATA interface_data;
  interface_data.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

  auto device_list = base::MakeRefCounted<StorageDeviceList>();
  while (SetupDiEnumDeviceInterfaces(
      interface_enumerator,
      NULL,                    // Device Info data.
      &GUID_DEVINTERFACE_DISK, // Only disk devices.
      index,
      &interface_data)) {
    AddDeviceInfo(interface_enumerator, &interface_data, device_list);
    index++;
  }

  DWORD error_code = GetLastError();

  if (error_code != ERROR_NO_MORE_ITEMS) {
    PLOG(ERROR) << "SetupDiEnumDeviceInterfaces failed";
    SetupDiDestroyDeviceInfoList(interface_enumerator);
    return nullptr;
  }

  SetupDiDestroyDeviceInfoList(interface_enumerator);
  return device_list;
}

} // namespace extensions
