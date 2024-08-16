// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/gpu_service_webview.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_utils.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_util.h"

namespace android_webview {

// static
GpuServiceWebView* GpuServiceWebView::GetInstance() {
  static GpuServiceWebView* gpu_service = CreateGpuServiceWebView();
  return gpu_service;
}

// static
GpuServiceWebView* GpuServiceWebView::CreateGpuServiceWebView() {
  gpu::GPUInfo gpu_info;
  gpu::GpuFeatureInfo gpu_feature_info;
  DCHECK(base::CommandLine::InitializedForCurrentProcess());
  gpu::GpuPreferences gpu_preferences =
      content::GetGpuPreferencesFromCommandLine();
  auto* command_line = base::CommandLine::ForCurrentProcess();
  gl::GLDisplay* display = gpu::InitializeGLThreadSafe(
      command_line, gpu_preferences, &gpu_info, &gpu_feature_info);
  if (!display) {
    LOG(FATAL) << "gpu::InitializeGLThreadSafe() failed.";
  }
  auto sync_point_manager = std::make_unique<gpu::SyncPointManager>();
  // The shared_image_manager will be shared between renderer thread and GPU
  // main thread, so it should be thread safe.
  auto shared_image_manager = std::make_unique<gpu::SharedImageManager>(
      true /* is_thread_safe */, true /* display_context_on_another_thread */);
  auto scheduler = std::make_unique<gpu::Scheduler>(sync_point_manager.get());
  return new GpuServiceWebView(
      std::move(sync_point_manager), std::move(shared_image_manager),
      std::move(scheduler), gpu_info, gpu_preferences, gpu_feature_info);
}

GpuServiceWebView::~GpuServiceWebView() = default;

GpuServiceWebView::GpuServiceWebView(
    std::unique_ptr<gpu::SyncPointManager> sync_point_manager,
    std::unique_ptr<gpu::SharedImageManager> shared_image_manager,
    std::unique_ptr<gpu::Scheduler> scheduler,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuFeatureInfo& gpu_feature_info)
    : sync_point_manager_(std::move(sync_point_manager)),
      shared_image_manager_(std::move(shared_image_manager)),
      scheduler_(std::move(scheduler)),
      gpu_info_(gpu_info),
      gpu_preferences_(gpu_preferences),
      gpu_feature_info_(gpu_feature_info) {}

}  // namespace android_webview
