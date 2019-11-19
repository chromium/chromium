// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PLATFORM_VERIFICATION_IMPL_H_
#define CHROME_BROWSER_MEDIA_PLATFORM_VERIFICATION_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/frame_service_base.h"
#include "media/mojo/mojom/platform_verification.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#endif

// Implements media::mojom::PlatformVerification. Can only be used on the
// UI thread because PlatformVerificationFlow lives on the UI thread.
class PlatformVerificationImpl final
    : public content::FrameServiceBase<media::mojom::PlatformVerification> {
 public:
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::PlatformVerification> receiver);

  PlatformVerificationImpl(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::PlatformVerification> receiver);

  // mojo::InterfaceImpl<PlatformVerification> implementation.
  void ChallengePlatform(const std::string& service_id,
                         const std::string& challenge,
                         ChallengePlatformCallback callback) final;
  void GetStorageId(uint32_t version, GetStorageIdCallback callback) final;

 private:
  // |this| can only be destructed as a FrameServiceBase.
  ~PlatformVerificationImpl() final;

#if defined(OS_CHROMEOS)
  using PlatformVerificationResult =
      chromeos::attestation::PlatformVerificationFlow::Result;

  void OnPlatformChallenged(ChallengePlatformCallback callback,
                            PlatformVerificationResult result,
                            const std::string& signed_data,
                            const std::string& signature,
                            const std::string& platform_key_certificate);
#endif

  void OnStorageIdResponse(GetStorageIdCallback callback,
                           const std::vector<uint8_t>& storage_id);

#if defined(OS_CHROMEOS)
  scoped_refptr<chromeos::attestation::PlatformVerificationFlow>
      platform_verification_flow_;
#endif

  content::RenderFrameHost* const render_frame_host_;
  base::WeakPtrFactory<PlatformVerificationImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_PLATFORM_VERIFICATION_IMPL_H_
