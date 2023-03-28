// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createSession;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.spoofVerification;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Manual;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link QualityEnforcer}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT)
@DisableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT_FORCED)
public class QualityEnforcerTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    // A not exist test page to triger 404.
    private static final String TEST_PAGE_404 = "/chrome/test/data/android/404.html";
    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private final CustomTabActivityTestRule mCustomTabActivityTestRule =
            new CustomTabActivityTestRule();
    private final EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public RuleChain mRuleChain = RuleChain.emptyRuleChain()
                                          .around(mCustomTabActivityTestRule)
                                          .around(mEmbeddedTestServerRule);

    private String mTestPage;
    private String mTestPage404;

    private String mErrorMessage;
    CallbackHelper mCallbackHelper = new CallbackHelper();

    CustomTabsCallback mCallback = new CustomTabsCallback() {
        @Override
        public Bundle extraCallbackWithResult(String callbackName, Bundle args) {
            if (callbackName.equals(QualityEnforcer.CRASH)) {
                mCallbackHelper.notifyCalled();
                mErrorMessage = args.getString(QualityEnforcer.KEY_CRASH_REASON);
            }
            return Bundle.EMPTY;
        }
    };

    @Before
    public void setUp() {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized();

        mEmbeddedTestServerRule.setServerUsesHttps(true); // TWAs only work with HTTPS.
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        mTestPage404 = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE_404);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1145181")
    public void notifiedWhenLaunch404() throws TimeoutException {
        launch(mTestPage404);
        mCallbackHelper.waitForFirst();
        assertEquals(mErrorMessage, "404 on " + mTestPage404);
    }

    @Test
    @MediumTest
    public void notifiedWhenNavigate404() throws TimeoutException {
        launch(mTestPage);
        mCustomTabActivityTestRule.loadUrl(mTestPage404);
        mCallbackHelper.waitForFirst();
        assertEquals(mErrorMessage, "404 on " + mTestPage404);
    }

    @Test
    @MediumTest
    @Manual(message = "This test only works when device is running offline.")
    // TODO(eirage): Figure out how to make it work on local device without changing network.
    public void notifiedOffline() throws TimeoutException {
        launch("https://example.com/");
        mCallbackHelper.waitForFirst();
        assertEquals(mErrorMessage, "Page unavailable offline: https://example.com/");
    }

    @Test
    @MediumTest
    public void notifiedDigitalAssetLinkFailed() throws TimeoutException {
        launchNotVerify(mTestPage);
        mCallbackHelper.waitForFirst();
    }

    public void launch(String testPage) throws TimeoutException {
        Intent intent = createTrustedWebActivityIntentWithCallback(testPage);
        spoofVerification(PACKAGE_NAME, testPage);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    public void launchNotVerify(String testPage) throws TimeoutException {
        Intent intent = createTrustedWebActivityIntentWithCallback(testPage);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    private Intent createTrustedWebActivityIntentWithCallback(String testPage)
            throws TimeoutException {
        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(mCallback).session;
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setComponent(new ComponentName(
                InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(testPage));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);
        return intent;
    }
}
