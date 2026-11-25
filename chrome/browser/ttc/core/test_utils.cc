// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/test_utils.h"

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"

namespace ttc {

MockConversation::MockConversation() = default;

MockConversation::~MockConversation() = default;

FakeSessionViewDelegate::FakeSessionViewDelegate() = default;

FakeSessionViewDelegate::~FakeSessionViewDelegate() = default;

Profile* FakeSessionViewDelegate::GetProfile() {
  return profile_;
}

BrowserWindowInterface* FakeSessionViewDelegate::GetBrowserWindowInterface() {
  return nullptr;
}

void FakeSessionViewDelegate::EndSessionAsync() {}

SessionLifecycle FakeSessionViewDelegate::GetSessionLifecycle() const {
  return session_lifecycle_;
}

bool FakeSessionViewDelegate::SetMicMuted(bool muted) {
  if (app_mic_muted_ == muted) {
    return false;
  }
  app_mic_muted_ = muted;
  return true;
}

bool FakeSessionViewDelegate::IsMicMuted() const {
  return app_mic_muted_;
}

Speaker FakeSessionViewDelegate::GetSpeaker() const {
  return Speaker::kIdle;
}

bool FakeSessionViewDelegate::CanSaveDebugTrace() const {
  return false;
}

void FakeSessionViewDelegate::SaveDebugTrace() {}

void TinyWait() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();
}

}  // namespace ttc
