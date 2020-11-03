// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_H_
#define CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/paint_preview_policy.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"

#if defined(os_android)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#endif  // defined(os_android)

namespace content {
class WebContents;
}  // namespace content

namespace paint_preview {

// A service for capturing and using Paint Previews per Tab. Captures are stored
// using Tab IDs as the key such that the data can be accessed even if the
// browser is restarted.
class PaintPreviewTabService : public PaintPreviewBaseService {
 public:
  PaintPreviewTabService(const base::FilePath& profile_dir,
                         base::StringPiece ascii_feature_name,
                         std::unique_ptr<PaintPreviewPolicy> policy,
                         bool is_off_the_record);
  ~PaintPreviewTabService() override;

  enum Status {
    kOk = 0,
    kDirectoryCreationFailed = 1,
    kCaptureFailed = 2,
    kProtoSerializationFailed = 3,
    kWebContentsGone = 4,
  };

  using FinishedCallback = base::OnceCallback<void(Status)>;
  using BooleanCallback = base::OnceCallback<void(bool)>;

  bool CacheInitialized() const { return cache_ready_; }

  // Captures a Paint Preview of |contents| which should be associated with
  // |tab_id| for storage. |callback| is invoked on completion to indicate
  // status.
  void CaptureTab(int tab_id,
                  content::WebContents* contents,
                  FinishedCallback callback);

  // Destroys the Paint Preview associated with |tab_id|. This MUST be called
  // when a tab is closed to ensure the captured contents don't outlive the tab.
  void TabClosed(int tab_id);

  // Checks if there is a capture taken for |tab_id|.
  bool HasCaptureForTab(int tab_id);

  // This should be called on startup with a list of restored tab ids
  // (|active_tab_ids|). This performs an audit over all Paint Previews stored
  // by this service and destroys any that don't correspond to active tabs. This
  // is required as TabClosed could have been interrupted or an accounting error
  // occurred.
  void AuditArtifacts(const std::vector<int>& active_tab_ids);

  // Override for GetCapturedPaintPreviewProto. Defaults expiry horizon to 72
  // hrs if not specified.
  void GetCapturedPaintPreviewProto(
      const DirectoryKey& key,
      base::Optional<base::TimeDelta> expiry_horizon,
      PaintPreviewBaseService::OnReadProtoCallback on_read_proto_callback)
      override;

#if defined(OS_ANDROID)
  // JNI wrapped versions of the above methods
  void CaptureTabAndroid(
      JNIEnv* env,
      jint j_tab_id,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      const base::android::JavaParamRef<jobject>& j_callback);
  void TabClosedAndroid(JNIEnv* env, jint j_tab_id);
  jboolean HasCaptureForTabAndroid(JNIEnv* env, jint j_tab_id);
  void AuditArtifactsAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jintArray>& j_tab_ids);
  jboolean IsCacheInitializedAndroid(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> GetPathAndroid(JNIEnv* env);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaRef() { return java_ref_; }
#endif  // defined(OS_ANDROID)

 private:
  // Caches current captures in |captured_tab_ids_|. Called as part of
  // initialization.
  void InitializeCache(const base::flat_set<DirectoryKey>& in_use_keys);

  // The FTN ID is to look-up the content::WebContents.
  void CaptureTabInternal(int tab_id,
                          const DirectoryKey& key,
                          int frame_tree_node_id,
                          content::GlobalFrameRoutingId frame_routing_id,
                          FinishedCallback callback,
                          const base::Optional<base::FilePath>& file_path);

  void OnCaptured(int tab_id,
                  const DirectoryKey& key,
                  int frame_tree_node_id,
                  FinishedCallback callback,
                  PaintPreviewBaseService::CaptureStatus status,
                  std::unique_ptr<CaptureResult> result);

  void OnFinished(int tab_id, FinishedCallback callback, bool success);

  void CleanupOldestFiles(int tab_id, const std::vector<DirectoryKey>& keys);

  void RunAudit(const std::vector<int>& active_tab_ids,
                const base::flat_set<DirectoryKey>& in_use_keys);

  bool cache_ready_;
  base::flat_set<int> captured_tab_ids_;
#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
#endif  // defined(OS_ANDROID)
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PaintPreviewTabService> weak_ptr_factory_{this};
};

}  // namespace paint_preview

#endif  // CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_H_
