// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/authenticated_connection.h"

#include "base/values.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"

namespace ash::quick_start {

namespace {

// MessagePayload key telling the phone specific options
// for how to handle account transfer and fallback.
constexpr char kBootstrapOptionsKey[] = "bootstrapOptions";

// bootstrapOptions key telling the phone the number of
// accounts are expected to transfer account to the target device.
constexpr char kAccountRequirementKey[] = "accountRequirement";

// bootstrapOptions key telling the phone how to handle
// challenge UI in case of fallback.
constexpr char kFlowTypeKey[] = "flowType";

// Maps to AccountRequirementSingle enum value for Account Requirement field
// meaning that at least one account is required on the phone. The user will
// select the specified account to transfer.
// Enum Source: go/bootstrap-options-account-requirement-single.
constexpr int kAccountRequirementSingle = 2;

// Maps to FlowTypeTargetChallenge enum value for Flow Type field meaning that
// the fallback challenge will happen on the target device.
// Enum Source: go/bootstrap-options-flow-type-target-challenge.
constexpr int kFlowTypeTargetChallenge = 2;

}  // namespace

AuthenticatedConnection::AuthenticatedConnection(
    NearbyConnection* nearby_connection)
    : Connection(nearby_connection) {}

AuthenticatedConnection::~AuthenticatedConnection() = default;

void AuthenticatedConnection::RequestAccountTransferAssertion() {
  SendBootstrapOptions();
}

void AuthenticatedConnection::SendBootstrapOptions() {
  base::Value::Dict bootstrap_options;
  bootstrap_options.Set(kAccountRequirementKey, kAccountRequirementSingle);
  bootstrap_options.Set(kFlowTypeKey, kFlowTypeTargetChallenge);

  base::Value::Dict message_payload;
  message_payload.Set(kBootstrapOptionsKey, std::move(bootstrap_options));

  SendPayload(message_payload);
  nearby_connection_->Read(
      base::BindOnce(&AuthenticatedConnection::OnBootstrapOptionsResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuthenticatedConnection::OnBootstrapOptionsResponse(
    absl::optional<std::vector<uint8_t>>) {
  NOTIMPLEMENTED();
}

}  // namespace ash::quick_start
