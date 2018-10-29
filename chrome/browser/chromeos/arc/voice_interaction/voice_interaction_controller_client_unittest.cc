// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/voice_interaction_controller_client.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/voice_interaction/fake_voice_interaction_controller.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class VoiceInteractionControllerClientTest : public ash::AshTestBase {
 public:
  VoiceInteractionControllerClientTest()
      : fake_user_manager_(
            std::make_unique<chromeos::FakeChromeUserManager>()) {}
  ~VoiceInteractionControllerClientTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Setup test profile.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_ = profile_builder.Build();

    // Setup dependencies
    arc_session_manager_ =
        std::make_unique<ArcSessionManager>(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    voice_interaction_controller_ =
        std::make_unique<FakeVoiceInteractionController>();

    voice_interaction_controller_client_ =
        std::make_unique<VoiceInteractionControllerClient>();
    voice_interaction_controller_client_->SetControllerForTesting(
        voice_interaction_controller_->CreateInterfacePtrAndBind());
    voice_interaction_controller_client_->SetProfile(profile_.get());
  }

  void TearDown() override {
    arc_session_manager_->Shutdown();
    arc_session_manager_.reset();
    voice_interaction_controller_.reset();
    voice_interaction_controller_client_.reset();
    profile_.reset();
    AshTestBase::TearDown();
  }

  FakeVoiceInteractionController* voice_interaction_controller() {
    return voice_interaction_controller_.get();
  }

  VoiceInteractionControllerClient* voice_interaction_controller_client() {
    return voice_interaction_controller_client_.get();
  }

  Profile* profile() { return profile_.get(); }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

  void FlushVoiceInteractionControllerMojo() {
    voice_interaction_controller_client()->FlushMojoForTesting();
  }

 private:
  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  user_manager::ScopedUserManager fake_user_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<FakeVoiceInteractionController> voice_interaction_controller_;
  std::unique_ptr<VoiceInteractionControllerClient>
      voice_interaction_controller_client_;
};

TEST_F(VoiceInteractionControllerClientTest, PrefChangeSendsNotification) {
  PrefService* prefs = profile()->GetPrefs();

  ASSERT_EQ(false, prefs->GetBoolean(prefs::kVoiceInteractionEnabled));
  prefs->SetBoolean(prefs::kVoiceInteractionEnabled, true);
  ASSERT_EQ(true, prefs->GetBoolean(prefs::kVoiceInteractionEnabled));
  voice_interaction_controller_client()->FlushMojoForTesting();
  EXPECT_EQ(
      true,
      voice_interaction_controller()->voice_interaction_settings_enabled());

  ASSERT_EQ(false, prefs->GetBoolean(prefs::kVoiceInteractionContextEnabled));
  prefs->SetBoolean(prefs::kVoiceInteractionContextEnabled, true);
  ASSERT_EQ(true, prefs->GetBoolean(prefs::kVoiceInteractionContextEnabled));
  voice_interaction_controller_client()->FlushMojoForTesting();
  EXPECT_EQ(
      true,
      voice_interaction_controller()->voice_interaction_context_enabled());

  ASSERT_EQ(false, prefs->GetBoolean(prefs::kVoiceInteractionHotwordEnabled));
  prefs->SetBoolean(prefs::kVoiceInteractionHotwordEnabled, true);
  ASSERT_EQ(true, prefs->GetBoolean(prefs::kVoiceInteractionHotwordEnabled));
  voice_interaction_controller_client()->FlushMojoForTesting();
  EXPECT_EQ(
      true,
      voice_interaction_controller()->voice_interaction_hotword_enabled());

  // Default setting is true.
  ASSERT_EQ(true,
            prefs->GetBoolean(prefs::kVoiceInteractionNotificationEnabled));
  prefs->SetBoolean(prefs::kVoiceInteractionNotificationEnabled, false);
  ASSERT_EQ(false,
            prefs->GetBoolean(prefs::kVoiceInteractionNotificationEnabled));
  voice_interaction_controller_client()->FlushMojoForTesting();
  EXPECT_EQ(
      false,
      voice_interaction_controller()->voice_interaction_notification_enabled());

  ASSERT_EQ(false,
            prefs->GetBoolean(prefs::kArcVoiceInteractionValuePropAccepted));
  prefs->SetBoolean(prefs::kArcVoiceInteractionValuePropAccepted, true);
  ASSERT_EQ(true,
            prefs->GetBoolean(prefs::kArcVoiceInteractionValuePropAccepted));
  voice_interaction_controller_client()->FlushMojoForTesting();
  EXPECT_EQ(
      true,
      voice_interaction_controller()->voice_interaction_setup_completed());

  ASSERT_EQ("", prefs->GetString(language::prefs::kApplicationLocale));
  prefs->SetString(language::prefs::kApplicationLocale, "en-CA");
  ASSERT_EQ("en-CA", prefs->GetString(language::prefs::kApplicationLocale));
  voice_interaction_controller_client()->FlushMojoForTesting();
  EXPECT_EQ("en-CA", voice_interaction_controller()->locale());

  ASSERT_EQ(false,
            prefs->GetBoolean(prefs::kVoiceInteractionLaunchWithMicOpen));
  prefs->SetBoolean(prefs::kVoiceInteractionLaunchWithMicOpen, true);
  ASSERT_EQ(true, prefs->GetBoolean(prefs::kVoiceInteractionLaunchWithMicOpen));
  voice_interaction_controller_client()->FlushMojoForTesting();
  EXPECT_EQ(true, voice_interaction_controller()->launch_with_mic_open());
}

}  // namespace arc
