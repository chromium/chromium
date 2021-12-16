// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/video/gpu_arc_video_service_host.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/protected_buffer_manager.mojom.h"
#include "ash/components/arc/mojom/video_decode_accelerator.mojom.h"
#include "ash/components/arc/mojom/video_decoder.mojom.h"
#include "ash/components/arc/mojom/video_encode_accelerator.mojom.h"
#include "ash/components/arc/mojom/video_protected_buffer_allocator.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_service_registry.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Singleton factory for GpuArcVideoKeyedService.
class GpuArcVideoKeyedServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          GpuArcVideoKeyedService,
          GpuArcVideoKeyedServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "GpuArcVideoKeyedServiceFactory";

  static GpuArcVideoKeyedServiceFactory* GetInstance() {
    static base::NoDestructor<GpuArcVideoKeyedServiceFactory> instance;
    return instance.get();
  }

 private:
  friend class base::NoDestructor<GpuArcVideoKeyedServiceFactory>;
  GpuArcVideoKeyedServiceFactory() = default;
  ~GpuArcVideoKeyedServiceFactory() override = default;
};

class VideoAcceleratorFactoryService : public mojom::VideoAcceleratorFactory {
 public:
  VideoAcceleratorFactoryService() = default;

  VideoAcceleratorFactoryService(const VideoAcceleratorFactoryService&) =
      delete;
  VideoAcceleratorFactoryService& operator=(
      const VideoAcceleratorFactoryService&) = delete;

  ~VideoAcceleratorFactoryService() override = default;

  void CreateDecodeAccelerator(
      mojo::PendingReceiver<mojom::VideoDecodeAccelerator> receiver,
      mojo::PendingRemote<
          mojom::ProtectedBufferManager> /*protected_buffer_manager*/)
      override {
    if (base::FeatureList::IsEnabled(arc::kOutOfProcessVideoDecoding)) {
      // TODO(b/195769334): we should check if accelerated video decode is
      // disabled by means of a flag/switch or by GPU bug workarounds.
      constexpr size_t kMaxArcVideoDecoderProcesses = 8u;
      if (oop_video_factories_.size() == kMaxArcVideoDecoderProcesses) {
        LOG(WARNING)
            << "Reached the maximum number of video decoder processes for ARC ("
            << kMaxArcVideoDecoderProcesses << ")";
        return;
      }
      mojo::Remote<mojom::VideoAcceleratorFactory> oop_video_factory;
      content::ServiceProcessHost::Launch(
          oop_video_factory.BindNewPipeAndPassReceiver(),
          content::ServiceProcessHost::Options()
              .WithDisplayName("ARC Video Decoder")
              .Pass());

      // Version 8 accepts a ProtectedBufferManager.
      oop_video_factory.RequireVersion(8);
      mojo::PendingRemote<mojom::ProtectedBufferManager>
          protected_buffer_manager;
      content::BindInterfaceInGpuProcess(
          protected_buffer_manager.InitWithNewPipeAndPassReceiver());

      oop_video_factory->CreateDecodeAccelerator(
          std::move(receiver), std::move(protected_buffer_manager));
      oop_video_factories_.Add(std::move(oop_video_factory));
      return;
    }
    content::BindInterfaceInGpuProcess(std::move(receiver));
  }

  void CreateVideoDecoder(
      mojo::PendingReceiver<mojom::VideoDecoder> receiver) override {
    content::BindInterfaceInGpuProcess(std::move(receiver));
  }

  void CreateEncodeAccelerator(
      mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver) override {
    content::BindInterfaceInGpuProcess(std::move(receiver));
  }

  void CreateProtectedBufferAllocator(
      mojo::PendingReceiver<mojom::VideoProtectedBufferAllocator> receiver)
      override {
    content::BindInterfaceInGpuProcess(std::move(receiver));
  }

 private:
  mojo::RemoteSet<mojom::VideoAcceleratorFactory> oop_video_factories_;
};

}  // namespace

// static
GpuArcVideoKeyedService* GpuArcVideoKeyedService::GetForBrowserContext(
    content::BrowserContext* context) {
  return GpuArcVideoKeyedServiceFactory::GetForBrowserContext(context);
}

GpuArcVideoKeyedService::GpuArcVideoKeyedService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->video()->SetHost(GpuArcVideoServiceHost::Get());
}

GpuArcVideoKeyedService::~GpuArcVideoKeyedService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->video()->SetHost(nullptr);
}

GpuArcVideoServiceHost::GpuArcVideoServiceHost()
    : video_accelerator_factory_(
          std::make_unique<VideoAcceleratorFactoryService>()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

GpuArcVideoServiceHost::~GpuArcVideoServiceHost() = default;

// static
GpuArcVideoServiceHost* GpuArcVideoServiceHost::GpuArcVideoServiceHost::Get() {
  static base::NoDestructor<GpuArcVideoServiceHost> instance;
  return instance.get();
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

  // The receiver will be removed automatically, when the receiver is destroyed.
  video_accelerator_factory_receivers_.Add(
      video_accelerator_factory_.get(),
      mojo::PendingReceiver<mojom::VideoAcceleratorFactory>(
          std::move(server_pipe)));
}

}  // namespace arc
