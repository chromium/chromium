// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.instantapps;

import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.net.Uri;
import android.nfc.NfcAdapter;
import android.provider.Browser;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Unit tests for {@link InstantAppsHandler}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class InstantAppsHandlerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private TestInstantAppsHandler mHandler;
    private Context mContext;

    private static final Uri URI = Uri.parse("http://sampleurl.com/foo");
    private static final String INSTANT_APP_URL = "http://sampleapp.com/boo";
    private static final Uri REFERRER_URI = Uri.parse("http://www.wikipedia.org/");

    private Intent createViewIntent() {
        return new Intent(Intent.ACTION_VIEW, URI);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        mContext = InstrumentationRegistry.getTargetContext();
        mHandler = new TestInstantAppsHandler();

        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean("applink.app_link_enabled", true);
        editor.putBoolean("applink.chrome_default_browser", true);
        editor.apply();
    }

    @Test
    @SmallTest
    public void testInstantAppsDisabled_incognito() {
        Intent i = createViewIntent();
        i.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);

        Assert.assertFalse(mHandler.handleIncomingIntent(mContext, i, false, true));
    }

    @Test
    @SmallTest
    public void testInstantAppsDisabled_doNotLaunch() {
        Intent i = createViewIntent();
        i.putExtra("com.google.android.gms.instantapps.DO_NOT_LAUNCH_INSTANT_APP", true);

        Assert.assertFalse(mHandler.handleIncomingIntent(mContext, i, false, true));
    }

    @Test
    @SmallTest
    public void testInstantAppsDisabled_mainIntent() {
        Intent i = new Intent(Intent.ACTION_MAIN);
        Assert.assertFalse(mHandler.handleIncomingIntent(mContext, i, false, true));
    }

    @Test
    @SmallTest
    public void testInstantAppsDisabled_intentOriginatingFromChrome() {
        Intent i = createViewIntent();
        i.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());

        Assert.assertFalse(mHandler.handleIncomingIntent(mContext, i, false, true));

        Intent signedIntent = createViewIntent();
        signedIntent.setPackage(mContext.getPackageName());
        IntentHandler.addTrustedIntentExtras(signedIntent);

        Assert.assertFalse(mHandler.handleIncomingIntent(mContext, signedIntent, false, true));
    }

    @Test
    @SmallTest
    public void testInstantAppsDisabled_launchFromShortcut() {
        Intent i = createViewIntent();
        i.putExtra(ShortcutHelper.EXTRA_SOURCE, 1);
        Assert.assertFalse(mHandler.handleIncomingIntent(mContext, i, false, true));
    }

    @Test
    @SmallTest
    public void testChromeNotDefault() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putBoolean("applink.chrome_default_browser", false);
        editor.apply();

        Assert.assertFalse(
                mHandler.handleIncomingIntent(mContext, createViewIntent(), false, true));

        // Even if Chrome is not default, launch Instant Apps for CustomTabs since those never
        // show disambiguation dialogs.
        Intent cti = createViewIntent()
                .putExtra("android.support.customtabs.extra.EXTRA_ENABLE_INSTANT_APPS", true);
        Assert.assertTrue(mHandler.handleIncomingIntent(mContext, cti, true, true));
    }

    @Test
    @SmallTest
    public void testInstantAppsEnabled() {
        Intent i = createViewIntent();
        Assert.assertTrue(mHandler.handleIncomingIntent(
                InstrumentationRegistry.getContext(), i, false, true));

        // Check that identical intent wouldn't be enabled for CustomTab flow.
        Assert.assertFalse(
                mHandler.handleIncomingIntent(InstrumentationRegistry.getContext(), i, true, true));

        // Add CustomTab specific extra and check it's now enabled.
        i.putExtra("android.support.customtabs.extra.EXTRA_ENABLE_INSTANT_APPS", true);
        Assert.assertTrue(
                mHandler.handleIncomingIntent(InstrumentationRegistry.getContext(), i, true, true));
    }

    @Test
    @SmallTest
    public void testNfcIntent() {
        Intent i = new Intent(NfcAdapter.ACTION_NDEF_DISCOVERED);
        i.setData(Uri.parse("http://instantapp.com/"));
        Assert.assertTrue(mHandler.handleIncomingIntent(
                InstrumentationRegistry.getContext(), i, false, true));
    }

    @Test
    @SmallTest
    public void testHandleNavigation_startAsyncCheck() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertFalse(
                                mHandler.handleNavigation(mContext, INSTANT_APP_URL, REFERRER_URI,
                                        mActivityTestRule.getActivity()
                                                .getTabModelSelector()
                                                .getCurrentTab())));
        Assert.assertFalse(mHandler.mLaunchInstantApp);
        Assert.assertTrue(mHandler.mStartedAsyncCall);
    }

    @Test
    @SmallTest
    public void testLaunchFromBanner() {
        // Intent to supervisor
        final Intent i = new Intent(Intent.ACTION_MAIN);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        new IntentFilter(Intent.ACTION_MAIN), null, true);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHandler.launchFromBanner(new InstantAppsBannerData("App", null,
                                INSTANT_APP_URL, REFERRER_URI, i, "Launch",
                                mActivityTestRule.getActivity()
                                        .getTabModelSelector()
                                        .getCurrentTab()
                                        .getWebContents(),
                                false)));

        // Started instant apps intent
        Assert.assertEquals(1, monitor.getHits());

        Assert.assertEquals(REFERRER_URI, i.getParcelableExtra(Intent.EXTRA_REFERRER));
        Assert.assertTrue(i.getBooleanExtra(InstantAppsHandler.IS_REFERRER_TRUSTED_EXTRA, false));
        Assert.assertTrue(
                i.getBooleanExtra(InstantAppsHandler.IS_USER_CONFIRMED_LAUNCH_EXTRA, false));
        Assert.assertEquals(mContext.getPackageName(),
                i.getStringExtra(InstantAppsHandler.TRUSTED_REFERRER_PKG_EXTRA));

        // After a banner launch, test that the next launch happens automatically

        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertTrue(
                                mHandler.handleNavigation(mContext, INSTANT_APP_URL, REFERRER_URI,
                                        mActivityTestRule.getActivity()
                                                .getTabModelSelector()
                                                .getCurrentTab())));
        Assert.assertFalse(mHandler.mStartedAsyncCall);
        Assert.assertTrue(mHandler.mLaunchInstantApp);
    }

    static class TestInstantAppsHandler extends InstantAppsHandler {
        // Keeps track of whether startCheckForInstantApps() has been called.
        public volatile boolean mStartedAsyncCall;
        // Keeps track of whether launchInstantAppForNavigation() has been called.
        public volatile boolean mLaunchInstantApp;

        @Override
        protected boolean tryLaunchingInstantApp(Context context, Intent intent,
                boolean isCustomTabsIntent, Intent fallbackIntent) {
            return true;
        }

        @Override
        protected boolean launchInstantAppForNavigation(Context context, String url, Uri referrer) {
            mLaunchInstantApp = true;
            return true;
        }

        @Override
        protected void maybeShowInstantAppBanner(
                Context context, String url, Uri referrer, Tab tab, boolean instantAppIsDefault) {
            mStartedAsyncCall = true;
        }
    }
}
