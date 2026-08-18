// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTDOWN_WATCHDOG_MAC_H_
#define CHROME_BROWSER_SHUTDOWN_WATCHDOG_MAC_H_

// Emergency shutdown watchdogs for macOS. When the OS is shutting down,
// rebooting, or updating, a hung Chrome shutdown blocks the OS flow. These
// watchdogs bound shutdown: record a non-fatal dump for triage, then
// terminate. Inert under --test-type and when a debugger is attached.
//
// Terminology: "shutdown" is the whole process-exit sequence once quit has
// been requested; "teardown" is the BrowserProcessImpl::StartTearDown phase
// of it.
namespace shutdown_watchdog {

// Bounds a SIGTERM-initiated shutdown. Blocks the calling thread until
// shutdown completes (see OnShutdownComplete()) or an emergency deadline
// passes. On expiry, records a non-fatal dump and terminates the process by
// re-raising SIGTERM; in that case it does not return.
void BlockOnSigtermShutdown();

// Bounds a UI-initiated shutdown, starting an emergency deadline when
// browser teardown begins. If shutdown has not completed (see
// OnShutdownComplete()) by the deadline, records a non-fatal dump and
// terminates the process with RESULT_CODE_HUNG. May be a no-op (inert
// configurations, or a SIGTERM watchdog already bounding this shutdown).
void OnBrowserTearDownStarted();

// Signals that shutdown has reached its terminal phase; any running
// watchdog stands down.
void OnShutdownComplete();

}  // namespace shutdown_watchdog

#endif  // CHROME_BROWSER_SHUTDOWN_WATCHDOG_MAC_H_
