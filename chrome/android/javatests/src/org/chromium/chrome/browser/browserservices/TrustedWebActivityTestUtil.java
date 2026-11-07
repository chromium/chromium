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
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.components.embedder_support.util.Origin;

import java.util.concurrent.TimeoutException;

/**
 * Common utilities for Trusted Web Activity tests.
 *
 * <p><b>How these helpers launch TWAs:</b> {@link #createTrustedWebActivityIntent} constructs an
 * {@code ACTION_VIEW} intent targeting {@code ChromeLauncherActivity}, which routes through {@code
 * LaunchIntentDispatcher} into {@link CustomTabActivity} from Chrome's own process and UID rather
 * than launching from an external client APK launcher activity. {@link #spoofVerification}
 * registers an in-memory Digital Asset Links bypass via {@link
 * ChromeOriginVerifier#addVerificationOverride}, and {@link #createSession} associates the test
 * session token with a caller package name via {@link
 * CustomTabsConnection#overridePackageNameForSessionForTesting}.
 *
 * <p><b>UID and security-boundary limitations:</b> Because both {@link CustomTabsService} session
 * creation and the {@code ChromeLauncherActivity} launch intent originate inside Chrome's own
 * process and UID (bypassing any external client APK launcher activity), caller and session checks
 * run under Chrome's own UID rather than a separate client app UID. As a result, these helpers do
 * not exercise cross-UID {@code Binder.getCallingUid()} session ownership checks, cross-UID {@code
 * content://} URI grant validation, independent client task roots, or separate-package OS runtime
 * permission isolation (unless binding specifically to the {@code
 * org.chromium.chrome.tests.support} helper APK for bound {@code TrustedWebActivityService} IPC
 * tests).
 */
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
        var token = SessionHolder.of(CustomTabsSessionToken.getSessionTokenFromIntent(intent));
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        connection.newSession(token.getToken());
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
        CurrentPageVerifier verifier = activity.getCurrentPageVerifier();
        new CurrentPageVerifierWaiter().start(verifier);
    }
}
