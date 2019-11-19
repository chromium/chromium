// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/authenticator.h"

#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace chromeos {
namespace parent_access {

namespace {

// Value ranges for access code config data.
constexpr base::TimeDelta kMinCodeValidity = base::TimeDelta::FromSeconds(60);
constexpr base::TimeDelta kMaxCodeValidity = base::TimeDelta::FromMinutes(60);
constexpr base::TimeDelta kMinClockDriftTolerance =
    base::TimeDelta::FromMinutes(0);
constexpr base::TimeDelta kMaxClockDriftTolerance =
    base::TimeDelta::FromMinutes(30);

// Dictionary keys used to serialize access code config data.
constexpr char kSharedSecretDictKey[] = "shared_secret";
constexpr char kCodeValidityDictKey[] = "access_code_ttl";
constexpr char kClockDriftDictKey[] = "clock_drift_tolerance";

}  // namespace

// static
base::Optional<AccessCodeConfig> AccessCodeConfig::FromDictionary(
    const base::DictionaryValue& dict) {
  const std::string* secret = dict.FindStringKey(kSharedSecretDictKey);
  if (!secret || secret->empty())
    return base::nullopt;

  base::Optional<int> validity = dict.FindIntKey(kCodeValidityDictKey);
  if (!(validity.has_value() && *validity >= kMinCodeValidity.InSeconds() &&
        *validity <= kMaxCodeValidity.InSeconds())) {
    return base::nullopt;
  }

  base::Optional<int> clock_drift = dict.FindIntKey(kClockDriftDictKey);
  if (!(clock_drift.has_value() &&
        *clock_drift >= kMinClockDriftTolerance.InSeconds() &&
        *clock_drift <= kMaxClockDriftTolerance.InSeconds())) {
    return base::nullopt;
  }

  return AccessCodeConfig(*secret, base::TimeDelta::FromSeconds(*validity),
                          base::TimeDelta::FromSeconds(*clock_drift));
}

AccessCodeConfig::AccessCodeConfig(const std::string& shared_secret,
                                   base::TimeDelta code_validity,
                                   base::TimeDelta clock_drift_tolerance)
    : shared_secret_(shared_secret),
      code_validity_(code_validity),
      clock_drift_tolerance_(clock_drift_tolerance) {
  DCHECK(!shared_secret_.empty());
  DCHECK(code_validity_ >= kMinCodeValidity);
  DCHECK(code_validity_ <= kMaxCodeValidity);
  DCHECK(clock_drift_tolerance_ >= kMinClockDriftTolerance);
  DCHECK(clock_drift_tolerance_ <= kMaxClockDriftTolerance);
}

AccessCodeConfig::AccessCodeConfig(AccessCodeConfig&&) = default;

AccessCodeConfig& AccessCodeConfig::operator=(AccessCodeConfig&&) = default;

AccessCodeConfig::~AccessCodeConfig() = default;

base::Value AccessCodeConfig::ToDictionary() const {
  base::Value config(base::Value::Type::DICTIONARY);
  config.SetKey(kSharedSecretDictKey, base::Value(shared_secret_));
  config.SetKey(kCodeValidityDictKey,
                base::Value(static_cast<int>(code_validity_.InSeconds())));
  config.SetKey(
      kClockDriftDictKey,
      base::Value(static_cast<int>(clock_drift_tolerance_.InSeconds())));
  return config;
}

AccessCode::AccessCode(const std::string& code,
                       base::Time valid_from,
                       base::Time valid_to)
    : code_(code), valid_from_(valid_from), valid_to_(valid_to) {
  DCHECK_EQ(6u, code_.length());
  DCHECK_GT(valid_to_, valid_from_);
}

AccessCode::AccessCode(const AccessCode&) = default;

AccessCode& AccessCode::operator=(const AccessCode&) = default;

AccessCode::~AccessCode() = default;

bool AccessCode::operator==(const AccessCode& rhs) const {
  return code_ == rhs.code() && valid_from_ == rhs.valid_from() &&
         valid_to_ == rhs.valid_to();
}

bool AccessCode::operator!=(const AccessCode& rhs) const {
  return code_ != rhs.code() || valid_from_ != rhs.valid_from() ||
         valid_to_ != rhs.valid_to();
}

std::ostream& operator<<(std::ostream& out, const AccessCode& code) {
  return out << code.code() << " [" << code.valid_from() << " - "
             << code.valid_to() << "]";
}

// static
constexpr base::TimeDelta Authenticator::kAccessCodeGranularity;

Authenticator::Authenticator(AccessCodeConfig config)
    : config_(std::move(config)) {
  bool result = hmac_.Init(config_.shared_secret());
  DCHECK(result);
}

Authenticator::~Authenticator() = default;

base::Optional<AccessCode> Authenticator::Generate(base::Time timestamp) const {
  DCHECK_LE(base::Time::UnixEpoch(), timestamp);

  // We find the beginning of the interval for the given timestamp and adjust by
  // the granularity.
  const int64_t interval =
      timestamp.ToJavaTime() / config_.code_validity().InMilliseconds();
  const int64_t interval_beginning_timestamp =
      interval * config_.code_validity().InMilliseconds();
  const int64_t adjusted_timestamp =
      interval_beginning_timestamp / kAccessCodeGranularity.InMilliseconds();

  // The algorithm for PAC generation is using data in Big-endian byte order to
  // feed HMAC.
  std::string big_endian_timestamp(sizeof(adjusted_timestamp), 0);
  base::WriteBigEndian(&big_endian_timestamp[0], adjusted_timestamp);

  std::vector<uint8_t> digest(hmac_.DigestLength());
  if (!hmac_.Sign(big_endian_timestamp, &digest[0], digest.size())) {
    LOG(ERROR) << "Signing HMAC data to generate Parent Access Code failed";
    return base::nullopt;
  }

  // Read 4 bytes in Big-endian order starting from |offset|.
  const int8_t offset = digest.back() & 0xf;
  int32_t result;
  std::vector<uint8_t> slice(digest.begin() + offset,
                             digest.begin() + offset + sizeof(result));
  base::ReadBigEndian(reinterpret_cast<char*>(slice.data()), &result);
  // Clear sign bit.
  result &= 0x7fffffff;

  const base::Time valid_from =
      base::Time::FromJavaTime(interval_beginning_timestamp);
  return AccessCode(base::StringPrintf("%06d", result % 1000000), valid_from,
                    valid_from + config_.code_validity());
}

base::Optional<AccessCode> Authenticator::Validate(const std::string& code,
                                                   base::Time timestamp) const {
  DCHECK_LE(base::Time::UnixEpoch(), timestamp);

  base::Time valid_from = timestamp - config_.clock_drift_tolerance();
  if (valid_from < base::Time::UnixEpoch())
    valid_from = base::Time::UnixEpoch();
  return ValidateInRange(code, valid_from,
                         timestamp + config_.clock_drift_tolerance());
}

base::Optional<AccessCode> Authenticator::ValidateInRange(
    const std::string& code,
    base::Time valid_from,
    base::Time valid_to) const {
  DCHECK_LE(base::Time::UnixEpoch(), valid_from);
  DCHECK_GE(valid_to, valid_from);

  const int64_t start_interval =
      valid_from.ToJavaTime() / kAccessCodeGranularity.InMilliseconds();
  const int64_t end_interval =
      valid_to.ToJavaTime() / kAccessCodeGranularity.InMilliseconds();
  for (int i = start_interval; i <= end_interval; ++i) {
    const base::Time generation_timestamp =
        base::Time::FromJavaTime(i * kAccessCodeGranularity.InMilliseconds());
    base::Optional<AccessCode> pac = Generate(generation_timestamp);
    if (pac.has_value() && pac->code() == code)
      return pac;
  }
  return base::nullopt;
}

}  // namespace parent_access
}  // namespace chromeos
