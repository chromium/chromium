// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/paint_preview/services/paint_preview_tab_service.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/paint_preview/services/paint_preview_tab_service_file_mixin.h"
#include "components/paint_preview/browser/file_manager.h"
#include "components/paint_preview/browser/warm_compositor.h"
#include "content/public/browser/render_process_host.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/paint_preview/android/jni_headers/PaintPreviewTabService_jni.h"
#endif  // defined(OS_ANDROID)

namespace paint_preview {

namespace {

constexpr size_t kMaxPerCaptureSizeBytes = 5 * 1000L * 1000L;  // 5 MB.

#if defined(OS_ANDROID)
void JavaBooleanCallbackAdapter(base::OnceCallback<void(bool)> callback,
                                PaintPreviewTabService::Status status) {
  DVLOG(1) << "Capture finished with status: " << status;
  std::move(callback).Run(status == PaintPreviewTabService::Status::kOk);
}
#endif  // defined(OS_ANDROID)

// Safe since Tab ID are just converted to strings to be directory keys.
int TabIdFromDirectoryKey(const DirectoryKey& key) {
  int out;
  bool success = base::StringToInt(key.AsciiDirname(), &out);
  DCHECK(success);
  return out;
}

}  // namespace

PaintPreviewTabService::TabServiceTask::TabServiceTask(
    int tab_id,
    const DirectoryKey& key,
    int frame_tree_node_id,
    content::GlobalFrameRoutingId frame_routing_id,
    base::ScopedClosureRunner capture_handle)
    : tab_id_(tab_id),
      key_(key),
      frame_tree_node_id_(frame_tree_node_id),
      frame_routing_id_(frame_routing_id),
      capture_handle_(std::move(capture_handle)) {}

PaintPreviewTabService::TabServiceTask::~TabServiceTask() = default;

PaintPreviewTabService::PaintPreviewTabService(
    std::unique_ptr<PaintPreviewFileMixin> file_mixin,
    std::unique_ptr<PaintPreviewPolicy> policy,
    bool is_off_the_record)
    : PaintPreviewBaseService(std::move(file_mixin),
                              std::move(policy),
                              is_off_the_record),
      cache_ready_(false) {
  GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::ListUsedKeys,
                     GetFileMixin()->GetFileManager()),
      base::BindOnce(&PaintPreviewTabService::InitializeCache,
                     weak_ptr_factory_.GetWeakPtr()));
  GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::GetTotalDiskUsage,
                     GetFileMixin()->GetFileManager()),
      base::BindOnce([](size_t size_bytes) {
        base::UmaHistogramMemoryKB(
            "Browser.PaintPreview.TabService.DiskUsageAtStartup",
            size_bytes / 1000);
      }));
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(Java_PaintPreviewTabService_Constructor(
      env, reinterpret_cast<intptr_t>(this),
      reinterpret_cast<intptr_t>(static_cast<PaintPreviewBaseService*>(this))));
#endif  // defined(OS_ANDROID)
}

PaintPreviewTabService::~PaintPreviewTabService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PaintPreviewTabService_onNativeDestroyed(env, java_ref_);
  java_ref_.Reset();
#endif  // defined(OS_ANDROID)
}

void PaintPreviewTabService::CaptureTab(int tab_id,
                                        content::WebContents* contents,
                                        bool accessibility_enabled,
                                        FinishedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the system is under memory pressure don't try to capture.
  auto* memory_monitor = base::MemoryPressureMonitor::Get();
  if (memory_monitor &&
      memory_monitor->GetCurrentPressureLevel() >=
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE)
    return;

  // Mark |contents| as being captured so that the renderer doesn't go away
  // until the capture is finished. This is done even before a file is created
  // to ensure the renderer doesn't go away while that happens.
  auto capture_handle =
      contents->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/true,
                                       /*stay_awake=*/true);

  auto file_manager = GetFileMixin()->GetFileManager();

  auto key = file_manager->CreateKey(tab_id);
  auto it = tasks_.emplace(
      tab_id, std::make_unique<TabServiceTask>(
                  tab_id, key, contents->GetMainFrame()->GetFrameTreeNodeId(),
                  contents->GetMainFrame()->GetGlobalFrameRoutingId(),
                  std::move(capture_handle)));
  if (!it.second) {
    std::move(callback).Run(Status::kCaptureInProgress);
    return;
  }
  it.first->second->SetCallback(std::move(callback).Then(
      base::BindOnce(&PaintPreviewTabService::DeleteTask,
                     weak_ptr_factory_.GetWeakPtr(), tab_id)));

  GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory,
                     GetFileMixin()->GetFileManager(), key, true),
      base::BindOnce(&PaintPreviewTabService::CaptureTabInternal,
                     weak_ptr_factory_.GetWeakPtr(),
                     it.first->second->GetWeakPtr(), accessibility_enabled));
}

