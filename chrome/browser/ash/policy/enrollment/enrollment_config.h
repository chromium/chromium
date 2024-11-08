// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_CONFIG_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_CONFIG_H_

#include <string>

#include "base/files/file_path.h"

class PrefService;

namespace ash {
class InstallAttributes;
class OobeConfiguration;
namespace system {
class StatisticsProvider;
}
}  // namespace ash

namespace policy {

// An enumeration of different enrollment licenses.
// Constants that should be in sync with `OobeTypes.LicenseType`.
enum class LicenseType {
  kNone = 0,
  kEnterprise = 1,
  kEducation = 2,
  kTerminal = 3
};

// Source of OOBE config, if the device has an OOBE configuration file and
// that config influences enrollment.
enum class OOBEConfigSource {
  // No OOBE config source field (or no OOBE config at all, if this isn't a type
  // of enrollment driven by OOBE config).
  kNone = 0,
  // Unrecognized content present in source field.
  kUnknown = 1,
  // OOBE config was created during remote deployment Flex installation.
  kRemoteDeployment = 2,
  // OOBE config was written to disk image by the (Flex) image packaging tool,
  // prior to installation.
  kPackagingTool = 3,
};

// A container keeping all parameters relevant to whether and how enterprise
// enrollment of a device should occur. This configures the behavior of the
// enrollment flow during OOBE, i.e. whether the enrollment screen starts
// automatically, whether the user can skip enrollment, and what domain to
// display as owning the device.
struct EnrollmentConfig {
  // Describes the enrollment mode, i.e. what triggered enrollment.
  enum Mode {
    // Enrollment not applicable.
    MODE_NONE = 0,
    // Manually triggered initial enrollment.
    MODE_MANUAL = 1,
    // Manually triggered re-enrollment.
    MODE_MANUAL_REENROLLMENT = 2,
    // Forced enrollment triggered by local OEM manifest or device requisition,
    // user can't skip.
    MODE_LOCAL_FORCED = 3,
    // Advertised enrollment triggered by local OEM manifest or device
    // requisition, user can skip.
    MODE_LOCAL_ADVERTISED = 4,
    // Server-backed-state-triggered forced enrollment, user can't skip.
    MODE_SERVER_FORCED = 5,
    // Server-backed-state-triggered advertised enrollment, user can skip.
    MODE_SERVER_ADVERTISED = 6,
    // Recover from "spontaneous unenrollment", user can't skip.
    MODE_RECOVERY = 7,
    // Start attestation-based enrollment.
    MODE_ATTESTATION = 8,
    // Start attestation-based enrollment and only uses that.
    MODE_ATTESTATION_LOCAL_FORCED = 9,
    // Server-backed-state-triggered attestation-based enrollment, user can't
    // skip.
    MODE_ATTESTATION_SERVER_FORCED = 10,
    // Forced enrollment triggered as a fallback to attestation re-enrollment,
    // user can't skip.
    MODE_ATTESTATION_MANUAL_FALLBACK = 11,

    // DEPRECATED_MODE_OFFLINE_DEMO = 12,
    // DEPRECATED_MODE_ENROLLED_ROLLBACK = 13,

    MODE_INITIAL_SERVER_FORCED = 14,
    // Server-backed-state-triggered attestation-based initial enrollment,
    // user can't skip.
    MODE_ATTESTATION_INITIAL_SERVER_FORCED = 15,
    // Forced enrollment triggered as a fallback to attestation initial
    // enrollment, user can't skip.
    MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK = 16,
    // An enterprise rollback just took place and the device was wiped.
    // Attempt to re-enroll with attestation. This is forced from the
    // client side. Cannot be skipped.
    MODE_ATTESTATION_ROLLBACK_FORCED = 17,
    // An enterprise rollback just took place and the device was wiped.
    // Attestation re-enrollment just failed, attempt manual enrollment as
    // fallback. Cannot be skipped.
    MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK = 18,
    // Forced initial enrollment triggered after presenting an enrollment
    // token (for Flex Auto Enrollment) to the server. Cannot be skipped.
    MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED = 19,
    // Forced manual enrollment triggered as a fallback to a failed
    // token-based enrollment. Cannot be skipped.
    MODE_ENROLLMENT_TOKEN_INITIAL_MANUAL_FALLBACK = 20,
  };

  // An enumeration of assigned upgrades that a device can after initial
  // enrollment.
  enum class AssignedUpgradeType {
    // Unspecified Upgrade
    kAssignedUpgradeTypeUnspecified = 0,
    // Chrome Enterprise Upgrade
    kAssignedUpgradeTypeChromeEnterprise = 1,
    // Kiosk & Signage Upgrade
    kAssignedUpgradeTypeKioskAndSignage = 2,
  };

  // Get the enrollment configuration that has been set up via signals such as
  // device requisition, OEM manifest, pre-existing installation-time attributes
  // or server-backed state retrieval. The configuration is stored in |config|,
  // |config.mode| will be MODE_NONE if there is no prescribed configuration.
  // |config.management_domain| will contain the domain the device is supposed
  // to be enrolled to as decided by factors such as forced re-enrollment,
  // enrollment recovery, or already-present install attributes. Note that
  // |config.management_domain| may be non-empty even if |config.mode| is
  // MODE_NONE.
  // |statistics_provider| would also be const if it had const access methods.
  // May alter the enrollment recovery flag in local state if it discovers
  // inconsistency there.
  static EnrollmentConfig GetPrescribedEnrollmentConfig();
  static EnrollmentConfig GetPrescribedEnrollmentConfig(
      PrefService* local_state,
      const ash::InstallAttributes& install_attributes,
      ash::system::StatisticsProvider* statistics_provider,
      const ash::OobeConfiguration* oobe_configuration);

