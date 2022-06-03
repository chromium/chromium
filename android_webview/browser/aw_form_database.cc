// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_form_database_service.h"
#include "android_webview/browser_jni_headers/AwFormDatabase_jni.h"
#include "base/android/jni_android.h"
#include "base/time/time.h"

using base::android::JavaParamRef;

namespace android_webview {

namespace {

AwFormDatabaseService* GetFormDatabaseService() {
  AwBrowserContext* context = AwBrowserContext::GetDefault();
  AwFormDatabaseService* service = context->GetFormDatabaseService();
  return service;
}

}  // anonymous namespace

// static
jboolean JNI_AwFormDatabase_HasFormData(JNIEnv*) {
  return GetFormDatabaseService()->HasFormData();
}

// static
void JNI_AwFormDatabase_ClearFormData(JNIEnv*) {
  GetFormDatabaseService()->ClearFormData();
}

}  // namespace android_webview