void PaintPreviewTabService::TabClosed(int tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Defer deletions until the cache is ready.
  if (!CacheInitialized()) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PaintPreviewTabService::TabClosed,
                       weak_ptr_factory_.GetWeakPtr(), tab_id),
        base::TimeDelta::FromSeconds(5));
    return;
  }

  auto file_manager = GetFileMixin()->GetFileManager();
  captured_tab_ids_.erase(tab_id);
  GetFileMixin()->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FileManager::DeleteArtifactSet, file_manager,
                                file_manager->CreateKey(tab_id)));
}

bool PaintPreviewTabService::HasCaptureForTab(int tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CacheInitialized() &&
         (captured_tab_ids_.find(tab_id) != captured_tab_ids_.end());
}

void PaintPreviewTabService::AuditArtifacts(
    const std::vector<int>& active_tab_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Defer deletions until the cache is ready.
  if (!CacheInitialized()) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PaintPreviewTabService::AuditArtifacts,
                       weak_ptr_factory_.GetWeakPtr(), active_tab_ids),
        base::TimeDelta::FromSeconds(5));
    return;
  }

  GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::ListUsedKeys,
                     GetFileMixin()->GetFileManager()),
      base::BindOnce(&PaintPreviewTabService::RunAudit,
                     weak_ptr_factory_.GetWeakPtr(), active_tab_ids));
}

#if defined(OS_ANDROID)
void PaintPreviewTabService::CaptureTabAndroid(
    JNIEnv* env,
    jint j_tab_id,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jboolean accessibility_enabled,
    const base::android::JavaParamRef<jobject>& j_callback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  CaptureTab(static_cast<int>(j_tab_id), web_contents, accessibility_enabled,
             base::BindOnce(
                 &JavaBooleanCallbackAdapter,
                 base::BindOnce(
                     &base::android::RunBooleanCallbackAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback))));
}

void PaintPreviewTabService::TabClosedAndroid(JNIEnv* env, jint j_tab_id) {
  TabClosed(static_cast<int>(j_tab_id));
}

jboolean PaintPreviewTabService::HasCaptureForTabAndroid(JNIEnv* env,
                                                         jint j_tab_id) {
  return static_cast<jboolean>(HasCaptureForTab(static_cast<int>(j_tab_id)));
}

void PaintPreviewTabService::AuditArtifactsAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jintArray>& j_tab_ids) {
  std::vector<int> tab_ids;
  base::android::JavaIntArrayToIntVector(env, j_tab_ids, &tab_ids);
  AuditArtifacts(tab_ids);
}

jboolean PaintPreviewTabService::IsCacheInitializedAndroid(JNIEnv* env) {
  return static_cast<jboolean>(CacheInitialized());
}

base::android::ScopedJavaLocalRef<jstring>
PaintPreviewTabService::GetPathAndroid(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, GetFileMixin()->GetFileManager()->GetPath().AsUTF8Unsafe());
}
#endif  // defined(OS_ANDROID)

void PaintPreviewTabService::DeleteTask(int tab_id) {
  tasks_.erase(tab_id);
}

void PaintPreviewTabService::InitializeCache(
    const base::flat_set<DirectoryKey>& in_use_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<int> tab_ids;
  tab_ids.reserve(in_use_keys.size());
  for (const auto& key : in_use_keys)
    tab_ids.push_back(TabIdFromDirectoryKey(key));

  captured_tab_ids_ = base::flat_set<int>(std::move(tab_ids));
  cache_ready_ = true;
}

