// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.tab.TabBrowserControlsState;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.TrustedWebUtils;

/**
 * Common utilities for Trusted Web Activity tests.
 */
public class TrustedWebActivityTestUtil {
    /** Creates an Intent that will launch a Custom Tab to the given |url|. */
    public static Intent createTrustedWebActivityIntent(String url) {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), url);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);
        return intent;
    }

    /** Caches a successful verification for the given |packageName| and |url|. */
    public static void spoofVerification(String packageName, String url) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> OriginVerifier.addVerificationOverride(packageName, Origin.create(url),
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
        return !TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> TabBrowserControlsState
                        .get(activity.getActivityTab())
                        .canShow());
    }
}
