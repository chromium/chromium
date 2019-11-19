// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/multiprocess_test.h"

#include <string.h>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/test/test_support_jni_headers/MainReturnCodeResult_jni.h"
#include "base/test/test_support_jni_headers/MultiprocessTestClientLauncher_jni.h"

namespace base {

// A very basic implementation for Android. On Android tests can run in an APK
// and we don't have an executable to exec*. This implementation does the bare
// minimum to execute the method specified by procname (in the child process).
//  - All options except |fds_to_remap| are ignored.
//
// NOTE: This MUST NOT run on the main thread of the NativeTest application.
Process SpawnMultiProcessTestChild(const std::string& procname,
                                   const CommandLine& base_command_line,
                                   const LaunchOptions& options) {
  JNIEnv* env = android::AttachCurrentThread();
  DCHECK(env);

  std::vector<int> fd_keys;
  std::vector<int> fd_fds;
  for (auto& iter : options.fds_to_remap) {
    fd_keys.push_back(iter.second);
    fd_fds.push_back(iter.first);
  }

  android::ScopedJavaLocalRef<jobjectArray> fds =
      android::Java_MultiprocessTestClientLauncher_makeFdInfoArray(
          env, base::android::ToJavaIntArray(env, fd_keys),
          base::android::ToJavaIntArray(env, fd_fds));

  CommandLine command_line(base_command_line);
  if (!command_line.HasSwitch(switches::kTestChildProcess)) {
    command_line.AppendSwitchASCII(switches::kTestChildProcess, procname);
  }

  android::ScopedJavaLocalRef<jobjectArray> j_argv =
      android::ToJavaArrayOfStrings(env, command_line.argv());
  jint pid = android::Java_MultiprocessTestClientLauncher_launchClient(
      env, j_argv, fds);
  return Process(pid);
}

bool WaitForMultiprocessTestChildExit(const Process& process,
                                      TimeDelta timeout,
                                      int* exit_code) {
  JNIEnv* env = android::AttachCurrentThread();
  DCHECK(env);

  base::android::ScopedJavaLocalRef<jobject> result_code =
      android::Java_MultiprocessTestClientLauncher_waitForMainToReturn(
          env, process.Pid(), static_cast<int32_t>(timeout.InMilliseconds()));
  if (result_code.is_null() ||
      Java_MainReturnCodeResult_hasTimedOut(env, result_code)) {
    return false;
  }
  if (exit_code) {
    *exit_code = Java_MainReturnCodeResult_getReturnCode(env, result_code);
  }
  return true;
}

bool TerminateMultiProcessTestChild(const Process& process,
                                    int exit_code,
                                    bool wait) {
  JNIEnv* env = android::AttachCurrentThread();
  DCHECK(env);

  return android::Java_MultiprocessTestClientLauncher_terminate(
      env, process.Pid(), exit_code, wait);
}

bool MultiProcessTestChildHasCleanExit(const Process& process) {
  JNIEnv* env = android::AttachCurrentThread();
  DCHECK(env);

  return android::Java_MultiprocessTestClientLauncher_hasCleanExit(
      env, process.Pid());
}

}  // namespace base