void PaintPreviewTabService::CaptureTabInternal(
    base::WeakPtr<TabServiceTask> task,
    bool accessibility_enabled,
    const base::Optional<base::FilePath>& file_path) {
  if (!task) {
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!file_path.has_value()) {
    task->OnCaptured(Status::kDirectoryCreationFailed);
    return;
  }
  auto* contents =
      content::WebContents::FromFrameTreeNodeId(task->frame_tree_node_id());
  auto* rfh = content::RenderFrameHost::FromID(task->frame_routing_id());
  if (!contents || !rfh || contents->IsBeingDestroyed() ||
      contents->GetMainFrame() != rfh || !rfh->IsCurrent() ||
      !rfh->IsRenderFrameCreated() || !rfh->IsRenderFrameLive()) {
    task->OnCaptured(Status::kWebContentsGone);
    return;
  }
  if (accessibility_enabled) {
    task->SetWaitForAccessibility();
    contents->RequestAXTreeSnapshot(
        base::BindOnce(&PaintPreviewFileMixin::WriteAXTreeUpdate,
                       GetFileMixin()->GetWeakPtr(), task->key(),
                       base::BindOnce(&PaintPreviewTabService::OnAXTreeWritten,
                                      weak_ptr_factory_.GetWeakPtr(), task)),
        ui::kAXModeWebContentsOnly,
        /* exclude_offscreen= */ false,
        /* max_nodes= */ 5000,
        /* timeout= */ {});
  }

  CaptureParams capture_params;
  capture_params.web_contents = contents;
  capture_params.render_frame_host = rfh;
  capture_params.root_dir = &file_path.value();
  capture_params.persistence = RecordingPersistence::kFileSystem;
  capture_params.clip_rect = gfx::Rect();
  capture_params.capture_links = true;
  capture_params.max_per_capture_size = kMaxPerCaptureSizeBytes;
  CapturePaintPreview(capture_params,
                      base::BindOnce(&PaintPreviewTabService::OnCaptured,
                                     weak_ptr_factory_.GetWeakPtr(), task));
}

void PaintPreviewTabService::OnAXTreeWritten(base::WeakPtr<TabServiceTask> task,
                                             bool result) {
  if (task) {
    task->OnAXTreeWritten(result);
  }
}

void PaintPreviewTabService::OnCaptured(
    base::WeakPtr<TabServiceTask> task,
    PaintPreviewBaseService::CaptureStatus status,
    std::unique_ptr<CaptureResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!task) {
    return;
  }

  task->ReleaseCaptureHandle();
  if (status != PaintPreviewBaseService::CaptureStatus::kOk ||
      !result->capture_success) {
    task->OnCaptured(Status::kCaptureFailed);
    return;
  }
  auto file_manager = GetFileMixin()->GetFileManager();
  GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::SerializePaintPreviewProto,
                     GetFileMixin()->GetFileManager(), task->key(),
                     result->proto, true),
      base::BindOnce(&PaintPreviewTabService::OnFinished,
                     weak_ptr_factory_.GetWeakPtr(), task));
}

void PaintPreviewTabService::OnFinished(base::WeakPtr<TabServiceTask> task,
                                        bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!task) {
    return;
  }
  int tab_id = task->tab_id();

  if (success) {
    captured_tab_ids_.insert(tab_id);
  }
  // WARNING: `task` may be invalidated by this call.
  task->OnCaptured(success ? Status::kOk : Status::kProtoSerializationFailed);

  // Remove all captures excluding the one just completed.
  AuditArtifacts({tab_id});
}

void PaintPreviewTabService::RunAudit(
    const std::vector<int>& active_tab_ids,
    const base::flat_set<DirectoryKey>& in_use_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto file_manager = GetFileMixin()->GetFileManager();
  std::vector<DirectoryKey> keys;
  keys.reserve(active_tab_ids.size());
  for (const auto& tab_id : active_tab_ids)
    keys.push_back(file_manager->CreateKey(tab_id));
  base::flat_set<DirectoryKey> active_tab_keys(std::move(keys));

  std::vector<DirectoryKey> keys_to_delete(active_tab_keys.size() +
                                           in_use_keys.size());
  auto it = std::set_difference(in_use_keys.begin(), in_use_keys.end(),
                                active_tab_keys.begin(), active_tab_keys.end(),
                                keys_to_delete.begin());
  keys_to_delete.resize(it - keys_to_delete.begin());

  // The performance of this is poor (O(n) per removal). However,
  // |keys_to_delete| should normally be small.
  for (const auto& key : keys_to_delete)
    captured_tab_ids_.erase(TabIdFromDirectoryKey(key));

  GetFileMixin()->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FileManager::DeleteArtifactSets,
                     GetFileMixin()->GetFileManager(), keys_to_delete));
}

}  // namespace paint_preview
