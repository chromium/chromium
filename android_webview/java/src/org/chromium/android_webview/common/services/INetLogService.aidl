// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

/**
 * Interface to help with storing net log files in the non-embedded process.
*/
interface INetLogService {
    // Creates a new file in the non-embedded process. The file name is
    // generated based on the PID, the creation time and the package name.
    // This method returns a ParcelFileDescriptor to the embedded process,
    // allowing it to write data to the new file.
    ParcelFileDescriptor streamLog(in long creationTime, in String packageName);
}