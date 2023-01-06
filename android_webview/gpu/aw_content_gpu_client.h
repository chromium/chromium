// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_GPU_AW_CONTENT_GPU_CLIENT_H_
#define ANDROID_WEBVIEW_GPU_AW_CONTENT_GPU_CLIENT_H_

#include "base/functional/callback.h"
#include "content/public/gpu/content_gpu_client.h"

namespace android_webview {

class AwContentGpuClient : public content::ContentGpuClient {
 public:
  using GetSyncPointManagerCallback =
      base::RepeatingCallback<gpu::SyncPointManager*()>;
  using GetSharedImageManagerCallback =
      base::RepeatingCallback<gpu::SharedImageManager*()>;
  using GetSchedulerCallback = base::RepeatingCallback<gpu::Scheduler*()>;
  using GetVizCompositorThreadRunnerCallback =
      base::RepeatingCallback<viz::VizCompositorThreadRunner*()>;

  AwContentGpuClient(
      GetSyncPointManagerCallback sync_point_manager_callback,
      GetSharedImageManagerCallback shared_image_manager_callback,
      GetSchedulerCallback scheduler_callback,
      GetVizCompositorThreadRunnerCallback
          viz_compositor_thread_runner_callback);

  AwContentGpuClient(const AwContentGpuClient&) = delete;
  AwContentGpuClient& operator=(const AwContentGpuClient&) = delete;

  ~AwContentGpuClient() override;

  // content::ContentGpuClient implementation.
  gpu::SyncPointManager* GetSyncPointManager() override;
  gpu::SharedImageManager* GetSharedImageManager() override;
  gpu::Scheduler* GetScheduler() override;
  viz::VizCompositorThreadRunner* GetVizCompositorThreadRunner() override;

 private:
  GetSyncPointManagerCallback sync_point_manager_callback_;
  GetSharedImageManagerCallback shared_image_manager_callback_;
  GetSchedulerCallback scheduler_callback_;
  GetVizCompositorThreadRunnerCallback viz_compositor_thread_runner_callback_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_GPU_AW_CONTENT_GPU_CLIENT_H_
