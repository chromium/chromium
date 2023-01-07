// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_DATA_DELETER_H_
#define CHROME_BROWSER_SESSIONS_SESSION_DATA_DELETER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"

class Profile;

// Clears cookies and local storage for origins that are defined as session-only
// through settings, extensions or enterprise policy and clears session cookies
// (cookies without expiration date) unless the startup preference is set to
// continue the previous session or |skip_session_cookies| is true.
// |callback| is called when session-only origin cookies and storage deletions
// are finished. It does not wait for deletion of regular session cookies since
// these cookies are cleaned up on startup as well.
class SessionDataDeleter {
 public:
  explicit SessionDataDeleter(Profile* profile);
  SessionDataDeleter(const SessionDataDeleter&) = delete;
  SessionDataDeleter& operator=(const SessionDataDeleter&) = delete;
  virtual ~SessionDataDeleter();

  // Starts deletion of session data. Keeps the Profile and browser process
  // alive until deletion is finished. Must not be called when the browser is
  // already shutting down (browser_shutdown::IsTryingToQuit()).
  virtual void DeleteSessionOnlyData(bool skip_session_cookies,
                                     base::OnceClosure callback);

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_DATA_DELETER_H_
