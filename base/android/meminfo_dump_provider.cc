// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/meminfo_dump_provider.h"
#include <jni.h>
#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/memory_jni/MemoryInfoBridge_jni.h"
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace base::android {

MeminfoDumpProvider::MeminfoDumpProvider() {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, kDumpProviderName, nullptr);
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

// static
MeminfoDumpProvider& MeminfoDumpProvider::Initialize() {
  static base::NoDestructor<MeminfoDumpProvider> instance;
  return *instance.get();
}

bool MeminfoDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  // This is best-effort, and will be wrong if there are other callers of
  // ActivityManager#getProcessMemoryInfo(), either in this process or from
  // another process which is allowed to do so (typically, adb).
  //
  // However, since the framework doesn't document throttling in any non-vague
  // terms and the results are not timestamped, this is the best we can do. The
  // delay and the rest of the assumptions here come from
  // https://android.googlesource.com/platform/frameworks/base/+/refs/heads/android13-dev/services/core/java/com/android/server/am/ActivityManagerService.java#4093.
  //
  // We could always report the value on pre-Q devices, but that would skew
  // reported data. Also, some OEMs may have cherry-picked the Q change, meaning
  // that it's safer and more accurate to not report likely-stale data on all
  // Android releases.
  base::TimeTicks now = base::TimeTicks::Now();
  bool stale_data = (now - last_collection_time_) < base::Minutes(5);

  // Background data dumps (as in the BACKGROUND level of detail, not the
  // application being in background) should not include stale data, since it
  // would confuse data in UMA. In particular, the background/foreground session
  // filter would no longer be accurate.
  if (stale_data && args.level_of_detail !=
                        base::trace_event::MemoryDumpLevelOfDetail::kDetailed) {
    return true;
  }

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(kDumpName);
  // Data is either expected to be fresh, or this is a manually requested dump,
  // and we should still report data, but note that it is stale.
  dump->AddScalar(kIsStaleName, "bool", stale_data);

  last_collection_time_ = now;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> memory_info =
      Java_MemoryInfoBridge_getActivityManagerMemoryInfoForSelf(env);
  // Tell the manager that collection failed. Since this is likely not a
  // transient failure, don't return an empty dump, and let the manager exclude
  // this provider from the next dump.
  if (memory_info.is_null()) {
    LOG(WARNING) << "Got a null value";
    return false;
  }

  ScopedJavaLocalRef<jclass> clazz{env, env->GetObjectClass(memory_info.obj())};

  jfieldID other_private_dirty_id =
      env->GetFieldID(clazz.obj(), "otherPrivateDirty", "I");
  jfieldID other_pss_id = env->GetFieldID(clazz.obj(), "otherPss", "I");

  int other_private_dirty_kb =
      env->GetIntField(memory_info.obj(), other_private_dirty_id);
  int other_pss_kb = env->GetIntField(memory_info.obj(), other_pss_id);

  // What "other" covers is not documented in Debug#MemoryInfo, nor in
  // ActivityManager#getProcessMemoryInfo. However, it calls
  // Debug#getMemoryInfo(), which ends up summing all the heaps in the range
  // [HEAP_DALVIK_OTHER, HEAP_OTHER_MEMTRACK]. See the definitions in
  // https://android.googlesource.com/platform/frameworks/base/+/0b7c1774ba42daef7c80bf2f00fe1c0327e756ae/core/jni/android_os_Debug.cpp#60,
  // and the code in android_os_Debug_getDirtyPagesPid() in the same file.
  dump->AddScalar(kPrivateDirtyMetricName, "bytes",
                  static_cast<uint64_t>(other_private_dirty_kb) * 1024);
  dump->AddScalar(kPssMetricName, "bytes",
                  static_cast<uint64_t>(other_pss_kb) * 1024);

  return true;
#else   // BUILDFLAG(ENABLE_BASE_TRACING)
  return false;
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

}  // namespace base::android
