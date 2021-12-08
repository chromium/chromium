// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/video_accelerator/oop_arc_video_accelerator_factory.h"

#include "ash/components/arc/video_accelerator/gpu_arc_video_decode_accelerator.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/gpu/macros.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace arc {

// TODO(b/195769334): we should plumb meaningful gpu::GpuPreferences and
// gpu::GpuDriverBugWorkarounds so that we can use them to control behaviors of
// the hardware decoder.
// TODO(b/195769334): plumb a ProtectedBufferManager.
OOPArcVideoAcceleratorFactory::OOPArcVideoAcceleratorFactory(
    mojo::PendingReceiver<mojom::VideoAcceleratorFactory> receiver)
    : receiver_(this, std::move(receiver)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

OOPArcVideoAcceleratorFactory::~OOPArcVideoAcceleratorFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OOPArcVideoAcceleratorFactory::CreateDecodeAccelerator(
    mojo::PendingReceiver<mojom::VideoDecodeAccelerator> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOGF(2);
  // Note that a well-behaved client should not reach this point twice because
  // there should be one process for each GpuArcVideoDecodeAccelerator. This is
  // guaranteed by arc::GpuArcVideoServiceHost in the browser process. Thus, we
  // don't bother validating that here because if the browser process is
  // compromised, we have bigger problems.
  // TODO(b/195769334): plumb a ProtectedBufferManager.
  auto decoder = std::make_unique<GpuArcVideoDecodeAccelerator>(
      gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds(),
      /*protected_buffer_manager=*/nullptr);
  auto decoder_receiver =
      mojo::MakeSelfOwnedReceiver(std::move(decoder), std::move(receiver));
  CHECK(decoder_receiver);
  decoder_receiver->set_connection_error_handler(
      base::BindOnce(&OOPArcVideoAcceleratorFactory::OnDecoderDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OOPArcVideoAcceleratorFactory::CreateEncodeAccelerator(
    mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver) {
  NOTIMPLEMENTED();
}

void OOPArcVideoAcceleratorFactory::CreateProtectedBufferAllocator(
    mojo::PendingReceiver<mojom::VideoProtectedBufferAllocator> receiver) {
  NOTREACHED();
}

void OOPArcVideoAcceleratorFactory::OnDecoderDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOGF(2);
  receiver_.reset();
}

}  // namespace arc
