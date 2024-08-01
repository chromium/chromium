// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_detailed_view.h"

#include <memory>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/user_manager/fake_user_manager.h"
#include "media/base/media_switches.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

class AudioDetailedViewTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    auto audio_detailed_view =
        std::make_unique<AudioDetailedView>(&detailed_view_delegate_);
    audio_detailed_view_ = audio_detailed_view.get();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(audio_detailed_view.release()->GetAsView());
  }

  void TearDown() override {
    widget_.reset();

    AshTestBase::TearDown();
  }

  views::Button* GetSettingsButton() {
    return audio_detailed_view_->settings_button_;
  }

  std::unique_ptr<views::Widget> widget_;
  FakeDetailedViewDelegate detailed_view_delegate_;
  raw_ptr<AudioDetailedView, DanglingUntriaged> audio_detailed_view_ = nullptr;
};

TEST_F(AudioDetailedViewTest, PressingSettingsButtonOpensSettings) {
  views::Button* settings_button = GetSettingsButton();

  // Clicking the button at the lock screen does nothing.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  LeftClickOn(settings_button);
  EXPECT_EQ(0, GetSystemTrayClient()->show_audio_settings_count());
  EXPECT_EQ(0u, detailed_view_delegate_.close_bubble_call_count());

  // Clicking the button in an active user session opens OS settings.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  LeftClickOn(settings_button);
  EXPECT_EQ(1, GetSystemTrayClient()->show_audio_settings_count());
  EXPECT_EQ(1u, detailed_view_delegate_.close_bubble_call_count());
}

class AudioDetailedViewAgcInfoTest
    : public AudioDetailedViewTest,
      public testing::WithParamInterface<testing::tuple<bool, bool>> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureStates(
        {{media::kIgnoreUiGains, IsIgnoreUiGainsEnabled()}});

    AudioDetailedViewTest::SetUp();

    CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
    CHECK(audio_handler);
    audio_handler->SetForceRespectUiGainsState(IsForceRespectUiGainsEnabled());

    account_id_ = Shell::Get()->session_controller()->GetActiveAccountId();
    registry_cache_.SetAccountId(account_id_);
    apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id_,
                                                             &registry_cache_);
    capability_access_cache_.SetAccountId(account_id_);
    apps::AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id_, &capability_access_cache_);

    audio_detailed_view_->Update();
  }

  void TearDown() override {
    AudioDetailedViewTest::TearDown();
    apps::AppRegistryCacheWrapper::Get().RemoveAppRegistryCache(
        &registry_cache_);
    apps::AppCapabilityAccessCacheWrapper::Get().RemoveAppCapabilityAccessCache(
        &capability_access_cache_);
    registry_cache_.ReinitializeForTesting();
  }

  bool IsIgnoreUiGainsEnabled() { return std::get<0>(GetParam()); }
  bool IsForceRespectUiGainsEnabled() { return std::get<1>(GetParam()); }

  views::View* GetAgcInfoView() {
    return audio_detailed_view_->GetViewByID(
        AudioDetailedView::AudioDetailedViewID::kAgcInfoView);
  }

  static apps::AppPtr MakeApp(const char* app_id, const char* name) {
    apps::AppPtr app =
        std::make_unique<apps::App>(apps::AppType::kChromeApp, app_id);
    app->name = name;
    app->short_name = name;
    return app;
  }

  static apps::CapabilityAccessPtr MakeCapabilityAccess(
      const char* app_id,
      std::optional<bool> mic) {
    apps::CapabilityAccessPtr access =
        std::make_unique<apps::CapabilityAccess>(app_id);
    access->camera = false;
    access->microphone = mic;
    return access;
  }

  void LaunchApp(const char* id,
                 const char* name,
                 std::optional<bool> use_mic) {
    std::vector<apps::AppPtr> registry_deltas;
    registry_deltas.push_back(MakeApp(id, name));
    registry_cache_.OnAppsForTesting(std::move(registry_deltas),
                                     apps::AppType::kUnknown,
                                     /* should_notify_initialized = */ false);

    std::vector<apps::CapabilityAccessPtr> capability_access_deltas;
    capability_access_deltas.push_back(MakeCapabilityAccess(id, use_mic));
    capability_access_cache_.OnCapabilityAccesses(
        std::move(capability_access_deltas));
  }

  AccountId account_id_;

  apps::AppRegistryCache registry_cache_;
  apps::AppCapabilityAccessCache capability_access_cache_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(AudioDetailedViewAgcInfoTest, AgcInfoRowShowInProperConditions) {
  const char* app_id = "app";
  const char* app_name = "App name";

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  audio_detailed_view_->OnSessionStateChanged(
      session_manager::SessionState::ACTIVE);

  views::View* agc_info = GetAgcInfoView();
  if (!IsIgnoreUiGainsEnabled()) {
    ASSERT_EQ(agc_info, nullptr);
    return;
  }
  ASSERT_NE(agc_info, nullptr);

  // Launch an app accessing mic and requesting ignore UI gains.
  LaunchApp(app_id, app_name, true);
  audio_detailed_view_->OnNumStreamIgnoreUiGainsChanged(1);
  EXPECT_EQ(agc_info->GetVisible(), !IsForceRespectUiGainsEnabled());

  // Launch an app accessing mic but not requesting ignore UI gains.
  LaunchApp(app_id, app_name, true);
  audio_detailed_view_->OnNumStreamIgnoreUiGainsChanged(0);
  EXPECT_EQ(agc_info->GetVisible(), false);

  // Launch an app not accessing mic but requesting ignore UI gains.
  // This should not happen in real cases though.
  LaunchApp(app_id, app_name, false);
  audio_detailed_view_->OnNumStreamIgnoreUiGainsChanged(1);
  EXPECT_EQ(agc_info->GetVisible(), false);

  // Launch an app not accessing mic and not requesting ignore UI gains.
  LaunchApp(app_id, app_name, false);
  audio_detailed_view_->OnNumStreamIgnoreUiGainsChanged(0);
  EXPECT_EQ(agc_info->GetVisible(), false);
}

INSTANTIATE_TEST_SUITE_P(AudioDetailedViewAgcInfoVisibleTest,
                         AudioDetailedViewAgcInfoTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool()));

}  // namespace ash
