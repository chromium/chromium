// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.picture_in_picture;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.content_public.browser.WebContents;

/** Utility class for testing AutoPictureInPictureTabHelper C++ logic via JNI. */
@JNINamespace("picture_in_picture")
public class AutoPictureInPictureTabHelperTestUtils {
    private AutoPictureInPictureTabHelperTestUtils() {}

    /**
     * Initializes the C++ test harness for the given {@link WebContents}. This must be called
     * before any other test utility methods for a given WebContents.
     */
    public static void initializeForTesting(WebContents webContents) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AutoPictureInPictureTabHelperTestUtilsJni.get()
                                .initializeForTesting(webContents));
    }

    /**
     * Checks if the given {@link WebContents} is currently in auto picture-in-picture. Because it's
     * used for polling on the UI thread, it must not block.
     */
    public static boolean isInAutoPictureInPicture(WebContents webContents) {
        return AutoPictureInPictureTabHelperTestUtilsJni.get()
                .isInAutoPictureInPicture(webContents);
    }

    /**
     * Polls the UI thread until the auto picture-in-picture state for the given {@link WebContents}
     * matches the {@code expectedInPip} state.
     *
     * @param webContents The WebContents to check.
     * @param expectedInPip The expected auto picture-in-picture state.
     * @param failureMessage The message to display if the criteria is not met.
     */
    public static void waitForAutoPictureInPictureState(
            WebContents webContents, boolean expectedInPip, String failureMessage) {
        CriteriaHelper.pollUiThread(
                () -> isInAutoPictureInPicture(webContents) == expectedInPip, failureMessage);
    }

    /** Checks if auto picture-in-picture has been registered for the given {@link WebContents}. */
    public static boolean hasAutoPictureInPictureBeenRegistered(WebContents webContents) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AutoPictureInPictureTabHelperTestUtilsJni.get()
                                .hasAutoPictureInPictureBeenRegistered(webContents));
    }

    /**
     * Overrides the media engagement score for the given {@link WebContents} for testing purposes.
     *
     * @param webContents The WebContents to modify.
     * @param hasHighEngagement The mock engagement value to set.
     */
    public static void setHasHighMediaEngagement(
            WebContents webContents, boolean hasHighEngagement) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AutoPictureInPictureTabHelperTestUtilsJni.get()
                                .setHasHighMediaEngagement(webContents, hasHighEngagement));
    }

    /**
     * Sets a mock audio focus state for the given {@link WebContents} for testing purposes.
     *
     * @param webContents The WebContents to modify.
     * @param hasFocus The mock audio focus state to set.
     */
    public static void setHasAudioFocusForTesting(WebContents webContents, boolean hasFocus) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AutoPictureInPictureTabHelperTestUtilsJni.get()
                                .setHasAudioFocusForTesting(webContents, hasFocus));
    }

    @NativeMethods
    interface Natives {
        void initializeForTesting(WebContents webContents);

        boolean isInAutoPictureInPicture(WebContents webContents);

        boolean hasAutoPictureInPictureBeenRegistered(WebContents webContents);

        void setHasHighMediaEngagement(WebContents webContents, boolean hasHighEngagement);

        void setHasAudioFocusForTesting(WebContents webContents, boolean hasFocus);
    }
}
