// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_PURGE_STALE_SCREEN_CAPTURE_PERMISSION_H_
#define CHROME_BROWSER_MAC_PURGE_STALE_SCREEN_CAPTURE_PERMISSION_H_

namespace chrome {

// Purge stale TCC screen capture permission for the bundle id of the running
// process, if applicable.
//
// A stale permission can exist when a previous version of an app's code signing
// designated requirement fails to validate the currently running app. The
// designated requirement active at the time of permission approval is stored in
// a macOS controlled database on disk. If this cached designated requirement is
// unable to validate the current running version of the app then screen capture
// permission will be denied. See https://crbug.com/1307502 for more details.
//
// This function only affects branded builds running on macOS 10.15+; macOS
// began supervising screen capture in macOS 10.15. If it is determined that the
// running app does not have screen capture permission a command will be run to
// purge any active screen capture permission. This will clear out a permission
// with a stale designated requirement while leaving systems with a valid
// permission untouched. A successful purge will only happen once. Subsequent
// calls to this function will be a NOP. If there is a failure while attempting
// to purge, subsequent calls to this function will attempt the purge up to a
// maximum of 3 attempts.
void PurgeStaleScreenCapturePermission();

}  // namespace chrome

#endif  // CHROME_BROWSER_MAC_PURGE_STALE_SCREEN_CAPTURE_PERMISSION_H_
