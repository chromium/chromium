// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/long_screenshots/long_screenshots_tab_service.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_monitor.h"
#include "components/paint_preview/browser/file_manager.h"
#include "content/public/browser/global_routing_id.h"

#include "chrome/browser/share/android/jni_headers/LongScreenshotsTabService_jni.h"

namespace long_screenshots {

using paint_preview::DirectoryKey;
using paint_preview::FileManager;

namespace {
// TODO(tgupta): Evaluate whether this is the right size.
constexpr size_t kMaxPerCaptureSizeBytes = 5 * 1000L * 1000L;  // 5 MB.

}  // namespace

LongScreenshotsTabService::LongScreenshotsTabService(
    std::unique_ptr<paint_preview::PaintPreviewFileMixin> file_mixin,
    std::unique_ptr<paint_preview::PaintPreviewPolicy> policy,
    bool is_off_the_record)
    : PaintPreviewBaseService(std::move(file_mixin),
                              std::move(policy),
                              is_off_the_record) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // TODO(tgupta): If using PlayerCompositorDelegate for compositing to bitmaps
  // reinterpret the service pointer as PaintPreviewBaseService.
  java_ref_.Reset(Java_LongScreenshotsTabService_Constructor(
      env,
      reinterpret_cast<intptr_t>(static_cast<PaintPreviewBaseService*>(this))));
}

LongScreenshotsTabService::~LongScreenshotsTabService() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_LongScreenshotsTabService_onNativeDestroyed(env, java_ref_);
  java_ref_.Reset();
}

void LongScreenshotsTabService::CaptureTab(int tab_id,
                                           content::WebContents* contents,
                                           int clipX,
                                           int clipY,
                                           int clipWidth,
                                           int clipHeight) {
  // If the system is under memory pressure don't try to capture.
  auto* memory_monitor = base::MemoryPressureMonitor::Get();
  if (memory_monitor &&
      memory_monitor->GetCurrentPressureLevel() >=
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_LongScreenshotsTabService_processCaptureTabStatus(
        env, java_ref_, Status::kLowMemoryDetected);
    return;
  }

  // Mark |contents| as being captured so that the renderer doesn't go away
  // until the capture is finished. This is done even before a file is created
  // to ensure the renderer doesn't go away while that happens.
  capture_handle_ =
      contents->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/true,
                                       /*stay_awake=*/true);

  auto file_manager = GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(tab_id);
  GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&paint_preview::FileManager::CreateOrGetDirectory,
                     GetFileMixin()->GetFileManager(), key, true),
      // TODO(tgupta): Check for AMP pages here and get the right node id.
      base::BindOnce(&LongScreenshotsTabService::CaptureTabInternal,
                     weak_ptr_factory_.GetWeakPtr(), tab_id, key,
                     contents->GetMainFrame()->GetFrameTreeNodeId(),
                     contents->GetMainFrame()->GetGlobalFrameRoutingId(), clipX,
                     clipY, clipWidth, clipHeight));
}

void LongScreenshotsTabService::CaptureTabInternal(
    int tab_id,
    const paint_preview::DirectoryKey& key,
    int frame_tree_node_id,
    content::GlobalFrameRoutingId frame_routing_id,
    int clipX,
    int clipY,
    int clipWidth,
    int clipHeight,
    const base::Optional<base::FilePath>& file_path) {
  if (!file_path.has_value()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_LongScreenshotsTabService_processCaptureTabStatus(
        env, java_ref_, Status::kDirectoryCreationFailed);
    return;
  }
  auto* contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);

  // There is a small chance RenderFrameHost may be destroyed when the UI thread
  // is used to create the directory.  By doing a lookup for the RenderFrameHost
  // and comparing it to the WebContent, we can ensure that the content is still
  // available for capture and WebContents::GetMainFrame did not return a
  // defunct pointer.
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id);
  if (!contents || !rfh || contents->IsBeingDestroyed() ||
      contents->GetMainFrame() != rfh || !rfh->IsCurrent()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_LongScreenshotsTabService_processCaptureTabStatus(
        env, java_ref_, Status::kWebContentsGone);
    return;
  }

  CaptureParams capture_params;
  capture_params.web_contents = contents;
  capture_params.root_dir = &file_path.value();
  capture_params.persistence = paint_preview::RecordingPersistence::kFileSystem;
  capture_params.clip_rect = gfx::Rect(clipX, clipY, clipWidth, clipHeight);
  capture_params.capture_links = false;
  capture_params.max_per_capture_size = kMaxPerCaptureSizeBytes;
  CapturePaintPreview(capture_params,
                      base::BindOnce(&LongScreenshotsTabService::OnCaptured,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void LongScreenshotsTabService::OnCaptured(
    paint_preview::PaintPreviewBaseService::CaptureStatus status,
    std::unique_ptr<paint_preview::CaptureResult> result) {
  capture_handle_.RunAndReset();

  JNIEnv* env = base::android::AttachCurrentThread();

  if (status != PaintPreviewBaseService::CaptureStatus::kOk ||
      !result->capture_success) {
    Java_LongScreenshotsTabService_processCaptureTabStatus(
        env, java_ref_, Status::kCaptureFailed);
    return;
  }

  std::string serialized;
  (result->proto).SerializeToString(&serialized);
  Java_LongScreenshotsTabService_processPaintPreviewResponse(
      env, java_ref_, base::android::ToJavaByteArray(env, serialized));
}

void LongScreenshotsTabService::DeleteAllLongScreenshotFiles() {
  GetFileMixin()->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FileManager::DeleteAll,
                                GetFileMixin()->GetFileManager()));
}

void LongScreenshotsTabService::CaptureTabAndroid(
    JNIEnv* env,
    jint j_tab_id,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jint clipX,
    jint clipY,
    jint clipWidth,
    jint clipHeight) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);

  CaptureTab(static_cast<int>(j_tab_id), web_contents, static_cast<int>(clipX),
             static_cast<int>(clipY), static_cast<int>(clipWidth),
             static_cast<int>(clipHeight));
}

void LongScreenshotsTabService::LongScreenshotsClosedAndroid(JNIEnv* env) {
  DeleteAllLongScreenshotFiles();
}
}  // namespace long_screenshots
