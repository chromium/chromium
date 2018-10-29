// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.customtabs.TrustedWebUtils;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for launching
 * {@link org.chromium.chrome.browser.customtabs.CustomTabActivity} in Trusted Web Activity Mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TrustedWebActivityTest {
    // TODO(peconn): Add test for navigating away from the trusted origin.
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String PACKAGE_NAME = "package.name";

    private EmbeddedTestServer mTestServer;
    private String mTestPage;

    @Before
    public void setUp() throws InterruptedException, ProcessInitException {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

        // TWAs only work with HTTPS.
        mTestServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        mTestPage = mTestServer.getURL(TEST_PAGE);
    }

    /** Creates an Intent that will launch a Custom Tab to the given |url|. */
    private static Intent createTrustedWebActivityIntent(String url) {
        Intent intent  = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), url);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);
        return intent;
    }

    /** Caches a successful verification for the given |packageName| and |url|. */
    private static void spoofVerification(String packageName, String url) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> OriginVerifier.addVerifiedOriginForPackage(packageName, new Origin(url),
                        CustomTabsService.RELATION_HANDLE_ALL_URLS));
    }

    /** Creates a Custom Tabs Session from the Intent, specifying the |packageName|. */
    private static void createSession(Intent intent, String packageName)
            throws TimeoutException, InterruptedException {
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        connection.newSession(token);

        connection.overridePackageNameForSessionForTesting(token, packageName);
    }

    private boolean isTrustedWebActivity() {
        // A key part of the Trusted Web Activity UI is the lack of browser controls.
        return !mCustomTabActivityTestRule.getActivity().getActivityTab().canShowBrowserControls();
    }

    @After
    public void tearDown() throws TimeoutException {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    public void launchesTwa() throws TimeoutException, InterruptedException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        spoofVerification(PACKAGE_NAME, mTestPage);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertTrue(isTrustedWebActivity());
    }

    @Test
    @MediumTest
    public void doesntLaunchTwa_WithoutFlag() throws TimeoutException, InterruptedException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        spoofVerification(PACKAGE_NAME, mTestPage);
        createSession(intent, PACKAGE_NAME);

        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertFalse(isTrustedWebActivity());
    }

    @Test
    @MediumTest
    public void leavesTwa_VerificationFailure() throws TimeoutException, InterruptedException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertFalse(isTrustedWebActivity());
    }
}
