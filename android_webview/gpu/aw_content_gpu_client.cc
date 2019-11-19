// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/gpu/aw_content_gpu_client.h"

namespace android_webview {

AwContentGpuClient::AwContentGpuClient(
    const GetSyncPointManagerCallback& sync_point_manager_callback,
    const GetSharedImageManagerCallback& shared_image_manager_callback,
    const GetVizCompositorThreadRunnerCallback&
        viz_compositor_thread_runner_callback)
    : sync_point_manager_callback_(sync_point_manager_callback),
      shared_image_manager_callback_(shared_image_manager_callback),
      viz_compositor_thread_runner_callback_(
          viz_compositor_thread_runner_callback) {}

AwContentGpuClient::~AwContentGpuClient() {}

gpu::SyncPointManager* AwContentGpuClient::GetSyncPointManager() {
  return sync_point_manager_callback_.Run();
}

gpu::SharedImageManager* AwContentGpuClient::GetSharedImageManager() {
  return shared_image_manager_callback_.Run();
}

viz::VizCompositorThreadRunner*
AwContentGpuClient::GetVizCompositorThreadRunner() {
  return viz_compositor_thread_runner_callback_.Run();
}

}  // namespace android_webview
