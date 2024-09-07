// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Intent;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.test.core.app.ApplicationProvider;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.components.embedder_support.util.Origin;

import java.util.concurrent.TimeoutException;

/** Common utilities for Trusted Web Activity tests. */
public class TrustedWebActivityTestUtil {
    /** Waits till verification either succeeds or fails. */
    private static class CurrentPageVerifierWaiter extends CallbackHelper {
        private final Runnable mVerificationObserver = this::onVerificationUpdate;

        private CurrentPageVerifier mVerifier;

        public void start(CurrentPageVerifier verifier) throws TimeoutException {
            mVerifier = verifier;
            if (mVerifier.getState().status != CurrentPageVerifier.VerificationStatus.PENDING) {
                return;
            }

            mVerifier.addVerificationObserver(mVerificationObserver);
            waitForOnly();
        }

        public void onVerificationUpdate() {
            mVerifier.removeVerificationObserver(mVerificationObserver);
            notifyCalled();
        }
    }

    /** Creates an Intent that will launch a Custom Tab to the given |url|. */
    public static Intent createTrustedWebActivityIntent(String url) {
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), url);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);
        return intent;
    }

    /**
     * Creates an Intent that will launch a Custom Tab to the given |url|, caches a successful
     * verification and creates the Custom Tabs Session from the intent.
     */
    public static Intent createTrustedWebActivityIntentAndVerifiedSession(
            String url, String packageName) throws TimeoutException {
        Intent intent = createTrustedWebActivityIntent(url);
        spoofVerification(packageName, url);
        createSession(intent, packageName);
        return intent;
    }

    /** Caches a successful verification for the given |packageName| and |url|. */
    public static void spoofVerification(String packageName, String url) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeOriginVerifier.addVerificationOverride(
                                packageName,
                                Origin.create(url),
                                CustomTabsService.RELATION_HANDLE_ALL_URLS));
    }

    /** Creates a Custom Tabs Session from the Intent, specifying the |packageName|. */
    public static void createSession(Intent intent, String packageName) throws TimeoutException {
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, packageName);
    }

    /** Checks if given instance of {@link CustomTabActivity} is a Trusted Web Activity. */
    public static boolean isTrustedWebActivity(CustomTabActivity activity) {
        // A key part of the Trusted Web Activity UI is the lack of browser controls.
        @BrowserControlsState
        int constraints =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return TabBrowserControlsConstraintsHelper.getConstraints(
                                    activity.getActivityTab());
                        });
        return constraints == BrowserControlsState.HIDDEN;
    }

    /** Waits till {@link CurrentPageVerifier} verification either succeeds or fails. */
    public static void waitForCurrentPageVerifierToFinish(CustomTabActivity activity)
            throws TimeoutException {
        CurrentPageVerifier verifier = activity.getComponent().resolveCurrentPageVerifier();
        new CurrentPageVerifierWaiter().start(verifier);
    }
}
