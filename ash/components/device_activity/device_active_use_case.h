// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_USE_CASE_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_USE_CASE_H_

#include "ash/components/device_activity/fresnel_service.pb.h"
#include "base/component_export.h"
#include "base/time/time.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

class PrefService;

namespace chromeos {
namespace system {
class StatisticsProvider;
}  // namespace system
}  // namespace chromeos

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash {
namespace device_activity {

// Fields used in setting device active metadata, that are explicitly
// required from outside of ASH_CHROME due to the dependency limitations
// on chrome browser.
struct COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY) ChromeDeviceMetadataParameters {
  version_info::Channel chromeos_channel;
  MarketSegment market_segment;
};

// Base class for device active use cases.
class COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY) DeviceActiveUseCase {
 public:
  DeviceActiveUseCase(
      const std::string& psm_device_active_secret,
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      const std::string& use_case_pref_key,
      private_membership::rlwe::RlweUseCase psm_use_case,
      PrefService* local_state);
  DeviceActiveUseCase(const DeviceActiveUseCase&) = delete;
  DeviceActiveUseCase& operator=(const DeviceActiveUseCase&) = delete;
  virtual ~DeviceActiveUseCase();

  // Generate the window identifier for the use case.
  virtual std::string GenerateUTCWindowIdentifier(base::Time ts) const = 0;

  // Generate Fresnel PSM import request body.
  // This will create the device metadata dimensions sent by PSM import by use
  // case.
  //
  // Important: Each new dimension added to metadata will need to be approved by
  // privacy.
  virtual ImportDataRequest GenerateImportRequestBody() = 0;

  PrefService* GetLocalState() const;

  // Return the last known ping timestamp from local state pref, by use case.
  // For example, the monthly use case will return the last known monthly
  // timestamp from the local state pref.
  base::Time GetLastKnownPingTimestamp() const;

  // Set the last known ping timestamp in local state pref.
  void SetLastKnownPingTimestamp(base::Time new_ts);

  // Return true if the |use_case_pref_key_| is not Unix Epoch (default value).
  bool IsLastKnownPingTimestampSet() const;

  // Retrieve the PSM use case.
  // The PSM dataset on the serverside is segmented by the PSM use case.
  private_membership::rlwe::RlweUseCase GetPsmUseCase() const;

  absl::optional<std::string> GetWindowIdentifier() const;

  // Method also assigns |psm_id_| to absl::nullopt so that subsequent calls to
  // GetPsmIdentifier use the updated window id value.
  void SetWindowIdentifier(absl::optional<std::string> window_id);

  // Calculates an HMAC of |message| using |key|, encoded as a hexadecimal
  // string. Return empty string if HMAC fails.
  std::string GetDigestString(const std::string& key,
                              const std::string& message) const;

  absl::optional<private_membership::rlwe::RlwePlaintextId> GetPsmIdentifier();

  void SetPsmIdentifier(
      absl::optional<private_membership::rlwe::RlwePlaintextId> psm_id);

  // Returns memory address to the |psm_rlwe_client_| unique pointer, or null if
  // not set.
  private_membership::rlwe::PrivateMembershipRlweClient* GetPsmRlweClient();

  // Generated on demand each time the state machine leaves the idle state.
  // Client Generates protos used in request body of Oprf and Query requests.
  void SetPsmRlweClient(
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
          psm_rlwe_client);

  // Determine if a device ping is needed for a given device window.
  // Performing this check helps reduce QPS to the |CheckingMembership|
  // network requests.
  bool IsDevicePingRequired(base::Time new_ping_ts) const;

 protected:
  // Retrieve full hardware class from MachineStatistics.
  // |DeviceActivityController| waits for object to finish loading, to avoid
  // callback logic in this class.
  std::string GetFullHardwareClass() const;

  // Retrieve the ChromeOS major version number.
  std::string GetChromeOSVersion() const;

  // Retrieve the ChromeOS release channel.
  Channel GetChromeOSChannel() const;

  // Retrieve the ChromeOS device market segment.
  MarketSegment GetMarketSegment() const;

 private:
  // Field is used to identify a fixed window of time for device active
  // counting. Privacy compliance is guaranteed by retrieving the
  // |psm_device_active_secret_| from chromeos, and performing an additional
  // HMAC-SHA256 hash on generated plaintext string.
  //
  // Generated on demand each time the state machine leaves the idle state.
  // It is reused by several states. It is reset to nullopt.
  // This field is used apart of PSM Oprf, Query, and Import requests.
  absl::optional<private_membership::rlwe::RlwePlaintextId>
  GeneratePsmIdentifier() const;

  // The ChromeOS platform code will provide a derived PSM device active secret
  // via callback.
  //
  // This secret is used to generate a PSM identifier for the reporting window.
  const std::string psm_device_active_secret_;

  // Creates a copy of chrome parameters, which is owned throughout
  // |DeviceActiveUseCase| object lifetime.
  const ChromeDeviceMetadataParameters chrome_passed_device_params_;

  // Key used to query the local state pref for the last ping timestamp by use
  // case.
  // For example, the monthly use case will store the key mapping to the last
  // monthly ping timestamp in the local state pref.
  const std::string use_case_pref_key_;

  // The PSM dataset on the serverside is segmented by the PSM use case.
  const private_membership::rlwe::RlweUseCase psm_use_case_;

  // Update last stored device active ping timestamps for PSM use cases.
  // On powerwash/recovery update |local_state_| to the most recent timestamp
  // |CheckMembership| was performed, as |local_state_| gets deleted.
  // |local_state_| outlives the lifetime of this class.
  // Used local state prefs are initialized by |DeviceActivityController|.
  PrefService* const local_state_;

  // Singleton lives throughout class lifetime.
  chromeos::system::StatisticsProvider* const statistics_provider_;

  // Generated on demand each time the state machine leaves the idle state.
  // This field is used to know which window the psm id is used for.
  absl::optional<std::string> window_id_;

  // Generated on demand each time the state machine leaves the idle state.
  // It is reused by several states. It is reset to nullopt.
  // This field is used apart of PSM Oprf, Query, and Import requests.
  absl::optional<private_membership::rlwe::RlwePlaintextId> psm_id_;

  // Generated on demand each time the state machine leaves the idle state.
  // Client Generates protos used in request body of Oprf and Query requests.
  std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>
      psm_rlwe_client_;
};

}  // namespace device_activity
}  // namespace ash

#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_USE_CASE_H_
