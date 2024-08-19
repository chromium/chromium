// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;

import java.lang.reflect.Constructor;

/**
 * Shell class for services provided by WebAPK to Chrome. Extracts code with implementation of
 * services from .dex file in Chrome APK.
 */
public class WebApkServiceFactory extends Service {
    /** Key for passing uid of only application allowed to call the service's methods. */
    public static final String KEY_HOST_BROWSER_UID = "host_browser_uid";

    private static final String TAG = "cr_WebApkServiceFactory";

    /** Name of the class with IBinder API implementation. */
    private static final String WEBAPK_SERVICE_IMPL_CLASS_NAME =
            "org.chromium.webapk.lib.runtime_library.WebApkServiceImpl";

    /** Key for passing id of icon to represent WebAPK notifications in status bar. */
    private static final String KEY_SMALL_ICON_ID = "small_icon_id";

    @Override
    public IBinder onBind(Intent intent) {
        final String hostBrowserPackage = HostBrowserUtils.computeHostBrowserPackageName(this);
        if (!HostBrowserUtils.doesBrowserSupportNotificationDelegation(hostBrowserPackage)) {
            Log.w(TAG, "Host browser does not support WebAPK notification delegation.");
            return null;
        }
        ClassLoader webApkClassLoader =
                HostBrowserClassLoader.getClassLoaderInstance(
                        this, hostBrowserPackage, WEBAPK_SERVICE_IMPL_CLASS_NAME);
        if (webApkClassLoader == null) {
            Log.w(TAG, "Unable to create ClassLoader.");
            return null;
        }

        try {
            Class<?> webApkServiceImplClass =
                    webApkClassLoader.loadClass(WEBAPK_SERVICE_IMPL_CLASS_NAME);
            Constructor<?> webApkServiceImplConstructor =
                    webApkServiceImplClass.getConstructor(Context.class, Bundle.class);
            int hostBrowserUid = WebApkUtils.getRemotePackageUid(this, hostBrowserPackage);
            Bundle bundle = new Bundle();
            bundle.putInt(KEY_SMALL_ICON_ID, WebApkUtils.getNotificationSmallIconId());
            bundle.putInt(KEY_HOST_BROWSER_UID, hostBrowserUid);
            IBinder webApkServiceImpl =
                    (IBinder) webApkServiceImplConstructor.newInstance(new Object[] {this, bundle});
            return new WebApkServiceImplWrapper(this, webApkServiceImpl, hostBrowserUid);
        } catch (Exception e) {
            Log.w(TAG, "Unable to create WebApkServiceImpl.");
            e.printStackTrace();
            return null;
        }
    }
}
