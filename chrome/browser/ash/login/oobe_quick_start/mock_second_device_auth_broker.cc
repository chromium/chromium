// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/mock_second_device_auth_broker.h"

#include <memory>

#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {
namespace {

constexpr char kDeviceId[] = "fake-device-id";

}  // namespace

using testing::Invoke;
using testing::WithArg;

MockSecondDeviceAuthBroker::MockSecondDeviceAuthBroker(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : SecondDeviceAuthBroker(
          kDeviceId,
          url_loader_factory,
          std::make_unique<attestation::MockAttestationFlow>()) {}

MockSecondDeviceAuthBroker::~MockSecondDeviceAuthBroker() = default;

void MockSecondDeviceAuthBroker::SetupChallengeBytesResponse(
    ChallengeBytesOrError challenge) {
  ON_CALL(*this, FetchChallengeBytes)
      .WillByDefault(
          WithArg<0>(Invoke([challenge](ChallengeBytesCallback callback) {
            std::move(callback).Run(challenge);
          })));
}

void MockSecondDeviceAuthBroker::SetupAttestationCertificateResponse(
    AttestationCertificateOrError cert) {
  ON_CALL(*this, FetchAttestationCertificate)
      .WillByDefault(
          WithArg<1>(Invoke([cert](AttestationCertificateCallback callback) {
            std::move(callback).Run(cert);
          })));
}
void MockSecondDeviceAuthBroker::SetupAuthCodeResponse(
    AuthCodeResponse response) {
  ON_CALL(*this, FetchAuthCode)
      .WillByDefault(WithArg<2>(Invoke([response](AuthCodeCallback callback) {
        std::move(callback).Run(response);
      })));
}

}  // namespace ash::quick_start
