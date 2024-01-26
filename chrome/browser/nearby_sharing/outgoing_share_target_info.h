// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_OUTGOING_SHARE_TARGET_INFO_H_
#define CHROME_BROWSER_NEARBY_SHARING_OUTGOING_SHARE_TARGET_INFO_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/nearby_sharing/share_target_info.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"

// A description of the outgoing connection to a remote device.
class OutgoingShareTargetInfo : public ShareTargetInfo {
 public:
  using PayloadPtr = ::nearby::connections::mojom::PayloadPtr;

  OutgoingShareTargetInfo();
  OutgoingShareTargetInfo(OutgoingShareTargetInfo&&);
  OutgoingShareTargetInfo& operator=(OutgoingShareTargetInfo&&);
  ~OutgoingShareTargetInfo() override;

  const std::optional<std::string>& obfuscated_gaia_id() const {
    return obfuscated_gaia_id_;
  }

  void set_obfuscated_gaia_id(std::string obfuscated_gaia_id) {
    obfuscated_gaia_id_ = std::move(obfuscated_gaia_id);
  }

  const std::vector<PayloadPtr>& text_payloads() const {
    return text_payloads_;
  }

  void set_text_payloads(std::vector<PayloadPtr> payloads) {
    text_payloads_ = std::move(payloads);
  }

  const std::vector<PayloadPtr>& file_payloads() const {
    return file_payloads_;
  }

  void set_file_payloads(std::vector<PayloadPtr> payloads) {
    file_payloads_ = std::move(payloads);
  }

  std::vector<PayloadPtr> ExtractTextPayloads();
  std::vector<PayloadPtr> ExtractFilePayloads();

 private:
  std::optional<std::string> obfuscated_gaia_id_;
  std::vector<PayloadPtr> text_payloads_;
  std::vector<PayloadPtr> file_payloads_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_OUTGOING_SHARE_TARGET_INFO_H_
