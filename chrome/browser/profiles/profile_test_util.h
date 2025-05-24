// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_TEST_UTIL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_TEST_UTIL_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile_selections.h"

class Profile;
class ProfileKeyedServiceFactory;
class ProfileManager;

namespace profiles::testing {

// Helper to call `ProfileManager::CreateProfileAsync` synchronously during
// tests. Returns the created `Profile`.
Profile& CreateProfileSync(ProfileManager* profile_manager,
                           const base::FilePath& path);

#if !BUILDFLAG(IS_ANDROID)
// Helper to call `::profiles::SwitchToProfile()` synchronously during tests.
void SwitchToProfileSync(const base::FilePath& path, bool always_create = true);

// Sets `policy::BrowserPolicyConnector::non_managed_domain_for_testing` to the
// provided domain, and clears it when this object goes out of scope.
class ScopedNonEnterpriseDomainSetterForTesting {
 public:
  // Uses `example.com` by default, value chosen as it is the one used at  a
  // pretty basic level in tests (`user_manager::kStubUserEmail`)
  // This is needed to prevent `TurnSyncOnHelper` for trying to make network
  // calls to fetch policy for the current account.
  // TODO(crbug.com/40831172): Try to get saner defaults for stub user
  // info or allowlisted domains.
  explicit ScopedNonEnterpriseDomainSetterForTesting(
      const char* domain = "example.com");
  virtual ~ScopedNonEnterpriseDomainSetterForTesting();
};
#endif  // BUILDFLAG(IS_ANDROID)

// Overrides the ProfileSelections that a ProfileKeyedServiceFactory uses to
// determine which profiles to create a service for.
class ScopedProfileSelectionsForFactoryTesting {
 public:
  ScopedProfileSelectionsForFactoryTesting(ProfileKeyedServiceFactory* factory,
                                           ProfileSelections selections);

  ~ScopedProfileSelectionsForFactoryTesting();

 private:
  raw_ptr<ProfileKeyedServiceFactory> factory_;
  ProfileSelections old_selections_;
};

// A testing wrapper to simulate a logged-in managed guest session.
// These sessions are only available for ChromeOS.
class ScopedTestManagedGuestSession {
 public:
  ScopedTestManagedGuestSession();
  ~ScopedTestManagedGuestSession();

  ScopedTestManagedGuestSession(const ScopedTestManagedGuestSession&) = delete;
  ScopedTestManagedGuestSession& operator=(
      const ScopedTestManagedGuestSession&) = delete;
};

}  // namespace profiles::testing
#endif  // CHROME_BROWSER_PROFILES_PROFILE_TEST_UTIL_H_
