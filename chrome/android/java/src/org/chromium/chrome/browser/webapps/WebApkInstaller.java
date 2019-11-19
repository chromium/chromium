// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.metrics.WebApkUma;

/**
 * Java counterpart to webapk_installer.h
 * Contains functionality to install / update WebAPKs.
 * This Java object is created by and owned by the native WebApkInstaller.
 */
public class WebApkInstaller {
    private static final String TAG = "WebApkInstaller";

    /** Weak pointer to the native WebApkInstaller. */
    private long mNativePointer;

    /** Talks to Google Play to install WebAPKs. */
    private final GooglePlayWebApkInstallDelegate mInstallDelegate;

    private WebApkInstaller(long nativePtr) {
        mNativePointer = nativePtr;
        mInstallDelegate = AppHooks.get().getGooglePlayWebApkInstallDelegate();
    }

    @CalledByNative
    private static WebApkInstaller create(long nativePtr) {
        return new WebApkInstaller(nativePtr);
    }

    @CalledByNative
    private void destroy() {
        mNativePointer = 0;
    }

    /**
     * Installs a WebAPK and monitors the installation.
     * @param packageName The package name of the WebAPK to install.
     * @param version The version of WebAPK to install.
     * @param title The title of the WebAPK to display during installation.
     * @param token The token from WebAPK Server.
     * @param source The source (either app banner or menu) that the install of a WebAPK was
     *               triggered.
     * @param icon The primary icon of the WebAPK to install.
     */
    @CalledByNative
    private void installWebApkAsync(final String packageName, int version, final String title,
            String token, final int source, final Bitmap icon) {
        // Check whether the WebAPK package is already installed. The WebAPK may have been installed
        // by another Chrome version (e.g. Chrome Dev). We have to do this check because the Play
        // install API fails silently if the package is already installed.
        if (isWebApkInstalled(packageName)) {
            notify(WebApkInstallResult.SUCCESS);
            return;
        }

        if (mInstallDelegate == null) {
            notify(WebApkInstallResult.FAILURE);
            WebApkUma.recordGooglePlayInstallResult(
                    WebApkUma.GooglePlayInstallResult.FAILED_NO_DELEGATE);
            return;
        }

        Callback<Integer> callback = new Callback<Integer>() {
            @Override
            public void onResult(Integer result) {
                WebApkInstaller.this.notify(result);
                if (result == WebApkInstallResult.FAILURE) return;

                // Stores the source info of WebAPK in WebappDataStorage.
                WebappRegistry.getInstance().register(
                        WebappRegistry.webApkIdForPackage(packageName),
                        new WebappRegistry.FetchWebappDataStorageCallback() {
                            @Override
                            public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                                WebApkInfo webApkInfo = WebApkInfo.create(packageName, null, source,
                                        false /* forceNavigation */,
                                        false /* canUseSplashFromContentProvider */,
                                        null /* shareData */,
                                        null /* shareDataActivityClassName */);
                                storage.updateFromWebappInfo(webApkInfo);
                                storage.updateSource(source);
                                storage.updateTimeOfLastCheckForUpdatedWebManifest();
                            }
                        });
            }
        };
        mInstallDelegate.installAsync(packageName, version, title, token, callback);
    }

    private void notify(@WebApkInstallResult int result) {
        if (mNativePointer != 0) {
            WebApkInstallerJni.get().onInstallFinished(
                    mNativePointer, WebApkInstaller.this, result);
        }
    }

    /**
     * Updates a WebAPK installation.
     * @param packageName The package name of the WebAPK to install.
     * @param version The version of WebAPK to install.
     * @param title The title of the WebAPK to display during installation.
     * @param token The token from WebAPK Server.
     */
    @CalledByNative
    private void updateAsync(
            String packageName, int version, String title, String token) {
        if (mInstallDelegate == null) {
            notify(WebApkInstallResult.FAILURE);
            return;
        }

        Callback<Integer> callback = new Callback<Integer>() {
            @Override
            public void onResult(Integer result) {
                WebApkInstaller.this.notify(result);
            }
        };
        mInstallDelegate.updateAsync(packageName, version, title, token, callback);
    }

    @CalledByNative
    private void checkFreeSpace() {
        new AsyncTask<Integer>() {
            @Override
            protected Integer doInBackground() {
                long availableSpaceInBytes = WebApkUma.getAvailableSpaceAboveLowSpaceLimit();

                if (availableSpaceInBytes > 0) return SpaceStatus.ENOUGH_SPACE;

                long cacheSizeInBytes = WebApkUma.getCacheDirSize();
                if (cacheSizeInBytes + availableSpaceInBytes > 0) {
                    return SpaceStatus.ENOUGH_SPACE_AFTER_FREE_UP_CACHE;
                }
                return SpaceStatus.NOT_ENOUGH_SPACE;
            }

            @Override
            protected void onPostExecute(Integer result) {
                WebApkInstallerJni.get().onGotSpaceStatus(
                        mNativePointer, WebApkInstaller.this, result);
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private boolean isWebApkInstalled(String packageName) {
        return PackageUtils.isPackageInstalled(ContextUtils.getApplicationContext(), packageName);
    }

    @NativeMethods
    interface Natives {
        void onInstallFinished(long nativeWebApkInstaller, WebApkInstaller caller,
                @WebApkInstallResult int result);
        void onGotSpaceStatus(long nativeWebApkInstaller, WebApkInstaller caller, int status);
    }
}
