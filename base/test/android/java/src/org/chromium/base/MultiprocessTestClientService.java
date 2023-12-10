// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.base.process_launcher.ChildProcessService;

/** The service implementation used to host all multiprocess test client code. */
public class MultiprocessTestClientService extends Service {
    private ChildProcessService mService;

    public MultiprocessTestClientService() {}

    @Override
    public void onCreate() {
        super.onCreate();
        mService =
                new ChildProcessService(
                        new MultiprocessTestClientServiceDelegate(), this, getApplicationContext());
        mService.onCreate();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mService.onDestroy();
        mService = null;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mService.onBind(intent);
    }
}
