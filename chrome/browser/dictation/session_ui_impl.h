// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_
#define CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/session_ui.h"

namespace tabs {
class TabInterface;
}

namespace dictation {

class SessionUiDelegate;
class DictationBubbleUi;

class SessionUiImpl : public SessionUi {
 public:
  explicit SessionUiImpl(tabs::TabInterface& tab, SessionUiDelegate& delegate);
  ~SessionUiImpl() override;

  SessionUiImpl(const SessionUiImpl&) = delete;
  SessionUiImpl& operator=(const SessionUiImpl&) = delete;

 private:
  friend class DictationSessionUiImplBrowserTest;
  void OnDictationBubbleCloseClicked();
  void OnToggleActiveStreamClicked();
  void OnSessionStateChanged(SessionState state);

  const base::raw_ref<SessionUiDelegate> controller_;

  base::CallbackListSubscription session_state_changed_subscription_;

  // This is the main bubble/toast that shows up at the top-center of the
  // screen.
  std::unique_ptr<DictationBubbleUi> bubble_ui_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_
