// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_DEVICES_UTIL_WIN_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_DEVICES_UTIL_WIN_H_

#include <stdint.h>

#include "base/process/process_handle.h"

// Given a window handle, returns the PID of the application's "main process"
// for audio capture purposes. If an error occurs, returns
// `base::kNullProcessId`.
//
// On Windows, a window is merely a GUI element created by a process and the
// window itself does not play any audio. A process is responsible for rendering
// the application audio. In some multi-process scenarios, the process that
// creates and owns the window might not be the same that renders audio - e.g.
// Chromium.
//
// Despite not being able to capture audio from windows, Windows has an OS API
// that allows an application to capture audio from a process tree instead.
// Given a HWND value, it is possible to obtain the process id (PID) of the
// process that created the window. Therefore, the best we can do is to try find
// the root process of the application's process tree and use it to capture
// audio. This function does that.
//
// Let "window process" be the process that created the window identified by the
// provided HWND. Depending on the type of the application, there are different
// definitions of "main process" for this function:
// - For UWP apps, the "main process" is the process that created the child
//   window with the "Windows.UI.Core.CoreWindow" class name.
// - For all other apps, the "main process" is the oldest ancestor process of
//  "window process" that shares the same executable image with "window
//  process".
base::ProcessId GetAppMainProcessId(intptr_t window_id);

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_CAPTURE_DEVICES_UTIL_WIN_H_
