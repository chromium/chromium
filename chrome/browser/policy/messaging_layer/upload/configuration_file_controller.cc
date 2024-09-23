// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/configuration_file_controller.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/byte_conversions.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/encryption/verification.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/status.h"
#include "components/version_info/version_info.h"

namespace reporting {
namespace {
// Checks if two lists contain the same destinations.
bool DestinationListsEqual(ListOfBlockedDestinations list_a,
                           ListOfBlockedDestinations list_b) {
  if (list_a.destinations_size() != list_b.destinations_size()) {
    return false;
  }
  return std::equal(list_a.destinations().begin(), list_a.destinations().end(),
                    list_b.destinations().begin());
}
}  // namespace

// Only used in testing. Feature for testing the configuration file in our
// automated tests. Not exposed on the UI.
BASE_FEATURE(kReportingConfigurationFileTestSignature,
             "ReportingConfigurationFileTestSignature",
             base::FEATURE_DISABLED_BY_DEFAULT);

ConfigurationFileController::~ConfigurationFileController() = default;

// The default constructor used for this class takes as a parameter a callback
// that sends information to missive after being verified by this class if
// needed. It also creates the signature verifier that will be used to verify
// the signature of the configuration file using the well-known prod key stored
// in Chrome.
ConfigurationFileController::ConfigurationFileController(
    UploadClient::UpdateConfigInMissiveCallback update_config_in_missive_cb)
    : ConfigurationFileController(std::move(update_config_in_missive_cb),
                                  ListOfBlockedDestinations(),
                                  version_info::GetMajorVersionNumberAsInt()) {}

int32_t ConfigurationFileController::HandleConfigurationFile(
    reporting::ConfigFile config_file) {
  // Sanity check to verify that the experiment is enabled.
  if (!base::FeatureList::IsEnabled(kShouldRequestConfigurationFile)) {
    // Returns -1 since this is not an actual error with the file provided.
    return kFeatureDisabled;
  }

  // Check if we got the same version that we were using already.
  // This might happen if the server returns the configuration file two times
  // with the same version because of two requests made close to each other, we
  // just ignore them if that's the case and return the current version. We also
  // just ignore the config file if the version is negative.
  if (config_file.version() == current_config_file_version_ ||
      config_file.version() < 0) {
    return current_config_file_version_;
  }

  // Verify signature.
  const auto signature_verification_status = VerifySignature(config_file);
  if (!signature_verification_status.ok()) {
    base::UmaHistogramEnumeration(
        "Browser.ERP.ConfigFileSignatureVerificationError",
        signature_verification_status.code(), error::Code::MAX_VALUE);
    return kConfigurationFileCorrupted;
  }

  // Check if we should send the list of blocked destinations to missive.
  if (HandleBlockedEventConfigs(config_file.blocked_event_configs())) {
    update_config_in_missive_cb_.Run(destinations_list_);
  }

  // If we reached here it means that we just got a new version of the config
  // file from the server so we update the version being sent on the payload and
  // the one stored by this class.
  current_config_file_version_ = config_file.version();
  return current_config_file_version_;
}

Status ConfigurationFileController::VerifySignature(ConfigFile config_file) {
  // We don't check the signature in our tests. This flag is only used inside
  // the tast tests.
  if (base::FeatureList::IsEnabled(kReportingConfigurationFileTestSignature)) {
    return Status::StatusOK();
  }

  // Sanity checks, this should never fail.
  if (!config_file.has_version()) {
    return Status{error::INVALID_ARGUMENT,
                  "Missing version information for config file"};
  }

  if (!config_file.has_config_file_signature()) {
    return Status{error::INVALID_ARGUMENT,
                  "Missing signature information for config file"};
  }

  // Verify the value signed on the server using the big-endian representation
  // of the configuration file version.
  return verifier_.Verify(
      base::as_string_view(base::U32ToBigEndian(config_file.version())),
      config_file.config_file_signature());
}

// static
std::unique_ptr<ConfigurationFileController>
ConfigurationFileController::CreateForTesting(
    UploadClient::UpdateConfigInMissiveCallback update_config_in_missive_cb,
    ListOfBlockedDestinations destinations_list,
    int os_version) {
  return base::WrapUnique(new ConfigurationFileController(
      std::move(update_config_in_missive_cb), std::move(destinations_list),
      os_version));
}

ConfigurationFileController::ConfigurationFileController(
    UploadClient::UpdateConfigInMissiveCallback update_config_in_missive_cb,
    ListOfBlockedDestinations destinations_list,
    int os_version)
    : update_config_in_missive_cb_(std::move(update_config_in_missive_cb)),
      verifier_(SignatureVerifier(SignatureVerifier::VerificationKey())),
      destinations_list_(std::move(destinations_list)),
      current_os_version_(os_version) {}

bool ConfigurationFileController::HandleBlockedEventConfigs(
    google::protobuf::RepeatedPtrField<EventConfig> blocked_event_configs) {
  // If the incoming list is empty and the stored list is also empty
  // we return false to signal to the parent function that it should not
  // send the list to missive.
  if (blocked_event_configs.empty() &&
      destinations_list_.destinations().empty()) {
    return false;
  }

  // If the configuration fetched from the server is empty and the stored list
  // is not empty we return true to signal to the parent function that it
  // should update the list in missive to an empty one.
  if (blocked_event_configs.empty() &&
      !destinations_list_.destinations().empty()) {
    destinations_list_ = ListOfBlockedDestinations();
    return true;
  }

  ListOfBlockedDestinations current_list;
  //  Check all the destinations, if they have version information
  //  we check whether it should be blocked or not.
  for (const auto& current_event : blocked_event_configs) {
    // If there is no minimum/maximum release version specified we add it
    // directly since it should be blocked on all versions.
    if (!current_event.has_minimum_release_version()) {
      current_list.add_destinations(current_event.destination());
      continue;
    }

    // Check if it should be blocked for the current version or not.
    std::optional<int32_t> maximum_version;
    maximum_version =
        current_event.has_maximum_release_version()
            ? std::optional<int32_t>(current_event.maximum_release_version())
            : std::nullopt;
    if (ShouldBeBlocked(current_event.minimum_release_version(),
                        maximum_version)) {
      current_list.add_destinations(current_event.destination());
    }
  }

  // Compare to see if the destinations list that we have right now is the same
  // as the one that is incoming from the server, if not then we swap the lists
  // and we notify the parent function that it should send the new list to
  // missive.
  if (DestinationListsEqual(current_list, destinations_list_)) {
    return false;
  }

  destinations_list_ = std::move(current_list);
  return true;
}

bool ConfigurationFileController::ShouldBeBlocked(
    int32_t minimum_version,
    std::optional<int32_t> maximum_version) const {
  if (maximum_version.has_value()) {
    return current_os_version_ >= minimum_version &&
           current_os_version_ <= maximum_version.value();
  }
  return current_os_version_ >= minimum_version;
}

}  // namespace reporting
