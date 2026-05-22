// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import android.app.Activity;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceInput;

/** Shared utility methods for the Keyboard Accessory component. */
@NullMarked
public class KeyboardAccessoryUtils {
    private KeyboardAccessoryUtils() {}

    private static final int MIN_WINDOW_HEIGHT_FOR_UNDOCKED_BAR_DP = 480;
    private static final int MIN_WINDOW_WIDTH_FOR_UNDOCKED_BAR_DP = 600;
    private static final int EXPANDED_WINDOW_WIDTH_FOR_UNDOCKED_BAR_DP = 840;

    /**
     * Returns true if the device matches the Large Form Factor criteria for the keyboard accessory.
     * Accounts for window dimensions, multi-window split states, and physical keyboard connection.
     */
    public static boolean isLargeFormFactor(
            @Nullable Activity activity, @Nullable KeyboardVisibilityDelegate keyboardDelegate) {
        if (activity == null || keyboardDelegate == null) {
            return false;
        }

        int windowWidthDp = activity.getResources().getConfiguration().screenWidthDp;
        int windowHeightDp = activity.getResources().getConfiguration().screenHeightDp;

        View contentView = activity.findViewById(android.R.id.content);
        boolean isPhysicalKeyboardConnected =
                DeviceInput.supportsAlphabeticKeyboard()
                        && !keyboardDelegate.isKeyboardShowing(contentView);

        if (windowWidthDp > EXPANDED_WINDOW_WIDTH_FOR_UNDOCKED_BAR_DP) {
            return windowHeightDp > MIN_WINDOW_HEIGHT_FOR_UNDOCKED_BAR_DP
                    || isPhysicalKeyboardConnected;
        }

        return windowWidthDp > MIN_WINDOW_WIDTH_FOR_UNDOCKED_BAR_DP
                && windowHeightDp > MIN_WINDOW_HEIGHT_FOR_UNDOCKED_BAR_DP
                && isPhysicalKeyboardConnected;
    }
}
