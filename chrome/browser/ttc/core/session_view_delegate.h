// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_VIEW_DELEGATE_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_VIEW_DELEGATE_H_

class Profile;

namespace ttc {

class SessionViewDelegate {
 public:
  virtual ~SessionViewDelegate() = default;

  // Returns the profile the session belongs to.
  virtual Profile* GetProfile() = 0;

  // Ends the session asynchronously. The session, and this delegate, will be
  // destroyed in a subsequent task so it is safe to call this from the
  // SessionView.
  virtual void EndSessionAsync() = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_VIEW_DELEGATE_H_
