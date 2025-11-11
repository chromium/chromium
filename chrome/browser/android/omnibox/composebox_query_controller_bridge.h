// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_string.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_search/internal/composebox_query_controller.h"
#include "third_party/jni_zero/jni_zero.h"

namespace content {
class WebContents;
}  //  namespace content

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
  base::android::ScopedJavaLocalRef<jobject> AddTabContext(
      JNIEnv* env,
      content::WebContents* web_contents);
  GURL GetAimUrl(JNIEnv* env, std::string& query_text);
  void RemoveAttachment(JNIEnv* env, const std::string& token);

  // ComposeboxQueryController::FileUploadStatusObserver:
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      contextual_search::FileUploadStatus file_upload_status,
      const std::optional<contextual_search::FileUploadErrorType>& error_type)
      override;

 private:
  void OnGetTabPageContext(
      JNIEnv* env,
      const base::UnguessableToken& context_token,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  raw_ptr<Profile> profile_;
  std::unique_ptr<ComposeboxQueryController> query_controller_;
  base::WeakPtrFactory<ComposeboxQueryControllerBridge> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_
