// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SHARED_MEMORY_HANDLE_H_
#define BASE_MEMORY_SHARED_MEMORY_HANDLE_H_

#include <stddef.h>

#include "base/unguessable_token.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/process/process_handle.h"
#include "base/win/windows_types.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include <mach/mach.h>
#include "base/base_export.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#elif defined(OS_POSIX)
#include <sys/types.h>
#include "base/file_descriptor_posix.h"
#elif defined(OS_FUCHSIA)
#include <zircon/types.h>
#endif

namespace base {

// SharedMemoryHandle is the smallest possible IPC-transportable "reference" to
// a shared memory OS resource. A "reference" can be consumed exactly once [by
// base::SharedMemory] to map the shared memory OS resource into the virtual
// address space of the current process.
// TODO(erikchen): This class should have strong ownership semantics to prevent
// leaks of the underlying OS resource. https://crbug.com/640840.
//
// DEPRECATED - Use {Writable,ReadOnly}SharedMemoryRegion instead.
// http://crbug.com/795291
class BASE_EXPORT SharedMemoryHandle {
 public:
  // The default constructor returns an invalid SharedMemoryHandle.
  SharedMemoryHandle();

  // Standard copy constructor. The new instance shares the underlying OS
  // primitives.
  SharedMemoryHandle(const SharedMemoryHandle& handle);

  // Standard assignment operator. The updated instance shares the underlying
  // OS primitives.
  SharedMemoryHandle& operator=(const SharedMemoryHandle& handle);

  // Closes the underlying OS resource.
  // The fact that this method needs to be "const" is an artifact of the
  // original interface for base::SharedMemory::CloseHandle.
  // TODO(erikchen): This doesn't clear the underlying reference, which seems
  // like a bug, but is how this class has always worked. Fix this:
  // https://crbug.com/716072.
  void Close() const;

  // Whether ownership of the underlying OS resource is implicitly passed to
  // the IPC subsystem during serialization.
  void SetOwnershipPassesToIPC(bool ownership_passes);
  bool OwnershipPassesToIPC() const;

  // Whether the underlying OS resource is valid.
  bool IsValid() const;

  // Duplicates the underlying OS resource. Using the return value as a
  // parameter to an IPC message will cause the IPC subsystem to consume the OS
  // resource.
  SharedMemoryHandle Duplicate() const;

  // Uniques identifies the shared memory region that the underlying OS resource
  // points to. Multiple SharedMemoryHandles that point to the same shared
  // memory region will have the same GUID. Preserved across IPC.
  base::UnguessableToken GetGUID() const;

  // Returns the size of the memory region that SharedMemoryHandle points to.
  size_t GetSize() const;

#if defined(OS_WIN)
  // Takes implicit ownership of |h|.
  // |guid| uniquely identifies the shared memory region pointed to by the
  // underlying OS resource. If the HANDLE is associated with another
  // SharedMemoryHandle, the caller must pass the |guid| of that
  // SharedMemoryHandle. Otherwise, the caller should generate a new
  // UnguessableToken.
  // Passing the wrong |size| has no immediate consequence, but may cause errors
  // when trying to map the SharedMemoryHandle at a later point in time.
  SharedMemoryHandle(HANDLE h, size_t size, const base::UnguessableToken& guid);
  HANDLE GetHandle() const;
#elif defined(OS_FUCHSIA)
  // Takes implicit ownership of |h|.
  // |guid| uniquely identifies the shared memory region pointed to by the
  // underlying OS resource. If the zx_handle_t is associated with another
  // SharedMemoryHandle, the caller must pass the |guid| of that
  // SharedMemoryHandle. Otherwise, the caller should generate a new
  // UnguessableToken.
  // Passing the wrong |size| has no immediate consequence, but may cause errors
  // when trying to map the SharedMemoryHandle at a later point in time.
  SharedMemoryHandle(zx_handle_t h,
                     size_t size,
                     const base::UnguessableToken& guid);
  zx_handle_t GetHandle() const;
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  // Makes a Mach-based SharedMemoryHandle of the given size. On error,
  // subsequent calls to IsValid() return false.
  // Passing the wrong |size| has no immediate consequence, but may cause errors
  // when trying to map the SharedMemoryHandle at a later point in time.
  SharedMemoryHandle(mach_vm_size_t size, const base::UnguessableToken& guid);

