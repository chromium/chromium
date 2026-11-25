// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_TEST_UTILS_H_
#define CHROME_BROWSER_TTC_CORE_TEST_UTILS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ttc/core/session_view_delegate.h"
#include "chrome/browser/ttc/core/states.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/ttc/app/public/conversation.h"
#include "components/ttc/app/public/page_context_types.h"
#include "components/ttc/app/public/session_journal.h"
#include "components/ttc/app/public/tool_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ttc {

class MockConversation : public Conversation {
 public:
  MockConversation();
  ~MockConversation() override;

  MOCK_METHOD(void, Start, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, SendTextInput, (const std::string&), (override));
  MOCK_METHOD(void, SendContextUpdate, (PageContextUpdate), (override));
  MOCK_METHOD(void, OnPageContextInvalidated, (), (override));
  MOCK_METHOD(void, SetMicMuted, (bool), (override));
  MOCK_METHOD(void, SetHotlAudioSink, (HotlAudioSinkCallback), (override));
  MOCK_METHOD(SessionJournal&, GetJournal, (), (override));
};

// A SessionViewDelegate whose state tests set directly.
class FakeSessionViewDelegate : public SessionViewDelegate {
 public:
  FakeSessionViewDelegate();
  ~FakeSessionViewDelegate() override;

  void set_profile(Profile* profile) { profile_ = profile; }
  void set_session_lifecycle(SessionLifecycle lifecycle) {
    session_lifecycle_ = lifecycle;
  }
  void set_app_mic_muted(bool muted) { app_mic_muted_ = muted; }

  // SessionViewDelegate:
  Profile* GetProfile() override;
  BrowserWindowInterface* GetBrowserWindowInterface() override;
  void EndSessionAsync() override;
  SessionLifecycle GetSessionLifecycle() const override;
  bool SetMicMuted(bool muted) override;
  bool IsMicMuted() const override;
  Speaker GetSpeaker() const override;
  bool CanSaveDebugTrace() const override;
  void SaveDebugTrace() override;

 private:
  raw_ptr<Profile> profile_ = nullptr;
  SessionLifecycle session_lifecycle_ = SessionLifecycle::kInitializing;
  bool app_mic_muted_ = false;
};

// Runs the message loop for a short amount of time. Useful to check something
// doesn't happen but, since it can't prove the thing won't happen later, use
// this only when there's no event to wait on.
void TinyWait();

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_TEST_UTILS_H_
