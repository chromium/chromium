// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_STARSCAN_WRITE_PROTECTOR_H_
#define PARTITION_ALLOC_STARSCAN_WRITE_PROTECTOR_H_

#include <cstddef>
#include <cstdint>
#include <mutex>

#include "partition_alloc/build_config.h"
#include "partition_alloc/internal_allocator_forward.h"
#include "partition_alloc/starscan/pcscan.h"
#include "partition_alloc/starscan/raceful_worklist.h"

namespace partition_alloc::internal {

// Interface for page protection/unprotection. This is used in DCScan to catch
// concurrent mutator writes. Protection is done when the scanner starts
// scanning a range. Unprotection happens at the end of the scanning phase.
class WriteProtector : public internal::InternalPartitionAllocated {
 public:
  virtual ~WriteProtector() = default;

  virtual void ProtectPages(uintptr_t begin, size_t length) = 0;
  virtual void UnprotectPages(uintptr_t begin, size_t length) = 0;

  virtual bool IsEnabled() const = 0;

  virtual PCScan::ClearType SupportedClearType() const = 0;
};

class NoWriteProtector final : public WriteProtector {
 public:
  void ProtectPages(uintptr_t, size_t) final {}
  void UnprotectPages(uintptr_t, size_t) final {}
  PCScan::ClearType SupportedClearType() const final;
  inline bool IsEnabled() const override;
};

bool NoWriteProtector::IsEnabled() const {
  return false;
}

#if PA_CONFIG(STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
class UserFaultFDWriteProtector final : public WriteProtector {
 public:
  UserFaultFDWriteProtector();

  UserFaultFDWriteProtector(const UserFaultFDWriteProtector&) = delete;
  UserFaultFDWriteProtector& operator=(const UserFaultFDWriteProtector&) =
      delete;

  void ProtectPages(uintptr_t, size_t) final;
  void UnprotectPages(uintptr_t, size_t) final;

  PCScan::ClearType SupportedClearType() const final;

  inline bool IsEnabled() const override;

 private:
  bool IsSupported() const;

  const int uffd_ = 0;
};

bool UserFaultFDWriteProtector::IsEnabled() const {
  return IsSupported();
}
#endif  // PA_CONFIG(STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_STARSCAN_WRITE_PROTECTOR_H_
