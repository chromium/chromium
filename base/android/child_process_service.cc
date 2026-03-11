// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/child_process_service.h"

#include <optional>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/android/self_compaction_manager.h"
#include "base/debug/dump_without_crashing.h"
#include "base/file_descriptor_store.h"
#include "base/logging.h"
#include "base/posix/global_descriptors.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/process_launcher_jni/ChildProcessService_jni.h"

using base::android::JavaIntArrayToIntVector;
using base::android::JavaRef;

namespace base {
namespace android {

void RegisterFileDescriptors(
    const std::vector<std::optional<std::string>>& keys,
    const std::vector<int32_t>& ids,
    const std::vector<int32_t>& fds,
    const std::vector<int64_t>& offsets,
    const std::vector<int64_t>& sizes) {
  DCHECK_EQ(keys.size(), ids.size());
  DCHECK_EQ(keys.size(), fds.size());
  DCHECK_EQ(keys.size(), offsets.size());
  DCHECK_EQ(keys.size(), sizes.size());

  for (size_t i = 0; i < keys.size(); i++) {
    base::MemoryMappedFile::Region region = {offsets[i],
                                             static_cast<size_t>(sizes[i])};
    const std::optional<std::string>& key = keys[i];
    int id = ids[i];
    int fd = fds[i];
    if (key) {
      base::FileDescriptorStore::GetInstance().Set(*key, base::ScopedFD(fd),
                                                   region);
    } else {
      base::GlobalDescriptors::GetInstance()->Set(static_cast<uint32_t>(id), fd,
                                                  region);
    }
  }
}

static void JNI_ChildProcessService_RegisterFileDescriptors(
    JNIEnv* env,
    const std::vector<std::optional<std::string>>& keys,
    const std::vector<int32_t>& ids,
    const std::vector<int32_t>& fds,
    const std::vector<int64_t>& offsets,
    const std::vector<int64_t>& sizes) {
  RegisterFileDescriptors(keys, ids, fds, offsets, sizes);
}

static void JNI_ChildProcessService_ExitChildProcess(JNIEnv* env) {
  VLOG(0) << "ChildProcessService: Exiting child process.";
  base::android::LibraryLoaderExitHook();
  _exit(0);
}

// Make sure this isn't inlined so it shows up in stack traces.
// the function body unique by adding a log line, so it doesn't get merged
// with other functions by link time optimizations (ICF).
NOINLINE static void JNI_ChildProcessService_DumpProcessStack(JNIEnv* env) {
  DumpProcessStack();
}

void DumpProcessStack() {
  LOG(ERROR) << "Dumping as requested.";
  base::debug::DumpWithoutCrashing();
}

static void JNI_ChildProcessService_OnSelfFreeze(JNIEnv* env) {
  OnSelfFreeze();
}

void OnSelfFreeze() {
  SelfCompactionManager::OnSelfFreeze();
}

}  // namespace android
}  // namespace base

DEFINE_JNI(ChildProcessService)
