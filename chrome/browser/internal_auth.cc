// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/internal_auth.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "crypto/hmac.h"

namespace {

typedef std::map<std::string, std::string> VarValueMap;

// Size of a tick in microseconds. This determines upper bound for average
// number of passports generated per time unit. This bound equals to
// (kMicrosecondsPerSecond / TickUs) calls per second.
const int64_t kTickUs = 10000;

// Verification window size in ticks; that means any passport expires in
// (kVerificationWindowTicks * TickUs / kMicrosecondsPerSecond) seconds.
const int kVerificationWindowTicks = 2000;

// Generation window determines how well we are able to cope with bursts of
// GeneratePassport calls those exceed upper bound on average speed.
const int kGenerationWindowTicks = 20;

// Makes no sense to compare other way round.
static_assert(kGenerationWindowTicks <= kVerificationWindowTicks,
    "generation window should not be larger than the verification window");
// We are not optimized for high value of kGenerationWindowTicks.
static_assert(kGenerationWindowTicks < 30,
    "generation window should not be too large");

// Regenerate key after this number of ticks.
const int kKeyRegenerationSoftTicks = 500000;
// Reject passports if key has not been regenerated in that number of ticks.
const int kKeyRegenerationHardTicks = kKeyRegenerationSoftTicks * 2;

// Limit for number of accepted var=value pairs. Feel free to bump this limit
// higher once needed.
const size_t kVarsLimit = 16;

// Limit for length of caller-supplied strings. Feel free to bump this limit
// higher once needed.
const size_t kStringLengthLimit = 512;

// Character used as a separator for construction of message to take HMAC of.
// It is critical to validate all caller-supplied data (used to construct
// message) to be clear of this separator because it could allow attacks.
const char kItemSeparator = '\n';

// Character used for var=value separation.
const char kVarValueSeparator = '=';

const size_t kKeySizeInBytes = 128 / 8;
const size_t kHMACSizeInBytes = 256 / 8;

// Length of base64 string required to encode given number of raw octets.
#define BASE64_PER_RAW(X) (X > 0 ? ((X - 1) / 3 + 1) * 4 : 0)

// Size of decimal string representing 64-bit tick.
const size_t kTickStringLength = 20;

// A passport consists of 2 parts: HMAC and tick.
const size_t kPassportSize =
    BASE64_PER_RAW(kHMACSizeInBytes) + kTickStringLength;

int64_t GetCurrentTick() {
  int64_t tick = base::Time::Now().ToInternalValue() / kTickUs;
  if (tick < kVerificationWindowTicks || tick < kKeyRegenerationHardTicks ||
      tick > std::numeric_limits<int64_t>::max() - kKeyRegenerationHardTicks) {
    return 0;
  }
  return tick;
}

bool IsDomainSane(const std::string& domain) {
  return !domain.empty() &&
      domain.size() <= kStringLengthLimit &&
      base::IsStringUTF8(domain) &&
      domain.find_first_of(kItemSeparator) == std::string::npos;
}

bool IsVarSane(const std::string& var) {
  static const char kAllowedChars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789"
      "_";
  static_assert(
      sizeof(kAllowedChars) == 26 + 26 + 10 + 1 + 1, "some mess with chars");
  // We must not allow kItemSeparator in anything used as an input to construct
  // message to sign.
  DCHECK(!base::Contains(kAllowedChars, kItemSeparator));
  DCHECK(!base::Contains(kAllowedChars, kVarValueSeparator));
  return !var.empty() &&
      var.size() <= kStringLengthLimit &&
      base::IsStringASCII(var) &&
      var.find_first_not_of(kAllowedChars) == std::string::npos &&
      !base::IsAsciiDigit(var[0]);
}

bool IsValueSane(const std::string& value) {
  return value.size() <= kStringLengthLimit &&
      base::IsStringUTF8(value) &&
      value.find_first_of(kItemSeparator) == std::string::npos;
}

bool IsVarValueMapSane(const VarValueMap& map) {
  if (map.size() > kVarsLimit)
    return false;
  for (auto it = map.begin(); it != map.end(); ++it) {
    const std::string& var = it->first;
    const std::string& value = it->second;
    if (!IsVarSane(var) || !IsValueSane(value))
      return false;
  }
  return true;
}

void ConvertVarValueMapToBlob(const VarValueMap& map, std::string* out) {
  out->clear();
  DCHECK(IsVarValueMapSane(map));
  for (auto it = map.begin(); it != map.end(); ++it)
    *out += it->first + kVarValueSeparator + it->second + kItemSeparator;
}

void CreatePassport(const std::string& domain,
                    const VarValueMap& map,
                    int64_t tick,
                    const crypto::HMAC* engine,
                    std::string* out) {
  DCHECK(engine);
  DCHECK(out);
  DCHECK(IsDomainSane(domain));
  DCHECK(IsVarValueMapSane(map));

  out->clear();
  std::string result(kPassportSize, '0');

  std::string blob;
  blob = domain + kItemSeparator;
  std::string tmp;
  ConvertVarValueMapToBlob(map, &tmp);
  blob += tmp + kItemSeparator + base::NumberToString(tick);

  std::string hmac;
  unsigned char* hmac_data = reinterpret_cast<unsigned char*>(
      base::WriteInto(&hmac, kHMACSizeInBytes + 1));
  if (!engine->Sign(blob, hmac_data, kHMACSizeInBytes)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  std::string hmac_base64 = base::Base64Encode(hmac);
  if (hmac_base64.size() != BASE64_PER_RAW(kHMACSizeInBytes)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  DCHECK(hmac_base64.size() < result.size());
  base::ranges::copy(hmac_base64, result.begin());

  std::string tick_decimal = base::NumberToString(tick);
  DCHECK(tick_decimal.size() <= kTickStringLength);
  base::ranges::copy(tick_decimal,
                     result.begin() + kPassportSize - tick_decimal.size());

  out->swap(result);
}

}  // namespace

class InternalAuthVerificationService {
 public:
  InternalAuthVerificationService()
      : key_change_tick_(0),
        dark_tick_(0) {
  }

  InternalAuthVerificationService(const InternalAuthVerificationService&) =
      delete;
  InternalAuthVerificationService& operator=(
      const InternalAuthVerificationService&) = delete;

  bool VerifyPassport(
      const std::string& passport,
      const std::string& domain,
      const VarValueMap& map) {
    int64_t current_tick = GetCurrentTick();
    int64_t tick = PreVerifyPassport(passport, domain, current_tick);
    if (tick == 0)
      return false;
    if (!IsVarValueMapSane(map))
      return false;
    std::string reference_passport;
    CreatePassport(domain, map, tick, engine_.get(), &reference_passport);
    if (passport != reference_passport) {
      // Consider old key.
      if (key_change_tick_ + get_verification_window_ticks() < tick) {
        return false;
      }
      if (old_key_.empty() || old_engine_ == nullptr)
        return false;
      CreatePassport(domain, map, tick, old_engine_.get(), &reference_passport);
      if (passport != reference_passport)
        return false;
    }

    // Record used tick to prevent reuse.
    base::circular_deque<int64_t>::iterator it =
        std::lower_bound(used_ticks_.begin(), used_ticks_.end(), tick);
    DCHECK(it == used_ticks_.end() || *it != tick);
    used_ticks_.insert(it, tick);

    // Consider pruning |used_ticks_|.
    if (used_ticks_.size() > 50) {
      dark_tick_ = std::max(dark_tick_,
          current_tick - get_verification_window_ticks());
      used_ticks_.erase(
          used_ticks_.begin(),
          std::lower_bound(used_ticks_.begin(), used_ticks_.end(),
                           dark_tick_ + 1));
    }
    return true;
  }

  void ChangeKey(const std::string& key) {
    old_key_.swap(key_);
    key_.clear();
    old_engine_.swap(engine_);
    engine_.reset();

    if (key.size() != kKeySizeInBytes)
      return;
    std::unique_ptr<crypto::HMAC> new_engine(
        new crypto::HMAC(crypto::HMAC::SHA256));
    if (!new_engine->Init(key))
      return;
    engine_.swap(new_engine);
    key_ = key;
    key_change_tick_ = GetCurrentTick();
  }

 private:
  static int get_verification_window_ticks() {
    return InternalAuthVerification::get_verification_window_ticks();
  }

  // Returns tick bound to given passport on success or zero on failure.
  int64_t PreVerifyPassport(const std::string& passport,
                            const std::string& domain,
                            int64_t current_tick) {
    if (passport.size() != kPassportSize || !base::IsStringASCII(passport) ||
        !IsDomainSane(domain) || current_tick <= dark_tick_ ||
        current_tick > key_change_tick_ + kKeyRegenerationHardTicks ||
        key_.empty() || engine_ == nullptr) {
      return 0;
    }

    // Passport consists of 2 parts: first hmac and then tick.
    std::string tick_decimal =
        passport.substr(BASE64_PER_RAW(kHMACSizeInBytes));
    DCHECK(tick_decimal.size() == kTickStringLength);
    int64_t tick = 0;
    if (!base::StringToInt64(tick_decimal, &tick) ||
        tick <= dark_tick_ ||
        tick > key_change_tick_ + kKeyRegenerationHardTicks ||
        tick < current_tick - get_verification_window_ticks() ||
        std::binary_search(used_ticks_.begin(), used_ticks_.end(), tick)) {
      return 0;
    }
    return tick;
  }

  // Current key.
  std::string key_;

  // We keep previous key in order to be able to verify passports during
  // regeneration time.  Keys are regenerated on a regular basis.
  std::string old_key_;

  // Corresponding HMAC engines.
  std::unique_ptr<crypto::HMAC> engine_;
  std::unique_ptr<crypto::HMAC> old_engine_;

  // Tick at a time of recent key regeneration.
  int64_t key_change_tick_;

  // Keeps track of ticks of successfully verified passports to prevent their
  // reuse. Size of this container is kept reasonably low by purging outdated
  // ticks.
  base::circular_deque<int64_t> used_ticks_;

  // Some ticks before |dark_tick_| were purged from |used_ticks_| container.
  // That means that we must not trust any tick less than or equal to dark tick.
  int64_t dark_tick_;
};

namespace {

static base::LazyInstance<InternalAuthVerificationService>::DestructorAtExit
    g_verification_service = LAZY_INSTANCE_INITIALIZER;
static base::LazyInstance<base::Lock>::Leaky
    g_verification_service_lock = LAZY_INSTANCE_INITIALIZER;

}  // namespace

class InternalAuthGenerationService : public base::ThreadChecker {
 public:
  InternalAuthGenerationService() : key_regeneration_tick_(0) {
    GenerateNewKey();
  }

  InternalAuthGenerationService(const InternalAuthGenerationService&) = delete;
  InternalAuthGenerationService& operator=(
      const InternalAuthGenerationService&) = delete;

  void GenerateNewKey() {
    DCHECK(CalledOnValidThread());
    std::unique_ptr<crypto::HMAC> new_engine(
        new crypto::HMAC(crypto::HMAC::SHA256));
    std::string key = base::RandBytesAsString(kKeySizeInBytes);
    if (!new_engine->Init(key))
      return;
    engine_.swap(new_engine);
    key_regeneration_tick_ = GetCurrentTick();
    g_verification_service.Get().ChangeKey(key);
    std::fill(key.begin(), key.end(), 0);
  }

  // Returns zero on failure.
  int64_t GetUnusedTick(const std::string& domain) {
    DCHECK(CalledOnValidThread());
    if (engine_ == nullptr) {
      NOTREACHED_IN_MIGRATION();
      return 0;
    }
    if (!IsDomainSane(domain))
      return 0;

    int64_t current_tick = GetCurrentTick();
    if (!used_ticks_.empty() && used_ticks_.back() > current_tick)
      current_tick = used_ticks_.back();
    for (bool first_iteration = true;; first_iteration = false) {
      if (current_tick < key_regeneration_tick_ + kKeyRegenerationHardTicks)
        break;
      if (!first_iteration)
        return 0;
      GenerateNewKey();
    }

    // Forget outdated ticks if any.
    used_ticks_.erase(
        used_ticks_.begin(),
        std::lower_bound(used_ticks_.begin(), used_ticks_.end(),
                         current_tick - kGenerationWindowTicks + 1));
    DCHECK(used_ticks_.size() <= kGenerationWindowTicks + 0u);
    if (used_ticks_.size() >= kGenerationWindowTicks + 0u) {
      // Average speed of GeneratePassport calls exceeds limit.
      return 0;
    }
    for (int64_t tick = current_tick;
         tick > current_tick - kGenerationWindowTicks; --tick) {
      int idx = static_cast<int>(used_ticks_.size()) -
          static_cast<int>(current_tick - tick + 1);
      if (idx < 0 || used_ticks_[idx] != tick) {
        DCHECK(!base::Contains(used_ticks_, tick));
        return tick;
      }
    }
    NOTREACHED_IN_MIGRATION();
    return 0;
  }

  std::string GeneratePassport(const std::string& domain,
                               const VarValueMap& map,
                               int64_t tick) {
    DCHECK(CalledOnValidThread());
    if (tick == 0) {
      tick = GetUnusedTick(domain);
      if (tick == 0)
        return std::string();
    }
    if (!IsVarValueMapSane(map))
      return std::string();

    std::string result;
    CreatePassport(domain, map, tick, engine_.get(), &result);
    used_ticks_.insert(
        std::lower_bound(used_ticks_.begin(), used_ticks_.end(), tick), tick);
    return result;
  }

 private:
  static int get_verification_window_ticks() {
    return InternalAuthVerification::get_verification_window_ticks();
  }

  std::unique_ptr<crypto::HMAC> engine_;
  int64_t key_regeneration_tick_;
  base::circular_deque<int64_t> used_ticks_;
};

namespace {

static base::LazyInstance<InternalAuthGenerationService>::DestructorAtExit
    g_generation_service = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
bool InternalAuthVerification::VerifyPassport(
    const std::string& passport,
    const std::string& domain,
    const VarValueMap& var_value_map) {
  base::AutoLock alk(g_verification_service_lock.Get());
  return g_verification_service.Get().VerifyPassport(
      passport, domain, var_value_map);
}

// static
void InternalAuthVerification::ChangeKey(const std::string& key) {
  base::AutoLock alk(g_verification_service_lock.Get());
  g_verification_service.Get().ChangeKey(key);
}

// static
int InternalAuthVerification::get_verification_window_ticks() {
  int candidate = kVerificationWindowTicks;
  if (verification_window_seconds_ > 0)
    candidate = verification_window_seconds_ *
        base::Time::kMicrosecondsPerSecond / kTickUs;
  return std::clamp(candidate, 1, kVerificationWindowTicks);
}

int InternalAuthVerification::verification_window_seconds_ = 0;

// static
std::string InternalAuthGeneration::GeneratePassport(
    const std::string& domain, const VarValueMap& var_value_map) {
  return g_generation_service.Get().GeneratePassport(domain, var_value_map, 0);
}

// static
void InternalAuthGeneration::GenerateNewKey() {
  g_generation_service.Get().GenerateNewKey();
}
