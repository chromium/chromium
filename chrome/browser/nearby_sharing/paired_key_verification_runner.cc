// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/paired_key_verification_runner.h"

#include <iomanip>
#include <iostream>

#include "base/bind.h"
#include "chrome/browser/nearby_sharing/certificates/common.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/proto/rpc_resources.pb.h"
#include "chrome/services/sharing/public/proto/wire_format.pb.h"

namespace {

// The size of the random byte array used for the encryption frame's signed data
// if a valid signature cannot be generated. This size is consistent with the
// GmsCore implementation.
const size_t kNearbyShareNumBytesRandomSignature = 72;

std::ostream& operator<<(
    std::ostream& out,
    const PairedKeyVerificationRunner::PairedKeyVerificationResult& obj) {
  out << static_cast<std::underlying_type<
      PairedKeyVerificationRunner::PairedKeyVerificationResult>::type>(obj);
  return out;
}

PairedKeyVerificationRunner::PairedKeyVerificationResult Convert(
    sharing::mojom::PairedKeyResultFrame::Status status) {
  switch (status) {
    case sharing::mojom::PairedKeyResultFrame_Status::kUnknown:
      return PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown;

    case sharing::mojom::PairedKeyResultFrame_Status::kSuccess:
      return PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess;

    case sharing::mojom::PairedKeyResultFrame_Status::kFail:
      return PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail;

    case sharing::mojom::PairedKeyResultFrame_Status::kUnable:
      return PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable;
  }
}

std::vector<uint8_t> PadPrefix(char prefix, std::vector<uint8_t> bytes) {
  bytes.insert(bytes.begin(), prefix);
  return bytes;
}

}  // namespace

PairedKeyVerificationRunner::PairedKeyVerificationRunner(
    const ShareTarget& share_target,
    const std::string& endpoint_id,
    const std::vector<uint8_t>& token,
    NearbyConnection* connection,
    const absl::optional<NearbyShareDecryptedPublicCertificate>& certificate,
    NearbyShareCertificateManager* certificate_manager,
    nearby_share::mojom::Visibility visibility,
    bool restrict_to_contacts,
    IncomingFramesReader* frames_reader,
    base::TimeDelta read_frame_timeout)
    : share_target_(share_target),
      endpoint_id_(endpoint_id),
      raw_token_(token),
      connection_(connection),
      certificate_(certificate),
      certificate_manager_(certificate_manager),
      visibility_(visibility),
      restrict_to_contacts_(restrict_to_contacts),
      frames_reader_(frames_reader),
      read_frame_timeout_(read_frame_timeout) {
  DCHECK(connection);
  DCHECK(certificate_manager);
  DCHECK(frames_reader);

  if (share_target.is_incoming) {
    local_prefix_ = kNearbyShareReceiverVerificationPrefix;
    remote_prefix_ = kNearbyShareSenderVerificationPrefix;
  } else {
    remote_prefix_ = kNearbyShareReceiverVerificationPrefix;
    local_prefix_ = kNearbyShareSenderVerificationPrefix;
  }
}

PairedKeyVerificationRunner::~PairedKeyVerificationRunner() = default;

void PairedKeyVerificationRunner::Run(
    base::OnceCallback<void(PairedKeyVerificationResult)> callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);

  SendPairedKeyEncryptionFrame();
  frames_reader_->ReadFrame(
      sharing::mojom::V1Frame::Tag::kPairedKeyEncryption,
      base::BindOnce(
          &PairedKeyVerificationRunner::OnReadPairedKeyEncryptionFrame,
          weak_ptr_factory_.GetWeakPtr()),
      read_frame_timeout_);
}

