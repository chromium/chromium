// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/android_input_receiver_compat.h"

#include <dlfcn.h>

#include "base/android/build_info.h"
#include "base/check.h"
#include "base/logging.h"

namespace base {

AndroidInputReceiverCompat::AndroidInputReceiverCompat() {
  DCHECK(IsSupportAvailable());

  void* main_dl_handle = dlopen("libandroid.so", RTLD_NOW);
  if (!main_dl_handle) {
    LOG(ERROR) << "Couldnt load android so";
    return;
  }

  *reinterpret_cast<void**>(&AInputTransferToken_fromJavaFn) =
      dlsym(main_dl_handle, "AInputTransferToken_fromJava");
  DCHECK(AInputTransferToken_fromJavaFn);

  *reinterpret_cast<void**>(&AInputTransferToken_toJavaFn) =
      dlsym(main_dl_handle, "AInputTransferToken_toJava");
  DCHECK(AInputTransferToken_toJavaFn);

  *reinterpret_cast<void**>(&AInputTransferToken_releaseFn) =
      dlsym(main_dl_handle, "AInputTransferToken_release");
  DCHECK(AInputTransferToken_releaseFn);

  *reinterpret_cast<void**>(&AInputEvent_toJavaFn) =
      dlsym(main_dl_handle, "AInputEvent_toJava");
  DCHECK(AInputEvent_toJavaFn);

  *reinterpret_cast<void**>(&AInputReceiverCallbacks_createFn) =
      dlsym(main_dl_handle, "AInputReceiverCallbacks_create");
  DCHECK(AInputReceiverCallbacks_createFn);

  *reinterpret_cast<void**>(&AInputReceiverCallbacks_releaseFn) =
      dlsym(main_dl_handle, "AInputReceiverCallbacks_release");
  DCHECK(AInputReceiverCallbacks_releaseFn);

  *reinterpret_cast<void**>(&AInputReceiverCallbacks_setMotionEventCallbackFn) =
      dlsym(main_dl_handle, "AInputReceiverCallbacks_setMotionEventCallback");
  DCHECK(AInputReceiverCallbacks_setMotionEventCallbackFn);

  *reinterpret_cast<void**>(&AInputReceiver_createUnbatchedInputReceiverFn) =
      dlsym(main_dl_handle, "AInputReceiver_createUnbatchedInputReceiver");
  DCHECK(AInputReceiver_createUnbatchedInputReceiverFn);

  *reinterpret_cast<void**>(&AInputReceiver_getInputTransferTokenFn) =
      dlsym(main_dl_handle, "AInputReceiver_getInputTransferToken");
  DCHECK(AInputReceiver_getInputTransferTokenFn);

  *reinterpret_cast<void**>(&AInputReceiver_releaseFn) =
      dlsym(main_dl_handle, "AInputReceiver_release");
  DCHECK(AInputReceiver_releaseFn);
}

// static
bool AndroidInputReceiverCompat::IsSupportAvailable() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_V;
}

// static
const AndroidInputReceiverCompat& AndroidInputReceiverCompat::GetInstance() {
  static AndroidInputReceiverCompat compat;
  return compat;
}

}  // namespace base
