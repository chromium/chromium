// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_CONFIGURATION_FILE_CONTROLLER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_CONFIGURATION_FILE_CONTROLLER_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "components/reporting/encryption/verification.h"
#include "components/reporting/proto/synced/configuration_file.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"

namespace reporting {

// Used to let the server know that the feature is disabled on the client side.
static constexpr int32_t kFeatureDisabled = -1;
// Used to let the server know that there is something wrong with the
// configuration file.
static constexpr int32_t kConfigurationFileCorrupted = -2;

BASE_DECLARE_FEATURE(kReportingConfigurationFileTestSignature);
class ConfigurationFileControllerTest;

class ConfigurationFileController {
 public:
  explicit ConfigurationFileController(
      UploadClient::UpdateConfigInMissiveCallback update_config_in_missive_cb);

  ConfigurationFileController(const ConfigurationFileController&) = delete;
  ConfigurationFileController& operator=(const ConfigurationFileController&) =
      delete;

  ~ConfigurationFileController();

  // Method called via callbacks from `RecordHandlerImpl` that handles the
  // incoming configuration file and performs the necessary checks to see if
  // the data needs to be passed to other places. This method returns the
  // version if it passed the checks or -2 if it didn't, this will signal the
  // server that something went wrong while verifying the config file provided.
  int32_t HandleConfigurationFile(ConfigFile config_file);

 private:
  friend class ConfigurationFileControllerTest;
  // Private constructors used for testing.
  static std::unique_ptr<ConfigurationFileController> CreateForTesting(
      UploadClient::UpdateConfigInMissiveCallback update_config_in_missive_cb,
      ListOfBlockedDestinations destinations_list,
      int os_version);

  ConfigurationFileController(
      UploadClient::UpdateConfigInMissiveCallback update_config_in_missive_cb,
      ListOfBlockedDestinations destinations_list,
      int os_version);

  //    Verifies the signature from the configuration file. Returns OK if the
  //    signature is valid, otherwise returns an error.
  Status VerifySignature(ConfigFile config_file);
  // Method that checks the destination list gotten from the server and checks
  // if the list is the same one as the one stored. Returns true if the list
  // should be sent to missive and false otherwise.
  bool HandleBlockedEventConfigs(
      google::protobuf::RepeatedPtrField<EventConfig> blocked_event_configs);
  // Method that checks if a metric or destination should be blocked for the
  // current OS version that the device is on at the moment. Returns true if the
  // metric or destination should be blocked and false otherwise.
  bool ShouldBeBlocked(int32_t minimum_version,
                       std::optional<int32_t> maximum_version) const;

  // Callback to update the configuration file in missive, only called
  // after verifying
  UploadClient::UpdateConfigInMissiveCallback update_config_in_missive_cb_;
  // Signature verifier used to verify that the configuration file gotten
  // from the server is valid. Initialized with the well-known public signature
  // verification key.
  SignatureVerifier verifier_;
  // List of blocked destinations, used to see if the incoming list of blocked
  // destinations should be sent to missive. This list is already filtered to
  // block/not block depending on the OS version of the device.
  ListOfBlockedDestinations destinations_list_;
  //  Populated during the creation of the class, stores the current major
  //  ChromeOS version number.
  int current_os_version_;
  // Stores the latest configuration file version gotten from the server.
  int32_t current_config_file_version_ = 0;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_CONFIGURATION_FILE_CONTROLLER_H_
