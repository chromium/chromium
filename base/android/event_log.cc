// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/event_log.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/EventLog_jni.h"

namespace base {
namespace android {

void EventLogWriteInt(int tag, int value) {
  Java_EventLog_writeEvent(jni_zero::AttachCurrentThread(), tag, value);
}

}  // namespace android
}  // namespace base
