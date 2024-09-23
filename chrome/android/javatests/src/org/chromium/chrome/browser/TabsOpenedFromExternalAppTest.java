// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.Referrer;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.network.mojom.ReferrerPolicy;

import java.util.concurrent.TimeoutException;

/** Test the behavior of tabs when opening a URL from an external app. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabsOpenedFromExternalAppTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    static final String HTTP_REFERRER = "http://chromium.org/";

    private static final String EXTERNAL_APP_1_ID = "app1";
    private static final String EXTERNAL_APP_2_ID = "app2";
    private static final String ANDROID_APP_REFERRER = "android-app://com.my.great.great.app/";
    private static final String HTTPS_REFERRER = "https://chromium.org/";
    private static final String HTTPS_REFERRER_WITH_PATH = "https://chromium.org/path1/path2";

    static class ElementFocusedCriteria implements Runnable {
        private final Tab mTab;
        private final String mElementId;

        public ElementFocusedCriteria(Tab tab, String elementId) {
            mTab = tab;
            // Add quotes to match returned value from JS.
            mElementId = "\"" + elementId + "\"";
        }

        @Override
        public void run() {
            String nodeId = null;
            try {
                StringBuilder sb = new StringBuilder();
                sb.append("(function() {");
                sb.append("  if (document.activeElement && document.activeElement.id) {");
                sb.append("    return document.activeElement.id;");
                sb.append("  }");
                sb.append("  return null;");
                sb.append("})();");

                String jsonText =
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                mTab.getWebContents(), sb.toString());
                if (jsonText.equalsIgnoreCase("null") || "".equals(jsonText)) {
                    nodeId = null;
                }
                nodeId = jsonText;
            } catch (TimeoutException e) {
                throw new CriteriaNotSatisfiedException(e);
            }
            Criteria.checkThat("Text-field in page not focused.", nodeId, Matchers.is(mElementId));
        }
    }

    static class ElementTextIsCriteria implements Runnable {
        private final Tab mTab;
        private final String mElementId;
        private final String mExpectedText;

        public ElementTextIsCriteria(Tab tab, String elementId, String expectedText) {
            mTab = tab;
            mElementId = elementId;
            mExpectedText = expectedText;
        }

        @Override
        public void run() {
            try {
                String text = DOMUtils.getNodeValue(mTab.getWebContents(), mElementId);
                Criteria.checkThat(
                        "Page does not have the text typed in.", text, Matchers.is(mExpectedText));
            } catch (TimeoutException e) {
                throw new CriteriaNotSatisfiedException(e);
            }
        }
    }

    /** Criteria checking that the page referrer has the expected value. */
    public static class ReferrerCriteria implements Runnable {
        private final Tab mTab;
        private final String mExpectedReferrer;
        private static final String GET_REFERRER_JS =
                "(function() { return document.referrer; })();";

        public ReferrerCriteria(Tab tab, String expectedReferrer) {
            mTab = tab;
            // Add quotes to match returned value from JS.
            mExpectedReferrer = "\"" + expectedReferrer + "\"";
        }

        @Override
        public void run() {
            String referrer = null;
            try {
                String jsonText =
                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                mTab.getWebContents(), GET_REFERRER_JS);
                if (jsonText.equalsIgnoreCase("null")) jsonText = "";
                referrer = jsonText;
            } catch (TimeoutException e) {
                throw new CriteriaNotSatisfiedException(e);
            }
            Criteria.checkThat(
                    "Referrer is not as expected.", referrer, Matchers.is(mExpectedReferrer));
        }
    }

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
    }

    /**
     * Launch the specified URL as if it was triggered by an external application with id appId.
     * Returns when the URL has been navigated to.
     */
    private static void launchUrlFromExternalApp(
            ChromeActivityTestRule testRule,
            String url,
            String expectedUrl,
            String appId,
            boolean createNewTab,
            Bundle extras,
            boolean firstParty) {
        final Intent intent = new Intent(Intent.ACTION_VIEW);
        if (appId != null) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, appId);
        }
        if (createNewTab) {
            intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        }
        intent.setData(Uri.parse(url));
        if (extras != null) intent.putExtras(extras);

        if (firstParty) {
            Context context = ApplicationProvider.getApplicationContext();
            intent.setPackage(context.getPackageName());
            IntentUtils.addTrustedIntentExtras(intent);
        }

        final Tab originalTab = testRule.getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(() -> testRule.getActivity().onNewIntent(intent));
        // NoTouchMode changes external app launch behaviour depending on whether Chrome is
        // foregrounded - which it is for these tests.
        if (createNewTab) {
            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(
                                "Failed to select different tab",
                                testRule.getActivity().getActivityTab(),
                                Matchers.not(originalTab));
                    });
        }
        ChromeTabUtils.waitForTabPageLoaded(testRule.getActivity().getActivityTab(), expectedUrl);
    }

    private void launchUrlFromExternalApp(
            String url, String expectedUrl, String appId, boolean createNewTab, Bundle extras) {
        launchUrlFromExternalApp(
                mActivityTestRule, url, expectedUrl, appId, createNewTab, extras, false);
    }

    private void launchUrlFromExternalApp(String url, String appId, boolean createNewTab) {
        launchUrlFromExternalApp(mActivityTestRule, url, url, appId, createNewTab, null, false);
    }

    private void assertBackPressSendsChromeToBackground() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Window does not have focus before pressing back.",
                            mActivityTestRule.getActivity().hasWindowFocus());
                    AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
                    mActivityTestRule.getActivity().onBackPressed();
                    Assert.assertTrue(
                            AsyncInitializationActivity.wasMoveTaskToBackInterceptedForTesting());
                    Assert.assertFalse(mActivityTestRule.getActivity().isFinishing());
                });
    }

    /** Tests that URLs opened from external apps can set an android-app scheme referrer. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testReferrer() {
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.startMainActivityOnBlankPage();
        Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(ANDROID_APP_REFERRER));
        launchUrlFromExternalApp(url, url, EXTERNAL_APP_1_ID, true, extras);
        CriteriaHelper.pollInstrumentationThread(
                new ReferrerCriteria(
                        mActivityTestRule.getActivity().getActivityTab(), ANDROID_APP_REFERRER),
                2000,
                200);
    }

    /** Tests that URLs opened from external apps cannot set an invalid android-app referrer. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testInvalidAndroidAppReferrer() {
        String invalidReferrer = "android-app:///note.the.extra.leading/";
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.startMainActivityOnBlankPage();
        Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(invalidReferrer));
        launchUrlFromExternalApp(url, url, EXTERNAL_APP_1_ID, true, extras);
        CriteriaHelper.pollInstrumentationThread(
                new ReferrerCriteria(mActivityTestRule.getActivity().getActivityTab(), ""),
                2000,
                200);
    }

    /** Tests that URLs opened from external apps cannot set an arbitrary referrer scheme. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testCannotSetArbitraryReferrer() {
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.startMainActivityOnBlankPage();
        String referrer = "foobar://totally.legit.referrer";
        Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(referrer));
        launchUrlFromExternalApp(url, url, EXTERNAL_APP_1_ID, true, extras);
        CriteriaHelper.pollInstrumentationThread(
                new ReferrerCriteria(mActivityTestRule.getActivity().getActivityTab(), ""),
                2000,
                200);
    }

    /** Tests that URLs opened from external applications cannot set an http:// referrer. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testNoHttpReferrer() {
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.startMainActivityOnBlankPage();
        Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(HTTP_REFERRER));

        launchUrlFromExternalApp(
                mActivityTestRule, url, url, EXTERNAL_APP_1_ID, true, extras, false);
        CriteriaHelper.pollInstrumentationThread(
                new ReferrerCriteria(mActivityTestRule.getActivity().getActivityTab(), ""),
                2000,
                200);
    }

    /** Tests that URLs opened from First party apps can set an http:// referrrer. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testHttpReferrerFromFirstParty() {
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.startMainActivityOnBlankPage();
        Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(HTTP_REFERRER));

        launchUrlFromExternalApp(
                mActivityTestRule, url, url, EXTERNAL_APP_1_ID, true, extras, true);
        CriteriaHelper.pollInstrumentationThread(
                new ReferrerCriteria(
                        mActivityTestRule.getActivity().getActivityTab(), HTTP_REFERRER),
                2000,
                200);
    }

    /** Tests that an https:// referrer is not stripped in case of downgrade with Origin Policy. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testReferrerPolicyHttpsReferrerPolicyOrigin() {
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndVerifyReferrerWithPolicy(
                url,
                mActivityTestRule,
                ReferrerPolicy.ORIGIN,
                HTTPS_REFERRER_WITH_PATH,
                HTTPS_REFERRER);
    }

    /**
     * Tests that an https:// referrer is not stripped in case of downgrade with Origin When Cross
     * Origin Policy.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testReferrerPolicyHttpsReferrerPolicyOriginWhenCrossOrigin() {
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndVerifyReferrerWithPolicy(
                url,
                mActivityTestRule,
                ReferrerPolicy.ORIGIN_WHEN_CROSS_ORIGIN,
                HTTPS_REFERRER_WITH_PATH,
                HTTPS_REFERRER);
    }

    /**
     * Tests that an https:// referrer is stripped in case of downgrade with Strict Origin Policy.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testReferrerPolicyHttpsReferrerPolicyStrictOrigin() {
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.startMainActivityOnBlankPage();
        loadUrlAndVerifyReferrerWithPolicy(
                url, mActivityTestRule, ReferrerPolicy.STRICT_ORIGIN, HTTPS_REFERRER, "");
    }

    /**
     * Launches a tab with the given url using the given {@link ChromeActivityTestRule}, adds a
     * {@link Referrer} with given policy and checks whether it matches the expected referrer after
     * loaded.
     */
    static void loadUrlAndVerifyReferrerWithPolicy(
            String url,
            ChromeActivityTestRule testRule,
            int policy,
            String referrer,
            String expectedReferrer) {
        Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(referrer));
        extras.putInt(IntentHandler.EXTRA_REFERRER_POLICY, policy);
        launchUrlFromExternalApp(testRule, url, url, EXTERNAL_APP_1_ID, true, extras, true);
        CriteriaHelper.pollInstrumentationThread(
                new ReferrerCriteria(testRule.getActivity().getActivityTab(), expectedReferrer),
                2000,
                200);
    }

    /** Tests that an https:// referrer is stripped in case of downgrade. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testHttpsReferrerFromFirstPartyNoDowngrade() {
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        mActivityTestRule.startMainActivityOnBlankPage();
        Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(HTTPS_REFERRER));
        launchUrlFromExternalApp(
                mActivityTestRule, url, url, EXTERNAL_APP_1_ID, true, extras, true);
        CriteriaHelper.pollInstrumentationThread(
                new ReferrerCriteria(mActivityTestRule.getActivity().getActivityTab(), ""),
                2000,
                200);
    }

    /** Tests that URLs opened from the same external app don't create new tabs. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testNoNewTabForSameApp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        int originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());

        // Launch a first URL from an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);
        // It should have opened in a new tab.
        int newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals(
                "Selected tab is not on the right URL.",
                url1,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        // Launch a new URL from the same app, it should open in the same tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, false);
        newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount, newTabCount);
        Assert.assertEquals(
                "Selected tab is not on the right URL.",
                url2,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        assertBackPressSendsChromeToBackground();
    }

    /**
     * Tests that URLs opened from an unspecified external app (no Browser.EXTRA_APPLICATION_ID in
     * the intent extras) don't create new tabs.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testNewTabForUnknownApp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        // Launch a first URL with an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);

        Assert.assertEquals(
                "Selected tab is not on the right URL.",
                url1,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        // Launch the same URL without app ID. It should open a new tab.
        int originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url1, null, false);
        int newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals(
                "Selected tab is not on the right URL.",
                url1,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        // Launch another URL without app ID. It should open a new tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url2, null, false);
        newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals(
                "Selected tab is not on the right URL.",
                url2,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        assertBackPressSendsChromeToBackground();
    }

    /**
     * Tests that URLs opened with the Browser.EXTRA_CREATE_NEW_TAB extra in the intent do create
     * new tabs.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testNewTabWithNewTabExtra() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        int originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());

        // Launch a first URL from an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);
        // It should have opened in a new tab.
        int newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals(
                "Selected tab is not on the right URL.",
                url1,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        // Launch a new URL from the same app with the right extra to open in a new tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, true);
        newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals(
                "Selected tab is not on the right URL.",
                url2,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        assertBackPressSendsChromeToBackground();
    }

    /**
     * Similar to testNoNewTabForSameApp but actually starting the application (not just opening a
     * tab) from the external app.
     */
    @Test
    @LargeTest
    @Feature({"Navigation", "Main"})
    public void testNoNewTabForSameAppOnStart() throws Exception {
        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        // Launch Clank from the external app.
        mActivityTestRule.startMainActivityFromExternalApp(url1, EXTERNAL_APP_1_ID);
        Assert.assertEquals(
                "Selected tab is not on the right URL.",
                url1,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        // Launch a new URL from the same app, it should open in the same tab.
        int originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, false);
        int newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount, newTabCount);
        Assert.assertEquals(
                "Selected tab is not on the right URL.",
                url2,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        assertBackPressSendsChromeToBackground();
    }

    /** Test that URLs opened from different external apps do create new tabs. */
    @Test
    @LargeTest
    @Feature({"Navigation", "Main"})
    public void testNewTabForDifferentApps() {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/test.html");

        // Launch a first URL from an app1.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);

        int originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());

        // Launch a second URL from an app2, it should open in a new tab.
        launchUrlFromExternalApp(url2, EXTERNAL_APP_2_ID, false);

        // It should have opened in a new tab.
        int newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals(
                "Selected tab is not on the right URL.",
                url2,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));

        // Also try with no app id, it should also open in a new tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url3, null, false);
        newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals(
                "Selected tab is not on the right URL.",
                url3,
                ChromeTabUtils.getUrlStringOnUiThread(
                        mActivityTestRule.getActivity().getActivityTab()));
    }
}
