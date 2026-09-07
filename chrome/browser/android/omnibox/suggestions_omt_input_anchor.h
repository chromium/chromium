// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_SUGGESTIONS_OMT_INPUT_ANCHOR_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_SUGGESTIONS_OMT_INPUT_ANCHOR_H_

#include <android/input.h>
#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr_exclusion.h"

struct AInputReceiver;
struct AInputReceiverCallbacks;

namespace omnibox {

// Holds the native state of the registered input receiver.
// Passed as context to AInputReceiverCallbacks to avoid global state races
// and enable multi-window support.
struct OmtInputReceiverState {
  // Global reference to the Java InputTransferToken of the overlay SurfaceView.
  base::android::ScopedJavaGlobalRef<jobject> overlay_token;
  // Global reference to the Java InputTransferToken of the main Chrome window.
  base::android::ScopedJavaGlobalRef<jobject> main_window_token;

  // Pointer to the native input receiver registering the input channel.
  // RAW_PTR_EXCLUSION: Opaque NDK C handles managed by the Android system
  // (libandroid.so) and not allocated via PartitionAlloc.
  RAW_PTR_EXCLUSION AInputReceiver* input_receiver = nullptr;
  // Pointer to the callbacks container registered with the input receiver.
  RAW_PTR_EXCLUSION AInputReceiverCallbacks* callbacks = nullptr;

  OmtInputReceiverState();
  ~OmtInputReceiverState();
};

// Handles a motion event intercepted on the background looper thread.
// Exposed for unit testing and NDK callback routing.
bool ProcessMotionEvent(void* context,
                        int32_t event_type,
                        int32_t action,
                        int64_t event_time_ns,
                        bool trigger_transfer = true);

// NDK motion event callback matching AInputReceiverCallbacks signature.
bool OnMotionEvent(void* context, AInputEvent* motion_event);

}  // namespace omnibox

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_SUGGESTIONS_OMT_INPUT_ANCHOR_H_
