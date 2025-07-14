// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.picture_in_picture;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
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
     * Polls the UI thread until the auto-PiP trigger state for the given {@link WebContents}
     * matches the {@code expectedInPip} state.
     *
     * <p>This state is true only when the browser automatically enters Picture-in-Picture, and
     * false if PiP was entered manually or is not active.
     *
     * @param webContents The WebContents to check.
     * @param expectedInPip The expected auto picture-in-picture state.
     * @param failureMessage The message to display if the criteria is not met.
     */
    public static void waitForAutoPictureInPictureState(
            WebContents webContents, boolean expectedInPip, String failureMessage) {
        CriteriaHelper.pollUiThread(
                () ->
                        AutoPictureInPictureTabHelperTestUtilsJni.get()
                                        .isInAutoPictureInPicture(webContents)
                                == expectedInPip,
                failureMessage);
    }

    /**
     * Polls the UI thread until the PiP window visibility state for the given {@link WebContents}
     * matches the {@code expectedInPip} state.
     *
     * <p>This state is true if a Picture-in-Picture window is visible, regardless of whether it was
     * triggered automatically or manually.
     *
     * @param webContents The WebContents to check.
     * @param expectedInPip The expected picture-in-picture video state.
     * @param failureMessage The message to display if the criteria is not met.
     */
    public static void waitForPictureInPictureVideoState(
            WebContents webContents, boolean expectedInPip, String failureMessage) {
        CriteriaHelper.pollUiThread(
                () ->
                        AutoPictureInPictureTabHelperTestUtilsJni.get()
                                        .hasPictureInPictureVideo(webContents)
                                == expectedInPip,
                failureMessage);
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
        void initializeForTesting(@JniType("content::WebContents*") WebContents webContents);

        boolean isInAutoPictureInPicture(@JniType("content::WebContents*") WebContents webContents);

        boolean hasAutoPictureInPictureBeenRegistered(
                @JniType("content::WebContents*") WebContents webContents);

        boolean hasPictureInPictureVideo(@JniType("content::WebContents*") WebContents webContents);

        void setHasHighMediaEngagement(
                @JniType("content::WebContents*") WebContents webContents,
                boolean hasHighEngagement);

        void setHasAudioFocusForTesting(
                @JniType("content::WebContents*") WebContents webContents, boolean hasFocus);
    }
}
