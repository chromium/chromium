// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.PackageUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUmaRecorder;
import org.chromium.components.webapps.WebApkInstallResult;

/**
 * Java counterpart to webapk_installer.h
 * Contains functionality to install / update WebAPKs.
 * This Java object is created by and owned by the native WebApkInstaller.
 */
public class WebApkInstaller {

    /** Weak pointer to the native WebApkInstaller. */
    private long mNativePointer;

    /** Talks to Google Play to install WebAPKs. */
    private final GooglePlayWebApkInstallDelegate mInstallDelegate;

    private final String mWebApkServerUrl;

    private WebApkInstaller(long nativePtr) {
        mNativePointer = nativePtr;
        mInstallDelegate = AppHooks.get().getGooglePlayWebApkInstallDelegate();
        mWebApkServerUrl = AppHooks.get().getWebApkServerUrl();
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
     *
     * @param packageName The package name of the WebAPK to install.
     * @param version The version of WebAPK to install.
     * @param title The title of the WebAPK to display during installation.
     * @param token The token from WebAPK Server.
     * @param source The source (either app banner or menu) that the install of a WebAPK was
     *     triggered.
     */
    @CalledByNative
    private void installWebApkAsync(
            @JniType("std::string") final String packageName,
            int version,
            @JniType("std::u16string") final String title,
            @JniType("std::string") String token,
            final int source) {
        // Check whether the WebAPK package is already installed. The WebAPK may have been installed
        // by another Chrome version (e.g. Chrome Dev). We have to do this check because the Play
        // install API fails silently if the package is already installed.
        if (isWebApkInstalled(packageName)) {
            notify(WebApkInstallResult.SUCCESS);
            return;
        }

        if (mInstallDelegate == null) {
            notify(WebApkInstallResult.NO_INSTALLER);
            WebApkUmaRecorder.recordGooglePlayInstallResult(
                    WebApkUmaRecorder.GooglePlayInstallResult.FAILED_NO_DELEGATE);
            return;
        }

        Callback<Integer> callback =
                (Integer result) -> {
                    WebApkInstaller.this.notify(result);
                    if (result == WebApkInstallResult.FAILURE) return;
                    var intentDataProvider =
                            WebApkIntentDataProviderFactory.create(
                                    new Intent(),
                                    packageName,
                                    null,
                                    source,
                                    /* forceNavigation= */ false,
                                    /* canUseSplashFromContentProvider= */ false,
                                    /* shareData= */ null,
                                    /* shareDataActivityClassName= */ null);

                    // Stores the source info of WebAPK in WebappDataStorage.
                    WebappRegistry.FetchWebappDataStorageCallback fetchCallback =
                            (WebappDataStorage storage) -> {
                                storage.updateFromWebappIntentDataProvider(intentDataProvider);
                                storage.updateSource(source);
                                storage.updateTimeOfLastCheckForUpdatedWebManifest();
                                WebApkSyncService.onWebApkUsed(
                                        intentDataProvider, storage, true /* isInstall */);
                            };
                    WebappRegistry.getInstance()
                            .register(
                                    WebappIntentUtils.getIdForWebApkPackage(packageName),
                                    fetchCallback);
                };
        mInstallDelegate.installAsync(packageName, version, title, token, callback);
    }

    private void notify(@WebApkInstallResult int result) {
        if (mNativePointer != 0) {
            WebApkInstallerJni.get().onInstallFinished(mNativePointer, result);
        }
    }

    /**
     * Updates a WebAPK installation.
     *
     * @param packageName The package name of the WebAPK to install.
     * @param version The version of WebAPK to install.
     * @param title The title of the WebAPK to display during installation.
     * @param token The token from WebAPK Server.
     */
    @CalledByNative
    private void updateAsync(
            @JniType("std::string") String packageName,
            int version,
            @JniType("std::u16string") String title,
            @JniType("std::string") String token) {
        if (mInstallDelegate == null) {
            notify(WebApkInstallResult.NO_INSTALLER);
            return;
        }

        Callback<Integer> callback =
                new Callback<Integer>() {
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
                long availableSpaceInBytes =
                        WebApkUmaRecorder.getAvailableSpaceAboveLowSpaceLimit();

                if (availableSpaceInBytes > 0) return SpaceStatus.ENOUGH_SPACE;

                long cacheSizeInBytes = WebApkUmaRecorder.getCacheDirSize();
                if (cacheSizeInBytes + availableSpaceInBytes > 0) {
                    return SpaceStatus.ENOUGH_SPACE_AFTER_FREE_UP_CACHE;
                }
                return SpaceStatus.NOT_ENOUGH_SPACE;
            }

            @Override
            protected void onPostExecute(Integer result) {
                WebApkInstallerJni.get().onGotSpaceStatus(mNativePointer, result);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @CalledByNative
    private @JniType("std::string") String getWebApkServerUrl() {
        return mWebApkServerUrl;
    }

    private boolean isWebApkInstalled(String packageName) {
        return PackageUtils.isPackageInstalled(packageName);
    }

    @NativeMethods
    interface Natives {
        void onInstallFinished(long nativeWebApkInstaller, @WebApkInstallResult int result);

        void onGotSpaceStatus(long nativeWebApkInstaller, int status);
    }
}
