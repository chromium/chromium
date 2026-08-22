// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/screen_state_receiver.h"

#include "base/android/jni_android.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/observer_list_threadsafe.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/screen_state_receiver_jni/ScreenStateReceiver_jni.h"

namespace base::android {

namespace {

class ScreenStateReceiverImpl {
 public:
  static ScreenStateReceiverImpl* GetInstance() {
    static base::NoDestructor<ScreenStateReceiverImpl> instance;
    return instance.get();
  }

  ScreenStateReceiverImpl() {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    Java_ScreenStateReceiver_registerThreadSafeNativeScreenStateObserver(env);
  }

  void AddObserver(ScreenStateReceiver::Observer* observer) {
    observers_->AddObserver(observer);
  }

  void RemoveObserver(ScreenStateReceiver::Observer* observer) {
    observers_->RemoveObserver(observer);
  }

  void OnScreenOff() {
    observers_->Notify(FROM_HERE, &ScreenStateReceiver::Observer::OnScreenOff);
  }

  void OnScreenOn() {
    observers_->Notify(FROM_HERE, &ScreenStateReceiver::Observer::OnScreenOn);
  }

 private:
  scoped_refptr<base::ObserverListThreadSafe<ScreenStateReceiver::Observer>>
      observers_ = base::MakeRefCounted<
          base::ObserverListThreadSafe<ScreenStateReceiver::Observer>>();
};

}  // namespace

// static
void ScreenStateReceiver::AddObserver(Observer* observer) {
  ScreenStateReceiverImpl::GetInstance()->AddObserver(observer);
}

// static
void ScreenStateReceiver::RemoveObserver(Observer* observer) {
  ScreenStateReceiverImpl::GetInstance()->RemoveObserver(observer);
}

static void JNI_ScreenStateReceiver_OnScreenOff(JNIEnv* env) {
  ScreenStateReceiverImpl::GetInstance()->OnScreenOff();
}

static void JNI_ScreenStateReceiver_OnScreenOn(JNIEnv* env) {
  ScreenStateReceiverImpl::GetInstance()->OnScreenOn();
}

DEFINE_JNI(ScreenStateReceiver)

}  // namespace base::android
