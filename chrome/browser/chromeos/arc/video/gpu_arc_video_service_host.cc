// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/video/gpu_arc_video_service_host.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/mojom/video_decode_accelerator.mojom.h"
#include "components/arc/mojom/video_encode_accelerator.mojom.h"
#include "components/arc/mojom/video_protected_buffer_allocator.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_service_registry.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"

namespace arc {

namespace {

// Singleton factory for GpuArcVideoServiceHost.
class GpuArcVideoServiceHostFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          GpuArcVideoServiceHost,
          GpuArcVideoServiceHostFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "GpuArcVideoServiceHostFactory";

  static GpuArcVideoServiceHostFactory* GetInstance() {
    return base::Singleton<GpuArcVideoServiceHostFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<GpuArcVideoServiceHostFactory>;
  GpuArcVideoServiceHostFactory() = default;
  ~GpuArcVideoServiceHostFactory() override = default;
};

class VideoAcceleratorFactoryService : public mojom::VideoAcceleratorFactory {
 public:
  VideoAcceleratorFactoryService() = default;

  ~VideoAcceleratorFactoryService() override = default;

  void CreateDecodeAccelerator(
      mojom::VideoDecodeAcceleratorRequest request) override {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            &content::BindInterfaceInGpuProcess<mojom::VideoDecodeAccelerator>,
            std::move(request)));
  }

  void CreateEncodeAccelerator(
      mojom::VideoEncodeAcceleratorRequest request) override {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            &content::BindInterfaceInGpuProcess<mojom::VideoEncodeAccelerator>,
            std::move(request)));
  }

  void CreateProtectedBufferAllocator(
      mojom::VideoProtectedBufferAllocatorRequest request) override {
    base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                   base::BindOnce(&content::BindInterfaceInGpuProcess<
                                      mojom::VideoProtectedBufferAllocator>,
                                  std::move(request)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoAcceleratorFactoryService);
};

}  // namespace

// static
GpuArcVideoServiceHost* GpuArcVideoServiceHost::GetForBrowserContext(
    content::BrowserContext* context) {
  return GpuArcVideoServiceHostFactory::GetForBrowserContext(context);
}

GpuArcVideoServiceHost::GpuArcVideoServiceHost(content::BrowserContext* context,
                                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      video_accelerator_factory_(
          std::make_unique<VideoAcceleratorFactoryService>()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->video()->SetHost(this);
}

GpuArcVideoServiceHost::~GpuArcVideoServiceHost() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->video()->SetHost(nullptr);
}

void GpuArcVideoServiceHost::OnBootstrapVideoAcceleratorFactory(
    OnBootstrapVideoAcceleratorFactoryCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Hardcode pid 0 since it is unused in mojo.
  const base::ProcessHandle kUnusedChildProcessHandle =
      base::kNullProcessHandle;
  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  std::string pipe_name = base::NumberToString(base::RandUint64());
  mojo::ScopedMessagePipeHandle server_pipe =
      invitation.AttachMessagePipe(pipe_name);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 kUnusedChildProcessHandle,
                                 channel.TakeLocalEndpoint());

  mojo::ScopedHandle client_handle = mojo::WrapPlatformHandle(
      channel.TakeRemoteEndpoint().TakePlatformHandle());
  std::move(callback).Run(std::move(client_handle), pipe_name);

  // The binding will be removed automatically, when the binding is destroyed.
  video_accelerator_factory_bindings_.AddBinding(
      video_accelerator_factory_.get(),
      mojom::VideoAcceleratorFactoryRequest(std::move(server_pipe)));
}

}  // namespace arc
