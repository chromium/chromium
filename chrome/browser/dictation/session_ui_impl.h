// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_
#define CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/dictation/session_ui.h"

class BrowserWindowInterface;

namespace dictation {

class SessionUiDelegate;

class SessionUiImpl : public SessionUi {
 public:
  explicit SessionUiImpl(BrowserWindowInterface& window,
                         SessionUiDelegate& delegate);
  ~SessionUiImpl() override;

  SessionUiImpl(const SessionUiImpl&) = delete;
  SessionUiImpl& operator=(const SessionUiImpl&) = delete;

 private:
  const base::raw_ref<SessionUiDelegate> controller_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_UI_IMPL_H_
