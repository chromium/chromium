// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_quick_start_decoder.h"

namespace ash::quick_start {

FakeQuickStartDecoder::~FakeQuickStartDecoder() = default;

FakeQuickStartDecoder::FakeQuickStartDecoder() = default;

mojo::PendingRemote<mojom::QuickStartDecoder>
FakeQuickStartDecoder::GetRemote() {
  mojo::PendingRemote<mojom::QuickStartDecoder> pending_remote;
  receiver_set_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}
void FakeQuickStartDecoder::DecodeBootstrapConfigurations(
    const std::vector<uint8_t>& data,
    DecodeBootstrapConfigurationsCallback callback) {
  EXPECT_EQ(expected_data_, data);
}

void FakeQuickStartDecoder::DecodeGetAssertionResponse(
    const std::vector<uint8_t>& data,
    DecodeGetAssertionResponseCallback callback) {
  EXPECT_EQ(expected_data_, data);
  std::move(callback).Run(mojom::GetAssertionResponse::New(
      response_status_, response_decoder_status_, response_decoder_error_,
      response_email_, response_credential_id_, response_data_,
      response_signature_));
}

void FakeQuickStartDecoder::SetExpectedData(
    std::vector<uint8_t> expected_data) {
  expected_data_ = expected_data;
}

void FakeQuickStartDecoder::SetAssertionResponse(
    mojom::GetAssertionResponse::GetAssertionStatus status,
    uint8_t decoder_status,
    uint8_t decoder_error,
    const std::string& email,
    const std::string& credential_id,
    const std::vector<uint8_t>& signature,
    const std::vector<uint8_t>& data) {
  response_status_ = status;
  response_decoder_status_ = decoder_status;
  response_decoder_error_ = decoder_error;
  response_email_ = email;
  response_credential_id_ = credential_id;
  response_signature_ = signature;
  response_data_ = data;
}

}  // namespace ash::quick_start
