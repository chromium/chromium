// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/long_screenshots/long_screenshots_tab_service.h"

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_monitor.h"
#include "components/google/core/common/google_util.h"
#include "components/paint_preview/browser/file_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/LongScreenshotsTabService_jni.h"

namespace long_screenshots {

using paint_preview::DirectoryKey;
using paint_preview::FileManager;

namespace {
// TODO(skare): Evaluate what to send, if anything; paint_preview team is
// changing the logic around capture discarding.
constexpr size_t kMaxPerCaptureSizeBytes = 50 * 1000L * 1000L;  // 50 MB.

// Host/regex pattern for Google AMP Cache URLs.
// See https://developers.google.com/amp/cache/overview#amp-cache-url-format
// for a definition of the format of AMP Cache URLs.
const char kGoogleAmpCacheHost[] = "cdn.ampproject.org";
const char kGoogleAmpCachePathPattern[] = "/[a-z]/(s/)?(.*)";

// Regex pattern for the path of Google AMP Viewer URLs.
const char kGoogleAmpViewerPathPattern[] = "/amp/(s/)?(.*)";

const char kGoogleNewsHost[] = "news.google.com";
const char kGoogleNewsPathPattern[] = "/articles/(.*)";

}  // namespace

// Used to free a CaptureResult if it is passed up to Java and cannot be used by
// the compositior for some reason.
void JNI_LongScreenshotsTabService_ReleaseCaptureResultPtr(
    JNIEnv* env,
    jlong j_capture_result_ptr) {
  // `j_capture_result_ptr` is checked to not be nullptr in Java.
  delete reinterpret_cast<paint_preview::CaptureResult*>(j_capture_result_ptr);
}

LongScreenshotsTabService::LongScreenshotsTabService(
    std::unique_ptr<paint_preview::PaintPreviewFileMixin> file_mixin,
    std::unique_ptr<paint_preview::PaintPreviewPolicy> policy,
    bool is_off_the_record)
    : PaintPreviewBaseService(std::move(file_mixin),
                              std::move(policy),
                              is_off_the_record),
      google_amp_cache_path_regex_(kGoogleAmpCachePathPattern),
      google_amp_viewer_path_regex_(kGoogleAmpViewerPathPattern),
      google_news_path_regex_(kGoogleNewsPathPattern) {
  DCHECK(google_amp_cache_path_regex_.ok());
  DCHECK(google_amp_viewer_path_regex_.ok());
  DCHECK(google_news_path_regex_.ok());

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
  capture_handle_.RunAndReset();
}

void LongScreenshotsTabService::CaptureTab(int tab_id,
                                           const GURL& url,
                                           content::WebContents* contents,
                                           int clip_x,
                                           int clip_y,
                                           int clip_width,
                                           int clip_height,
                                           bool in_memory) {
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
  capture_handle_ = contents->IncrementCapturerCount(
      gfx::Size(), /*stay_hidden=*/true,
      /*stay_awake=*/true, /*is_activity=*/true);
  content::RenderFrameHost* rfh =
      GetRootRenderFrameHost(contents->GetPrimaryMainFrame(), url);
  if (in_memory) {
    CaptureTabInternal(tab_id, rfh->GetFrameTreeNodeId(), rfh->GetGlobalId(),
                       clip_x, clip_y, clip_width, clip_height, in_memory,
                       std::nullopt);
    return;
  }

  auto file_manager = GetFileMixin()->GetFileManager();
  auto key = file_manager->CreateKey(tab_id);
  GetFileMixin()->GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&paint_preview::FileManager::CreateOrGetDirectory,
                     GetFileMixin()->GetFileManager(), key, true),
      base::BindOnce(&LongScreenshotsTabService::CaptureTabInternal,
                     weak_ptr_factory_.GetWeakPtr(), tab_id,
                     rfh->GetFrameTreeNodeId(), rfh->GetGlobalId(), clip_x,
                     clip_y, clip_width, clip_height, in_memory));
}

