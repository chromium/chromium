// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/locked_fullscreen/arc_locked_fullscreen_manager.h"

#include <memory>
#include <tuple>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/arc/locked_fullscreen/arc_locked_fullscreen_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/experiences/arc/session/arc_session_runner.h"
#include "chromeos/ash/experiences/arc/test/arc_util_test_support.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_session.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr char kMuteAudioWithSuccessHistogram[] = "Arc.MuteAudioSuccess";
constexpr char kUnmuteAudioWithSuccessHistogram[] = "Arc.UnmuteAudioSuccess";
constexpr char kUserEmail[] = "test@example.com";
constexpr char kUserGaiaId[] = "1234567890";

class ArcLockedFullscreenManagerTest
    : public ::testing::TestWithParam<std::tuple<bool, bool>> {
 protected:
  ArcLockedFullscreenManagerTest() {
    // Force the test to mute ARC audio instead of adopting the deprecated flow
    // that disables ARC. The deprecated flow is already tested with ARC session
    // manager browser tests.
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kBocaOnTaskMuteArcAudio);
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    // Initialize fake clients and enable ARC through command line.
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    // Force ARC session manager to skip UI.
    ArcSessionManager::SetUiEnabledForTesting(false);
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    // Initialize a testing profile and the user manager. Needed to test ARC.
    user_manager_ = std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        local_state_.Get(), ash::CrosSettings::Get());
    user_manager_->Initialize();

    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kUserEmail, GaiaId(kUserGaiaId)));
    ASSERT_TRUE(user_manager::TestHelper(user_manager_.get())
                    .AddRegularUser(account_id));
    const std::string user_id_hash =
        user_manager::TestHelper::GetFakeUsernameHash(account_id);
    user_manager_->UserLoggedIn(account_id, user_id_hash);
    profile_ = profile_manager_.CreateTestingProfile(kUserEmail);

    // Initialize the locked fullscreen manager.
    arc_locked_fullscreen_manager_ =
        std::make_unique<ArcLockedFullscreenManager>(profile());

    // Initialize session manager with a fake ARC session.
    arc_session_manager_->SetProfile(profile());
    arc_session_manager_->Initialize();
    arc_session_manager_->RequestEnable();
  }

  void TearDown() override {
    // Reset ARC session manager before shutting down the Concierge client since
    // it is observing it.
    arc_session_manager_.reset();
    ash::ConciergeClient::Shutdown();
    user_manager_->Destroy();
  }

  bool IsMuteAudioRequest() const { return std::get<0>(GetParam()); }

  bool IsMuteOrUnmuteAudioSuccess() const { return std::get<1>(GetParam()); }

  TestingProfile* profile() { return profile_.get(); }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

  ArcLockedFullscreenManager* arc_locked_fullscreen_manager() {
    return arc_locked_fullscreen_manager_.get();
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::ScopedCrosSettingsTestHelper cros_settings_helper_;
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  std::unique_ptr<user_manager::UserManagerImpl> user_manager_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal(),
                                         &local_state_};
  session_manager::SessionManager session_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcLockedFullscreenManager> arc_locked_fullscreen_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_P(ArcLockedFullscreenManagerTest, RequestBeforeArcStartup) {
  ASSERT_NE(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Set up downstream client for testing purposes.
  ash::FakeConciergeClient* const concierge_client =
      ash::FakeConciergeClient::Get();
  vm_tools::concierge::SuccessFailureResponse mute_audio_response;
  mute_audio_response.set_success(IsMuteOrUnmuteAudioSuccess());
  concierge_client->set_mute_vm_audio_response(mute_audio_response);

  // Request mute or unmute audio before ARC startup.
  arc_locked_fullscreen_manager()->UpdateForLockedFullscreenMode(
      IsMuteAudioRequest());
  ASSERT_EQ(concierge_client->mute_vm_audio_call_count(), 0);

  // Emulate successful startup.
  arc_session_manager()->StartArcForTesting();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Verify request goes through now.
  EXPECT_EQ(concierge_client->mute_vm_audio_call_count(), 1);
  base::RunLoop().RunUntilIdle();
  const char* histogram_name = IsMuteAudioRequest()
                                   ? kMuteAudioWithSuccessHistogram
                                   : kUnmuteAudioWithSuccessHistogram;
  histogram_tester()->ExpectUniqueSample(histogram_name,
                                         IsMuteOrUnmuteAudioSuccess(), 1);
}

TEST_P(ArcLockedFullscreenManagerTest, RequestAfterArcStartup) {
  ASSERT_NE(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Set up downstream client for testing purposes.
  ash::FakeConciergeClient* const concierge_client =
      ash::FakeConciergeClient::Get();
  vm_tools::concierge::SuccessFailureResponse mute_audio_response;
  mute_audio_response.set_success(IsMuteOrUnmuteAudioSuccess());
  concierge_client->set_mute_vm_audio_response(mute_audio_response);

  // Emulate successful startup.
  arc_session_manager()->StartArcForTesting();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Request mute or unmute audio and verify request goes through.
  arc_locked_fullscreen_manager()->UpdateForLockedFullscreenMode(
      IsMuteAudioRequest());
  EXPECT_EQ(concierge_client->mute_vm_audio_call_count(), 1);
  base::RunLoop().RunUntilIdle();
  const char* histogram_name = IsMuteAudioRequest()
                                   ? kMuteAudioWithSuccessHistogram
                                   : kUnmuteAudioWithSuccessHistogram;
  histogram_tester()->ExpectUniqueSample(histogram_name,
                                         IsMuteOrUnmuteAudioSuccess(), 1);
}

TEST_P(ArcLockedFullscreenManagerTest, OnArcRestart) {
  ASSERT_NE(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Set up downstream client for testing purposes.
  ash::FakeConciergeClient* const concierge_client =
      ash::FakeConciergeClient::Get();
  vm_tools::concierge::SuccessFailureResponse mute_audio_response;
  mute_audio_response.set_success(IsMuteOrUnmuteAudioSuccess());
  concierge_client->set_mute_vm_audio_response(mute_audio_response);

  // Request mute or unmute audio before ARC startup.
  arc_locked_fullscreen_manager()->UpdateForLockedFullscreenMode(
      IsMuteAudioRequest());
  ASSERT_EQ(concierge_client->mute_vm_audio_call_count(), 0);

  // Emulate successful startup.
  arc_session_manager()->StartArcForTesting();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Verify request goes through now.
  ASSERT_EQ(concierge_client->mute_vm_audio_call_count(), 1);
  base::RunLoop().RunUntilIdle();
  const char* histogram_name = IsMuteAudioRequest()
                                   ? kMuteAudioWithSuccessHistogram
                                   : kUnmuteAudioWithSuccessHistogram;
  histogram_tester()->ExpectUniqueSample(histogram_name,
                                         IsMuteOrUnmuteAudioSuccess(), 1);

  // Stop and restart ARC and verify mute or unmute audio request is only
  // submitted if either of the following are true:
  // 1. Audio was originally muted.
  // 2. Previous unmute request failed.
  arc_session_manager()->StopAndEnableArc();
  arc_session_manager()->StartArcForTesting();
  base::RunLoop().RunUntilIdle();
  if (IsMuteAudioRequest() || !IsMuteOrUnmuteAudioSuccess()) {
    EXPECT_EQ(concierge_client->mute_vm_audio_call_count(), 2);
    histogram_tester()->ExpectUniqueSample(histogram_name,
                                           IsMuteOrUnmuteAudioSuccess(), 2);
  } else {
    EXPECT_EQ(concierge_client->mute_vm_audio_call_count(), 1);
  }
}

INSTANTIATE_TEST_SUITE_P(ArcLockedFullscreenManagerTests,
                         ArcLockedFullscreenManagerTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace
}  // namespace arc
