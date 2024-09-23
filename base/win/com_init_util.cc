// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/com_init_util.h"

#include <windows.h>

#include <stdint.h>
#include <winternl.h>

#include "base/logging.h"
#include "base/notreached.h"

namespace base {
namespace win {

namespace {

#if DCHECK_IS_ON()
const char kComNotInitialized[] = "COM is not initialized on this thread.";
#endif  // DCHECK_IS_ON()

// Derived from combase.dll.
struct OleTlsData {
  enum ApartmentFlags {
    LOGICAL_THREAD_REGISTERED = 0x2,
    STA = 0x80,
    MTA = 0x140,
  };

  uintptr_t thread_base;
  uintptr_t sm_allocator;
  DWORD apartment_id;
  DWORD apartment_flags;
  // There are many more fields than this, but for our purposes, we only care
  // about |apartment_flags|. Correctly declaring the previous types allows this
  // to work between x86 and x64 builds.
};

OleTlsData* GetOleTlsData() {
  TEB* teb = NtCurrentTeb();
  return reinterpret_cast<OleTlsData*>(teb->ReservedForOle);
}

}  // namespace

ComApartmentType GetComApartmentTypeForThread() {
  OleTlsData* ole_tls_data = GetOleTlsData();
  if (!ole_tls_data)
    return ComApartmentType::NONE;

  if (ole_tls_data->apartment_flags & OleTlsData::ApartmentFlags::STA)
    return ComApartmentType::STA;

  if ((ole_tls_data->apartment_flags & OleTlsData::ApartmentFlags::MTA) ==
      OleTlsData::ApartmentFlags::MTA) {
    return ComApartmentType::MTA;
  }

  return ComApartmentType::NONE;
}

#if DCHECK_IS_ON()

void AssertComInitialized(const char* message) {
  if (GetComApartmentTypeForThread() != ComApartmentType::NONE)
    return;

  // COM worker threads don't always set up the apartment, but they do perform
  // some thread registration, so we allow those.
  OleTlsData* ole_tls_data = GetOleTlsData();
  if (ole_tls_data && (ole_tls_data->apartment_flags &
                       OleTlsData::ApartmentFlags::LOGICAL_THREAD_REGISTERED)) {
    return;
  }

  NOTREACHED() << (message ? message : kComNotInitialized);
}

void AssertComApartmentType(ComApartmentType apartment_type) {
  DCHECK_EQ(apartment_type, GetComApartmentTypeForThread());
}

#endif  // DCHECK_IS_ON()

}  // namespace win
}  // namespace base
