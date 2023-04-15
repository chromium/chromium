// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAKE_QUICK_START_DECODER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAKE_QUICK_START_DECODER_H_

#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

class FakeQuickStartDecoder : public mojom::QuickStartDecoder {
 public:
  FakeQuickStartDecoder();
  FakeQuickStartDecoder(const FakeQuickStartDecoder&) = delete;
  FakeQuickStartDecoder(FakeQuickStartDecoder&&) = delete;
  ~FakeQuickStartDecoder() override;

  mojo::PendingRemote<mojom::QuickStartDecoder> GetRemote();

  // mojom::QuickStartDecoder:
  void DecodeBootstrapConfigurations(
      const std::vector<uint8_t>& data,
      DecodeBootstrapConfigurationsCallback callback) override;
  void DecodeGetAssertionResponse(
      const std::vector<uint8_t>& data,
      DecodeGetAssertionResponseCallback callback) override;
  void DecodeWifiCredentialsResponse(
      const std::vector<uint8_t>& data,
      DecodeWifiCredentialsResponseCallback callback) override;

  void SetExpectedData(std::vector<uint8_t> expected_data);
  void SetAssertionResponse(
      mojom::GetAssertionResponse::GetAssertionStatus status,
      uint8_t decoder_status,
      uint8_t decoder_error,
      const std::string& email,
      const std::string& credential_id,
      const std::vector<uint8_t>& signature,
      const std::vector<uint8_t>& data);

  void SetWifiCredentialsResponse(
      mojom::GetWifiCredentialsResponsePtr response);

 private:
  std::vector<uint8_t> expected_data_;
  mojom::GetAssertionResponse::GetAssertionStatus response_status_;
  uint8_t response_decoder_status_;
  uint8_t response_decoder_error_;
  std::string response_email_;
  std::string response_credential_id_;
  std::vector<uint8_t> response_signature_;
  std::vector<uint8_t> response_data_;
  mojo::ReceiverSet<ash::quick_start::mojom::QuickStartDecoder> receiver_set_;
  mojom::GetWifiCredentialsResponsePtr wifi_credentials_response_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FAKE_QUICK_START_DECODER_H_
