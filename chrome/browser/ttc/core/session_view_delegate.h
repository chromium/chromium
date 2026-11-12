// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_VIEW_DELEGATE_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_VIEW_DELEGATE_H_

#include "chrome/browser/ttc/core/states.h"

class BrowserWindowInterface;
class Profile;

namespace ttc {

class SessionViewDelegate {
 public:
  virtual ~SessionViewDelegate() = default;

  // Returns the profile the session belongs to.
  virtual Profile* GetProfile() = 0;

  // Returns the browser window the session is currently associated with. May
  // be null if there is no suitable window.
  virtual BrowserWindowInterface* GetBrowserWindowInterface() = 0;

  // Ends the session asynchronously. The session, and this delegate, will be
  // destroyed in a subsequent task so it is safe to call this from the
  // SessionView.
  virtual void EndSessionAsync() = 0;

  // Returns the session's current lifecycle state.
  virtual SessionLifecycle GetSessionLifecycle() const = 0;

  // Mutes or unmutes the user's microphone. Returns whether the state changed.
  virtual bool SetMicMuted(bool muted) = 0;

  // Returns whether the user muted the microphone in TTC, as opposed to the OS
  // or hardware muting it (see `SessionController::OnSystemMicMuteChanged()`).
  virtual bool IsMicMuted() const = 0;

  // Returns the party currently speaking.
  virtual Speaker GetSpeaker() const = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_VIEW_DELEGATE_H_
