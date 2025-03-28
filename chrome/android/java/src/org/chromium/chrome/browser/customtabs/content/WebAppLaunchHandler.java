// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static androidx.browser.trusted.LaunchHandlerClientMode.FOCUS_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_NEW;

import androidx.browser.trusted.LaunchHandlerClientMode.ClientMode;

import java.util.Arrays;

/**
 * Manages web application launch configurations based on client mode. Provides methods to process
 * client mode and generate launch parameters.
 */
public class WebAppLaunchHandler {

    public static final @ClientMode int DEFAULT_CLIENT_MODE = NAVIGATE_EXISTING;

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

    /**
     * Generates WebAppLaunchParams based on the AndroidX representation of the client mode.
     *
     * @param clientModeParam The AndroidX representation of the client mode.
     * @param targetUrl The URL to launch.
     * @param packageName Android package name of the web app is being launched.
     * @return The generated WebAppLaunchParams object.
     */
    public static WebAppLaunchParams getLaunchParams(
            @ClientMode int clientModeParam, String targetUrl, String packageName) {
        @ClientMode int clientMode = getClientMode(clientModeParam);

        boolean startedNewNavigation = clientMode != FOCUS_EXISTING;
        return new WebAppLaunchParams(startedNewNavigation, targetUrl, packageName);
    }

    private WebAppLaunchHandler() {}
}
