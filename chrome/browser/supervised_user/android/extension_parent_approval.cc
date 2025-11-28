// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/android/extension_parent_approval.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/supervised_user/extension_parent_approval_jni_headers/ExtensionParentApproval_jni.h"

using base::android::JavaParamRef;

// Stores the callback passed in to an ongoing RequestExtensionApproval call.
// We can only have a single extension approval in progress at a time on Android
// as the implementation is a bottom sheet (which is dismissed if it loses
// focus).
base::OnceCallback<void(extensions::SupervisedExtensionApprovalResult)>*
GetOnApprovalCallback() {
  static base::NoDestructor<
      base::OnceCallback<void(extensions::SupervisedExtensionApprovalResult)>>
      callback;
  return callback.get();
}

// static
bool ExtensionParentApproval::IsExtensionApprovalSupported() {
  return Java_ExtensionParentApproval_isExtensionApprovalSupported(
      base::android::AttachCurrentThread());
}

// static
void ExtensionParentApproval::RequestExtensionApproval(
    content::WebContents* web_contents,
    base::OnceCallback<void(extensions::SupervisedExtensionApprovalResult)>
        callback) {
  if (!GetOnApprovalCallback()->is_null()) {
    // There is a pending operation in progress. This is
    // possible if for example the user clicks the request approval button in
    // quick succession before the auth bottom sheet is displayed.
    // Recover by just dropping the second operation.
    std::move(callback).Run(
        extensions::SupervisedExtensionApprovalResult::kCanceled);
    return;
  }

  ui::WindowAndroid* window_android =
      web_contents->GetNativeView()->GetWindowAndroid();

  *GetOnApprovalCallback() = std::move(callback);

  // Call site for JNI function.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ExtensionParentApproval_requestExtensionApproval(
      env, window_android->GetJavaObject());
}

static void JNI_ExtensionParentApproval_OnCompletion(JNIEnv* env,
                                                     jint result_value) {
  // Check that we have a callback stored from the extension approval request
  // and call it.
  auto* cb = GetOnApprovalCallback();
  DCHECK(!cb->is_null());
  extensions::SupervisedExtensionApprovalResult result_enum =
      static_cast<extensions::SupervisedExtensionApprovalResult>(result_value);
  std::move(*cb).Run(result_enum);
}

DEFINE_JNI(ExtensionParentApproval)
