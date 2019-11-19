// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.Log;

import org.chromium.base.task.TaskTraits;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.identity_service.IIdentityService;

/**
 * Provides APIs for browsers to communicate with WebAPK Identity services. Each WebAPK has its own
 * "WebAPK Identity service".
 */
public class WebApkIdentityServiceClient {
    /**
     * Used to notify the consumer after checking whether the caller browser backs the WebAPK.
     *  |browserPackageName| is the package name of the browser which backs the WebAPK.
     */
    public interface CheckBrowserBacksWebApkCallback {
        void onChecked(boolean doesBrowserBackWebApk, String browserPackageName);
    }

    /**
     * Before shell APK version 6, all WebAPKs are installed from browsers, and that browser is the
     * runtime host and specified in the WebAPK's AndroidManifest.xml. In shell APK version 6, we
     * introduced logic to allow user to choose runtime host browser for WebAPKs not bound to any
     * browser, and for WebAPKs installed from browsers when the browser is subsequently
     * uninstalled. However, the browser cannot track of WebAPKs which are backed by the browser
     * because the browser is not notified if a user changes the runtime host of a WebAPK by
     * clearing the WebAPK's data. Besides, the browser loses the knowledge of WebAPKs if a user
     * clears the browser's data. Therefore, a browser doesn't know whether it is the runtime host
     * of a WebAPK without asking the WebAPK. An Identity service is introduced in shell APK version
     * 16 to allow browsers to query the runtime host of a WebAPK.
     */
    public static final int SHELL_APK_VERSION_SUPPORTING_SWITCH_RUNTIME_HOST = 6;

    public static final String ACTION_WEBAPK_IDENTITY_SERVICE = "org.webapk.IDENTITY_SERVICE_API";
    private static final String TAG = "WebApkIdentityService";

    private static WebApkIdentityServiceClient sInstance;

    /** Manages connections between the browser application and WebAPK Identity services. */
    private WebApkServiceConnectionManager mConnectionManager;

    public static WebApkIdentityServiceClient getInstance(TaskTraits uiThreadTaskTraits) {
        if (sInstance == null) {
            sInstance = new WebApkIdentityServiceClient(uiThreadTaskTraits);
        }
        return sInstance;
    }

    private WebApkIdentityServiceClient(TaskTraits uiThreadTaskTraits) {
        mConnectionManager = new WebApkServiceConnectionManager(
                uiThreadTaskTraits, null /* category */, ACTION_WEBAPK_IDENTITY_SERVICE);
    }

    /**
     * Checks whether a WebAPK is backed by the browser with {@link browserContext}.
     * @param browserContext The browser context.
     * @param webApkPackageName The package name of the WebAPK.
     * @param callback The callback to be called after querying the runtime host is done.
     */
    public void checkBrowserBacksWebApkAsync(final Context browserContext,
            final String webApkPackageName, final CheckBrowserBacksWebApkCallback callback) {
        WebApkServiceConnectionManager.ConnectionCallback connectionCallback =
                new WebApkServiceConnectionManager.ConnectionCallback() {
                    @Override
                    public void onConnected(IBinder service) {
                        String browserPackageName = browserContext.getPackageName();
                        if (service == null) {
                            onGotWebApkRuntimeHost(browserPackageName,
                                    maybeExtractRuntimeHostFromMetaData(
                                            browserContext, webApkPackageName),
                                    callback);
                            return;
                        }

                        IIdentityService identityService =
                                IIdentityService.Stub.asInterface(service);
                        String runtimeHost = null;
                        try {
                            // The runtime host could be null if the WebAPK hasn't bound to any
                            // browser yet.
                            runtimeHost = identityService.getRuntimeHostBrowserPackageName();
                        } catch (RemoteException e) {
                            Log.w(TAG, "Failed to get runtime host from the Identity service.");
                        }
                        onGotWebApkRuntimeHost(browserPackageName, runtimeHost, callback);
                    }
                };
        mConnectionManager.connect(browserContext, webApkPackageName, connectionCallback);
    }

    /**
     * Called after fetching the WebAPK's backing browser.
     * @param browserPackageName The browser's package name.
     * @param webApkBackingBrowserPackageName The package name of the WebAPK's backing browser.
     * @param callback The callback to notify whether {@link browserPackageName} backs the WebAPK.
     */
    private static void onGotWebApkRuntimeHost(String browserPackageName,
            String webApkBackingBrowserPackageName, CheckBrowserBacksWebApkCallback callback) {
        callback.onChecked(TextUtils.equals(webApkBackingBrowserPackageName, browserPackageName),
                webApkBackingBrowserPackageName);
    }

    /**
     * Extracts the backing browser from the WebAPK's meta data.
     * See {@link WebApkIdentityServiceClient#SHELL_APK_VERSION_SUPPORTING_SWITCH_RUNTIME_HOST} for
     * more details.
     */
    private static String maybeExtractRuntimeHostFromMetaData(
            Context context, String webApkPackageName) {
        Bundle metadata = readMetaData(context, webApkPackageName);
        if (metadata == null
                || metadata.getInt(WebApkMetaDataKeys.SHELL_APK_VERSION)
                        >= SHELL_APK_VERSION_SUPPORTING_SWITCH_RUNTIME_HOST) {
            // The backing browser in the WebAPK's meta data may not be the one which actually backs
            // the WebAPK. The user may have switched the backing browser.
            return null;
        }

        return metadata.getString(WebApkMetaDataKeys.RUNTIME_HOST);
    }

    /** Returns the <meta-data> in the Android Manifest of the given package name. */
    private static Bundle readMetaData(Context context, String packageName) {
        ApplicationInfo ai = null;
        try {
            ai = context.getPackageManager().getApplicationInfo(
                    packageName, PackageManager.GET_META_DATA);
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }
        return ai.metaData;
    }

    /** Disconnects all the connections to WebAPK Identity services. */
    public static void disconnectAll(Context appContext) {
        if (sInstance == null) return;

        sInstance.mConnectionManager.disconnectAll(appContext);
    }
}