void LongScreenshotsTabService::CaptureTabInternal(
    int tab_id,
    content::FrameTreeNodeId frame_tree_node_id,
    content::GlobalRenderFrameHostId frame_routing_id,
    int clip_x,
    int clip_y,
    int clip_width,
    int clip_height,
    bool in_memory,
    const std::optional<base::FilePath>& file_path) {
  if (!in_memory && !file_path.has_value()) {
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
  // available for capture and WebContents::GetPrimaryMainFrame did not return a
  // defunct pointer.
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id);
  if (!contents || !rfh || contents->IsBeingDestroyed() || !rfh->IsActive()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_LongScreenshotsTabService_processCaptureTabStatus(
        env, java_ref_, Status::kWebContentsGone);
    return;
  }

  CaptureParams capture_params;
  capture_params.web_contents = contents;
  if (!in_memory) {
    capture_params.root_dir = &file_path.value();
  }
  capture_params.persistence =
      in_memory ? paint_preview::RecordingPersistence::kMemoryBuffer
                : paint_preview::RecordingPersistence::kFileSystem;
  capture_params.render_frame_host = rfh;
  capture_params.clip_rect = gfx::Rect(clip_x, clip_y, clip_width, clip_height);
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

  result->proto.mutable_metadata()->clear_chrome_version();
  Java_LongScreenshotsTabService_processPaintPreviewResponse(
      env, java_ref_, reinterpret_cast<jlong>(result.release()));
}

content::RenderFrameHost* LongScreenshotsTabService::GetRootRenderFrameHost(
    content::RenderFrameHost* main_frame,
    const GURL& url) {
  if (!IsAmpUrl(url)) {
    return main_frame;
  }

  std::vector<content::RenderFrameHost*> child_frames;
  main_frame->ForEachRenderFrameHostWithAction(
      [main_frame, &child_frames](content::RenderFrameHost* rfh) {
        // All frames get traversed in breadth-first order.
        // If a direct child is found, skip traversing its children.
        if (rfh->GetParent() == main_frame) {
          child_frames.push_back(rfh);
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });

  // In AMP pages the main frame should have exactly one child subframe.
  if (child_frames.size() != 1) {
    return main_frame;
  }
  return child_frames[0];
}

bool LongScreenshotsTabService::IsAmpUrl(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }

  // Check for "*.cdn.ampproject.org" URLs.
  if (url.DomainIs(kGoogleAmpCacheHost) &&
      re2::RE2::FullMatch(url.path(), google_amp_cache_path_regex_)) {
    return true;
  }

  // Check for "www.google.TLD/amp/" URLs.
  if (google_util::IsGoogleDomainUrl(
          url, google_util::DISALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS) &&
      re2::RE2::FullMatch(url.path(), google_amp_viewer_path_regex_)) {
    return true;
  }

  // Check for "news.google.com/articles/*".
  if (url.DomainIs(kGoogleNewsHost) &&
      re2::RE2::FullMatch(url.path(), google_news_path_regex_)) {
    return true;
  }

  return false;
}

void LongScreenshotsTabService::DeleteAllLongScreenshotFiles() {
  GetFileMixin()->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FileManager::DeleteAll,
                                GetFileMixin()->GetFileManager()));
}

void LongScreenshotsTabService::CaptureTabAndroid(
    JNIEnv* env,
    jint j_tab_id,
    const base::android::JavaParamRef<jobject>& j_gurl,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jint clip_x,
    jint clip_y,
    jint clip_width,
    jint clip_height,
    jboolean in_memory) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_gurl);

  CaptureTab(static_cast<int>(j_tab_id), url, web_contents,
             static_cast<int>(clip_x), static_cast<int>(clip_y),
             static_cast<int>(clip_width), static_cast<int>(clip_height),
             static_cast<bool>(in_memory));
}

void LongScreenshotsTabService::LongScreenshotsClosedAndroid(JNIEnv* env) {
  DeleteAllLongScreenshotFiles();
}
}  // namespace long_screenshots
