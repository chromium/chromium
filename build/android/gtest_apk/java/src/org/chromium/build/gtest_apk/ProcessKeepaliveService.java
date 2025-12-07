// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.gtest_apk;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/**
 * Noop service that keeps the process alive long enough to submit test results after the Activities
 * finish.
 */
public class ProcessKeepaliveService extends Service {
    private final IProcessKeepaliveService.Stub mBinder = new IProcessKeepaliveService.Stub() {};

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
