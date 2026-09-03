// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_SESSION_VIEW_H_
#define CHROME_BROWSER_TTC_SESSION_VIEW_H_

#include "base/memory/raw_ref.h"

namespace ttc {

class SessionViewDelegate;

class SessionView {
 public:
  explicit SessionView(SessionViewDelegate& delegate);
  ~SessionView();
  SessionView(const SessionView&) = delete;
  SessionView& operator=(const SessionView&) = delete;

 private:
  // Safe because the delegate is guaranteed to outlive this object. Assigned on
  // construction.
  const raw_ref<SessionViewDelegate> delegate_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_SESSION_VIEW_H_
