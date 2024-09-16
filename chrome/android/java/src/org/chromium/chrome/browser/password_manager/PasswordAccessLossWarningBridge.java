// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningHelper;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.chrome.browser.access_loss.PwdAccessLossNotificationCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * Glue code. Used by C++ to launch the password access loss warning bridge. Allows injecting helper
 * from chrome_java.
 */
class PasswordAccessLossWarningBridge {
    PasswordAccessLossWarningHelper mHelper;
    PwdAccessLossNotificationCoordinator mNotificationCoordinator;

    public PasswordAccessLossWarningBridge(
            Activity activity, BottomSheetController bottomSheetController, Profile profile) {
        mHelper =
                new PasswordAccessLossWarningHelper(
                        activity,
                        bottomSheetController,
                        profile,
                        LaunchIntentDispatcher::createCustomTabActivityIntent);
        mNotificationCoordinator =
                new PwdAccessLossNotificationCoordinator(activity.getBaseContext());
    }

    @CalledByNative
    @Nullable
    static PasswordAccessLossWarningBridge create(WindowAndroid windowAndroid, Profile profile) {
        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        if (bottomSheetController == null) {
            return null;
        }
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            return null;
        }
        return new PasswordAccessLossWarningBridge(activity, bottomSheetController, profile);
    }

    @CalledByNative
    public void show(@PasswordAccessLossWarningType int warningType) {
        mHelper.show(warningType);
    }

    // TODO: crbug.com/354886517 - Introduce the showNotification method and call it from the cpp
    // bridge.
}
