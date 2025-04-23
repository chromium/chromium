// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/collaboration/android/collaboration_controller_delegate_android.h"

#include "base/android/jni_string.h"
#include "base/debug/dump_without_crashing.h"
#include "components/collaboration/public/android/conversion_utils.h"
#include "components/data_sharing/public/android/conversion_utils.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_bridge.h"
#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_utils.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/collaboration/internal_jni_headers/CollaborationControllerDelegateImpl_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using ResultCallback =
    collaboration::CollaborationControllerDelegate::ResultCallback;
using ResultWithGroupTokenCallback = collaboration::
    CollaborationControllerDelegate::ResultWithGroupTokenCallback;

namespace collaboration {

static void JNI_CollaborationControllerDelegateImpl_RunResultCallback(
    JNIEnv* env,
    jint joutcome,
    jlong callback) {
  std::unique_ptr<ResultCallback> callback_ptr =
      conversion::GetNativeResultCallbackFromJava(callback);
  CollaborationControllerDelegate::Outcome outcome =
      static_cast<CollaborationControllerDelegate::Outcome>(joutcome);
  std::move(*callback_ptr).Run(outcome);
}

static void JNI_CollaborationControllerDelegateImpl_RunExitCallback(
    JNIEnv* env,
    jlong callback) {
  std::unique_ptr<base::OnceClosure> callback_ptr =
      conversion::GetNativeExitCallbackFromJava(callback);
  std::move(*callback_ptr).Run();
}

static void JNI_CollaborationControllerDelegateImpl_DeleteExitCallback(
    JNIEnv* env,
    jlong callback) {
  std::unique_ptr<base::OnceClosure> callback_ptr =
      conversion::GetNativeExitCallbackFromJava(callback);
  callback_ptr.reset();
}

static void
JNI_CollaborationControllerDelegateImpl_RunResultWithGroupTokenCallback(
    JNIEnv* env,
    jint joutcome,
    const JavaParamRef<jstring>& group_id,
    const JavaParamRef<jstring>& access_token,
    jlong callback) {
  std::unique_ptr<ResultWithGroupTokenCallback> callback_ptr =
      conversion::GetNativeResultWithGroupTokenCallbackFromJava(callback);
  CollaborationControllerDelegate::Outcome outcome =
      static_cast<CollaborationControllerDelegate::Outcome>(joutcome);

  std::optional<data_sharing::GroupToken> token = std::nullopt;
  if (outcome == CollaborationControllerDelegate::Outcome::kSuccess) {
    token = data_sharing::GroupToken(
        data_sharing::GroupId(ConvertJavaStringToUTF8(env, group_id)),
        ConvertJavaStringToUTF8(env, access_token));
  }
  std::move(*callback_ptr).Run(outcome, token);
}

static jlong JNI_CollaborationControllerDelegateImpl_CreateNativeObject(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  std::unique_ptr<CollaborationControllerDelegate> delegate_unique_ptr =
      std::make_unique<CollaborationControllerDelegateAndroid>(j_object);

  return conversion::GetJavaDelegateUniquePtr(std::move(delegate_unique_ptr));
}

CollaborationControllerDelegateAndroid::CollaborationControllerDelegateAndroid(
    const base::android::JavaParamRef<jobject>& j_object) {
  DCHECK(j_object);
  java_obj_.Reset(base::android::ScopedJavaGlobalRef<jobject>(j_object));
}

CollaborationControllerDelegateAndroid::
    ~CollaborationControllerDelegateAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!on_flow_finished_called_) {
    base::debug::DumpWithoutCrashing();
  }
  Java_CollaborationControllerDelegateImpl_clearNativePtr(env, java_obj_);
}

void CollaborationControllerDelegateAndroid::PrepareFlowUI(
    base::OnceClosure exit_callback,
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_prepareFlowUI(
      env, java_obj_,
      conversion::GetJavaExitCallbackPtr(std::move(exit_callback)),
      conversion::GetJavaResultCallbackPtr(std::move(result)));
}

void CollaborationControllerDelegateAndroid::ShowError(const ErrorInfo& error,
                                                       ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_showError(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(env, error.error_header),
      base::android::ConvertUTF8ToJavaString(env, error.error_body),
      conversion::GetJavaResultCallbackPtr(std::move(result)));
}

void CollaborationControllerDelegateAndroid::Cancel(ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_cancel(
      env, java_obj_, conversion::GetJavaResultCallbackPtr(std::move(result)));
}

void CollaborationControllerDelegateAndroid::ShowAuthenticationUi(
    FlowType flow_type,
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_showAuthenticationUi(
      env, java_obj_, conversion::GetJavaResultCallbackPtr(std::move(result)));
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
      conversion::GetJavaResultCallbackPtr(std::move(result)));
}

