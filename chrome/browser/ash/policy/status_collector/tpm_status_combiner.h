// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_TPM_STATUS_COMBINER_H_
#define CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_TPM_STATUS_COMBINER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/policy/status_collector/device_status_collector.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

// This class is meant to combine multiple statuses around TPM and enrollment.
// Because the result `enterprise_management::TpmStatusInfo` comes from
// different sources of D-Bus calls, this class is designed to be used as a
// shared pointer that resides in multiple callbacks. When all the replies of
// the D-Bus calls, which are sent by the user of this class, are finished, this
// class combines the results into a single `enterprise_policy::TpmStatusInfo`
// and get destroyed naturally when all the callbacks of the D-Bus calls are
// done.
//
// Note that in order to increase test coverage of `DeviceStatusCollector`, this
// class doesn't have its own unittest; instead, it is tested along with
// `DeviceStatusCollector` end-to-end.
class TpmStatusCombiner : public base::RefCounted<TpmStatusCombiner> {
 public:
  // The passed `callback` is invoked when all the D-Bus responses of
  // interest are received.
  explicit TpmStatusCombiner(DeviceStatusCollector::TpmStatusReceiver callback);

  // Not copyable or movable.
  TpmStatusCombiner(const TpmStatusCombiner&) = delete;
  TpmStatusCombiner& operator=(const TpmStatusCombiner&) = delete;
  TpmStatusCombiner(TpmStatusCombiner&&) = delete;
  TpmStatusCombiner& operator=(TpmStatusCombiner&&) = delete;

  // Designed to be the callback of
  // `TpmManagerClient::GetTpmNonsensitiveStatus()`.
  void OnGetTpmStatus(
      const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply);
  // Designed to be the callback of `AttestationClient::GetStatus()`.
  void OnGetEnrollmentStatus(const ::attestation::GetStatusReply& reply);
  // Designed to be the callback of
  // `AttestationClient::GetDictionaryAttackInfoReply()`.
  void OnGetDictionaryAttackInfo(
      const ::tpm_manager::GetDictionaryAttackInfoReply& reply);
  // Designed to be the callback of
  // `TpmManagerClient::OnGetSupportedFeatures()`.
  void OnGetSupportedFeatures(
      const ::tpm_manager::GetSupportedFeaturesReply& reply);

 private:
  // `RefCounted` subclass requires the destructor to be non-public.
  friend class base::RefCounted<TpmStatusCombiner>;
  ~TpmStatusCombiner();

  // Called when receiving any D-Bus response. If it's the last D-Bus response
  // we expect to handle, runs the callback passed in
  void RunCallbackIfComplete();

  // Invoked when all D-Bus response are handled.
  DeviceStatusCollector::TpmStatusReceiver callback_;

  // The combined result passed into `callback_`.
  enterprise_management::TpmStatusInfo tpm_status_info_;

  // Indicates each D-Bus response being received or not.
  bool has_tpm_status_ = false;
  bool has_enrollment_status_ = false;
  bool has_dictionary_attack_info_ = false;
  bool has_supported_features_ = false;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_STATUS_COLLECTOR_TPM_STATUS_COMBINER_H_
