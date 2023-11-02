// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/tpm_status_combiner.h"

#include <utility>

#include "base/logging.h"

namespace policy {

TpmStatusCombiner::TpmStatusCombiner(
    DeviceStatusCollector::TpmStatusReceiver callback)
    : callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
}

TpmStatusCombiner::~TpmStatusCombiner() = default;

void TpmStatusCombiner::OnGetTpmStatus(
    const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply) {
  has_tpm_status_ = true;
  if (reply.status() == ::tpm_manager::STATUS_SUCCESS) {
    tpm_status_info_.set_enabled(reply.is_enabled());
    tpm_status_info_.set_owned(reply.is_owned());
    // Wiped owner password means the TPm initialization is done and no any
    // further operations needed.
    tpm_status_info_.set_tpm_initialized(reply.is_owned() &&
                                         !reply.is_owner_password_present());
    tpm_status_info_.set_owner_password_is_present(
        reply.is_owner_password_present());
  } else {
    LOG(WARNING) << "Failed to get tpm status.";
  }
  RunCallbackIfComplete();
}

void TpmStatusCombiner::OnGetEnrollmentStatus(
    const ::attestation::GetStatusReply& reply) {
  has_enrollment_status_ = true;
  if (reply.status() == ::attestation::STATUS_SUCCESS) {
    tpm_status_info_.set_attestation_prepared(reply.prepared_for_enrollment());
    tpm_status_info_.set_attestation_enrolled(reply.enrolled());
  } else {
    LOG(WARNING) << "Failed to get enrollment info.";
  }

  RunCallbackIfComplete();
}

void TpmStatusCombiner::OnGetDictionaryAttackInfo(
    const ::tpm_manager::GetDictionaryAttackInfoReply& reply) {
  has_dictionary_attack_info_ = true;
  if (reply.status() == ::tpm_manager::STATUS_SUCCESS) {
    tpm_status_info_.set_dictionary_attack_counter(
        reply.dictionary_attack_counter());
    tpm_status_info_.set_dictionary_attack_threshold(
        reply.dictionary_attack_threshold());
    tpm_status_info_.set_dictionary_attack_lockout_in_effect(
        reply.dictionary_attack_lockout_in_effect());
    tpm_status_info_.set_dictionary_attack_lockout_seconds_remaining(
        reply.dictionary_attack_lockout_seconds_remaining());
  } else {
    LOG(WARNING) << "Failed to get dictionary attack info.";
  }

  RunCallbackIfComplete();
}

void TpmStatusCombiner::OnGetSupportedFeatures(
    const ::tpm_manager::GetSupportedFeaturesReply& reply) {
  has_supported_features_ = true;
  if (reply.status() == ::tpm_manager::STATUS_SUCCESS) {
    enterprise_management::TpmSupportedFeatures* const
        tpm_supported_features_proto =
            tpm_status_info_.mutable_tpm_supported_features();
    tpm_supported_features_proto->set_is_allowed(reply.is_allowed());
    tpm_supported_features_proto->set_support_pinweaver(
        reply.support_pinweaver());
    tpm_supported_features_proto->set_support_runtime_selection(
        reply.support_runtime_selection());
    tpm_supported_features_proto->set_support_u2f(reply.support_u2f());
  } else {
    LOG(WARNING) << "Failed to get supported features.";
  }

  RunCallbackIfComplete();
}

void TpmStatusCombiner::RunCallbackIfComplete() {
  if (!has_tpm_status_ || !has_enrollment_status_ ||
      !has_dictionary_attack_info_ || !has_supported_features_)
    return;
  std::move(callback_).Run(tpm_status_info_);
}

}  // namespace policy
