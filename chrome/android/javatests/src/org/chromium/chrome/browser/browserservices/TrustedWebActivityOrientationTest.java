// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createSession;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createTrustedWebActivityIntent;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.isTrustedWebActivity;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.spoofVerification;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.net.Uri;

import androidx.browser.trusted.ScreenOrientation;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for launching {@link
 * org.chromium.chrome.browser.customtabs.CustomTabActivity} in Trusted Web Activity Mode with
 * default orientation set.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TrustedWebActivityOrientationTest {
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();
    public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain()
                    .around(mCustomTabActivityTestRule)
                    .around(mEmbeddedTestServerRule);

    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    @Before
    public void setUp() {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized();

        mEmbeddedTestServerRule.setServerUsesHttps(true); // TWAs only work with HTTPS.

        // Map non-localhost-URLs to localhost. Navigations to non-localhost URLs will throw a
        // certificate error.
        Uri mapToUri = Uri.parse(mEmbeddedTestServerRule.getServer().getURL("/"));
        CommandLine.getInstance()
                .appendSwitchWithValue(
                        ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
    }

    @Test
    @MediumTest
    public void defaultOrientationIsSet() throws TimeoutException {
        final String mTestPage =
                mEmbeddedTestServerRule.getServer().getURL("/chrome/test/data/android/simple.html");

        Intent intent = createTrustedWebActivityIntent(mTestPage);
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_SCREEN_ORIENTATION,
                ScreenOrientation.LANDSCAPE);
        launchCustomTabActivity(intent);

        assertTrue(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));

        // Check that the browser is not in fullscreen.
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        final TabWebContentsDelegateAndroid delegate = TabTestUtils.getTabWebContentsDelegate(tab);
        FullscreenTestUtils.waitForFullscreenFlag(
                tab, false, mCustomTabActivityTestRule.getActivity());
        FullscreenTestUtils.waitForPersistentFullscreen(delegate, false);

        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, mCustomTabActivityTestRule.getActivity());

        JavaScriptUtils.executeJavaScript(
                tab.getWebContents(), "screen.orientation.lock('portrait');");
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mCustomTabActivityTestRule.getActivity().getRequestedOrientation(),
                            Matchers.is(ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT));
                });

        JavaScriptUtils.executeJavaScript(tab.getWebContents(), "screen.orientation.unlock();");
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mCustomTabActivityTestRule.getActivity().getRequestedOrientation(),
                            Matchers.is(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE));
                });
    }

    public void launchCustomTabActivity(Intent intent) throws TimeoutException {
        String url = intent.getData().toString();
        spoofVerification(PACKAGE_NAME, url);
        createSession(intent, PACKAGE_NAME);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }
}
