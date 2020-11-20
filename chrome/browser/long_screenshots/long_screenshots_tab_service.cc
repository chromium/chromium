// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/long_screenshots/long_screenshots_tab_service.h"

#include "base/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_monitor.h"
#include "components/paint_preview/browser/file_manager.h"

namespace long_screenshots {

using paint_preview::DirectoryKey;
using paint_preview::FileManager;

namespace {
// TODO(tgupta): Evaluate whether this is the right size.
constexpr size_t kMaxPerCaptureSizeBytes = 5 * 1000L * 1000L;  // 5 MB.
}  // namespace

LongScreenshotsTabService::LongScreenshotsTabService(
    const base::FilePath& profile_dir,
    base::StringPiece ascii_feature_name,
    std::unique_ptr<paint_preview::PaintPreviewPolicy> policy,
    bool is_off_the_record)
    : PaintPreviewBaseService(profile_dir,
                              ascii_feature_name,
                              std::move(policy),
                              is_off_the_record) {
  // TODO(tgupta): Populate this.
}

LongScreenshotsTabService::~LongScreenshotsTabService() {
  // TODO(tgupta): Populate this.
}

void LongScreenshotsTabService::CaptureTab(int tab_id,
                                           content::WebContents* contents,
                                           FinishedCallback callback) {
  // If the system is under memory pressure don't try to capture.
  auto* memory_monitor = base::MemoryPressureMonitor::Get();
  if (memory_monitor &&
      memory_monitor->GetCurrentPressureLevel() >=
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE) {
    // TODO(tgupta): Consider returning a callback with an error message.
    return;
  }

  // Mark |contents| as being captured so that the renderer doesn't go away
  // until the capture is finished. This is done even before a file is created
  // to ensure the renderer doesn't go away while that happens.
  contents->IncrementCapturerCount(gfx::Size(), true);

  auto file_manager = GetFileManager();
  auto key = file_manager->CreateKey(tab_id);
  GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&paint_preview::FileManager::CreateOrGetDirectory,
                     GetFileManager(), key, true),
      // TODO(tgupta): Check for AMP pages here and get the right node id.
      base::BindOnce(&LongScreenshotsTabService::CaptureTabInternal,
                     weak_ptr_factory_.GetWeakPtr(), tab_id, key,
                     contents->GetMainFrame()->GetFrameTreeNodeId(),
                     contents->GetMainFrame()->GetGlobalFrameRoutingId(),
                     std::move(callback)));
}

void LongScreenshotsTabService::CaptureTabInternal(
    int tab_id,
    const paint_preview::DirectoryKey& key,
    int frame_tree_node_id,
    content::GlobalFrameRoutingId frame_routing_id,
    FinishedCallback callback,
    const base::Optional<base::FilePath>& file_path) {
  if (!file_path.has_value()) {
    std::move(callback).Run(Status::kDirectoryCreationFailed);
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
    std::move(callback).Run(Status::kWebContentsGone);
    return;
  }
  // TODO(tgupta): Modify this call to specify the size of the capture rather
  // than the whole area.
  CapturePaintPreview(
      contents, file_path.value(), gfx::Rect(), true, kMaxPerCaptureSizeBytes,
      base::BindOnce(&LongScreenshotsTabService::OnCaptured,
                     weak_ptr_factory_.GetWeakPtr(), tab_id, key,
                     frame_tree_node_id, std::move(callback)));
}

void LongScreenshotsTabService::OnCaptured(
    int tab_id,
    const paint_preview::DirectoryKey& key,
    int frame_tree_node_id,
    FinishedCallback callback,
    paint_preview::PaintPreviewBaseService::CaptureStatus status,
    std::unique_ptr<paint_preview::CaptureResult> result) {
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
      base::BindOnce(&LongScreenshotsTabService::OnFinished,
                     weak_ptr_factory_.GetWeakPtr(), tab_id,
                     std::move(callback)));
}

void LongScreenshotsTabService::OnFinished(int tab_id,
                                           FinishedCallback callback,
                                           bool success) {
  std::move(callback).Run(success ? Status::kOk
                                  : Status::kProtoSerializationFailed);
}

void LongScreenshotsTabService::DeleteAllLongScreenshotFiles() {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&FileManager::DeleteAll, GetFileManager()));
}

}  // namespace long_screenshots
