// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_PAIRED_KEY_VERIFICATION_RUNNER_H_
#define CHROME_BROWSER_NEARBY_SHARING_PAIRED_KEY_VERIFICATION_RUNNER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "chrome/browser/nearby_sharing/incoming_frames_reader.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder_types.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"

class PairedKeyVerificationRunner {
 public:
  enum class PairedKeyVerificationResult {
    // Default value for verification result.
    kUnknown,
    // Succeeded with verification.
    kSuccess,
    // Failed to verify.
    kFail,
    // Unable to verify. Occurs when missing proper certificates.
    kUnable,
  };

  PairedKeyVerificationRunner(
      const ShareTarget& share_target,
      const std::string& endpoint_id,
      const std::vector<uint8_t>& token,
      NearbyConnection* connection,
      const std::optional<NearbyShareDecryptedPublicCertificate>& certificate,
      NearbyShareCertificateManager* certificate_manager,
      nearby_share::mojom::Visibility visibility,
      bool restrict_to_contacts,
      IncomingFramesReader* frames_reader,
      base::TimeDelta read_frame_timeout);

  ~PairedKeyVerificationRunner();

  void Run(base::OnceCallback<void(PairedKeyVerificationResult)> callback);

 private:
  void SendPairedKeyEncryptionFrame();
  void OnReadPairedKeyEncryptionFrame(
      std::optional<sharing::mojom::V1FramePtr> frame);
  void OnReadPairedKeyResultFrame(
      std::vector<PairedKeyVerificationResult> verification_results,
      std::optional<sharing::mojom::V1FramePtr> frame);
  void SendPairedKeyResultFrame(PairedKeyVerificationResult result);
  PairedKeyVerificationResult VerifyRemotePublicCertificate(
      const sharing::mojom::V1FramePtr& frame);
  PairedKeyVerificationResult VerifyPairedKeyEncryptionFrame(
      const sharing::mojom::V1FramePtr& frame);
  PairedKeyVerificationResult MergeResults(
      const std::vector<PairedKeyVerificationResult>& results);
  void SendCertificateInfo();

  ShareTarget share_target_;
  std::string endpoint_id_;
  std::vector<uint8_t> raw_token_;
  raw_ptr<NearbyConnection> connection_;
  std::optional<NearbyShareDecryptedPublicCertificate> certificate_;
  raw_ptr<NearbyShareCertificateManager> certificate_manager_;
  nearby_share::mojom::Visibility visibility_;
  bool restrict_to_contacts_;
  raw_ptr<IncomingFramesReader> frames_reader_;
  const base::TimeDelta read_frame_timeout_;
  base::OnceCallback<void(PairedKeyVerificationResult)> callback_;

  char local_prefix_;
  char remote_prefix_;

  base::WeakPtrFactory<PairedKeyVerificationRunner> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_PAIRED_KEY_VERIFICATION_RUNNER_H_