void PairedKeyVerificationRunner::OnReadPairedKeyEncryptionFrame(
    absl::optional<sharing::mojom::V1FramePtr> frame) {
  if (!frame) {
    NS_LOG(WARNING) << __func__
                    << ": Failed to read remote paired key encrpytion";
    std::move(callback_).Run(PairedKeyVerificationResult::kFail);
    return;
  }

  std::vector<PairedKeyVerificationResult> verification_results;

  PairedKeyVerificationResult remote_public_certificate_result =
      VerifyRemotePublicCertificate(*frame);
  verification_results.push_back(remote_public_certificate_result);
  NS_LOG(VERBOSE) << __func__
                  << ": Remote public certificate verification result "
                  << remote_public_certificate_result;

  if (remote_public_certificate_result ==
      PairedKeyVerificationResult::kSuccess) {
    SendCertificateInfo();
  } else if (restrict_to_contacts_) {
    NS_LOG(VERBOSE) << __func__
                    << ": we are only allowing connections with contacts. "
                       "Rejecting connection from unknown ShareTarget - "
                    << share_target_.id;
    std::move(callback_).Run(PairedKeyVerificationResult::kFail);
    return;
  }

  PairedKeyVerificationResult local_result =
      VerifyPairedKeyEncryptionFrame(*frame);
  verification_results.push_back(local_result);
  NS_LOG(VERBOSE) << __func__ << ": Paired key encryption verification result "
                  << local_result;

  SendPairedKeyResultFrame(local_result);

  frames_reader_->ReadFrame(
      sharing::mojom::V1Frame::Tag::kPairedKeyResult,
      base::BindOnce(&PairedKeyVerificationRunner::OnReadPairedKeyResultFrame,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(verification_results)),
      read_frame_timeout_);
}

void PairedKeyVerificationRunner::OnReadPairedKeyResultFrame(
    std::vector<PairedKeyVerificationResult> verification_results,
    absl::optional<sharing::mojom::V1FramePtr> frame) {
  if (!frame) {
    NS_LOG(WARNING) << __func__ << ": Failed to read remote paired key result";
    std::move(callback_).Run(PairedKeyVerificationResult::kFail);
    return;
  }

  PairedKeyVerificationResult key_result =
      Convert(frame.value()->get_paired_key_result()->status);
  verification_results.push_back(key_result);
  NS_LOG(VERBOSE) << __func__ << ": Paired key result frame result "
                  << key_result;

  PairedKeyVerificationResult combined_result =
      MergeResults(verification_results);
  NS_LOG(VERBOSE) << __func__ << ": Combined verification result "
                  << combined_result;
  std::move(callback_).Run(combined_result);
}

void PairedKeyVerificationRunner::SendPairedKeyResultFrame(
    PairedKeyVerificationResult result) {
  sharing::nearby::Frame frame;
  frame.set_version(sharing::nearby::Frame::V1);
  sharing::nearby::V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(sharing::nearby::V1Frame::PAIRED_KEY_RESULT);
  sharing::nearby::PairedKeyResultFrame* result_frame =
      v1_frame->mutable_paired_key_result();

  switch (result) {
    case PairedKeyVerificationResult::kUnable:
      result_frame->set_status(sharing::nearby::PairedKeyResultFrame::UNABLE);
      break;

    case PairedKeyVerificationResult::kSuccess:
      result_frame->set_status(sharing::nearby::PairedKeyResultFrame::SUCCESS);
      break;

    case PairedKeyVerificationResult::kFail:
      result_frame->set_status(sharing::nearby::PairedKeyResultFrame::FAIL);
      break;

    case PairedKeyVerificationResult::kUnknown:
      result_frame->set_status(sharing::nearby::PairedKeyResultFrame::UNKNOWN);
      break;
  }

  std::vector<uint8_t> data(frame.ByteSize());
  frame.SerializeToArray(data.data(), frame.ByteSize());

  connection_->Write(std::move(data));
}

void PairedKeyVerificationRunner::SendCertificateInfo() {
  // TODO(https://crbug.com/1114765): Update once the bug is resolved.
  std::vector<nearbyshare::proto::PublicCertificate> certificates;

  if (certificates.empty())
    return;

  sharing::nearby::Frame frame;
  frame.set_version(sharing::nearby::Frame::V1);
  sharing::nearby::V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(sharing::nearby::V1Frame::CERTIFICATE_INFO);
  sharing::nearby::CertificateInfoFrame* cert_frame =
      v1_frame->mutable_certificate_info();
  for (const auto& certificate : certificates) {
    sharing::nearby::PublicCertificate* cert =
        cert_frame->add_public_certificate();
    cert->set_secret_id(certificate.secret_id());
    cert->set_authenticity_key(certificate.secret_key());
    cert->set_public_key(certificate.public_key());
    cert->set_start_time(certificate.start_time().seconds() * 1000);
    cert->set_end_time(certificate.end_time().seconds() * 1000);
    cert->set_encrypted_metadata_bytes(certificate.encrypted_metadata_bytes());
    cert->set_metadata_encryption_key_tag(
        certificate.metadata_encryption_key_tag());
  }

  std::vector<uint8_t> data(frame.ByteSize());
  frame.SerializeToArray(data.data(), frame.ByteSize());

  connection_->Write(std::move(data));
}

