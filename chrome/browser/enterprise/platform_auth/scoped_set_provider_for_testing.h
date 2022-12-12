// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_SCOPED_SET_PROVIDER_FOR_TESTING_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_SCOPED_SET_PROVIDER_FOR_TESTING_H_

#include <memory>

namespace enterprise_auth {

class PlatformAuthProvider;

// Overrides the process-wide `PlatformAuthProviderManager` instance's
// `PlatformAuthProvider` implementation for test purposes.
class ScopedSetProviderForTesting {
 public:
  explicit ScopedSetProviderForTesting(
      std::unique_ptr<PlatformAuthProvider> platform);
  ScopedSetProviderForTesting(const ScopedSetProviderForTesting&) = delete;
  ScopedSetProviderForTesting& operator=(const ScopedSetProviderForTesting&) =
      delete;
  ~ScopedSetProviderForTesting();

 private:
  std::unique_ptr<PlatformAuthProvider> previous_;
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_SCOPED_SET_PROVIDER_FOR_TESTING_H_
