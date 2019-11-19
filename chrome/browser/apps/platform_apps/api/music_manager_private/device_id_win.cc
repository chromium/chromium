// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/music_manager_private/device_id.h"

// Note: The order of header includes is important, as we want both pre-Vista
// and post-Vista data structures to be defined, specifically
// PIP_ADAPTER_ADDRESSES and PMIB_IF_ROW2.

#include <limits.h>
#include <stddef.h>
#include <winsock2.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/windows_version.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "rlz/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#endif

namespace chrome_apps {
namespace api {

namespace {

typedef base::Callback<bool(const void* bytes, size_t size)>
    IsValidMacAddressCallback;

class MacAddressProcessor {
 public:
  MacAddressProcessor(const IsValidMacAddressCallback& is_valid_mac_address)
      : is_valid_mac_address_(is_valid_mac_address), found_index_(ULONG_MAX) {}

  // Iterate through the interfaces, looking for the valid MAC address with the
  // lowest IfIndex.
  void ProcessAdapterAddress(PIP_ADAPTER_ADDRESSES address) {
    if (address->IfType == IF_TYPE_TUNNEL)
      return;

    ProcessPhysicalAddress(address->IfIndex, address->PhysicalAddress,
                           address->PhysicalAddressLength);
  }

  void ProcessInterfaceRow(const PMIB_IF_ROW2 row) {
    if (row->Type == IF_TYPE_TUNNEL ||
        !row->InterfaceAndOperStatusFlags.HardwareInterface) {
      return;
    }

    ProcessPhysicalAddress(row->InterfaceIndex, row->PhysicalAddress,
                           row->PhysicalAddressLength);
  }

  std::string mac_address() const { return found_mac_address_; }

 private:
  void ProcessPhysicalAddress(NET_IFINDEX index,
                              const void* bytes,
                              size_t size) {
    if (index >= found_index_ || size == 0)
      return;

    if (!is_valid_mac_address_.Run(bytes, size))
      return;

    found_mac_address_ = base::ToLowerASCII(base::HexEncode(bytes, size));
    found_index_ = index;
  }

  const IsValidMacAddressCallback& is_valid_mac_address_;
  std::string found_mac_address_;
  NET_IFINDEX found_index_;
};

std::string GetMacAddressFromGetAdaptersAddresses(
    const IsValidMacAddressCallback& is_valid_mac_address) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // MS recommends a default size of 15k.
  ULONG bufferSize = 15 * 1024;
  // Disable as much as we can, since all we want is MAC addresses.
  ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER |
                GAA_FLAG_SKIP_FRIENDLY_NAME | GAA_FLAG_SKIP_MULTICAST |
                GAA_FLAG_SKIP_UNICAST;
  std::vector<unsigned char> buffer(bufferSize);
  PIP_ADAPTER_ADDRESSES adapterAddresses =
      reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&buffer.front());

  DWORD result =
      GetAdaptersAddresses(AF_UNSPEC, flags, 0, adapterAddresses, &bufferSize);
  if (result == ERROR_BUFFER_OVERFLOW) {
    buffer.resize(bufferSize);
    adapterAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(&buffer.front());
    result = GetAdaptersAddresses(AF_UNSPEC, flags, 0, adapterAddresses,
                                  &bufferSize);
  }

  if (result != NO_ERROR) {
    VLOG(ERROR) << "GetAdapatersAddresses failed with error " << result;
    return "";
  }

  MacAddressProcessor processor(is_valid_mac_address);
  for (; adapterAddresses != NULL; adapterAddresses = adapterAddresses->Next) {
    processor.ProcessAdapterAddress(adapterAddresses);
  }
  return processor.mac_address();
}

std::string GetMacAddressFromGetIfTable2(
    const IsValidMacAddressCallback& is_valid_mac_address) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // This is available on Vista+ only.
  base::ScopedNativeLibrary library(base::FilePath(L"Iphlpapi.dll"));

  typedef DWORD(NETIOAPI_API_ * GetIfTablePtr)(PMIB_IF_TABLE2*);
  typedef void(NETIOAPI_API_ * FreeMibTablePtr)(PMIB_IF_TABLE2);

  GetIfTablePtr getIfTable = reinterpret_cast<GetIfTablePtr>(
      library.GetFunctionPointer("GetIfTable2"));
  FreeMibTablePtr freeMibTablePtr = reinterpret_cast<FreeMibTablePtr>(
      library.GetFunctionPointer("FreeMibTable"));
  if (getIfTable == NULL || freeMibTablePtr == NULL) {
    VLOG(ERROR) << "Could not get proc addresses for machine identifier.";
    return "";
  }

  PMIB_IF_TABLE2 ifTable = NULL;
  DWORD result = getIfTable(&ifTable);
  if (result != NO_ERROR || ifTable == NULL) {
    VLOG(ERROR) << "GetIfTable failed with error " << result;
    return "";
  }

  MacAddressProcessor processor(is_valid_mac_address);
  for (size_t i = 0; i < ifTable->NumEntries; i++) {
    processor.ProcessInterfaceRow(&(ifTable->Table[i]));
  }

  if (ifTable != NULL) {
    freeMibTablePtr(ifTable);
    ifTable = NULL;
  }
  return processor.mac_address();
}

void GetMacAddress(const IsValidMacAddressCallback& is_valid_mac_address,
                   const DeviceId::IdCallback& callback) {
  std::string mac_address =
      GetMacAddressFromGetAdaptersAddresses(is_valid_mac_address);
  if (mac_address.empty())
    mac_address = GetMacAddressFromGetIfTable2(is_valid_mac_address);

  static bool error_logged = false;
  if (mac_address.empty() && !error_logged) {
    error_logged = true;
    LOG(ERROR) << "Could not find appropriate MAC address.";
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(callback, mac_address));
}

std::string GetRlzMachineId() {
#if BUILDFLAG(ENABLE_RLZ)
  std::string machine_id;
  if (!rlz_lib::GetMachineId(&machine_id))
    return std::string();
  return machine_id;
#else
  return std::string();
#endif
}

void GetMacAddressCallback(const DeviceId::IdCallback& callback,
                           const std::string& mac_address) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string machine_id = GetRlzMachineId();
  if (mac_address.empty() || machine_id.empty()) {
    callback.Run("");
    return;
  }
  callback.Run(mac_address + machine_id);
}

}  // namespace

// static
void DeviceId::GetRawDeviceId(const IdCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::PostTask(
      FROM_HERE, traits(),
      base::Bind(&GetMacAddress, base::Bind(&DeviceId::IsValidMacAddress),
                 base::Bind(&GetMacAddressCallback, callback)));
}

}  // namespace api
}  // namespace chrome_apps