void CollaborationControllerDelegateAndroid::ShowShareDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultWithGroupTokenCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();

  if (std::holds_alternative<base::Uuid>(either_id)) {
    Java_CollaborationControllerDelegateImpl_showShareDialog(
        env, java_obj_,
        tab_groups::UuidToJavaString(env, std::get<base::Uuid>(either_id)),
        /*localId=*/nullptr,
        conversion::GetJavaResultWithGroupTokenCallbackPtr(std::move(result)));
    return;
  }

  Java_CollaborationControllerDelegateImpl_showShareDialog(
      env, java_obj_, /*syncId=*/nullptr,
      tab_groups::TabGroupSyncConversionsBridge::ToJavaTabGroupId(
          env, std::get<tab_groups::LocalTabGroupID>(either_id)),
      conversion::GetJavaResultWithGroupTokenCallbackPtr(std::move(result)));
}

void CollaborationControllerDelegateAndroid::OnUrlReadyToShare(
    const data_sharing::GroupId& group_id,
    const GURL& url,
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_onUrlReadyToShare(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(env, group_id.value()),
      url::GURLAndroid::FromNativeGURL(env, url),
      conversion::GetJavaResultCallbackPtr(std::move(result)));
}

void CollaborationControllerDelegateAndroid::ShowManageDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();

  if (std::holds_alternative<base::Uuid>(either_id)) {
    Java_CollaborationControllerDelegateImpl_showManageDialog(
        env, java_obj_,
        tab_groups::UuidToJavaString(env, std::get<base::Uuid>(either_id)),
        /*localId=*/nullptr,
        conversion::GetJavaResultCallbackPtr(std::move(result)));
    return;
  }
  Java_CollaborationControllerDelegateImpl_showManageDialog(
      env, java_obj_, /*syncId=*/nullptr,
      tab_groups::TabGroupSyncConversionsBridge::ToJavaTabGroupId(
          env, std::get<tab_groups::LocalTabGroupID>(either_id)),
      conversion::GetJavaResultCallbackPtr(std::move(result)));
}

void CollaborationControllerDelegateAndroid::ShowLeaveDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();

  if (std::holds_alternative<base::Uuid>(either_id)) {
    Java_CollaborationControllerDelegateImpl_showLeaveDialog(
        env, java_obj_,
        tab_groups::UuidToJavaString(env, std::get<base::Uuid>(either_id)),
        /*localId=*/nullptr,
        conversion::GetJavaResultCallbackPtr(std::move(result)));
    return;
  }
  Java_CollaborationControllerDelegateImpl_showLeaveDialog(
      env, java_obj_, /*syncId=*/nullptr,
      tab_groups::TabGroupSyncConversionsBridge::ToJavaTabGroupId(
          env, std::get<tab_groups::LocalTabGroupID>(either_id)),
      conversion::GetJavaResultCallbackPtr(std::move(result)));
}

void CollaborationControllerDelegateAndroid::ShowDeleteDialog(
    const tab_groups::EitherGroupID& either_id,
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();

  if (std::holds_alternative<base::Uuid>(either_id)) {
    Java_CollaborationControllerDelegateImpl_showDeleteDialog(
        env, java_obj_,
        tab_groups::UuidToJavaString(env, std::get<base::Uuid>(either_id)),
        /*localId=*/nullptr,
        conversion::GetJavaResultCallbackPtr(std::move(result)));
    return;
  }
  Java_CollaborationControllerDelegateImpl_showDeleteDialog(
      env, java_obj_, /*syncId=*/nullptr,
      tab_groups::TabGroupSyncConversionsBridge::ToJavaTabGroupId(
          env, std::get<tab_groups::LocalTabGroupID>(either_id)),
      conversion::GetJavaResultCallbackPtr(std::move(result)));
}

void CollaborationControllerDelegateAndroid::PromoteTabGroup(
    const data_sharing::GroupId& group_id,
    ResultCallback result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_promoteTabGroup(
      env, java_obj_,
      base::android::ConvertUTF8ToJavaString(env, group_id.value()),
      conversion::GetJavaResultCallbackPtr(std::move(result)));
}

void CollaborationControllerDelegateAndroid::PromoteCurrentScreen() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_promoteCurrentScreen(env, java_obj_);
}

void CollaborationControllerDelegateAndroid::OnFlowFinished() {
  on_flow_finished_called_ = true;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_CollaborationControllerDelegateImpl_onFlowFinished(env, java_obj_);
}

ScopedJavaLocalRef<jobject>
CollaborationControllerDelegateAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace collaboration
