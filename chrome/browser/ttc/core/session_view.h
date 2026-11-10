// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_VIEW_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_VIEW_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/states.h"

namespace ttc {

class SessionViewDelegate;

// Base class for the object holding and implementing all UI interaction for a
// session, which the session owns for as long as it exists.
class SessionView : public SessionController::Observer {
 public:
  SessionView(const SessionView&) = delete;
  SessionView& operator=(const SessionView&) = delete;
  ~SessionView() override;

  UiState GetUiState() const;

  // SessionController::Observer:
  void OnSessionLifecycleChanged(SessionLifecycle lifecycle) override;
  void OnMicMutedChanged(bool muted) override;

 protected:
  explicit SessionView(SessionViewDelegate& delegate);

  SessionViewDelegate& delegate() const { return *delegate_; }

  virtual void OnUiStateChanged(UiState state) {}

 private:
  UiState ComputeUiState() const;
  void UpdateUiState();

  UiState ui_state_ = UiState::kInitializing;

  // Safe because the delegate is guaranteed to outlive this object.
  const raw_ref<SessionViewDelegate> delegate_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_VIEW_H_
