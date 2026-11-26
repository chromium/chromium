// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/fake_glic_side_panel_coordinator.h"

#include <utility>

#include "ui/views/view.h"

namespace glic {

FakeGlicSidePanelCoordinator::FakeGlicSidePanelCoordinator(
    tabs::TabInterface* tab,
    bool showing)
    : GlicSidePanelCoordinator(tab),
      state_(showing ? State::kShown : State::kClosed) {}

FakeGlicSidePanelCoordinator::~FakeGlicSidePanelCoordinator() = default;

void FakeGlicSidePanelCoordinator::Show(const ShowOptions& options) {
  SetState(State::kShown);
}

void FakeGlicSidePanelCoordinator::Close(const CloseOptions& options) {
  SetState(State::kClosed);
}

bool FakeGlicSidePanelCoordinator::IsShowing() const {
  return state_ == State::kShown;
}

GlicSidePanelCoordinator::State FakeGlicSidePanelCoordinator::state() {
  return state_;
}

bool FakeGlicSidePanelCoordinator::SupportsPeek() const {
  return false;
}

base::CallbackListSubscription FakeGlicSidePanelCoordinator::AddStateCallback(
    base::RepeatingCallback<void(State)> callback) {
  return state_callbacks_.Add(std::move(callback));
}

void FakeGlicSidePanelCoordinator::SetContentsView(
    std::unique_ptr<views::View> contents_view) {}

int FakeGlicSidePanelCoordinator::GetPreferredWidth() {
  return 0;
}

bool FakeGlicSidePanelCoordinator::IsGlicSidePanelActive() {
  return state_ != State::kClosed;
}

void FakeGlicSidePanelCoordinator::SetState(State state) {
  state_ = state;
  state_callbacks_.Notify(state_);
}

}  // namespace glic
