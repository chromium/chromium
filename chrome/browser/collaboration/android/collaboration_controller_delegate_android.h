// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COLLABORATION_ANDROID_COLLABORATION_CONTROLLER_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_COLLABORATION_ANDROID_COLLABORATION_CONTROLLER_DELEGATE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/supports_user_data.h"
#include "components/collaboration/public/collaboration_controller_delegate.h"
#include "components/collaboration/public/collaboration_flow_type.h"

namespace collaboration {

// Android side implementation of a CollaborationControllerDelegate.
class CollaborationControllerDelegateAndroid
    : public CollaborationControllerDelegate {
 public:
  explicit CollaborationControllerDelegateAndroid(
      const base::android::JavaParamRef<jobject>& j_object);
  ~CollaborationControllerDelegateAndroid() override;

  // CollaborationControllerDelegate.
  void PrepareFlowUI(base::OnceCallback<void()> exit_callback,
                     ResultCallback result) override;
  void ShowError(const ErrorInfo& error, ResultCallback result) override;
  void Cancel(ResultCallback result) override;
  void ShowAuthenticationUi(FlowType flow_type, ResultCallback result) override;
  void NotifySignInAndSyncStatusChange() override;
  void ShowJoinDialog(const data_sharing::GroupToken& token,
                      const data_sharing::SharedDataPreview& preview_data,
                      ResultCallback result) override;
  void ShowShareDialog(
      const tab_groups::EitherGroupID& either_id,
      CollaborationControllerDelegate::ResultWithGroupTokenCallback result)
      override;
  void OnUrlReadyToShare(const data_sharing::GroupId& group_id,
                         const GURL& url,
                         ResultCallback result) override;
  void ShowManageDialog(const tab_groups::EitherGroupID& either_id,
                        ResultCallback result) override;
  void ShowLeaveDialog(const tab_groups::EitherGroupID& either_id,
                       ResultCallback result) override;
  void ShowDeleteDialog(const tab_groups::EitherGroupID& either_id,
                        ResultCallback result) override;
  void PromoteTabGroup(const data_sharing::GroupId& group_id,
                       ResultCallback result) override;
  void PromoteCurrentScreen() override;
  void OnFlowFinished() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;

 private:
  bool on_flow_finished_called_{false};
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
};

}  // namespace collaboration

#endif  // CHROME_BROWSER_COLLABORATION_ANDROID_COLLABORATION_CONTROLLER_DELEGATE_ANDROID_H_
