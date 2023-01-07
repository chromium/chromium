// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/tpm_enrollment_key_signing_service.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chromeos/ash/components/dbus/attestation/attestation.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

TpmEnrollmentKeySigningService::TpmEnrollmentKeySigningService() = default;

TpmEnrollmentKeySigningService::~TpmEnrollmentKeySigningService() = default;

void TpmEnrollmentKeySigningService::SignData(const std::string& data,
                                              SigningCallback callback) {
  ::attestation::SignSimpleChallengeRequest request;
  request.set_username("");
  request.set_key_label(ash::attestation::kEnterpriseEnrollmentKey);
  request.set_challenge(data);
  ash::AttestationClient::Get()->SignSimpleChallenge(
      request, base::BindOnce(&TpmEnrollmentKeySigningService::OnDataSigned,
                              weak_ptr_factory_.GetWeakPtr(), data,
                              std::move(callback)));
}

void TpmEnrollmentKeySigningService::OnDataSigned(
    const std::string& data,
    SigningCallback callback,
    const ::attestation::SignSimpleChallengeReply& reply) {
  enterprise_management::SignedData em_signed_data;
  ash::attestation::SignedData att_signed_data;
  const bool success =
      reply.status() == ::attestation::STATUS_SUCCESS &&
      att_signed_data.ParseFromString(reply.challenge_response());
  if (success) {
    em_signed_data.set_data(att_signed_data.data());
    em_signed_data.set_signature(att_signed_data.signature());
    em_signed_data.set_extra_data_bytes(att_signed_data.data().size() -
                                        data.size());
  }
  std::move(callback).Run(success, em_signed_data);
}

}  // namespace policy
