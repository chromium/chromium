// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/test/glic_user_session_test_helper.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/fake_cros_network_config.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "google_apis/gaia/gaia_id.h"

namespace ash {

GlicUserSessionTestHelper::GlicUserSessionTestHelper() = default;

GlicUserSessionTestHelper::~GlicUserSessionTestHelper() {
  CHECK(!need_post_profile_teardown_)
      << "Make sure PostProfileTearDown is called after profile is destroyed";
}

void GlicUserSessionTestHelper::PreProfileSetUp(
    ProfileManager* profile_manager) {
  CHECK(!need_post_profile_teardown_);
  need_post_profile_teardown_ = true;

  CHECK(profile_manager);
  CHECK(profile_manager->GetLoadedProfiles().empty())
      << "PreProfileSetUp must be called before profile is created";
  profile_manager_observation_.Observe(profile_manager);

  network_handler_test_helper_ =
      std::make_unique<ash::NetworkHandlerTestHelper>();
  fake_cros_network_config_ =
      std::make_unique<chromeos::network_config::FakeCrosNetworkConfig>();
  ash::network_config::OverrideInProcessInstanceForTesting(
      fake_cros_network_config_.get());

  session_manager_ = std::make_unique<session_manager::SessionManager>(
      std::make_unique<session_manager::FakeSessionManagerDelegate>());

  CHECK(!user_manager::UserManager::IsInitialized())
      << "UserManager must not be nested";
  user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
      std::make_unique<user_manager::FakeUserManagerDelegate>(),
      TestingBrowserProcess::GetGlobal()->local_state()));
  session_manager_->OnUserManagerCreated(user_manager_.Get());

  // Simulate user creation and login.
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      TestingProfile::kDefaultProfileUserName, GaiaId("1234567890")));
  CHECK(user_manager::TestHelper(user_manager::UserManager::Get())
            .AddRegularUser(account_id));
  session_manager_->CreateSession(
      account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id),
      /*new_user=*/false, /*has_active_session=*/false);

  // Create `ScopedAccountIdAnnotator` to set up the user-to-profile mapping
  // needed for `BrowserContextHelper`, when a profile is created after this
  // returns. Since this helper expects the caller to create only one profile,
  // it will cause crash if second profile is created.
  scoped_account_id_annotator_ =
      std::make_unique<ScopedAccountIdAnnotator>(profile_manager, account_id);
}

void GlicUserSessionTestHelper::PostProfileTearDown() {
  CHECK(need_post_profile_teardown_);
  need_post_profile_teardown_ = false;

  CHECK(!TestingBrowserProcess::GetGlobal()->profile_manager())
      << "ProfileManager should be destroyed before PostProfileTearDown";

  session_manager_.reset();
  user_manager_.Reset();

  ash::network_config::OverrideInProcessInstanceForTesting(nullptr);
  fake_cros_network_config_.reset();
  network_handler_test_helper_.reset();
}

void GlicUserSessionTestHelper::OnProfileManagerDestroying() {
  scoped_account_id_annotator_.reset();
  profile_manager_observation_.Reset();
}

}  // namespace ash
