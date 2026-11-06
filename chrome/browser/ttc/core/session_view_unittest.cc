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

  // SessionViewDelegate:
  Profile* GetProfile() override { return nullptr; }
  BrowserWindowInterface* GetBrowserWindowInterface() override {
    return nullptr;
  }
  void EndSessionAsync() override {}
  SessionLifecycle GetSessionLifecycle() const override {
    return session_lifecycle_;
  }

 private:
  SessionLifecycle session_lifecycle_ = SessionLifecycle::kInitializing;
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

}  // namespace

}  // namespace ttc
