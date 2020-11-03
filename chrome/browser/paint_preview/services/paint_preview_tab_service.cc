// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/paint_preview/services/paint_preview_tab_service.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "components/paint_preview/browser/file_manager.h"
#include "components/paint_preview/browser/warm_compositor.h"
#include "content/public/browser/render_process_host.h"
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

constexpr size_t kMaxPerCaptureSizeBytes = 5 * 1000L * 1000L;    // 5 MB.
constexpr size_t kMaximumTotalCaptureSize = 25 * 1000L * 1000L;  // 25 MB.
// The time horizon after which unused paint previews will be deleted.
constexpr int kExpiryHorizonHrs = 72;

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

PaintPreviewTabService::PaintPreviewTabService(
    const base::FilePath& profile_dir,
    base::StringPiece ascii_feature_name,
    std::unique_ptr<PaintPreviewPolicy> policy,
    bool is_off_the_record)
    : PaintPreviewBaseService(profile_dir,
                              ascii_feature_name,
                              std::move(policy),
                              is_off_the_record),
      cache_ready_(false) {
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&FileManager::ListUsedKeys, GetFileManager()),
      base::BindOnce(&PaintPreviewTabService::InitializeCache,
                     weak_ptr_factory_.GetWeakPtr()));
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::GetTotalDiskUsage, GetFileManager()),
      base::BindOnce([](size_t size_bytes) {
        base::UmaHistogramMemoryKB(
            "Browser.PaintPreview.TabService.DiskUsageAtStartup",
            size_bytes / 1000);
      }));
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(Java_PaintPreviewTabService_Constructor(
      env, reinterpret_cast<intptr_t>(this)));
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
                                        FinishedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Mark |contents| as being captured so that the renderer doesn't go away
  // until the capture is finished. This is done even before a file is created
  // to ensure the renderer doesn't go away while that happens.
  contents->IncrementCapturerCount(gfx::Size(), true);

  auto file_manager = GetFileManager();
  auto key = file_manager->CreateKey(tab_id);
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::CreateOrGetDirectory, GetFileManager(), key,
                     true),
      base::BindOnce(&PaintPreviewTabService::CaptureTabInternal,
                     weak_ptr_factory_.GetWeakPtr(), tab_id, key,
                     contents->GetMainFrame()->GetFrameTreeNodeId(),
                     content::GlobalFrameRoutingId(
                       contents->GetMainFrame()->GetProcess()->GetID(),
                       contents->GetMainFrame()->GetRoutingID()),
                     std::move(callback)));
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

  auto file_manager = GetFileManager();
  captured_tab_ids_.erase(tab_id);
  GetTaskRunner()->PostTask(
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

  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&FileManager::ListUsedKeys, GetFileManager()),
      base::BindOnce(&PaintPreviewTabService::RunAudit,
                     weak_ptr_factory_.GetWeakPtr(), active_tab_ids));
}

void PaintPreviewTabService::GetCapturedPaintPreviewProto(
    const DirectoryKey& key,
    base::Optional<base::TimeDelta> expiry_horizon,
    PaintPreviewBaseService::OnReadProtoCallback on_read_proto_callback) {
  PaintPreviewBaseService::GetCapturedPaintPreviewProto(
      key,
      expiry_horizon.has_value()
          ? expiry_horizon.value()
          : base::TimeDelta::FromHours(kExpiryHorizonHrs),
      std::move(on_read_proto_callback));
}

#if defined(OS_ANDROID)
void PaintPreviewTabService::CaptureTabAndroid(
    JNIEnv* env,
    jint j_tab_id,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jobject>& j_callback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  CaptureTab(static_cast<int>(j_tab_id), web_contents,
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
      env, GetFileManager()->GetPath().AsUTF8Unsafe());
}
#endif  // defined(OS_ANDROID)

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
    int tab_id,
    const DirectoryKey& key,
    int frame_tree_node_id,
    content::GlobalFrameRoutingId frame_routing_id,
    FinishedCallback callback,
    const base::Optional<base::FilePath>& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!file_path.has_value()) {
    std::move(callback).Run(Status::kDirectoryCreationFailed);
    return;
  }
  auto* contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id);
  if (!contents || !rfh || contents->IsBeingDestroyed() ||
      contents->GetMainFrame() != rfh || !rfh->IsCurrent()) {
    std::move(callback).Run(Status::kWebContentsGone);
    return;
  }
  CapturePaintPreview(
      contents, file_path.value(), gfx::Rect(), true, kMaxPerCaptureSizeBytes,
      base::BindOnce(&PaintPreviewTabService::OnCaptured,
                     weak_ptr_factory_.GetWeakPtr(), tab_id, key,
                     frame_tree_node_id, std::move(callback)));
}

void PaintPreviewTabService::OnCaptured(
    int tab_id,
    const DirectoryKey& key,
    int frame_tree_node_id,
    FinishedCallback callback,
    PaintPreviewBaseService::CaptureStatus status,
    std::unique_ptr<CaptureResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (web_contents)
    web_contents->DecrementCapturerCount(true);

  if (status != PaintPreviewBaseService::CaptureStatus::kOk ||
      !result->capture_success) {
    std::move(callback).Run(Status::kCaptureFailed);
    return;
  }
  auto file_manager = GetFileManager();
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::SerializePaintPreviewProto, GetFileManager(),
                     key, result->proto, true),
      base::BindOnce(&PaintPreviewTabService::OnFinished,
                     weak_ptr_factory_.GetWeakPtr(), tab_id,
                     std::move(callback)));
}

void PaintPreviewTabService::OnFinished(int tab_id,
                                        FinishedCallback callback,
                                        bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (success)
    captured_tab_ids_.insert(tab_id);
  std::move(callback).Run(success ? Status::kOk
                                  : Status::kProtoSerializationFailed);
  auto file_manager = GetFileManager();
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FileManager::GetOldestArtifactsForCleanup, file_manager,
                     kMaximumTotalCaptureSize,
                     base::TimeDelta::FromHours(kExpiryHorizonHrs)),
      base::BindOnce(&PaintPreviewTabService::CleanupOldestFiles,
                     weak_ptr_factory_.GetWeakPtr(), tab_id));
}

void PaintPreviewTabService::CleanupOldestFiles(
    int tab_id,
    const std::vector<DirectoryKey>& keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<DirectoryKey> keys_to_delete;
  keys_to_delete.reserve(keys.size());
  for (const auto& key : keys) {
    auto id = TabIdFromDirectoryKey(key);
    if (id == tab_id)
      continue;

    captured_tab_ids_.erase(id);
    keys_to_delete.push_back(key);
  }

  GetTaskRunner()->PostTask(FROM_HERE,
                            base::BindOnce(&FileManager::DeleteArtifactSets,
                                           GetFileManager(), keys_to_delete));
}

void PaintPreviewTabService::RunAudit(
    const std::vector<int>& active_tab_ids,
    const base::flat_set<DirectoryKey>& in_use_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto file_manager = GetFileManager();
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
  // |keys_to_delete| should normally be 0 or small and this is only run once at
  // startup.
  for (const auto& key : keys_to_delete)
    captured_tab_ids_.erase(TabIdFromDirectoryKey(key));

  GetTaskRunner()->PostTask(FROM_HERE,
                            base::BindOnce(&FileManager::DeleteArtifactSets,
                                           GetFileManager(), keys_to_delete));
}

}  // namespace paint_preview
