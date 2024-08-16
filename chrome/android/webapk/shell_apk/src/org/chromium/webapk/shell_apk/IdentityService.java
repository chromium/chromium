// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

import org.chromium.webapk.lib.common.identity_service.IIdentityService;

/** IdentityService allows browsers to query information about the WebAPK. */
public class IdentityService extends Service {
    private final IIdentityService.Stub mBinder =
            new IIdentityService.Stub() {
                @Override
                public String getRuntimeHostBrowserPackageName() {
                    return HostBrowserUtils.computeHostBrowserPackageName(getApplicationContext());
                }
            };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
}
