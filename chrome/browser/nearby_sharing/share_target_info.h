// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_INFO_H_
#define CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_INFO_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "chrome/browser/nearby_sharing/incoming_frames_reader.h"
#include "chrome/browser/nearby_sharing/paired_key_verification_runner.h"
#include "chrome/browser/nearby_sharing/payload_tracker.h"
#include "chrome/browser/nearby_sharing/transfer_update_callback.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"

class NearbyConnection;

// Additional information about the connection to a remote device.
class ShareTargetInfo {
 public:
  ShareTargetInfo();
  ShareTargetInfo(ShareTargetInfo&&);
  ShareTargetInfo& operator=(ShareTargetInfo&&);
  virtual ~ShareTargetInfo();

  const std::optional<std::string>& endpoint_id() const { return endpoint_id_; }

  void set_endpoint_id(std::string endpoint_id) {
    endpoint_id_ = std::move(endpoint_id);
  }

  const std::optional<NearbyShareDecryptedPublicCertificate>& certificate()
      const {
    return certificate_;
  }

  void set_certificate(NearbyShareDecryptedPublicCertificate certificate) {
    certificate_ = std::move(certificate);
  }

  NearbyConnection* connection() const { return connection_; }

  void set_connection(NearbyConnection* connection) {
    connection_ = connection;
  }

  TransferUpdateCallback* transfer_update_callback() const {
    return transfer_update_callback_.get();
  }

  void set_transfer_update_callback(
      std::unique_ptr<TransferUpdateCallback> transfer_update_callback) {
    transfer_update_callback_ = std::move(transfer_update_callback);
  }

  const std::optional<std::string>& token() const { return token_; }

  void set_token(std::string token) { token_ = std::move(token); }

  IncomingFramesReader* frames_reader() const { return frames_reader_.get(); }

  void set_frames_reader(std::unique_ptr<IncomingFramesReader> frames_reader) {
    frames_reader_ = std::move(frames_reader);
  }

  PairedKeyVerificationRunner* key_verification_runner() {
    return key_verification_runner_.get();
  }

  void set_key_verification_runner(
      std::unique_ptr<PairedKeyVerificationRunner> key_verification_runner) {
    key_verification_runner_ = std::move(key_verification_runner);
  }

  base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener>
  payload_tracker() {
    return payload_tracker_->GetWeakPtr();
  }

  void set_payload_tracker(std::unique_ptr<PayloadTracker> payload_tracker) {
    payload_tracker_ = std::move(payload_tracker);
  }

 private:
  std::optional<std::string> endpoint_id_;
  std::optional<NearbyShareDecryptedPublicCertificate> certificate_;
  raw_ptr<NearbyConnection> connection_ = nullptr;
  std::unique_ptr<TransferUpdateCallback> transfer_update_callback_;
  std::optional<std::string> token_;
  std::unique_ptr<IncomingFramesReader> frames_reader_;
  std::unique_ptr<PairedKeyVerificationRunner> key_verification_runner_;
  std::unique_ptr<PayloadTracker> payload_tracker_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_INFO_H_