void PairedKeyVerificationRunner::SendPairedKeyEncryptionFrame() {
  absl::optional<std::vector<uint8_t>> signature =
      certificate_manager_->SignWithPrivateCertificate(
          visibility_, PadPrefix(local_prefix_, raw_token_));
  if (!signature || signature->empty()) {
    signature = GenerateRandomBytes(kNearbyShareNumBytesRandomSignature);
  }

  std::vector<uint8_t> certificate_id_hash;
  if (certificate_) {
    certificate_id_hash = certificate_->HashAuthenticationToken(raw_token_);
  }
  if (certificate_id_hash.empty()) {
    certificate_id_hash =
        GenerateRandomBytes(kNearbyShareNumBytesAuthenticationTokenHash);
  }

  sharing::nearby::Frame frame;
  frame.set_version(sharing::nearby::Frame::V1);
  sharing::nearby::V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(sharing::nearby::V1Frame::PAIRED_KEY_ENCRYPTION);
  sharing::nearby::PairedKeyEncryptionFrame* encryption_frame =
      v1_frame->mutable_paired_key_encryption();
  encryption_frame->set_signed_data(signature->data(), signature->size());
  encryption_frame->set_secret_id_hash(certificate_id_hash.data(),
                                       certificate_id_hash.size());
  std::vector<uint8_t> data(frame.ByteSize());
  frame.SerializeToArray(data.data(), frame.ByteSize());

  connection_->Write(std::move(data));
}

PairedKeyVerificationRunner::PairedKeyVerificationResult
PairedKeyVerificationRunner::VerifyRemotePublicCertificate(
    const sharing::mojom::V1FramePtr& frame) {
  absl::optional<std::vector<uint8_t>> hash =
      certificate_manager_->HashAuthenticationTokenWithPrivateCertificate(
          visibility_, raw_token_);
  if (hash && *hash == frame->get_paired_key_encryption()->secret_id_hash) {
    NS_LOG(VERBOSE) << __func__
                    << ": Successfully verified remote public certificate.";
    return PairedKeyVerificationResult::kSuccess;
  }

  NS_LOG(VERBOSE) << __func__
                  << ": Unable to verify remote public certificate.";
  return PairedKeyVerificationResult::kUnable;
}

PairedKeyVerificationRunner::PairedKeyVerificationResult
PairedKeyVerificationRunner::VerifyPairedKeyEncryptionFrame(
    const sharing::mojom::V1FramePtr& frame) {
  if (!certificate_) {
    NS_LOG(VERBOSE) << __func__
                    << ": Unable to verify remote paired key encryption frame. "
                       "Certificate not found.";
    return PairedKeyVerificationResult::kUnable;
  }

  if (!certificate_->VerifySignature(
          PadPrefix(remote_prefix_, raw_token_),
          frame->get_paired_key_encryption()->signed_data)) {
    NS_LOG(VERBOSE) << __func__
                    << ": Unable to verify remote paired key encryption frame. "
                       "Signature verification failed.";
    return PairedKeyVerificationResult::kFail;
  }

  if (!share_target_.is_known) {
    NS_LOG(VERBOSE) << __func__
                    << ": Unable to verify remote paired key encryption frame. "
                       "Remote side is not a known share target.";
    return PairedKeyVerificationResult::kUnable;
  }

  NS_LOG(VERBOSE)
      << __func__
      << ": Successfully verified remote paired key encryption frame.";
  return PairedKeyVerificationResult::kSuccess;
}

PairedKeyVerificationRunner::PairedKeyVerificationResult
PairedKeyVerificationRunner::MergeResults(
    const std::vector<PairedKeyVerificationResult>& results) {
  bool all_success = true;
  for (const auto& result : results) {
    if (result == PairedKeyVerificationResult::kFail)
      return result;

    if (result != PairedKeyVerificationResult::kSuccess)
      all_success = false;
  }

  return all_success ? PairedKeyVerificationResult::kSuccess
                     : PairedKeyVerificationResult::kUnable;
}
