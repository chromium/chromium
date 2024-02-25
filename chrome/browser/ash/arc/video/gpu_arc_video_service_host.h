// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_VIDEO_GPU_ARC_VIDEO_SERVICE_HOST_H_
#define CHROME_BROWSER_ASH_ARC_VIDEO_GPU_ARC_VIDEO_SERVICE_HOST_H_

#include "ash/components/arc/mojom/video.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class takes requests for accessing the VideoAcceleratorFactory, from
// which video decode (or encode) accelerators could be created.
//
// This class runs in the browser process, while the created instances of
// VideoDecodeAccelerator or VideoEncodeAccelerator run in the GPU process.
//
// Lives on the UI thread.
class GpuArcVideoServiceHost : public mojom::VideoHost {
 public:
  static GpuArcVideoServiceHost* Get();

  // arc::mojom::VideoHost implementation.
  void OnBootstrapVideoAcceleratorFactory(
      OnBootstrapVideoAcceleratorFactoryCallback callback) override;

  GpuArcVideoServiceHost(const GpuArcVideoServiceHost&) = delete;
  GpuArcVideoServiceHost& operator=(const GpuArcVideoServiceHost&) = delete;

 private:
  friend class base::NoDestructor<GpuArcVideoServiceHost>;

  GpuArcVideoServiceHost();
  ~GpuArcVideoServiceHost() override;

  std::unique_ptr<mojom::VideoAcceleratorFactory> video_accelerator_factory_;
  mojo::ReceiverSet<mojom::VideoAcceleratorFactory>
      video_accelerator_factory_receivers_;
};

class GpuArcVideoKeyedService : public KeyedService {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static GpuArcVideoKeyedService* GetForBrowserContext(
      content::BrowserContext* context);

  static void EnsureFactoryBuilt();

  GpuArcVideoKeyedService(content::BrowserContext* context,
                          ArcBridgeService* bridge_service);
  GpuArcVideoKeyedService(const GpuArcVideoKeyedService&) = delete;
  GpuArcVideoKeyedService& operator=(const GpuArcVideoKeyedService&) = delete;
  ~GpuArcVideoKeyedService() override;

 private:
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_VIDEO_GPU_ARC_VIDEO_SERVICE_HOST_H_