  // Makes a Mach-based SharedMemoryHandle from |memory_object|, a named entry
  // in the current task. The memory region has size |size|.
  // Passing the wrong |size| has no immediate consequence, but may cause errors
  // when trying to map the SharedMemoryHandle at a later point in time.
  SharedMemoryHandle(mach_port_t memory_object,
                     mach_vm_size_t size,
                     const base::UnguessableToken& guid);

  // Exposed so that the SharedMemoryHandle can be transported between
  // processes.
  mach_port_t GetMemoryObject() const;

  // The SharedMemoryHandle must be valid.
  // Returns whether the SharedMemoryHandle was successfully mapped into memory.
  // On success, |memory| is an output variable that contains the start of the
  // mapped memory.
  bool MapAt(off_t offset, size_t bytes, void** memory, bool read_only);
#elif defined(OS_POSIX)
  // Creates a SharedMemoryHandle from an |fd| supplied from an external
  // service.
  // Passing the wrong |size| has no immediate consequence, but may cause errors
  // when trying to map the SharedMemoryHandle at a later point in time.
  static SharedMemoryHandle ImportHandle(int fd, size_t size);

  // Returns the underlying OS resource.
  int GetHandle() const;

  // Invalidates [but doesn't close] the underlying OS resource. This will leak
  // unless the caller is careful.
  int Release();
#endif

#if defined(OS_ANDROID)
  // Marks the current file descriptor as read-only, for the purpose of
  // mapping. This is independent of the region's read-only status.
  void SetReadOnly() { read_only_ = true; }

  // Returns true iff the descriptor is to be used for read-only
  // mappings.
  bool IsReadOnly() const { return read_only_; }

  // Returns true iff the corresponding region is read-only.
  bool IsRegionReadOnly() const;

  // Try to set the region read-only. This will fail any future attempt
  // at read-write mapping.
  bool SetRegionReadOnly() const;
#endif

#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
  // Constructs a SharedMemoryHandle backed by a FileDescriptor. The newly
  // created instance has the same ownership semantics as base::FileDescriptor.
  // This typically means that the SharedMemoryHandle takes ownership of the
  // |fd| if |auto_close| is true. Unfortunately, it's common for existing code
  // to make shallow copies of SharedMemoryHandle, and the one that is finally
  // passed into a base::SharedMemory is the one that "consumes" the fd.
  //
  // |guid| uniquely identifies the shared memory region pointed to by the
  // underlying OS resource. If |file_descriptor| is associated with another
  // SharedMemoryHandle, the caller must pass the |guid| of that
  // SharedMemoryHandle. Otherwise, the caller should generate a new
  // UnguessableToken.
  // Passing the wrong |size| has no immediate consequence, but may cause errors
  // when trying to map the SharedMemoryHandle at a later point in time.
  SharedMemoryHandle(const base::FileDescriptor& file_descriptor,
                     size_t size,
                     const base::UnguessableToken& guid);
#endif

 private:
#if defined(OS_WIN)
  HANDLE handle_ = nullptr;

  // Whether passing this object as a parameter to an IPC message passes
  // ownership of |handle_| to the IPC stack. This is meant to mimic the
  // behavior of the |auto_close| parameter of FileDescriptor. This member only
  // affects attachment-brokered SharedMemoryHandles.
  // Defaults to |false|.
  bool ownership_passes_to_ipc_ = false;
#elif defined(OS_FUCHSIA)
  zx_handle_t handle_ = ZX_HANDLE_INVALID;
  bool ownership_passes_to_ipc_ = false;
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  friend class SharedMemory;
  friend bool CheckReadOnlySharedMemoryHandleForTesting(
      SharedMemoryHandle handle);

  mach_port_t memory_object_ = MACH_PORT_NULL;

  // Whether passing this object as a parameter to an IPC message passes
  // ownership of |memory_object_| to the IPC stack. This is meant to mimic
  // the behavior of the |auto_close| parameter of FileDescriptor.
  // Defaults to |false|.
  bool ownership_passes_to_ipc_ = false;
#elif defined(OS_ANDROID)
  friend class SharedMemory;

  FileDescriptor file_descriptor_;
  bool read_only_ = false;
#elif defined(OS_POSIX)
  FileDescriptor file_descriptor_;
#endif

  base::UnguessableToken guid_;

  // The size of the region referenced by the SharedMemoryHandle.
  size_t size_ = 0;
};

}  // namespace base

#endif  // BASE_MEMORY_SHARED_MEMORY_HANDLE_H_
