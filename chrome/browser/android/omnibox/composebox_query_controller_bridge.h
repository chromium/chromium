// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_string.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/contextual_search/internal/composebox_query_controller.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "third_party/jni_zero/jni_zero.h"

namespace content {
class WebContents;
}  //  namespace content

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

class Profile;
class GURL;

class ComposeboxQueryControllerBridge
    : public ComposeboxQueryController::FileUploadStatusObserver {
 public:
  explicit ComposeboxQueryControllerBridge(
      Profile* profile,
      const base::android::JavaParamRef<jobject>& java_obj);
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
  base::android::ScopedJavaLocalRef<jobject> AddTabContextFromCache(
      JNIEnv* env,
      long tab_id);
  void GetAimUrl(JNIEnv* env,
                 GURL url,
                 const base::android::JavaRef<jobject>& j_callback);
  void GetImageGenerationUrl(JNIEnv* env,
                             GURL url,
                             const base::android::JavaRef<jobject>& j_callback);
  void RemoveAttachment(JNIEnv* env, const std::string& token);
  bool IsPdfUploadEligible(JNIEnv* env);
  bool IsCreateImagesEligible(JNIEnv* env);

  std::unique_ptr<lens::proto::LensOverlaySuggestInputs>
  CreateLensOverlaySuggestInputs() const;

  // ComposeboxQueryController::FileUploadStatusObserver:
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      contextual_search::FileUploadStatus file_upload_status,
      const std::optional<contextual_search::FileUploadErrorType>& error_type)
      override;

  base::WeakPtr<ComposeboxQueryControllerBridge> AsWeakPtr();

 private:
  void OnGetTabPageContext(
      JNIEnv* env,
      const base::UnguessableToken& context_token,
      std::unique_ptr<lens::ContextualInputData> page_content_data);
  void OnGetPageContentFromCache(
      JNIEnv* env,
      const base::UnguessableToken& context_token,
      std::optional<optimization_guide::proto::PageContext> page_context);

  std::unique_ptr<ComposeboxQueryController::CreateSearchUrlRequestInfo>
  CreateSearchUrlRequestInfoFromUrl(GURL url);

  raw_ptr<Profile> profile_;
  std::unique_ptr<ComposeboxQueryController> query_controller_;
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  base::WeakPtrFactory<ComposeboxQueryControllerBridge> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_COMPOSEBOX_QUERY_CONTROLLER_BRIDGE_H_
