// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static androidx.browser.trusted.LaunchHandlerClientMode.FOCUS_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_NEW;

import androidx.browser.trusted.LaunchHandlerClientMode.ClientMode;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;

/**
 * Manages web application launch configurations based on client mode. Provides methods to process
 * client mode and work with launch queue.
 */
@JNINamespace("webapps")
public class WebAppLaunchHandler {

    public static final @ClientMode int DEFAULT_CLIENT_MODE = NAVIGATE_EXISTING;

    /** The LaunchParams value to be sent to a web app launch queue on app launch. */
    private final WebAppLaunchParams mLaunchParams;

    /**
     * Retrieves the ClientMode enum value from a given AndroidX enum. Defaults to
     * DEFAULT_CLIENT_MODE if the value is invalid or AUTO.
     *
     * @param clientMode The AndroidX representation of the client mode.
     * @return The corresponding ClientMode enum value.
     */
    public static @ClientMode int getClientMode(@ClientMode int clientMode) {
        if (Arrays.asList(NAVIGATE_EXISTING, FOCUS_EXISTING, NAVIGATE_NEW).contains(clientMode)) {
            return clientMode;
        } else {
            return DEFAULT_CLIENT_MODE;
        }
    }

    public WebAppLaunchHandler(@ClientMode int clientMode, String targetUrl, String packageName) {
        mLaunchParams = getLaunchParams(clientMode, targetUrl, packageName);
    }

    /** Returns whether this launch triggers a navigation */
    public boolean getStartNewNavigation() {
        return mLaunchParams.startNewNavigation;
    }

    /**
     * Initiates the launch process for a web application tab notifying a launch queue.
     *
     * @param webContents Web contents object of the tab is being launched.
     */
    public void notifyLaunchQueue(WebContents webContents) {
        WebAppLaunchHandlerJni.get()
                .notifyLaunchQueue(
                        webContents,
                        mLaunchParams.startNewNavigation,
                        mLaunchParams.targetUrl,
                        mLaunchParams.packageName);
    }

    /**
     * Generates WebAppLaunchParams based on the AndroidX representation of the client mode.
     *
     * @param clientModeParam The AndroidX representation of the client mode.
     * @param targetUrl The URL to launch.
     * @param packageName Android package name of the web app is being launched.
     * @return The generated WebAppLaunchParams object.
     */
    private static WebAppLaunchParams getLaunchParams(
            @ClientMode int clientModeParam, String targetUrl, String packageName) {
        @ClientMode int clientMode = getClientMode(clientModeParam);

        boolean startedNewNavigation = clientMode != FOCUS_EXISTING;
        return new WebAppLaunchParams(startedNewNavigation, targetUrl, packageName);
    }

    /**
     * Takes the WebContents object of the tab that is being launched and notifies the launch queue
     * with this object and associated launch parameters.
     */
    @NativeMethods
    public interface Natives {
        void notifyLaunchQueue(
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("bool") boolean startNewNavigation,
                @JniType("std::string") String startUrl,
                @JniType("std::string") String packageName);
    }
}
