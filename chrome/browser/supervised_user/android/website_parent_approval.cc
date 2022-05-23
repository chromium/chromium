// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/android/website_parent_approval.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/supervised_user/jni_headers/WebsiteParentApproval_jni.h"

using base::android::JavaParamRef;

// static
bool WebsiteParentApproval::IsLocalApprovalSupported() {
  return Java_WebsiteParentApproval_isLocalApprovalSupported(
      base::android::AttachCurrentThread());
}

void WebsiteParentApproval::RequestLocalApproval() {
  Java_WebsiteParentApproval_requestLocalApproval(
      base::android::AttachCurrentThread());
}
