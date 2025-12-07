// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/rate_limiting_key_manager.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/format_macros.h"
#include "base/hash/hash.h"
#include "base/strings/stringprintf.h"
#include "crypto/random.h"

namespace safe_browsing {

class RateLimitingKeyManager::RateLimitingKey {
 public:
  RateLimitingKey(base::Time creation_time, const std::string& stable_input)
      : expiry_(creation_time + kTimeToLive),
        value_(GenerateNewValue(stable_input)) {}

  ~RateLimitingKey() = default;

  const std::string& value() const { return value_; }

  bool is_expired(base::Time now) const { return expiry_ < now; }

 private:
  // How many random bytes to use as a nonce in generating the key.
  static constexpr size_t kRandomNonceLength = 16;

  // Generates a new rate_limiting_key value by hashing `stable_input` with a
  // random nonce. Implementation of the rate_limiting_key format is subject
  // to change and should not be relied on.
  static std::string GenerateNewValue(const std::string& stable_input) {
    // Generate some random bytes.
    std::vector<uint8_t> bytes = crypto::RandBytesAsVector(kRandomNonceLength);
    // Concatenate the stable_input.
    std::copy(stable_input.begin(), stable_input.end(),
              std::back_inserter(bytes));
    // Hash the whole thing and output as a string.
    size_t hash = base::FastHash(bytes);
    return base::StringPrintf("%" PRIuS, hash);
  }

  base::Time expiry_;
  std::string value_;
};

RateLimitingKeyManager::RateLimitingKeyManager(const std::string& stable_input)
    : stable_input_(stable_input) {}

RateLimitingKeyManager::~RateLimitingKeyManager() = default;

const std::string& RateLimitingKeyManager::GetCurrentRateLimitingKey(
    const std::string& profile_id) {
  base::Time now = base::Time::Now();
  auto it = rate_limiting_keys_.find(profile_id);
  if (it == rate_limiting_keys_.end() || it->second.is_expired(now)) {
    GarbageCollectExpired(now);
    bool inserted;
    std::tie(it, inserted) = rate_limiting_keys_.insert_or_assign(
        profile_id, RateLimitingKey{now, stable_input_});
    // Since we deleted the expired keys above, the insertion should always
    // work.
    CHECK(inserted);
  }
  return it->second.value();
}

void RateLimitingKeyManager::GarbageCollectExpired(base::Time now) {
  for (auto it = rate_limiting_keys_.begin();
       it != rate_limiting_keys_.end();) {
    if (it->second.is_expired(now)) {
      it = rate_limiting_keys_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace safe_browsing
