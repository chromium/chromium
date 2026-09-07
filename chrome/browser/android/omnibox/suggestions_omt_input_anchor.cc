// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/suggestions_omt_input_anchor.h"

#include <android/input.h>
#include <android/looper.h>
#include <dlfcn.h>
#include <jni.h>

#include <algorithm>

#include "base/android/android_input_receiver_compat.h"
#include "base/android/scoped_input_event.h"
#include "base/android/scoped_java_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/SuggestionsOmtInputAnchor_jni.h"

// Forward declare ASurfaceControl type from NDK.
typedef struct ASurfaceControl ASurfaceControl;

typedef ASurfaceControl* (*pASurfaceControl_fromJava)(JNIEnv*, jobject);

// Dynamically loads the ASurfaceControl_fromJava function pointer from
// libandroid.so with thread-safe static initialization.
static pASurfaceControl_fromJava GetASurfaceControl_fromJava() {
  static pASurfaceControl_fromJava func = []() {
    void* main_dl_handle = dlopen("libandroid.so", RTLD_NOW);
    return main_dl_handle ? reinterpret_cast<pASurfaceControl_fromJava>(dlsym(
                                main_dl_handle, "ASurfaceControl_fromJava"))
                          : nullptr;
  }();
  return func;
}

namespace omnibox {

OmtInputReceiverState::OmtInputReceiverState() = default;
OmtInputReceiverState::~OmtInputReceiverState() = default;

bool ProcessMotionEvent(void* context,
                        int32_t event_type,
                        int32_t action,
                        int64_t event_time_ns,
                        bool trigger_transfer) {
  // Filter out non-motion events.
  if (event_type != AINPUT_EVENT_TYPE_MOTION) {
    return false;
  }

  // Intercept touch DOWN natively to log latency metrics and trigger transfer.
  if ((action & AMOTION_EVENT_ACTION_MASK) != AMOTION_EVENT_ACTION_DOWN) {
    return false;
  }

  // Calculate elapsed duration from hardware touch timestamp to background
  // execution. Clamped to 0ms to guard against synthetic events or timestamp
  // precision differences.
  base::TimeDelta delay = std::max(
      base::TimeDelta(), base::TimeTicks::Now() -
                             base::TimeTicks::FromJavaNanoTime(event_time_ns));
  base::UmaHistogramTimes("Android.Omnibox.OMTPrefetch.TouchDownDelay", delay);

  if (trigger_transfer) {
    auto* state = static_cast<OmtInputReceiverState*>(context);
    if (!state) {
      return false;
    }
    JNIEnv* env = jni_zero::AttachCurrentThread();
    // Transfer the touch gesture focus back to the main Chrome window to
    // continue normal touch dispatch.
    Java_SuggestionsOmtInputAnchor_transferTouch(env, state->overlay_token,
                                                 state->main_window_token);
  }

  return true;
}

bool OnMotionEvent(void* context, AInputEvent* motion_event) {
  int32_t event_type = AInputEvent_getType(motion_event);
  int32_t action = AMotionEvent_getAction(motion_event);
  int64_t event_time_ns = AMotionEvent_getEventTime(motion_event);
  return ProcessMotionEvent(context, event_type, action, event_time_ns);
}

}  // namespace omnibox

// Unregisters the background thread input receiver and releases associated
// native callbacks and resources. Must be called on the background
// HandlerThread to satisfy NDK thread constraints.
static void JNI_SuggestionsOmtInputAnchor_UnregisterInputReceiver(
    JNIEnv* env,
    int64_t state_address) {
  if (!state_address) {
    return;
  }

  omnibox::OmtInputReceiverState* state =
      reinterpret_cast<omnibox::OmtInputReceiverState*>(state_address);
  const auto& compat = base::AndroidInputReceiverCompat::GetInstance();

  if (state->input_receiver) {
    compat.AInputReceiver_releaseFn(state->input_receiver);
  }

  if (state->callbacks) {
    compat.AInputReceiverCallbacks_releaseFn(state->callbacks);
  }

  delete state;
}

// Registers the background thread input receiver to intercept touches on the
// suggestions overlay. Should be called on the background HandlerThread when
// the overlay surface is created.
// Returns a pointer to the allocated OmtInputReceiverState as a long to Java.
static int64_t JNI_SuggestionsOmtInputAnchor_RegisterInputReceiver(
    JNIEnv* env,
    const jni_zero::JavaRef<::android::view::JSurfaceControl>& surface_control,
    const jni_zero::JavaRef<::android::window::JInputTransferToken>&
        main_window_token) {
  const auto& compat = base::AndroidInputReceiverCompat::GetInstance();
  if (!compat.IsSupportAvailable()) {
    return 0;
  }

  auto from_java_sc = GetASurfaceControl_fromJava();
  if (!from_java_sc) {
    return 0;
  }

  ASurfaceControl* native_sc = from_java_sc(env, surface_control.obj());
  if (!native_sc) {
    return 0;
  }

  ALooper* native_looper = ALooper_forThread();
  if (!native_looper) {
    native_looper = ALooper_prepare(0);
  }
  if (!native_looper) {
    return 0;
  }

  // Heap-allocated to persist across asynchronous input event callbacks on the
  // background thread until explicitly freed in UnregisterInputReceiver. Its
  // address is passed as context to AInputReceiverCallbacks and returned to
  // Java as a lifecycle handle.
  auto* state = new omnibox::OmtInputReceiverState();

  AInputReceiverCallbacks* callbacks =
      compat.AInputReceiverCallbacks_createFn(state);
  if (!callbacks) {
    delete state;
    return 0;
  }

  compat.AInputReceiverCallbacks_setMotionEventCallbackFn(
      callbacks, omnibox::OnMotionEvent);

  // Retrieve native main window token for input channel creation.
  AInputTransferToken* native_main_window_token =
      compat.AInputTransferToken_fromJavaFn(env, main_window_token.obj());
  if (!native_main_window_token) {
    compat.AInputReceiverCallbacks_releaseFn(callbacks);
    delete state;
    return 0;
  }

  // Create native input receiver.
  AInputReceiver* receiver =
      compat.AInputReceiver_createUnbatchedInputReceiverFn(
          native_looper, native_main_window_token, native_sc, callbacks);

  // Release main window token reference immediately as the input receiver
  // duplicates the token internally during creation.
  compat.AInputTransferToken_releaseFn(native_main_window_token);

  if (!receiver) {
    compat.AInputReceiverCallbacks_releaseFn(callbacks);
    delete state;
    return 0;
  }

  AInputTransferToken* native_receiver_token =
      compat.AInputReceiver_getInputTransferTokenFn(receiver);
  if (!native_receiver_token) {
    compat.AInputReceiver_releaseFn(receiver);
    compat.AInputReceiverCallbacks_releaseFn(callbacks);
    delete state;
    return 0;
  }

  auto receiver_token_java = jni_zero::AdoptRef(
      env, compat.AInputTransferToken_toJavaFn(env, native_receiver_token));
  // Release receiver token reference after converting to Java reference.
  compat.AInputTransferToken_releaseFn(native_receiver_token);

  state->overlay_token.Reset(env, receiver_token_java);
  state->main_window_token.Reset(env, main_window_token);
  state->input_receiver = receiver;
  state->callbacks = callbacks;

  return reinterpret_cast<int64_t>(state);
}

DEFINE_JNI(SuggestionsOmtInputAnchor)
