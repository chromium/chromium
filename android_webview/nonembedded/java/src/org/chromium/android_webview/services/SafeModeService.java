// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.IBinder;
import android.os.Process;

import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.common.services.ISafeModeService;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.util.List;

import javax.annotation.concurrent.GuardedBy;

/**
 * A Service to manage WebView SafeMode state. This Service exposes an interface by which trusted
 * services (as determined by a hardcoded allowlist) can enable or disable WebView SafeMode. This
 * Service is then responsible for propagating this information to embedded WebView implementations
 * as they start up.
 */
public final class SafeModeService extends Service {
    private static final String TAG = "WebViewSafeMode";

    private static final Object sLock = new Object();

    private boolean isCallerTrusted() {
        final Context context = ContextUtils.getApplicationContext();
        PackageManager pm = context.getPackageManager();
        String[] packagesInUid = pm.getPackagesForUid(Binder.getCallingUid());

        if (packagesInUid == null) {
            Log.e(TAG,
                    "Unable to find any packages associated with calling UID ("
                            + Binder.getCallingUid() + ")");
            return false;
        }

        if (Binder.getCallingUid() == Process.myUid()) {
            // Trust the nonembedded WebView provider UID. We don't currently expect the WebView
            // provider itself to enable SafeMode in production (although we may consider this in
            // the future). Right now, this is permitted for testing purposes.
            return true;
        }

        // TODO(ntfschr): add actual trusted services once SafeMode is ready for production.
        return false;
    }

    private final ISafeModeService.Stub mBinder = new ISafeModeService.Stub() {
        @Override
        public void setSafeMode(List<String> actions) {
            if (!isCallerTrusted()) {
                throw new SecurityException("setSafeMode() may only be called by a trusted app");
            }

            synchronized (sLock) {
                SafeModeService.this.setSafeMode(actions);
            }
        }
    };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    /**
     * Sets the SafeMode config. This includes persisting the set of actions, toggling component
     * state, etc.
     */
    @GuardedBy("sLock")
    private void setSafeMode(List<String> actions) {
        // TODO(ntfschr): persist the list of actions once we figure out the right representation on
        // disk.
        ComponentName safeModeComponent =
                new ComponentName(this, SafeModeController.SAFE_MODE_STATE_COMPONENT);

        int newState = actions == null || actions.isEmpty()
                ? PackageManager.COMPONENT_ENABLED_STATE_DEFAULT
                : PackageManager.COMPONENT_ENABLED_STATE_ENABLED;
        getPackageManager().setComponentEnabledSetting(
                safeModeComponent, newState, PackageManager.DONT_KILL_APP);
    }
}