  static EnrollmentConfig GetDemoModeEnrollmentConfig();

  EnrollmentConfig();
  EnrollmentConfig(const EnrollmentConfig& config);
  ~EnrollmentConfig();

  // Whether enrollment should be triggered.
  bool should_enroll() const { return mode != MODE_NONE; }

  // Whether we fell back into manual enrollment.
  bool is_manual_fallback() const {
    return mode == MODE_ATTESTATION_MANUAL_FALLBACK ||
           mode == MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK ||
           mode == MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK ||
           mode == MODE_ENROLLMENT_TOKEN_INITIAL_MANUAL_FALLBACK;
  }

  // Whether enrollment is forced. The user can't skip the enrollment step
  // during OOBE if this returns true.
  bool is_forced() const {
    return mode == MODE_LOCAL_FORCED || mode == MODE_SERVER_FORCED ||
           mode == MODE_ATTESTATION_LOCAL_FORCED ||
           mode == MODE_ATTESTATION_SERVER_FORCED ||
           mode == MODE_INITIAL_SERVER_FORCED ||
           mode == MODE_ATTESTATION_INITIAL_SERVER_FORCED ||
           mode == MODE_ATTESTATION_ROLLBACK_FORCED || mode == MODE_RECOVERY ||
           mode == MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED ||
           is_manual_fallback();
  }

  // Whether this configuration is in attestation mode per server request.
  bool is_mode_attestation_server() const {
    return mode == MODE_ATTESTATION_SERVER_FORCED ||
           mode == MODE_ATTESTATION_INITIAL_SERVER_FORCED;
  }

  // Whether this configuration is in initial attestation forced mode per server
  // request.
  bool is_mode_initial_attestation_server_forced() const {
    return mode == MODE_ATTESTATION_INITIAL_SERVER_FORCED;
  }

  // Whether this configuration is in attestation mode per client request.
  bool is_mode_attestation_client() const {
    return mode == MODE_ATTESTATION || mode == MODE_ATTESTATION_LOCAL_FORCED ||
           mode == MODE_ATTESTATION_ROLLBACK_FORCED;
  }

  // Whether this configuration is in attestation mode.
  bool is_mode_attestation() const {
    return is_mode_attestation_client() || is_mode_attestation_server();
  }

  // Whether this configuration is in token-based mode.
  bool is_mode_token() const {
    return mode == MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED;
  }

  // Whether this configuration's mode causes the device to automatically
  // enroll without user interaction.
  bool is_automatic_enrollment() const {
    return is_mode_attestation() || is_mode_token();
  }

  // Whether this configuration is an automatic enrollment mode that has a
  // manual fallback. I.e. after a failed attempt at automatic enrolling,
  // manual enrollment will be triggered.
  bool is_mode_with_manual_fallback() const {
    return is_mode_attestation_server() ||
           mode == MODE_ATTESTATION_ROLLBACK_FORCED || is_mode_token();
  }

  // Whether this configuration is in OAuth mode.
  bool is_mode_oauth() const {
    return mode != MODE_NONE && !is_automatic_enrollment();
  }

  constexpr bool operator==(const EnrollmentConfig& other) const = default;

  // Returns an effective config that should be used for upcoming enrollment:
  // * Returns prescribed config if the current one prescribes an enrollment.
  // * Returns manual enrollment config, if the current one does not prescribe
  // enrollment.
  EnrollmentConfig GetEffectiveConfig() const;

  // Returns a manual fallback config corresponding to the given automatic
  // enrollment config.
  EnrollmentConfig GetManualFallbackConfig() const;

  // Indicates the enrollment flow variant to trigger during OOBE.
  Mode mode = MODE_NONE;

  // The domain to enroll the device to, if applicable. If this is not set, the
  // device may be enrolled to any domain. Note that for the case where the
  // device is not already locked to a certain domain, this value is used for
  // display purposes only and the server makes the final decision on which
  // domain the device should be enrolled with. If the device is already locked
  // to a domain, policy validation during enrollment will verify the domains
  // match.
  std::string management_domain;

  // Is a license packaged with device or not.
  bool is_license_packaged_with_device = false;

  // Which type of license device has.
  LicenseType license_type = LicenseType::kNone;

  // The assigned upgrade for a device after initial enrollment. Chrome
  // Enterprise Upgrade is the default upgrade for ZTE devices, unless other is
  // specified in the server-backed initial state retrieval.
  AssignedUpgradeType assigned_upgrade_type =
      AssignedUpgradeType::kAssignedUpgradeTypeChromeEnterprise;

  // User's email which can be passed from the Gaia screen in the enrollment
  // nudge flow.
  std::string enrollment_nudge_email;

  // Enrollment token to use for authentication (for Flex Auto Enrollment).
  std::string enrollment_token;

  // Source of OOBE config, if the device has an OOBE configuration file and
  // that config influences enrollment.
  OOBEConfigSource oobe_config_source = OOBEConfigSource::kNone;

 private:
  // Hold fields to be filled corresponding to ones in `EnrollmentConfig`.
  struct PrescribedConfig;
  struct PrescribedLicense;

  EnrollmentConfig(PrescribedConfig prescribed_config,
                   PrescribedLicense prescribed_license);
};

std::ostream& operator<<(std::ostream& os, const EnrollmentConfig::Mode& mode);

std::ostream& operator<<(std::ostream& os, const EnrollmentConfig& config);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_CONFIG_H_
