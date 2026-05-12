// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_
#define CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/browser/dictation/session_ui.h"

class BrowserWindowInterface;

namespace dictation {

class SessionUiDelegate;
class DictationBubbleUi;

class SessionUiImpl : public SessionUi {
 public:
  explicit SessionUiImpl(BrowserWindowInterface& window,
                         SessionUiDelegate& delegate);
  ~SessionUiImpl() override;

  SessionUiImpl(const SessionUiImpl&) = delete;
  SessionUiImpl& operator=(const SessionUiImpl&) = delete;

 private:
  friend class DictationSessionUiImplBrowserTest;
  void OnDictationBubbleCloseClicked();

  const base::raw_ref<SessionUiDelegate> controller_;

  // This is the main bubble/toast that shows up at the top-center of the
  // screen.
  std::unique_ptr<DictationBubbleUi> bubble_ui_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_
