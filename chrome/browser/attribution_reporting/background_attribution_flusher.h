// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ATTRIBUTION_REPORTING_BACKGROUND_ATTRIBUTION_FLUSHER_H_
#define CHROME_BROWSER_ATTRIBUTION_REPORTING_BACKGROUND_ATTRIBUTION_FLUSHER_H_

#include <jni.h>

#include <vector>

#include "base/android/jni_android.h"
#include "base/callback_forward.h"

// This class is responsible for communicating with java code to handle flushing
// events received in the background to native code.
class BackgroundAttributionFlusher {
 public:
  BackgroundAttributionFlusher();
  ~BackgroundAttributionFlusher();

  BackgroundAttributionFlusher(const BackgroundAttributionFlusher&) = delete;
  BackgroundAttributionFlusher& operator=(const BackgroundAttributionFlusher&) =
      delete;

  void FlushPreNativeAttributions(base::OnceClosure completed_callback);
  void OnFlushComplete(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobj_;
  bool pre_native_flush_complete_ = false;
  bool pre_native_flush_in_progress_ = false;
  std::vector<base::OnceClosure> pending_callbacks_;
};

#endif  // CHROME_BROWSER_ATTRIBUTION_REPORTING_BACKGROUND_ATTRIBUTION_FLUSHER_H_
