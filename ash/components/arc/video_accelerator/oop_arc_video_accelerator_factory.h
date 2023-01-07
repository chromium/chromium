// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_OOP_ARC_VIDEO_ACCELERATOR_FACTORY_H_
#define ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_OOP_ARC_VIDEO_ACCELERATOR_FACTORY_H_

#include "ash/components/arc/mojom/video.mojom.h"
#include "ash/components/arc/mojom/video_decode_accelerator.mojom.h"
#include "ash/components/arc/mojom/video_decoder.mojom.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/components/cdm_factory_daemon/mojom/browser_cdm_factory.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace arc {

class DecoderProtectedBufferManager;
class GpuArcVideoDecodeAccelerator;

// An OOPArcVideoAcceleratorFactory runs in its own process and wraps a single
// GpuArcVideoDecodeAccelerator. When the GpuArcVideoDecodeAccelerator gets
// disconnected, OOPArcVideoAcceleratorFactory disconnects its own |receiver_|
// (this is intended to signal the mojo::ServiceFactory that it can destroy the
// OOPArcVideoAcceleratorFactory which in turn should trigger the end of the
// process).
class OOPArcVideoAcceleratorFactory
    : public arc::mojom::VideoAcceleratorFactory {
 public:
  OOPArcVideoAcceleratorFactory(
      mojo::PendingReceiver<mojom::VideoAcceleratorFactory> receiver);
  OOPArcVideoAcceleratorFactory(const OOPArcVideoAcceleratorFactory&) = delete;
  OOPArcVideoAcceleratorFactory& operator=(
      const OOPArcVideoAcceleratorFactory&) = delete;
  ~OOPArcVideoAcceleratorFactory() override;

  // arc::mojom:::VideoAcceleratorFactory implementation.
  void CreateDecodeAccelerator(
      mojo::PendingReceiver<mojom::VideoDecodeAccelerator> receiver,
      mojo::PendingRemote<mojom::ProtectedBufferManager>
          protected_buffer_manager,
      mojo::PendingRemote<chromeos::cdm::mojom::BrowserCdmFactory>
          browser_cdm_factory) override;
  void CreateVideoDecoder(
      mojo::PendingReceiver<mojom::VideoDecoder> receiver) override;
  void CreateEncodeAccelerator(
      mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver) override;
  void CreateProtectedBufferAllocator(
      mojo::PendingReceiver<mojom::VideoProtectedBufferAllocator> receiver)
      override;

 private:
  void OnDecoderDisconnected();

  mojo::Receiver<mojom::VideoAcceleratorFactory> receiver_;
  scoped_refptr<DecoderProtectedBufferManager> protected_buffer_manager_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OOPArcVideoAcceleratorFactory> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_VIDEO_ACCELERATOR_OOP_ARC_VIDEO_ACCELERATOR_FACTORY_H_
