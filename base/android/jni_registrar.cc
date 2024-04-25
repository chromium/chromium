// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/android/jni_registrar.h"

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace android {

bool RegisterNativeMethods(JNIEnv* env,
                           const RegistrationMethod* method,
                           size_t count) {
  TRACE_EVENT0("startup", "base_android::RegisterNativeMethods");
  const RegistrationMethod* end = method + count;
  while (method != end) {
    if (!method->func(env)) {
      DLOG(ERROR) << method->name << " failed registration!";
      return false;
    }
    method++;
  }
  return true;
}

}  // namespace android
}  // namespace base
