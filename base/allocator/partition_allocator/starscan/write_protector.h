// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_WRITE_PROTECTOR_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_WRITE_PROTECTOR_H_

#include <cstddef>
#include <cstdint>
#include <mutex>

#include "base/allocator/partition_allocator/starscan/metadata_allocator.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/starscan/raceful_worklist.h"
#include "build/build_config.h"

namespace base {
namespace internal {

// Interface for page protection/unprotection. This is used in DCScan to catch
// concurrent mutator writes. Protection is done when the scanner starts
// scanning a range. Unprotection happens at the end of the scanning phase.
class WriteProtector : public AllocatedOnPCScanMetadataPartition {
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

#if defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
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

#endif  // defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_WRITE_PROTECTOR_H_
