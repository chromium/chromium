// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/attribution_reporting/background_attribution_flusher.h"

#include "base/callback.h"
#include "chrome/browser/attribution_reporting/android/jni_headers/BackgroundAttributionFlusher_jni.h"

BackgroundAttributionFlusher::BackgroundAttributionFlusher() = default;

BackgroundAttributionFlusher::~BackgroundAttributionFlusher() {
  if (jobj_) {
    Java_BackgroundAttributionFlusher_nativeDestroyed(
        base::android::AttachCurrentThread(), jobj_);
  }
}

void BackgroundAttributionFlusher::FlushPreNativeAttributions(
    base::OnceClosure completed_callback) {
  if (pre_native_flush_complete_) {
    std::move(completed_callback).Run();
    return;
  }

  pending_callbacks_.push_back(std::move(completed_callback));

  if (pre_native_flush_in_progress_)
    return;

  jobj_ = Java_BackgroundAttributionFlusher_Constructor(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
  pre_native_flush_in_progress_ = true;
  Java_BackgroundAttributionFlusher_flushPreNativeAttributions(
      base::android::AttachCurrentThread(), jobj_);
}

void BackgroundAttributionFlusher::OnFlushComplete(JNIEnv* env) {
  pre_native_flush_complete_ = true;
  jobj_ = nullptr;
  for (auto& callback : pending_callbacks_)
    std::move(callback).Run();
  pending_callbacks_.clear();
}
