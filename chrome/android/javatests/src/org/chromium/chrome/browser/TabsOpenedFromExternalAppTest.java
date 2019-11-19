// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;
import android.view.ContextMenu;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.common.Referrer;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.network.mojom.ReferrerPolicy;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Test the behavior of tabs when opening a URL from an external app.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class TabsOpenedFromExternalAppTest {
    @Rule
    public ChromeActivityTestRule<? extends ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule(ChromeTabbedActivity.class);

    static final String HTTP_REFERRER = "http://chromium.org/";

    private static final String EXTERNAL_APP_1_ID = "app1";
    private static final String EXTERNAL_APP_2_ID = "app2";
    private static final String ANDROID_APP_REFERRER = "android-app://com.my.great.great.app";
    private static final String HTTPS_REFERRER = "https://chromium.org/";
    private static final String HTTPS_REFERRER_WITH_PATH = "https://chromium.org/path1/path2";

    static class ElementFocusedCriteria extends Criteria {
        private final Tab mTab;
        private final String mElementId;

        public ElementFocusedCriteria(Tab tab, String elementId) {
            super("Text-field in page not focused.");
            mTab = tab;
            // Add quotes to match returned value from JS.
            mElementId = "\"" + elementId + "\"";
        }

        @Override
        public boolean isSatisfied() {
            String nodeId;
            try {
                StringBuilder sb = new StringBuilder();
                sb.append("(function() {");
                sb.append("  if (document.activeElement && document.activeElement.id) {");
                sb.append("    return document.activeElement.id;");
                sb.append("  }");
                sb.append("  return null;");
                sb.append("})();");

                String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mTab.getWebContents(), sb.toString());
                if (jsonText.equalsIgnoreCase("null") || "".equals(jsonText)) {
                    nodeId = null;
                }
                nodeId = jsonText;
            } catch (TimeoutException e) {
                e.printStackTrace();
                Assert.fail("Failed to retrieve focused node: TimeoutException was thrown");
                return false;
            }
            return TextUtils.equals(mElementId, nodeId);
        }
    }

    static class ElementTextIsCriteria extends Criteria {
        private final Tab mTab;
        private final String mElementId;
        private final String mExpectedText;

        public ElementTextIsCriteria(Tab tab, String elementId, String expectedText) {
            super("Page does not have the text typed in.");
            mTab = tab;
            mElementId = elementId;
            mExpectedText = expectedText;
        }

        @Override
        public boolean isSatisfied() {
            try {
                String text = DOMUtils.getNodeValue(mTab.getWebContents(), mElementId);
                return TextUtils.equals(mExpectedText, text);
            } catch (TimeoutException e) {
                e.printStackTrace();
                return false;
            }
        }
    }

    /**
     * Criteria checking that the page referrer has the expected value.
     */
    public static class ReferrerCriteria extends Criteria {
        private final Tab mTab;
        private final String mExpectedReferrer;
        private static final String GET_REFERRER_JS =
                "(function() { return document.referrer; })();";

        public ReferrerCriteria(Tab tab, String expectedReferrer) {
            super("Referrer is not as expected.");
            mTab = tab;
            // Add quotes to match returned value from JS.
            mExpectedReferrer = "\"" + expectedReferrer + "\"";
        }

        @Override
        public boolean isSatisfied() {
            String referrer;
            try {
                String jsonText = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        mTab.getWebContents(), GET_REFERRER_JS);
                if (jsonText.equalsIgnoreCase("null")) jsonText = "";
                referrer = jsonText;
            } catch (TimeoutException e) {
                e.printStackTrace();
                Assert.fail("TimeoutException was thrown");
                return false;
            }
            return TextUtils.equals(mExpectedReferrer, referrer);
        }
    }

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Launch the specified URL as if it was triggered by an external application with id appId.
     * Returns when the URL has been navigated to.
     */
    private static void launchUrlFromExternalApp(ChromeActivityTestRule testRule, String url,
            String expectedUrl, String appId, boolean createNewTab, Bundle extras,
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
            Context context = InstrumentationRegistry.getTargetContext();
            intent.setPackage(context.getPackageName());
            IntentHandler.addTrustedIntentExtras(intent);
        }

        final Tab originalTab = testRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> testRule.getActivity().onNewIntent(intent));
        // NoTouchMode changes external app launch behaviour depending on whether Chrome is
        // foregrounded - which it is for these tests.
        if (createNewTab) {
            CriteriaHelper.pollUiThread(new Criteria("Failed to select different tab") {
                @Override
                public boolean isSatisfied() {
                    return testRule.getActivity().getActivityTab() != originalTab;
                }
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

    /**
     * Tests that URLs opened from external apps can set an android-app scheme referrer.
     */
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
                2000, 200);
    }

    /**
     * Tests that URLs opened from external apps cannot set an invalid android-app referrer.
     */
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
                new ReferrerCriteria(mActivityTestRule.getActivity().getActivityTab(), ""), 2000,
                200);
    }

    /**
     * Tests that URLs opened from external apps cannot set an arbitrary referrer scheme.
     */
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
                new ReferrerCriteria(mActivityTestRule.getActivity().getActivityTab(), ""), 2000,
                200);
    }

    /**
     * Tests that URLs opened from external applications cannot set an http:// referrer.
     */
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
                new ReferrerCriteria(mActivityTestRule.getActivity().getActivityTab(), ""), 2000,
                200);
    }

    /**
     * Tests that URLs opened from First party apps can set an http:// referrrer.
     */
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
                2000, 200);
    }

    /**
     * Tests that an https:// referrer is not stripped in case of downgrade with Origin Policy.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testReferrerPolicyHttpsReferrerPolicyOrigin() {
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        launchAndVerifyReferrerWithPolicy(url, mActivityTestRule, ReferrerPolicy.ORIGIN,
                HTTPS_REFERRER_WITH_PATH, HTTPS_REFERRER);
    }

    /**
     * Tests that an https:// referrer is not stripped in case of downgrade
     * with Origin When Cross Origin Policy.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testReferrerPolicyHttpsReferrerPolicyOriginWhenCrossOrigin() {
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        launchAndVerifyReferrerWithPolicy(url, mActivityTestRule,
                ReferrerPolicy.ORIGIN_WHEN_CROSS_ORIGIN, HTTPS_REFERRER_WITH_PATH, HTTPS_REFERRER);
    }

    /**
     * Tests that an https:// referrer is stripped in case of downgrade with Strict Origin Policy.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testReferrerPolicyHttpsReferrerPolicyStrictOrigin() {
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        launchAndVerifyReferrerWithPolicy(
                url, mActivityTestRule, ReferrerPolicy.STRICT_ORIGIN, HTTPS_REFERRER, "");
    }

    /**
     * Launches a tab with the given url using the given {@link ChromeActivityTestRule},
     * adds a {@link Referrer} with given policy and checks whether it matches the expected
     * referrer after loaded.
     */
    static void launchAndVerifyReferrerWithPolicy(String url, ChromeActivityTestRule testRule,
            int policy, String referrer, String expectedReferrer) {
        testRule.startMainActivityOnBlankPage();
        Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(referrer));
        extras.putInt(IntentHandler.EXTRA_REFERRER_POLICY, policy);
        launchUrlFromExternalApp(testRule, url, url, EXTERNAL_APP_1_ID, true, extras, true);
        CriteriaHelper.pollInstrumentationThread(
                new ReferrerCriteria(testRule.getActivity().getActivityTab(), expectedReferrer),
                2000, 200);
    }

    /**
     * Tests that an https:// referrer is stripped in case of downgrade.
     */
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
                new ReferrerCriteria(mActivityTestRule.getActivity().getActivityTab(), ""), 2000,
                200);
    }

    /**
     * Tests that URLs opened from the same external app don't create new tabs.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testNoNewTabForSameApp() {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        int originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());

        // Launch a first URL from an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);
        // It should have opened in a new tab.
        int newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals("Selected tab is not on the right URL.", url1,
                mActivityTestRule.getActivity().getActivityTab().getUrl());

        // Launch a new URL from the same app, it should open in the same tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, false);
        newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount, newTabCount);
        Assert.assertEquals("Selected tab is not on the right URL.", url2,
                mActivityTestRule.getActivity().getActivityTab().getUrl());

        // And pressing back should close Clank.
        Assert.assertTrue("Window does not have focus before pressing back.",
                mActivityTestRule.getActivity().hasWindowFocus());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onBackPressed());
        CriteriaHelper.pollUiThread(
                Criteria.equals(false, () -> mActivityTestRule.getActivity().hasWindowFocus()));
    }

    /**
     * Tests that URLs opened from an unspecified external app (no Browser.EXTRA_APPLICATION_ID in
     * the intent extras) don't create new tabs.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testNewTabForUnknownApp() {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");


        // Launch a first URL with an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);

        Assert.assertEquals("Selected tab is not on the right URL.", url1,
                mActivityTestRule.getActivity().getActivityTab().getUrl());

        // Launch the same URL without app ID. It should open a new tab.
        int originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url1, null, false);
        int newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals("Selected tab is not on the right URL.", url1,
                mActivityTestRule.getActivity().getActivityTab().getUrl());

        // Launch another URL without app ID. It should open a new tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url2, null, false);
        newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals("Selected tab is not on the right URL.", url2,
                mActivityTestRule.getActivity().getActivityTab().getUrl());

        // And pressing back should close Clank.
        Assert.assertTrue("Window does not have focus before pressing back.",
                mActivityTestRule.getActivity().hasWindowFocus());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onBackPressed());
        CriteriaHelper.pollUiThread(
                Criteria.equals(false, () -> mActivityTestRule.getActivity().hasWindowFocus()));
    }

    /**
     * Tests that URLs opened with the Browser.EXTRA_CREATE_NEW_TAB extra in
     * the intent do create new tabs.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testNewTabWithNewTabExtra() {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        int originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());

        // Launch a first URL from an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);
        // It should have opened in a new tab.
        int newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals("Selected tab is not on the right URL.", url1,
                mActivityTestRule.getActivity().getActivityTab().getUrl());

        // Launch a new URL from the same app with the right extra to open in a new tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, true);
        newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals("Selected tab is not on the right URL.", url2,
                mActivityTestRule.getActivity().getActivityTab().getUrl());

        // And pressing back should close Clank.
        Assert.assertTrue("Window does not have focus before pressing back.",
                mActivityTestRule.getActivity().hasWindowFocus());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onBackPressed());
        CriteriaHelper.pollUiThread(
                Criteria.equals(false, () -> mActivityTestRule.getActivity().hasWindowFocus()));
    }

    /**
     * Similar to testNoNewTabForSameApp but actually starting the application (not just opening a
     * tab) from the external app.
     */
    @Test
    @LargeTest
    @Feature({"Navigation", "Main"})
    public void testNoNewTabForSameAppOnStart() {
        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        // Launch Clank from the external app.
        mActivityTestRule.startMainActivityFromExternalApp(url1, EXTERNAL_APP_1_ID);
        Assert.assertEquals("Selected tab is not on the right URL.", url1,
                mActivityTestRule.getActivity().getActivityTab().getUrl());

        // Launch a new URL from the same app, it should open in the same tab.
        int originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, false);
        int newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount, newTabCount);
        Assert.assertEquals("Selected tab is not on the right URL.", url2,
                mActivityTestRule.getActivity().getActivityTab().getUrl());

        // And pressing back should close Clank.
        Assert.assertTrue("Window does not have focus before pressing back.",
                mActivityTestRule.getActivity().hasWindowFocus());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onBackPressed());
        CriteriaHelper.pollUiThread(
                Criteria.equals(false, () -> mActivityTestRule.getActivity().hasWindowFocus()));
    }

    /**
     * Test that URLs opened from different external apps do create new tabs.
     */
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
        Assert.assertEquals("Selected tab is not on the right URL.", url2,
                mActivityTestRule.getActivity().getActivityTab().getUrl());

        // Also try with no app id, it should also open in a new tab.
        originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url3, null, false);
        newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals("Selected tab is not on the right URL.", url3,
                mActivityTestRule.getActivity().getActivityTab().getUrl());
    }

    /**
     * Tests that a tab is not reused when launched from the same app as an already opened tab and
     * when the user has navigated elsewhere manually in the same tab.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testNewTabAfterNavigation() {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/test.html");

        // Launch a first URL from an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);

        // Now simulate the user manually navigating to another URL.
        mActivityTestRule.loadUrl(url3);

        // Launch a second URL from the same app, it should open in a new tab.
        int originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, false);
        int newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals("Selected tab is not on the right URL.", url2,
                mActivityTestRule.getActivity().getActivityTab().getUrl());
    }

    /**
     * Tests that a tab is not reused when launched from the same app as an already opened tab and
     * when the user has entered text in the page.
     * @throws InterruptedException
     */
    /**
     * @LargeTest
     * @Feature({"Navigation"})
     */
    @Test
    @FlakyTest(message = "http://crbug.com/6467101")
    public void testNewTabWhenPageEdited() throws TimeoutException {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        // Launch a first URL from an app.
        launchUrlFromExternalApp(url1, EXTERNAL_APP_1_ID, false);

        // Focus the text-field and type something.
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        DOMUtils.focusNode(tab.getWebContents(), "textField");

        // Some processing needs to happen before the test-field has the focus.
        CriteriaHelper.pollInstrumentationThread(
                new ElementFocusedCriteria(
                        mActivityTestRule.getActivity().getActivityTab(), "textField"),
                2000, 200);

        // Now type something.
        InstrumentationRegistry.getInstrumentation().sendStringSync("banana");

        // We also have to wait for the text to happen in the page.
        CriteriaHelper.pollInstrumentationThread(
                new ElementTextIsCriteria(
                        mActivityTestRule.getActivity().getActivityTab(), "textField", "banana"),
                2000, 200);

        // Launch a second URL from the same app, it should open in a new tab.
        int originalTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        launchUrlFromExternalApp(url2, EXTERNAL_APP_1_ID, false);
        int newTabCount = ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity());
        Assert.assertEquals("Incorrect number of tabs open", originalTabCount + 1, newTabCount);
        Assert.assertEquals("Selected tab is not on the right URL.", url2,
                mActivityTestRule.getActivity().getActivityTab().getUrl());
    }

    private static class TestTabObserver extends EmptyTabObserver {
        private ContextMenu mContextMenu;

        @Override
        public void onContextMenuShown(Tab tab, ContextMenu menu) {
            mContextMenu = menu;
        }
    }

    /**
     * Catches regressions for https://crbug.com/495877.
     */
    @Test
    @FlakyTest(message = "https://crbug.com/571030")
    @MediumTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    public void testBackgroundSvelteTabIsSelectedAfterClosingExternalTab() throws Exception {
        // Start up Chrome and immediately close its tab -- it gets in the way.
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) ()
                        -> TabModelUtils.closeTabByIndex(
                                mActivityTestRule.getActivity().getCurrentTabModel(), 0));
        CriteriaHelper.pollUiThread(Criteria.equals(0,
                () -> mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount()));

        // Defines one gigantic link spanning the whole page that creates a new
        // window with chrome/test/data/android/google.html.
        final String hrefLink = UrlUtils.encodeHtmlDataUri("<html>"
                + "  <head>"
                + "    <title>href link page</title>"
                + "    <meta name='viewport'"
                + "        content='width=device-width initial-scale=0.5, maximum-scale=0.5'>"
                + "    <style>"
                + "      body {margin: 0em;} div {width: 100%; height: 100%; background: #011684;}"
                + "    </style>"
                + "  </head>"
                + "  <body>"
                + "    <a href='" + mTestServer.getURL("/chrome/test/data/android/google.html")
                + "' target='_blank'><div></div></a>"
                + "  </body>"
                + "</html>");

        // Open a tab via an external application.
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(hrefLink));
        intent.setClassName(InstrumentationRegistry.getTargetContext().getPackageName(),
                ChromeTabbedActivity.class.getName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, "com.legit.totes");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        InstrumentationRegistry.getTargetContext().startActivity(intent);

        CriteriaHelper.pollUiThread(Criteria.equals(1,
                () -> mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount()));
        ApplicationTestUtils.assertWaitForPageScaleFactorMatch(
                mActivityTestRule.getActivity(), 0.5f);

        // Long press the center of the page, which should bring up the context menu.
        final TestTabObserver observer = new TestTabObserver();
        mActivityTestRule.getActivity().getActivityTab().addObserver(observer);
        Assert.assertNull(observer.mContextMenu);
        final View view = TestThreadUtils.runOnUiThreadBlocking(
                (Callable<View>) ()
                        -> mActivityTestRule.getActivity().getActivityTab().getContentView());
        TouchCommon.longPressView(view);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return observer.mContextMenu != null;
            }
        });
        mActivityTestRule.getActivity().getActivityTab().removeObserver(observer);

        // Select the "open in new tab" option.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertTrue(observer.mContextMenu.performIdentifierAction(
                                R.id.contextmenu_open_in_new_tab, 0)));

        // The second tab should open in the background.
        CriteriaHelper.pollUiThread(Criteria.equals(2,
                () -> mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount()));

        // Hitting "back" should close the tab, minimize Chrome, and select the background tab.
        // Confirm that the number of tabs is correct and that closing the tab didn't cause a crash.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onBackPressed());
        CriteriaHelper.pollUiThread(Criteria.equals(1,
                () -> mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount()));
    }

    /**
     * Tests that a Weblite url from an external app uses the lite_url param when Data Reduction
     * Proxy previews are being used.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add("enable-spdy-proxy-auth")
    @EnableFeatures(
            {"DataReductionProxyDecidesTransform", "DataReductionProxyEnabledWithNetworkService"})
    public void
    testLaunchWebLiteURL() {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url = mTestServer.getURL("/chrome/test/data/android/about.html");

        // Launch a first URL from an app.
        launchUrlFromExternalApp(
                "http://googleweblight.com/i?u=" + url, url, EXTERNAL_APP_1_ID, false, null);

        Assert.assertEquals("Selected tab is not on the right URL.", url,
                mActivityTestRule.getActivity().getActivityTab().getUrl());
    }

    /**
     * Tests that a Weblite url from an external app does not use the lite_url param when Data
     * Reduction Proxy previews are not being used.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add("enable-spdy-proxy-auth")
    @DisableFeatures("DataReductionProxyDecidesTransform")
    public void testLaunchWebLiteURLNoPreviews() {
        mActivityTestRule.startMainActivityOnBlankPage();

        String url = "http://googleweblight.com/i?u=chrome/test/data/android/about.html";

        // Launch a first URL from an app.
        launchUrlFromExternalApp(url, url, EXTERNAL_APP_1_ID, false, null);

        Assert.assertEquals("Selected tab is not on the right URL.", url,
                mActivityTestRule.getActivity().getActivityTab().getUrl());
    }
}
