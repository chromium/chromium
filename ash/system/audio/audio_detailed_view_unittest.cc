// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_detailed_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// This class exists to stub out the CloseBubble() call. This allows tests to
// directly construct the detailed view, without depending on the entire quick
// settings bubble and view hierarchy.
class FakeDetailedViewDelegate : public DetailedViewDelegate {
 public:
  FakeDetailedViewDelegate()
      : DetailedViewDelegate(/*tray_controller=*/nullptr) {}
  ~FakeDetailedViewDelegate() override = default;

  // DetailedViewDelegate:
  void CloseBubble() override { ++close_bubble_count_; }

  int close_bubble_count_ = 0;
};

}  // namespace

class AudioDetailedViewTest : public AshTestBase {
 public:
  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kAudioSettingsPage}, {});

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
  raw_ptr<AudioDetailedView, ExperimentalAsh> audio_detailed_view_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AudioDetailedViewTest, PressingSettingsButtonOpensSettings) {
  views::Button* settings_button = GetSettingsButton();

  // Clicking the button at the lock screen does nothing.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  LeftClickOn(settings_button);
  EXPECT_EQ(0, GetSystemTrayClient()->show_audio_settings_count());
  EXPECT_EQ(0, detailed_view_delegate_.close_bubble_count_);

  // Clicking the button in an active user session opens OS settings.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  LeftClickOn(settings_button);
  EXPECT_EQ(1, GetSystemTrayClient()->show_audio_settings_count());
  EXPECT_EQ(1, detailed_view_delegate_.close_bubble_count_);
}

}  // namespace ash