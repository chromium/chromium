// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Browser;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.ReferrerCondition;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tabmodel.TabCountChangedCondition;
import org.chromium.content_public.common.Referrer;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.network.mojom.ReferrerPolicy;

/** Test the behavior of tabs when opening a URL from an external app. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests various ways of launching the app")
public class TabsOpenedFromExternalAppTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    static final String HTTP_REFERRER = "http://chromium.org/";

    private static final String EXTERNAL_APP_1_ID = "app1";
    private static final String EXTERNAL_APP_2_ID = "app2";
    private static final String ANDROID_APP_REFERRER = "android-app://com.my.great.great.app/";
    private static final String HTTPS_REFERRER = "https://chromium.org/";
    private static final String HTTPS_REFERRER_WITH_PATH = "https://chromium.org/path1/path2";

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mTestServer = mActivityTestRule.getTestServer();
    }

    private static Intent createNewTabIntent(String url, String appId, String referrer) {
        final Intent intent = new Intent(Intent.ACTION_VIEW);
        if (appId != null) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, appId);
        }
        if (referrer != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(referrer));
        }
        intent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        intent.setData(Uri.parse(url));
        return intent;
    }

    private static Intent createViewIntent(String url, String appId) {
        final Intent intent = new Intent(Intent.ACTION_VIEW);
        if (appId != null) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, appId);
        }
        intent.setData(Uri.parse(url));
        return intent;
    }

    private static void putReferrerPolicyExtra(Intent intent, int policy) {
        intent.putExtra(IntentHandler.EXTRA_REFERRER_POLICY, policy);
    }

    private static void putFirstPartyExtra(Intent intent) {
        Context context = ApplicationProvider.getApplicationContext();
        intent.setPackage(context.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
    }

    private static TripBuilder sendNewIntentTo(CtaPageStation page, Intent intent) {
        return page.runOnUiThreadTo(() -> page.getActivity().onNewIntent(intent));
    }

    private static WebPageStation newPageStationInNewTab(String expectedUrl) {
        return WebPageStation.newBuilder()
                .initOpeningNewTab()
                .withExpectedUrlSubstring(expectedUrl)
                .build();
    }

    private void assertBackPressSendsChromeToBackground() {
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
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        Intent intent = createNewTabIntent(url, EXTERNAL_APP_1_ID, ANDROID_APP_REFERRER);
        WebPageStation newPage = newPageStationInNewTab(url);

        sendNewIntentTo(page, intent)
                .arriveAtAnd(newPage)
                .waitFor(new ReferrerCondition(newPage.loadedTabElement, ANDROID_APP_REFERRER));
    }

    /** Tests that URLs opened from external apps cannot set an invalid android-app referrer. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testInvalidAndroidAppReferrer() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        String invalidReferrer = "android-app:///note.the.extra.leading/";
        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        Intent intent = createNewTabIntent(url, EXTERNAL_APP_1_ID, invalidReferrer);
        WebPageStation newPage = newPageStationInNewTab(url);

        sendNewIntentTo(page, intent)
                .arriveAtAnd(newPage)
                .waitFor(new ReferrerCondition(newPage.loadedTabElement, ""));
    }

    /** Tests that URLs opened from external apps cannot set an arbitrary referrer scheme. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testCannotSetArbitraryReferrer() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        String referrer = "foobar://totally.legit.referrer";
        Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_REFERRER, Uri.parse(referrer));
        Intent intent = createNewTabIntent(url, EXTERNAL_APP_1_ID, referrer);
        WebPageStation newPage = newPageStationInNewTab(url);

        sendNewIntentTo(page, intent)
                .arriveAtAnd(newPage)
                .waitFor(new ReferrerCondition(newPage.loadedTabElement, ""));
    }

    /** Tests that URLs opened from external applications cannot set an http:// referrer. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testNoHttpReferrer() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        Intent intent = createNewTabIntent(url, EXTERNAL_APP_1_ID, HTTP_REFERRER);
        WebPageStation newPage = newPageStationInNewTab(url);

        sendNewIntentTo(page, intent)
                .arriveAtAnd(newPage)
                .waitFor(new ReferrerCondition(newPage.loadedTabElement, ""));
    }

    /** Tests that URLs opened from First party apps can set an http:// referrrer. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testHttpReferrerFromFirstParty() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        String url = mTestServer.getURL("/chrome/test/data/android/about.html");

        Intent intent = createNewTabIntent(url, EXTERNAL_APP_1_ID, HTTP_REFERRER);
        putFirstPartyExtra(intent);
        WebPageStation newPage = newPageStationInNewTab(url);

        sendNewIntentTo(page, intent)
                .arriveAtAnd(newPage)
                .waitFor(new ReferrerCondition(newPage.loadedTabElement, HTTP_REFERRER));
    }

    /** Tests that an https:// referrer is not stripped in case of downgrade with Origin Policy. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testReferrerPolicyHttpsReferrerPolicyOrigin() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        loadUrlAndVerifyReferrerWithPolicy(
                url, page, ReferrerPolicy.ORIGIN, HTTPS_REFERRER_WITH_PATH, HTTPS_REFERRER);
    }

    /**
     * Tests that an https:// referrer is not stripped in case of downgrade with Origin When Cross
     * Origin Policy.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testReferrerPolicyHttpsReferrerPolicyOriginWhenCrossOrigin() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        loadUrlAndVerifyReferrerWithPolicy(
                url,
                page,
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
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        loadUrlAndVerifyReferrerWithPolicy(
                url, page, ReferrerPolicy.STRICT_ORIGIN, HTTPS_REFERRER, "");
    }

    /**
     * Launches a tab with the given url when already displaying a |page|, adds a {@link Referrer}
     * with given policy and checks whether it matches the expected referrer after loaded.
     */
    static WebPageStation loadUrlAndVerifyReferrerWithPolicy(
            String url, CtaPageStation page, int policy, String referrer, String expectedReferrer) {
        Intent intent = createNewTabIntent(url, EXTERNAL_APP_1_ID, referrer);
        putReferrerPolicyExtra(intent, policy);
        putFirstPartyExtra(intent);
        WebPageStation newPage = newPageStationInNewTab(url);

        sendNewIntentTo(page, intent)
                .arriveAtAnd(newPage)
                .waitFor(new ReferrerCondition(newPage.loadedTabElement, expectedReferrer));
        return newPage;
    }

    /** Tests that an https:// referrer is stripped in case of downgrade. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    public void testHttpsReferrerFromFirstPartyNoDowngrade() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        String url = mTestServer.getURL("/chrome/test/data/android/about.html");
        Intent intent = createNewTabIntent(url, EXTERNAL_APP_1_ID, HTTPS_REFERRER);
        putFirstPartyExtra(intent);
        WebPageStation newPage = newPageStationInNewTab(url);

        sendNewIntentTo(page, intent)
                .arriveAtAnd(newPage)
                .waitFor(new ReferrerCondition(newPage.loadedTabElement, ""));
    }

    /** Tests that URLs opened from the same external app don't create new tabs. */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/350395970")
    public void testNoNewTabForSameApp() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        // Launch a first URL from an app.
        Intent intent1 = createViewIntent(url1, EXTERNAL_APP_1_ID);
        page =
                sendNewIntentTo(page, intent1)
                        .waitForAnd(new TabCountChangedCondition(page.getTabModel(), +1))
                        .arriveAt(newPageStationInNewTab(url1));

        // Launch a new URL from the same app, it should close the current tab and open in a new
        // one.
        Intent intent2 = createViewIntent(url2, EXTERNAL_APP_1_ID);
        sendNewIntentTo(page, intent2)
                .waitForAnd(new TabCountChangedCondition(page.getTabModel(), 0))
                .arriveAt(newPageStationInNewTab(url2));

        assertBackPressSendsChromeToBackground();
    }

    /**
     * Tests that URLs opened from an unspecified external app (no Browser.EXTRA_APPLICATION_ID in
     * the intent extras) don't create new tabs.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/350395970")
    public void testNewTabForUnknownApp() {
        CtaPageStation page = mActivityTestRule.startOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        // Launch a first URL with an app.
        Intent intent1 = createViewIntent(url1, EXTERNAL_APP_1_ID);
        page = sendNewIntentTo(page, intent1).arriveAt(newPageStationInNewTab(url1));

        // Launch the same URL without app ID. It should open a new tab.
        Intent intent2 = createViewIntent(url1, /* appId= */ null);
        page =
                sendNewIntentTo(page, intent2)
                        .waitForAnd(new TabCountChangedCondition(page.getTabModel(), +1))
                        .arriveAt(newPageStationInNewTab(url1));

        // Launch another URL without app ID. It should open a new tab.
        Intent intent3 = createViewIntent(url2, /* appId= */ null);
        sendNewIntentTo(page, intent3)
                .waitForAnd(new TabCountChangedCondition(page.getTabModel(), +1))
                .arriveAt(newPageStationInNewTab(url2));

        assertBackPressSendsChromeToBackground();
    }

    /**
     * Tests that URLs opened with the Browser.EXTRA_CREATE_NEW_TAB extra in the intent do create
     * new tabs.
     */
    @Test
    @LargeTest
    @Feature({"Navigation"})
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/350395970")
    public void testNewTabWithNewTabExtra() {
        CtaPageStation page = mActivityTestRule.startOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        // Launch a first URL from an app.
        Intent intent1 = createViewIntent(url1, EXTERNAL_APP_1_ID);
        page =
                sendNewIntentTo(page, intent1)
                        .waitForAnd(new TabCountChangedCondition(page.getTabModel(), +1))
                        .arriveAt(newPageStationInNewTab(url1));

        // Launch a new URL from the same app with the right extra to open in a new tab.
        Intent intent2 = createNewTabIntent(url2, EXTERNAL_APP_1_ID, /* referrer= */ null);
        sendNewIntentTo(page, intent2)
                .waitForAnd(new TabCountChangedCondition(page.getTabModel(), +1))
                .arriveAt(newPageStationInNewTab(url2));

        assertBackPressSendsChromeToBackground();
    }

    /**
     * Similar to testNoNewTabForSameApp but actually starting the application (not just opening a
     * tab) from the external app.
     */
    @Test
    @LargeTest
    @Feature({"Navigation", "Main"})
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.TIRAMISU,
            message = "crbug.com/350395970")
    public void testNoNewTabForSameAppOnStart() {
        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");

        // Launch Clank from the external app.
        Intent intent1 = new Intent(Intent.ACTION_VIEW);
        intent1.putExtra(Browser.EXTRA_APPLICATION_ID, EXTERNAL_APP_1_ID);
        CtaPageStation page = mActivityTestRule.startWithIntentPlusUrlAtWebPage(intent1, url1);

        // Launch a new URL from the same app, it should close the current tab and open in a new
        // one.
        Intent intent2 = createViewIntent(url2, EXTERNAL_APP_1_ID);
        sendNewIntentTo(page, intent2)
                .waitForAnd(new TabCountChangedCondition(page.getTabModel(), 0))
                .arriveAt(newPageStationInNewTab(url2));

        assertBackPressSendsChromeToBackground();
    }

    /** Test that URLs opened from different external apps do create new tabs. */
    @Test
    @LargeTest
    @Feature({"Navigation", "Main"})
    public void testNewTabForDifferentApps() {
        CtaPageStation page = mActivityTestRule.startOnBlankPage();

        String url1 = mTestServer.getURL("/chrome/test/data/android/google.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/about.html");
        String url3 = mTestServer.getURL("/chrome/test/data/android/test.html");

        // Launch a first URL from an app1.
        Intent intent1 = createViewIntent(url1, EXTERNAL_APP_1_ID);
        page = sendNewIntentTo(page, intent1).arriveAt(newPageStationInNewTab(url1));

        // Launch a second URL from an app2, it should open in a new tab.
        Intent intent2 = createViewIntent(url2, EXTERNAL_APP_2_ID);
        page =
                sendNewIntentTo(page, intent2)
                        .waitForAnd(new TabCountChangedCondition(page.getTabModel(), +1))
                        .arriveAt(newPageStationInNewTab(url2));

        // Also try with no app id, it should also open in a new tab.
        Intent intent3 = createViewIntent(url3, /* appId= */ null);
        sendNewIntentTo(page, intent3)
                .waitForAnd(new TabCountChangedCondition(page.getTabModel(), +1))
                .arriveAt(newPageStationInNewTab(url3));
    }
}
