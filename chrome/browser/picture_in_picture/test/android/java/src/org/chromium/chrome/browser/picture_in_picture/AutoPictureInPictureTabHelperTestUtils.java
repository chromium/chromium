// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.picture_in_picture;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

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
                () -> {
                    if (webContents == null || webContents.isDestroyed()) {
                        // If WebContents is gone, it cannot be in auto-PiP.
                        // This satisfies the condition if we expect PiP to be false.
                        return !expectedInPip;
                    }
                    boolean isInAutoPip =
                            AutoPictureInPictureTabHelperTestUtilsJni.get()
                                    .isInAutoPictureInPicture(webContents);

                    return isInAutoPip == expectedInPip;
                },
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
     * Overrides the is using camera or microphone value for the given {@link WebContents} for
     * testing purposes.
     *
     * @param webContents The WebContents to modify.
     * @param isUsingCameraOrMicrophone The mock value to set.
     */
    public static void setIsUsingCameraOrMicrophone(
            WebContents webContents, boolean isUsingCameraOrMicrophone) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AutoPictureInPictureTabHelperTestUtilsJni.get()
                                .setIsUsingCameraOrMicrophone(
                                        webContents, isUsingCameraOrMicrophone));
    }

    /**
     * Sets the content setting for a given URL and waits for it to be applied.
     *
     * @param profile The profile to set the content setting for.
     * @param contentSettingsType The content setting type to set.
     * @param url The URL to set the content setting for.
     * @param value The content setting value to set.
     */
    public static void setPermission(
            Profile profile,
            @ContentSettingsType.EnumType int contentSettingsType,
            String url,
            @ContentSetting int value) {
        PermissionInfo info =
                new PermissionInfo(
                        contentSettingsType,
                        url,
                        /* embedder= */ null,
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE);
        ThreadUtils.runOnUiThreadBlocking(() -> info.setContentSetting(profile, value));

        // Wait for the setting to be updated.
        CriteriaHelper.pollUiThread(() -> info.getContentSetting(profile) == value);
    }

    /**
     * Gets the number of times the auto-pip dismiss prompt has been shown for the given URL.
     *
     * @param webContents The WebContents to check.
     * @param url The URL to check.
     * @return The number of times the dismiss prompt has been shown.
     */
    public static int getDismissCountForTesting(WebContents webContents, String url) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AutoPictureInPictureTabHelperTestUtilsJni.get()
                                .getDismissCountForTesting(webContents, new GURL(url)));
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

        void setIsUsingCameraOrMicrophone(
                @JniType("content::WebContents*") WebContents webContents,
                boolean isUsingCameraOrMicrophone);

        int getDismissCountForTesting(
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("GURL") GURL url);
    }
}
