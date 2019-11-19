// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.webapps.WebApkActivity;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.ui.base.WindowAndroid;

/**
 * Handles requesting the android runtime permissions for the permission update infobar.
 */
class PermissionUpdateInfoBarDelegate implements PermissionCallback {
    private final WebContents mWebContents;
    private final String[] mAndroidPermisisons;
    private long mNativePtr;
    private ActivityStateListener mActivityStateListener;

    @CalledByNative
    private static PermissionUpdateInfoBarDelegate create(
            long nativePtr, WebContents webContents, String[] permissions) {
        return new PermissionUpdateInfoBarDelegate(nativePtr, webContents, permissions);
    }

    private PermissionUpdateInfoBarDelegate(
            long nativePtr, WebContents webContents, String[] permissions) {
        mNativePtr = nativePtr;
        mAndroidPermisisons = permissions;
        mWebContents = webContents;
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePtr = 0;
        if (mActivityStateListener != null) {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
            mActivityStateListener = null;
        }
    }

    @CalledByNative
    private void requestPermissions() {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) {
            PermissionUpdateInfoBarDelegateJni.get().onPermissionResult(
                    mNativePtr, PermissionUpdateInfoBarDelegate.this, false);
            return;
        }

        boolean canRequestAllPermissions = true;
        for (int i = 0; i < mAndroidPermisisons.length; i++) {
            canRequestAllPermissions &=
                    (windowAndroid.hasPermission(mAndroidPermisisons[i])
                            || windowAndroid.canRequestPermission(mAndroidPermisisons[i]));
        }

        Activity activity = windowAndroid.getActivity().get();
        if (canRequestAllPermissions) {
            windowAndroid.requestPermissions(mAndroidPermisisons, this);
            if (activity instanceof WebApkActivity) {
                WebApkUma.recordAndroidRuntimePermissionPromptInWebApk(mAndroidPermisisons);
            }
        } else {
            if (activity == null) {
                PermissionUpdateInfoBarDelegateJni.get().onPermissionResult(
                        mNativePtr, PermissionUpdateInfoBarDelegate.this, false);
                return;
            }

            mActivityStateListener = new ActivityStateListener() {
                @Override
                public void onActivityStateChange(Activity activity, int newState) {
                    if (newState == ActivityState.DESTROYED) {
                        ApplicationStatus.unregisterActivityStateListener(this);
                        mActivityStateListener = null;

                        PermissionUpdateInfoBarDelegateJni.get().onPermissionResult(
                                mNativePtr, PermissionUpdateInfoBarDelegate.this, false);
                    } else if (newState == ActivityState.RESUMED) {
                        ApplicationStatus.unregisterActivityStateListener(this);
                        mActivityStateListener = null;

                        notifyPermissionResult();
                    }
                }
            };
            ApplicationStatus.registerStateListenerForActivity(mActivityStateListener, activity);

            Intent settingsIntent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
            settingsIntent.setData(
                    Uri.parse("package:" + ContextUtils.getApplicationContext().getPackageName()));
            settingsIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(settingsIntent);
        }
    }

    @Override
    public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
        notifyPermissionResult();
    }

    private void notifyPermissionResult() {
        boolean hasAllPermissions = true;
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) {
            hasAllPermissions = false;
        } else {
            for (int i = 0; i < mAndroidPermisisons.length; i++) {
                hasAllPermissions &= windowAndroid.hasPermission(mAndroidPermisisons[i]);
            }
        }
        if (mNativePtr != 0) {
            PermissionUpdateInfoBarDelegateJni.get().onPermissionResult(
                    mNativePtr, PermissionUpdateInfoBarDelegate.this, hasAllPermissions);
        }
    }

    @NativeMethods
    interface Natives {
        void onPermissionResult(long nativePermissionUpdateInfoBarDelegate,
                PermissionUpdateInfoBarDelegate caller, boolean allPermissionsGranted);
    }
}
