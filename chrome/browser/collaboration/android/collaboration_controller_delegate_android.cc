// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/android/collaboration_controller_delegate_android.h"

#include "base/android/jni_string.h"
#include "components/data_sharing/public/android/conversion_utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/collaboration/internal_jni_headers/CollaborationControllerDelegateImpl_jni.h"

using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using ResultCallback =
    collaboration::CollaborationControllerDelegate::ResultCallback;

namespace collaboration {

namespace {

jlong GetCallbackPointer(ResultCallback result) {
  std::unique_ptr<ResultCallback> wrapped_callback =
      std::make_unique<ResultCallback>(std::move(result));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  wrapped_callback.release();

  return j_native_ptr;
}

}  // namespace

static void JNI_CollaborationControllerDelegateImpl_RunResultCallback(
    JNIEnv* env,
    jint joutcome,
    jlong callback) {
  std::unique_ptr<ResultCallback> callback_ptr(
      reinterpret_cast<ResultCallback*>(callback));
  CollaborationControllerDelegate::Outcome outcome =
      static_cast<CollaborationControllerDelegate::Outcome>(joutcome);
  std::move(*callback_ptr).Run(outcome);
}

CollaborationControllerDelegateAndroid::CollaborationControllerDelegateAndroid(
    ScopedJavaGlobalRef<jobject> java_obj) {
  DCHECK(java_obj);
  java_obj_.Reset(java_obj);
}

CollaborationControllerDelegateAndroid::
    ~CollaborationControllerDelegateAndroid() = default;

void CollaborationControllerDelegateAndroid::PrepareFlowUI(
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_prepareFlowUI(
      env, java_obj_, GetCallbackPointer(std::move(result)));
}

void CollaborationControllerDelegateAndroid::ShowError(const ErrorInfo& error,
                                                       ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_showError(
      env, java_obj_, static_cast<int>(error.type),
      GetCallbackPointer(std::move(result)));
}

void CollaborationControllerDelegateAndroid::Cancel(ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_cancel(
      env, java_obj_, GetCallbackPointer(std::move(result)));
}

void CollaborationControllerDelegateAndroid::ShowAuthenticationUi(
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_showAuthenticationUi(
      env, java_obj_, GetCallbackPointer(std::move(result)));
}

void CollaborationControllerDelegateAndroid::NotifySignInAndSyncStatusChange() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_notifySignInAndSyncStatusChange(
      env, java_obj_);
}

void CollaborationControllerDelegateAndroid::ShowJoinDialog(
    const data_sharing::GroupToken& token,
    const data_sharing::SharedDataPreview& preview_data,
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_showJoinDialog(
      env, java_obj_,
      data_sharing::conversion::CreateJavaGroupToken(env, token),
      data_sharing::conversion::CreateJavaSharedTabGroupPreview(
          env, preview_data.shared_tab_group_preview.value()),
      GetCallbackPointer(std::move(result)));
}

void CollaborationControllerDelegateAndroid::ShowShareDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_showShareDialog(
      env, java_obj_, GetCallbackPointer(std::move(result)));
}

void CollaborationControllerDelegateAndroid::PromoteTabGroup(
    const data_sharing::GroupId& group_id,
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_promoteTabGroup(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(env, group_id.value()),
      GetCallbackPointer(std::move(result)));
}

void CollaborationControllerDelegateAndroid::PromoteCurrentScreen() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_promoteCurrentScreen(env, java_obj_);
}

ScopedJavaLocalRef<jobject>
CollaborationControllerDelegateAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace collaboration
