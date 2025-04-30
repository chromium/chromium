// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static androidx.browser.trusted.LaunchHandlerClientMode.FOCUS_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_EXISTING;
import static androidx.browser.trusted.LaunchHandlerClientMode.NAVIGATE_NEW;

import android.net.Uri;
import android.text.TextUtils;

import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.LaunchHandlerClientMode.ClientMode;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.List;

/**
 * Manages web application launch configurations based on client mode. Provides methods to process
 * client mode and work with launch queue.
 */
@JNINamespace("webapps")
public class WebAppLaunchHandler {

    public static final @ClientMode int DEFAULT_CLIENT_MODE = NAVIGATE_EXISTING;

    private final WebContents mWebContents;
    private final CustomTabActivityNavigationController mNavigationController;
    private final Verifier mVerifier;
    private final CurrentPageVerifier mCurrentPageVerfier;

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
     * Creates a new instance of {@link WebAppLaunchHandler}.
     *
     * @param verifier The {@link Verifier} to use for verifying the target url.
     * @param currentPageVerifier The {@link CurrentPageVerifier} to use for verifying the current
     *     page.
     * @param navigationController The {@link CustomTabActivityNavigationController} to handle
     *     navigation within the Custom Tab.
     * @param webContents The {@link WebContents} associated with the tab.
     * @return A new {@link WebAppLaunchHandler} instance.
     */
    public static WebAppLaunchHandler create(
            Verifier verifier,
            CurrentPageVerifier currentPageVerifier,
            CustomTabActivityNavigationController navigationController,
            WebContents webContents) {
        return new WebAppLaunchHandler(
                verifier, currentPageVerifier, navigationController, webContents);
    }

    private WebAppLaunchHandler(
            Verifier verifier,
            CurrentPageVerifier currentPageVerifier,
            CustomTabActivityNavigationController navigationController,
            WebContents webContents) {
        mWebContents = webContents;
        mNavigationController = navigationController;
        mVerifier = verifier;
        mCurrentPageVerfier = currentPageVerifier;
    }

    /**
     * Generates WebAppLaunchParams based on the AndroidX representation of the client mode.
     *
     * @param newNavigationStarted Whether this launch triggered a navigation.
     * @param targetUrl The URL to launch.
     * @param packageName Android package name of the web app is being launched.
     * @param fileHandlingData Files to be opened for a case it's file open launch. Optional param.
     * @return The generated WebAppLaunchParams object.
     */
    private WebAppLaunchParams getLaunchParams(
            boolean newNavigationStarted,
            String targetUrl,
            String packageName,
            @Nullable FileHandlingData fileHandlingData) {
        List<Uri> fileUris = null;
        if (fileHandlingData != null) {
            fileUris = fileHandlingData.uris;
        }

        return new WebAppLaunchParams(newNavigationStarted, targetUrl, packageName, fileUris);
    }

    /**
     * Handles an intent that triggers a TWA creation. It doesn't trigger a url loading because in
     * the case of initial intent it has been triggered earlier. Passes launch params to a launch
     * queue. Always uses startNewNavigation = true because opening new custom tab is always url
     * loading. In case if the target url provided in the intentDataProvider is out of scope of the
     * TWA a launch queue will not be notified.
     *
     * @param intentDataProvider Provides incoming intent with Custom Tabs specific customization
     *     data.
     */
    public void handleInitialIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        WebAppLaunchParams launchParams =
                getLaunchParams(
                        /* newNavigationStarted= */ true,
                        intentDataProvider.getUrlToLoad(),
                        intentDataProvider.getClientPackageName(),
                        intentDataProvider.getFileHandlingData());

        maybeNotifyLaunchQueue(launchParams);
    }

    /**
     * Handles an intent that comes after a TWA creation. It triggers a url loading in case of
     * navigate-existing mode. It opens a new TWA in a new task in a case of navigate-new mode. It
     * Passes launch params to a launch queue. In case if the target url provided in the
     * intentDataProvider is out of scope of the TWA a launch queue will not be notified. If
     * currently open page is out of scope of the TWA and it's not going to be reloaded a launch
     * queue will not be notified as well.
     *
     * @param intentDataProvider Provides incoming intent with Custom Tabs specific customization
     *     data.
     */
    public void handleNewIntent(BrowserServicesIntentDataProvider intentDataProvider) {
        @ClientMode int clientMode = getClientMode(intentDataProvider.getLaunchHandlerClientMode());

        if (clientMode != NAVIGATE_NEW) {
            boolean startNavigation =
                    clientMode == NAVIGATE_EXISTING
                            && !TextUtils.isEmpty(intentDataProvider.getUrlToLoad());

            if (startNavigation) {
                LoadUrlParams params = new LoadUrlParams(intentDataProvider.getUrlToLoad());
                mNavigationController.navigate(params, intentDataProvider.getIntent());
            }

            WebAppLaunchParams launchParams =
                    getLaunchParams(
                            startNavigation,
                            intentDataProvider.getUrlToLoad(),
                            intentDataProvider.getClientPackageName(),
                            intentDataProvider.getFileHandlingData());

            maybeNotifyLaunchQueue(launchParams);
        }
    }

    private void maybeNotifyLaunchQueue(WebAppLaunchParams launchParams) {

        if (!launchParams.newNavigationStarted) {
            // Check if the URL of the current page is in the web app scope.
            // Launch params should not be sent to a not verified origin.
            CurrentPageVerifier.VerificationState state = mCurrentPageVerfier.getState();
            if (state == null || state.status != CurrentPageVerifier.VerificationStatus.SUCCESS) {
                return;
            }
        }

        mVerifier
                .verify(launchParams.targetUrl)
                .then(
                        (verified) -> {
                            if (!verified) return;
                            WebAppLaunchHandlerJni.get()
                                    .notifyLaunchQueue(
                                            mWebContents,
                                            launchParams.newNavigationStarted,
                                            launchParams.targetUrl,
                                            launchParams.packageName,
                                            launchParams.fileUris);
                        });
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
                @JniType("std::string") String packageName,
                @JniType("std::vector<std::string>") String[] fileUris);
    }
}
