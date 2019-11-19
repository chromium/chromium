// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_GPU_AW_CONTENT_GPU_CLIENT_H_
#define ANDROID_WEBVIEW_GPU_AW_CONTENT_GPU_CLIENT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/gpu/content_gpu_client.h"

namespace android_webview {

class AwContentGpuClient : public content::ContentGpuClient {
 public:
  using GetSyncPointManagerCallback =
      base::RepeatingCallback<gpu::SyncPointManager*()>;
  using GetSharedImageManagerCallback =
      base::RepeatingCallback<gpu::SharedImageManager*()>;
  using GetVizCompositorThreadRunnerCallback =
      base::RepeatingCallback<viz::VizCompositorThreadRunner*()>;

  AwContentGpuClient(
      const GetSyncPointManagerCallback& sync_point_manager_callback,
      const GetSharedImageManagerCallback& shared_image_manager_callback,
      const GetVizCompositorThreadRunnerCallback&
          viz_compositor_thread_runner_callback);
  ~AwContentGpuClient() override;

  // content::ContentGpuClient implementation.
  gpu::SyncPointManager* GetSyncPointManager() override;
  gpu::SharedImageManager* GetSharedImageManager() override;
  viz::VizCompositorThreadRunner* GetVizCompositorThreadRunner() override;

 private:
  GetSyncPointManagerCallback sync_point_manager_callback_;
  GetSharedImageManagerCallback shared_image_manager_callback_;
  GetVizCompositorThreadRunnerCallback viz_compositor_thread_runner_callback_;
  DISALLOW_COPY_AND_ASSIGN(AwContentGpuClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_GPU_AW_CONTENT_GPU_CLIENT_H_
