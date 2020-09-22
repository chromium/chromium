// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.StrictMode;

import org.chromium.webapk.lib.common.identity_service.IIdentityService;

/** IdentityService allows browsers to query information about the WebAPK. */
public class IdentityService extends Service {
    private final IIdentityService.Stub mBinder = new IIdentityService.Stub() {
        @Override
        public String getRuntimeHostBrowserPackageName() {
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
            try {
                return HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(
                        getApplicationContext());
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
