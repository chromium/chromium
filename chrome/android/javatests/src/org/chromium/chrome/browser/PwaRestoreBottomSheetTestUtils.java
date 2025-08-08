// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

import java.util.concurrent.TimeoutException;

/** Static methods for use in tests to manipulate the PWA App list for restoring. */
@JNINamespace("webapps")
public class PwaRestoreBottomSheetTestUtils {
    private static final CallbackHelper sCallbackHelper = new CallbackHelper();

    public static void waitForWebApkDatabaseInitialization() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                    PwaRestoreBottomSheetTestUtilsJni.get()
                            .waitForWebApkDatabaseInitialization(
                                    ProfileManager.getLastUsedRegularProfile());
                });
        sCallbackHelper.waitForNext();
    }

    /** Set the app list to use for testing. */
    public static void setAppListForRestoring(String[][] appList) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PwaRestoreBottomSheetTestUtilsJni.get()
                            .setAppListForRestoring(
                                    appList, ProfileManager.getLastUsedRegularProfile());
                });
    }

    @CalledByNative
    private static void onWebApkDatabaseInitialized(boolean success) {
        sCallbackHelper.notifyCalled();
    }

    @NativeMethods
    interface Natives {
        void waitForWebApkDatabaseInitialization(@JniType("Profile*") Profile profile);

        void setAppListForRestoring(String[][] appList, @JniType("Profile*") Profile profile);
    }
}
