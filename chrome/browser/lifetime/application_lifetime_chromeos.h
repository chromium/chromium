// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_CHROMEOS_H_
#define CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_CHROMEOS_H_

class PrefService;

namespace chrome {

// Requests a relaunch from ChromeOS update engine.
// Only use this if there's an update pending.
void RelaunchForUpdate();

// True if there's a system update pending.
bool UpdatePending();

// Sets kApplicationLocale in |local_state| for the login screen on the next
// application start, if it is forced to a specific value due to enterprise
// policy or the owner's locale.  Returns true if any pref has been modified.
bool SetLocaleForNextStart(PrefService* local_state);

// Returns true if we sent or are planning to send a stop session request to
// session manager.
bool IsSendingStopRequestToSessionManager();

// Sets the flag to send a stop request to session manager instead of shutting
// down/restarting Chrome in place.
void SetSendStopRequestToSessionManager(bool should_send_request = true);

// Sends stop request to session manager. If there is an update pending, this
// will reboot.
void StopSession();

// Records MarkAsCleanShutdown is called to clean the exit type.
void LogMarkAsCleanShutdown();

}  // namespace chrome

#endif  // CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_CHROMEOS_H_
