// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_H_
#define CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/paint_preview_policy.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "content/public/browser/global_routing_id.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
class WebContents;
}  // namespace content

namespace paint_preview {

// A service for capturing and using Paint Previews per Tab. Captures are stored
// using Tab IDs as the key such that the data can be accessed even if the
// browser is restarted.
class PaintPreviewTabService : public PaintPreviewBaseService {
 public:
  PaintPreviewTabService(std::unique_ptr<PaintPreviewFileMixin> file_mixin,
                         std::unique_ptr<PaintPreviewPolicy> policy,
                         bool is_off_the_record);
  ~PaintPreviewTabService() override;

  enum Status {
    kOk = 0,
    kDirectoryCreationFailed = 1,
    kCaptureFailed = 2,
    kProtoSerializationFailed = 3,
    kWebContentsGone = 4,
    kCaptureInProgress = 5,
    kInvalid = 6,
  };

  using FinishedCallback = base::OnceCallback<void(Status)>;
  using BooleanCallback = base::OnceCallback<void(bool)>;

  bool CacheInitialized() const { return cache_ready_; }

  // Captures a Paint Preview of |contents| which should be associated with
  // |tab_id| for storage. |callback| is invoked on completion to indicate
  // status.
  void CaptureTab(int tab_id,
                  content::WebContents* contents,
                  bool accessibility_enabled,
                  float page_scale_factor,
                  int scroll_offset_x,
                  int scroll_offset_y,
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

#if BUILDFLAG(IS_ANDROID)
  // JNI wrapped versions of the above methods
  void CaptureTabAndroid(
      JNIEnv* env,
      jint j_tab_id,
      const base::android::JavaParamRef<jobject>& j_web_contents,
      jboolean j_accessibility_enabled,
      jfloat j_page_scale_factor,
      jint j_x,
      jint j_y,
      const base::android::JavaParamRef<jobject>& j_callback);
  void TabClosedAndroid(JNIEnv* env, jint j_tab_id);
  jboolean HasCaptureForTabAndroid(JNIEnv* env, jint j_tab_id);
  void AuditArtifactsAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jintArray>& j_tab_ids);
  jboolean IsCacheInitializedAndroid(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jstring> GetPathAndroid(JNIEnv* env);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaRef() { return java_ref_; }
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  class TabServiceTask {
   public:
    using FinishedCallback = base::OnceCallback<void(Status)>;

    TabServiceTask(int tab_id,
                   const DirectoryKey& key,
                   content::FrameTreeNodeId frame_tree_node_id,
                   content::GlobalRenderFrameHostId frame_routing_id,
                   float page_scale_factor,
                   int x,
                   int y,
                   base::ScopedClosureRunner capture_handle);
    ~TabServiceTask();

    TabServiceTask(const TabServiceTask& other) = delete;
    TabServiceTask& operator=(const TabServiceTask& other) = delete;

    int tab_id() const { return tab_id_; }
    const DirectoryKey& key() const { return key_; }
    content::FrameTreeNodeId frame_tree_node_id() const {
      return frame_tree_node_id_;
    }
    content::GlobalRenderFrameHostId frame_routing_id() const {
      return frame_routing_id_;
    }
    float page_scale_factor() const { return page_scale_factor_; }
    int scroll_offset_x() const { return scroll_offset_x_; }
    int scroll_offset_y() const { return scroll_offset_y_; }

    void SetWaitForAccessibility() { wait_for_accessibility_ = true; }

    void SetCallback(FinishedCallback callback) {
      finished_callback_ = std::move(callback);
    }

    void OnAXTreeWritten(bool success) {
      wait_for_accessibility_ = false;
      if (status_ != kInvalid && finished_callback_) {
        std::move(finished_callback_).Run(status_);
      }
    }

    void OnCaptured(Status status) {
      status_ = status;
      if (!wait_for_accessibility_ && finished_callback_) {
        std::move(finished_callback_).Run(status_);
      }
    }

    base::WeakPtr<TabServiceTask> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

    void ReleaseCaptureHandle() { capture_handle_.RunAndReset(); }

   private:
    int tab_id_;
    DirectoryKey key_;
    content::FrameTreeNodeId frame_tree_node_id_;
    content::GlobalRenderFrameHostId frame_routing_id_;
    float page_scale_factor_;
    int scroll_offset_x_;
    int scroll_offset_y_;

    bool wait_for_accessibility_{false};
    Status status_{kInvalid};

    base::ScopedClosureRunner capture_handle_;

    FinishedCallback finished_callback_;
    base::WeakPtrFactory<TabServiceTask> weak_ptr_factory_{this};
  };

  void DeleteTask(int tab_id);

  // Caches current captures in |captured_tab_ids_|. Called as part of
  // initialization.
  void InitializeCache(const base::flat_set<DirectoryKey>& in_use_keys);

  // The FTN ID is to look-up the content::WebContents.
  void CaptureTabInternal(base::WeakPtr<TabServiceTask> task,
                          bool accessibility_enabled,
                          const std::optional<base::FilePath>& file_path);

  void OnAXTreeWritten(base::WeakPtr<TabServiceTask> task, bool result);

  void OnCaptured(base::WeakPtr<TabServiceTask> task,
                  PaintPreviewBaseService::CaptureStatus status,
                  std::unique_ptr<CaptureResult> result);

  void OnFinished(base::WeakPtr<TabServiceTask> task, bool success);

  void CleanupOldestFiles(int tab_id, const std::vector<DirectoryKey>& keys);

  void RunAudit(const std::vector<int>& active_tab_ids,
                const base::flat_set<DirectoryKey>& in_use_keys);

  bool cache_ready_;
  base::flat_set<int> captured_tab_ids_;
  base::flat_map<int, std::unique_ptr<TabServiceTask>> tasks_;
#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
#endif  // BUILDFLAG(IS_ANDROID)
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PaintPreviewTabService> weak_ptr_factory_{this};
};

}  // namespace paint_preview

#endif  // CHROME_BROWSER_PAINT_PREVIEW_SERVICES_PAINT_PREVIEW_TAB_SERVICE_H_
