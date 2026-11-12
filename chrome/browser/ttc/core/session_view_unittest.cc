// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_view.h"

#include <vector>

#include "chrome/browser/ttc/core/session_view_delegate.h"
#include "chrome/browser/ttc/core/states.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

class FakeSessionViewDelegate : public SessionViewDelegate {
 public:
  void set_session_lifecycle(SessionLifecycle lifecycle) {
    session_lifecycle_ = lifecycle;
  }
  void set_app_mic_muted(bool muted) { app_mic_muted_ = muted; }

  // SessionViewDelegate:
  Profile* GetProfile() override { return nullptr; }
  BrowserWindowInterface* GetBrowserWindowInterface() override {
    return nullptr;
  }
  void EndSessionAsync() override {}
  SessionLifecycle GetSessionLifecycle() const override {
    return session_lifecycle_;
  }
  bool SetMicMuted(bool muted) override {
    if (app_mic_muted_ == muted) {
      return false;
    }
    app_mic_muted_ = muted;
    return true;
  }
  bool IsMicMuted() const override { return app_mic_muted_; }
  Speaker GetSpeaker() const override { return Speaker::kIdle; }

 private:
  SessionLifecycle session_lifecycle_ = SessionLifecycle::kInitializing;
  bool app_mic_muted_ = false;
};

class FakeSessionView : public SessionView {
 public:
  explicit FakeSessionView(SessionViewDelegate& delegate)
      : SessionView(delegate) {}

  const std::vector<UiState>& ui_states() const { return ui_states_; }

 private:
  // SessionView:
  void OnUiStateChanged(UiState state) override { ui_states_.push_back(state); }

  std::vector<UiState> ui_states_;
};

class TtcSessionViewTest : public testing::Test {
 protected:
  void SetSessionLifecycle(SessionLifecycle lifecycle) {
    delegate_.set_session_lifecycle(lifecycle);
    view_.OnSessionLifecycleChanged(lifecycle);
  }

  void SetMicMuted(bool muted) {
    delegate_.set_app_mic_muted(muted);
    view_.OnMicMutedChanged(muted);
  }

  FakeSessionViewDelegate delegate_;
  FakeSessionView view_{delegate_};
};

TEST_F(TtcSessionViewTest, StartsInitializing) {
  EXPECT_EQ(view_.GetUiState(), UiState::kInitializing);

  SetSessionLifecycle(SessionLifecycle::kInitializing);
  EXPECT_EQ(view_.GetUiState(), UiState::kInitializing);
  EXPECT_THAT(view_.ui_states(), testing::IsEmpty());
}

TEST_F(TtcSessionViewTest, FollowsLifecycle) {
  SetSessionLifecycle(SessionLifecycle::kLive);
  EXPECT_EQ(view_.GetUiState(), UiState::kLive);
  EXPECT_THAT(view_.ui_states(), testing::ElementsAre(UiState::kLive));
}

TEST_F(TtcSessionViewTest, KeepsLastStateOnceFinished) {
  SetSessionLifecycle(SessionLifecycle::kFinished);
  EXPECT_EQ(view_.GetUiState(), UiState::kInitializing);
  EXPECT_THAT(view_.ui_states(), testing::IsEmpty());
}

TEST_F(TtcSessionViewTest, FollowsMicMute) {
  SetSessionLifecycle(SessionLifecycle::kLive);

  SetMicMuted(true);
  EXPECT_EQ(view_.GetUiState(), UiState::kMicMuted);
  SetMicMuted(false);
  EXPECT_THAT(
      view_.ui_states(),
      testing::ElementsAre(UiState::kLive, UiState::kMicMuted, UiState::kLive));
}

TEST_F(TtcSessionViewTest, KeepsShowingMutedOnceFinished) {
  SetSessionLifecycle(SessionLifecycle::kLive);
  SetMicMuted(true);

  // A finished session keeps showing its last state.
  delegate_.set_app_mic_muted(false);
  SetSessionLifecycle(SessionLifecycle::kFinished);
  EXPECT_EQ(view_.GetUiState(), UiState::kMicMuted);
}

}  // namespace

}  // namespace ttc
