// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/bundle_utils.h"

#include <android/dlext.h>
#include <dlfcn.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base_jni/BundleUtils_jni.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/notreached.h"

// These symbols are added by the lld linker when creating a partitioned shared
// library. The symbols live in the base library, and are used to properly load
// the other partitions (feature libraries) when needed.
struct PartitionIndexEntry {
  int32_t name_relptr;
  int32_t addr_relptr;
  uint32_t size;
};
static_assert(sizeof(PartitionIndexEntry) == 12U,
              "Unexpected PartitionIndexEntry size");

// Marked as weak_import because these symbols are lld-specific. The method that
// uses them will only be invoked in builds that have lld-generated partitions.
extern PartitionIndexEntry __part_index_begin[] __attribute__((weak_import));
extern PartitionIndexEntry __part_index_end[] __attribute__((weak_import));

namespace base {
namespace android {

namespace {

// Takes as input a "rel pointer", which is a pointer to a 32-bit integer that
// contains the offset to add to the pointer, in order to find the actual
// desired pointer address.
//
// # Safety
// If the value in the pointer does not provide an offset from the pointer that
// stays inside the same allocation, Undefined Behaviour can result.
UNSAFE_BUFFER_USAGE void* ReadRelPtr(int32_t* relptr) {
  // SAFETY: This relies on the caller to provide a valid pointer + value.
  return UNSAFE_BUFFERS(reinterpret_cast<char*>(relptr) + *relptr);
}

}  // namespace

// static
std::string BundleUtils::ResolveLibraryPath(const std::string& library_name,
                                            const std::string& split_name) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_path = Java_BundleUtils_getNativeLibraryPath(
      env, ConvertUTF8ToJavaString(env, library_name),
      ConvertUTF8ToJavaString(env, split_name));
  // TODO(crbug.com/40656179): Remove this tolerance.
  if (!java_path) {
    return std::string();
  }
  return ConvertJavaStringToUTF8(env, java_path);
}

// static
bool BundleUtils::IsBundle() {
  return Java_BundleUtils_isBundleForNative(AttachCurrentThread());
}

// static
void* BundleUtils::DlOpenModuleLibraryPartition(const std::string& library_name,
                                                const std::string& partition,
                                                const std::string& split_name) {
  // TODO(crbug.com/40656179): Remove this tolerance.
  std::string library_path = ResolveLibraryPath(library_name, split_name);
  if (library_path.empty()) {
    return nullptr;
  }

  // Linear search is required here because the partition descriptors are not
  // ordered. If a large number of partitions come into existence, lld could be
  // modified to sort the partitions.
  DCHECK(__part_index_begin != nullptr);
  DCHECK(__part_index_end != nullptr);
  // SAFETY: `__part_index_begin` and `__part_index_end` are provided by the
  // linker (https://lld.llvm.org/Partitions.html) and we rely on the linker to
  // provide pointers that are part of the same allocation with
  // `__part_index_begin <= __part_index_end`.
  auto parts = UNSAFE_BUFFERS(
      span<PartitionIndexEntry>(__part_index_begin, __part_index_end));
  for (PartitionIndexEntry& part : parts) {
    std::string name(static_cast<const char*>(
        // SAFETY: `name_relptr` plus its value points to a nul-terminated
        // string containing the soname of the partition. This pointer and
        // offset is provided by the linker and thus assumed to always be
        // correct. https://lld.llvm.org/Partitions.html
        UNSAFE_BUFFERS(ReadRelPtr(&part.name_relptr))));
    if (name == partition) {
      android_dlextinfo info = {};
      info.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
      info.reserved_addr =
          // SAFETY: `addr_offset` field is a relative pointer to the
          // partition's load address. This pointer and offset is provided by
          // the linker and thus assumed to always be correct.
          // https://lld.llvm.org/Partitions.html
          UNSAFE_BUFFERS(ReadRelPtr(&part.addr_relptr));
      info.reserved_size = part.size;

#if __ANDROID_API__ >= 24
      return android_dlopen_ext(library_path.c_str(), RTLD_LOCAL, &info);
#else
      // When targeting pre-N, such as for Cronet, android_dlopen_ext() might
      // not be available on the system.
      CHECK(0) << "android_dlopen_ext not available";
#endif
    }
  }

  NOTREACHED();
}

}  // namespace android
}  // namespace base
