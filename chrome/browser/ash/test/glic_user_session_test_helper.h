// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_TEST_GLIC_USER_SESSION_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_TEST_GLIC_USER_SESSION_TEST_HELPER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/user_manager/scoped_user_manager.h"

class ProfileManager;
class ProfileManagerObserver;

namespace chromeos::network_config {
class FakeCrosNetworkConfig;
}  // namespace chromeos::network_config

namespace session_manager {
class SessionManager;
}  // namespace session_manager

namespace ash {

class NetworkHandlerTestHelper;
class ScopedAccountIdAnnotator;

// Sets up user session for GLIC.
// This test helper can be used in unit tests that require GLIC features.
// This is designed to make sure `GlicEnabling::IsProfileEligible` returns true
// in unit tests.
// TODO(crbug.com/461743495): Replace this with a better mechanism to provide
// ChromeOS user session environment for testing of browser implementation.
class GlicUserSessionTestHelper : public ProfileManagerObserver {
 public:
  GlicUserSessionTestHelper();
  GlicUserSessionTestHelper(const GlicUserSessionTestHelper&) = delete;
  GlicUserSessionTestHelper& operator=(const GlicUserSessionTestHelper&) =
      delete;
  ~GlicUserSessionTestHelper() override;

  // This must be called before profile is set up.
  // `profile_manager` must be non-null.
  void PreProfileSetUp(ProfileManager* profile_manager);

  // This must be called after profile is destroyed.
  void PostProfileTearDown();

 private:
  // ProfileManagerObserver override:
  void OnProfileManagerDestroying() override;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  user_manager::ScopedUserManager user_manager_;

  std::unique_ptr<ScopedAccountIdAnnotator> scoped_account_id_annotator_;
  bool need_post_profile_teardown_ = false;

  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<chromeos::network_config::FakeCrosNetworkConfig>
      fake_cros_network_config_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_TEST_GLIC_USER_SESSION_TEST_HELPER_H_
