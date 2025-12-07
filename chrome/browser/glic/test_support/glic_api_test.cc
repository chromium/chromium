// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_api_test.h"

namespace glic {

WebUIStateListener::WebUIStateListener(Host* host)
    : host_(host ? host->GetWeakPtr() : nullptr) {
  CHECK(host_);
  host_->AddObserver(this);
  states_.push_back(host_->GetPrimaryWebUiState());
}

WebUIStateListener::~WebUIStateListener() {
  if (!host_) {
    return;
  }
  host_->RemoveObserver(this);
}

void WebUIStateListener::WebUiStateChanged(mojom::WebUiState state) {
  states_.push_back(state);
}

// Returns if `state` has been seen. Consumes all observed states up to the
// point where this state is seen.
void WebUIStateListener::WaitForWebUiState(mojom::WebUiState state) {
  ASSERT_TRUE(host_);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    while (!states_.empty()) {
      if (states_.front() != state) {
        states_.pop_front();
        continue;
      }
      return true;
    }
    return false;
  })) << "Timed out waiting for WebUI state "
      << state << ". State =" << host_->GetPrimaryWebUiState();
}

CurrentViewListener::CurrentViewListener(Host* host) : host_(host) {
  host_->AddObserver(this);
  views_.push_back(host_->GetPrimaryCurrentView());
}

CurrentViewListener::~CurrentViewListener() {
  host_->RemoveObserver(this);
}

void CurrentViewListener::OnViewChanged(mojom::CurrentView view) {
  views_.push_back(view);
}

// Returns if `state` has been seen. Consumes all observed states up to the
// point where this state is seen.
void CurrentViewListener::WaitForCurrentView(mojom::CurrentView view) {
  ASSERT_TRUE(base::test::RunUntil([&]() {
    while (!views_.empty()) {
      if (views_.front() != view) {
        views_.pop_front();
        continue;
      }
      return true;
    }
    return false;
  })) << "Timed out waiting for WebUI state "
      << view << ". State =" << host_->GetPrimaryCurrentView();
}

}  // namespace glic
