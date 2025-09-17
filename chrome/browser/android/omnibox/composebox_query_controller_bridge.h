// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "third_party/jni_zero/jni_zero.h"

class Profile;
class GURL;

class ComposeboxQueryControllerBridge
    : public ComposeboxQueryController::FileUploadStatusObserver {
 public:
  explicit ComposeboxQueryControllerBridge(Profile* profile);
  ~ComposeboxQueryControllerBridge() override;
  void Destroy(JNIEnv* env);
  void NotifySessionStarted(JNIEnv* env);
  void NotifySessionAbandoned(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobject> AddFile(
      JNIEnv* env,
      std::string& file_name,
      std::string& file_type,
      const jni_zero::JavaParamRef<jobject>& file_data);
  GURL GetAimUrl(JNIEnv* env, std::string& query_text);
  void RemoveAttachment(JNIEnv* env, const std::string& token);

  // ComposeboxQueryController::FileUploadStatusObserver:
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      composebox_query::mojom::FileUploadStatus file_upload_status,
      const std::optional<FileUploadErrorType>& error_type) override;

 private:
  std::unique_ptr<ComposeboxQueryController> query_controller_;
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_
