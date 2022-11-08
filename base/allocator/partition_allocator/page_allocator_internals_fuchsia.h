// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements memory allocation primitives for PageAllocator using
// Fuchsia's VMOs (Virtual Memory Objects). VMO API is documented in
// https://fuchsia.dev/fuchsia-src/zircon/objects/vm_object . A VMO is a kernel
// object that corresponds to a set of memory pages. VMO pages may be mapped
// to an address space. The code below creates VMOs for each memory allocations
// and maps them to the default address space of the current process.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_FUCHSIA_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_FUCHSIA_H_

#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>

#include <cstdint>

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_base/fuchsia/fuchsia_logging.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"

namespace partition_alloc::internal {

namespace {

// Returns VMO name for a PageTag.
const char* PageTagToName(PageTag tag) {
  switch (tag) {
    case PageTag::kBlinkGC:
      return "cr_blink_gc";
    case PageTag::kPartitionAlloc:
      return "cr_partition_alloc";
    case PageTag::kChromium:
      return "cr_chromium";
    case PageTag::kV8:
      return "cr_v8";
    default:
      PA_DCHECK(false);
      return "";
  }
}

zx_vm_option_t PageAccessibilityToZxVmOptions(
    PageAccessibilityConfiguration accessibility) {
  switch (accessibility.permissions) {
    case PageAccessibilityConfiguration::kRead:
      return ZX_VM_PERM_READ;
    case PageAccessibilityConfiguration::kReadWrite:
    case PageAccessibilityConfiguration::kReadWriteTagged:
      return ZX_VM_PERM_READ | ZX_VM_PERM_WRITE;
    case PageAccessibilityConfiguration::kReadExecuteProtected:
    case PageAccessibilityConfiguration::kReadExecute:
      return ZX_VM_PERM_READ | ZX_VM_PERM_EXECUTE;
    case PageAccessibilityConfiguration::kReadWriteExecute:
      return ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE;
    default:
      PA_NOTREACHED();
      [[fallthrough]];
    case PageAccessibilityConfiguration::kInaccessible:
      return 0;
  }
}

}  // namespace

// zx_vmar_map() will fail if the VMO cannot be mapped at |vmar_offset|, i.e.
// |hint| is not advisory.
constexpr bool kHintIsAdvisory = false;

std::atomic<int32_t> s_allocPageErrorCode{0};

uintptr_t SystemAllocPagesInternal(
    uintptr_t hint,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageTag page_tag,
    [[maybe_unused]] int file_descriptor_for_shared_alloc) {
  zx::vmo vmo;
  zx_status_t status = zx::vmo::create(length, 0, &vmo);
  if (status != ZX_OK) {
    PA_ZX_DLOG(INFO, status) << "zx_vmo_create";
    return 0;
  }

  const char* vmo_name = PageTagToName(page_tag);
  status = vmo.set_property(ZX_PROP_NAME, vmo_name, strlen(vmo_name));

  // VMO names are used only for debugging, so failure to set a name is not
  // fatal.
  PA_ZX_DCHECK(status == ZX_OK, status);

  if (page_tag == PageTag::kV8) {
    // V8 uses JIT. Call zx_vmo_replace_as_executable() to allow code execution
    // in the new VMO.
    status = vmo.replace_as_executable(zx::resource(), &vmo);
    if (status != ZX_OK) {
      PA_ZX_DLOG(INFO, status) << "zx_vmo_replace_as_executable";
      return 0;
    }
  }

  zx_vm_option_t options = PageAccessibilityToZxVmOptions(accessibility);

  uint64_t vmar_offset = 0;
  if (hint) {
    vmar_offset = hint;
    options |= ZX_VM_SPECIFIC;
  }

  uint64_t address;
  status =
      zx::vmar::root_self()->map(options, vmar_offset, vmo,
                                 /*vmo_offset=*/0, length, &address);
  if (status != ZX_OK) {
    // map() is expected to fail if |hint| is set to an already-in-use location.
    if (!hint) {
      PA_ZX_DLOG(ERROR, status) << "zx_vmar_map";
    }
    return 0;
  }

  return address;
}

uintptr_t TrimMappingInternal(uintptr_t base_address,
                              size_t base_length,
                              size_t trim_length,
                              PageAccessibilityConfiguration accessibility,
                              size_t pre_slack,
                              size_t post_slack) {
  PA_DCHECK(base_length == trim_length + pre_slack + post_slack);

  // Unmap head if necessary.
  if (pre_slack) {
    zx_status_t status = zx::vmar::root_self()->unmap(base_address, pre_slack);
    PA_ZX_CHECK(status == ZX_OK, status);
  }

  // Unmap tail if necessary.
  if (post_slack) {
    zx_status_t status = zx::vmar::root_self()->unmap(
        base_address + pre_slack + trim_length, post_slack);
    PA_ZX_CHECK(status == ZX_OK, status);
  }

  return base_address + pre_slack;
}

bool TrySetSystemPagesAccessInternal(
    uint64_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility) {
  zx_status_t status = zx::vmar::root_self()->protect(
      PageAccessibilityToZxVmOptions(accessibility), address, length);
  return status == ZX_OK;
}

void SetSystemPagesAccessInternal(
    uint64_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility) {
  zx_status_t status = zx::vmar::root_self()->protect(
      PageAccessibilityToZxVmOptions(accessibility), address, length);
  PA_ZX_CHECK(status == ZX_OK, status);
}

void FreePagesInternal(uint64_t address, size_t length) {
  zx_status_t status = zx::vmar::root_self()->unmap(address, length);
  PA_ZX_CHECK(status == ZX_OK, status);
}

void DiscardSystemPagesInternal(uint64_t address, size_t length) {
  // TODO(https://crbug.com/1022062): Mark pages as discardable, rather than
  // forcibly de-committing them immediately, when Fuchsia supports it.
  zx_status_t status = zx::vmar::root_self()->op_range(
      ZX_VMO_OP_DECOMMIT, address, length, nullptr, 0);
  PA_ZX_CHECK(status == ZX_OK, status);
}

void DecommitSystemPagesInternal(
    uint64_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  if (accessibility_disposition ==
      PageAccessibilityDisposition::kRequireUpdate) {
    SetSystemPagesAccess(address, length,
                         PageAccessibilityConfiguration(
                             PageAccessibilityConfiguration::kInaccessible));
  }

  // TODO(https://crbug.com/1022062): Review whether this implementation is
  // still appropriate once DiscardSystemPagesInternal() migrates to a "lazy"
  // discardable API.
  DiscardSystemPagesInternal(address, length);
}

void DecommitAndZeroSystemPagesInternal(uintptr_t address, size_t length) {
  SetSystemPagesAccess(address, length,
                       PageAccessibilityConfiguration(
                           PageAccessibilityConfiguration::kInaccessible));

  // TODO(https://crbug.com/1022062): this implementation will likely no longer
  // be appropriate once DiscardSystemPagesInternal() migrates to a "lazy"
  // discardable API.
  DiscardSystemPagesInternal(address, length);
}

void RecommitSystemPagesInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageAccessibilityDisposition accessibility_disposition) {
  // On Fuchsia systems, the caller needs to simply read the memory to recommit
  // it. However, if decommit changed the permissions, recommit has to change
  // them back.
  if (accessibility_disposition ==
      PageAccessibilityDisposition::kRequireUpdate) {
    SetSystemPagesAccess(address, length, accessibility);
  }
}

bool TryRecommitSystemPagesInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageAccessibilityDisposition accessibility_disposition) {
  // On Fuchsia systems, the caller needs to simply read the memory to recommit
  // it. However, if decommit changed the permissions, recommit has to change
  // them back.
  if (accessibility_disposition ==
      PageAccessibilityDisposition::kRequireUpdate) {
    return TrySetSystemPagesAccess(address, length, accessibility);
  }
  return true;
}

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_INTERNALS_FUCHSIA_H_
