// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_RATE_LIMITING_KEY_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_RATE_LIMITING_KEY_MANAGER_H_

#include <map>

#include "base/time/time.h"

namespace safe_browsing {

// Class that manages state in order to populate the
// ClientDownloadRequest.rate_limiting_key field, a string field populated on
// some platforms (currently Android). This key is used by the server to
// pseudonymously identify individual clients to rate-limit or throttle them if
// they are sending too many requests. The value is derived by hashing a stable
// input string with a random nonce. These rate_limiting_keys are only
// persistent up to a short TTL, and are periodically regenerated with a new
// random nonce. They are stored in memory and do not persist across Chrome
// restarts.
//
// The RateLimitingKeyManager stores the keys in a map keyed by Profile
// identifier. It must be instantiated with a `stable_input` value, which is
// thereafter cached and used to generate all new rate_limiting_keys.
class RateLimitingKeyManager {
 public:
  // How long each rate_limiting_key value may be used for.
  // This value is public for tests.
  static constexpr base::TimeDelta kTimeToLive = base::Minutes(15);

  explicit RateLimitingKeyManager(const std::string& stable_input);
  ~RateLimitingKeyManager();

  // Returns a non-expired rate_limiting_key value for the Profile with given
  // `UniqueId()`.
  const std::string& GetCurrentRateLimitingKey(const std::string& profile_id);

 private:
  class RateLimitingKey;

  // Deletes any expired RateLimitingKeys in the map.
  void GarbageCollectExpired(base::Time now);

  // The stable input used in generating RateLimitingKeys.
  const std::string stable_input_;

  // Map from Profile's UniqueId() to that Profile's most recent
  // rate_limiting_key. Expired entries are garbage-collected periodically
  // (whenever new ones are inserted).
  std::map<std::string, RateLimitingKey> rate_limiting_keys_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_RATE_LIMITING_KEY_MANAGER_H_
