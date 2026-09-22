// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_STATES_H_
#define CHROME_BROWSER_TTC_CORE_STATES_H_

namespace ttc {

enum class ServiceState {
  // TTC is unavailable for this profile.
  kProfileIneligible,
  // TTC is available but no session is in progress.
  kSessionInactive,
  // A session is in progress.
  kSessionActive,
};

// The lifecycle state of a single TTC session.
enum class SessionLifecycle {
  // The session has been created but the backend isn't yet connected.
  kInitializing,
  // The backend is connected and the session is interactive.
  kLive,
  // The session has been disconnected and is no longer usable.
  kFinished,
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_STATES_H_
