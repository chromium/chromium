// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_AUTHENTICATOR_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_AUTHENTICATOR_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "crypto/hmac.h"

namespace ash {
namespace parent_access {

// Configuration used to generate and verify parent access code.
class AccessCodeConfig {
 public:
  // Returns AccessCodeConfig created from a |dictionary|, if the |dictionary|
  // contains valid config data.
  static std::optional<AccessCodeConfig> FromDictionary(
      const base::Value::Dict& dictionary);

  // TODO(agawronska): Make constructor private.
  // To create valid AccessCodeConfig:
  // * |shared_secret| cannot be empty
  // * |code_validity| needs to be in between 30s and 3600s
  // * |clock_drift_tolerance| needs to be between 0 and 1800s
  // The above restrictions are applied to AccessCodeConfig policy that is the
  // main source of this configuration.
  AccessCodeConfig(const std::string& shared_secret,
                   base::TimeDelta code_validity,
                   base::TimeDelta clock_drift_tolerance);
  AccessCodeConfig(AccessCodeConfig&&);
  AccessCodeConfig& operator=(AccessCodeConfig&&);

  AccessCodeConfig(const AccessCodeConfig&) = delete;
  AccessCodeConfig& operator=(const AccessCodeConfig&) = delete;

  ~AccessCodeConfig();

  // Secret shared between child and parent devices.
  const std::string& shared_secret() const { return shared_secret_; }

  // Time that access code is valid for.
  base::TimeDelta code_validity() const { return code_validity_; }

  // The allowed difference between the clock on child and parent devices.
  base::TimeDelta clock_drift_tolerance() const {
    return clock_drift_tolerance_;
  }

  // Converts the AccessCodeConfig object to its dictionary equivalent.
  base::Value::Dict ToDictionary() const;

 private:
  std::string shared_secret_;
  base::TimeDelta code_validity_;
  base::TimeDelta clock_drift_tolerance_;
};

// Parent access code that can be used to authorize various actions on child
// user's device.
// Typical lifetime of the code is 10 minutes and clock difference between
// generating and validating device is half of the code lifetime. Clock
// difference is accounted for during code validation.
class AccessCode {
 public:
  // To create valid AccessCode:
  // * |code| needs to be 6 characters long
  // * |valid_to| needs to be greater than |valid_from|
  AccessCode(const std::string& code,
             base::Time valid_from,
             base::Time valid_to);
  AccessCode(const AccessCode&);
  AccessCode& operator=(const AccessCode&);
  ~AccessCode();

  // Parent access code.
  const std::string& code() const { return code_; }

  // Code validity start time.
  base::Time valid_from() const { return valid_from_; }

  // Code expiration time.
  base::Time valid_to() const { return valid_to_; }

  bool operator==(const AccessCode&) const;
  bool operator!=(const AccessCode&) const;
  friend std::ostream& operator<<(std::ostream&, const AccessCode&);

 private:
  std::string code_;
  base::Time valid_from_;
  base::Time valid_to_;
};

// Generates and validates parent access codes.
// Does not support timestamp from before Unix Epoch.
class Authenticator {
 public:
  // Granularity of which generation and verification of access code are carried
  // out. Should not exceed code validity period.
  static constexpr base::TimeDelta kAccessCodeGranularity = base::Minutes(1);

  explicit Authenticator(AccessCodeConfig config);

  Authenticator(const Authenticator&) = delete;
  Authenticator& operator=(const Authenticator&) = delete;

  ~Authenticator();

  // Generates parent access code from the given |timestamp|. Returns the code
  // if generation was successful. |timestamp| needs to be greater or equal Unix
  // Epoch.
  std::optional<AccessCode> Generate(base::Time timestamp) const;

  // Returns AccessCode structure with validity information, if |code| is
  // valid for the given timestamp. |timestamp| needs to be greater or equal
  // Unix Epoch.
  std::optional<AccessCode> Validate(const std::string& code,
                                     base::Time timestamp) const;

 private:
  // Returns AccessCode structure with validity information, if |code| is valid
  // for the range [|valid_from|, |valid_to|). |valid_to| needs to be greater or
  // equal to |valid_from|. |valid_from| needs to be greater or equal Unix
  // Epoch.
  std::optional<AccessCode> ValidateInRange(const std::string& code,
                                            base::Time valid_from,
                                            base::Time valid_to) const;

  // Configuration used to generate and validate parent access code.
  const AccessCodeConfig config_;

  // Keyed-hash message authentication generator.
  crypto::HMAC hmac_{crypto::HMAC::SHA1};
};

}  // namespace parent_access
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_AUTHENTICATOR_H_
