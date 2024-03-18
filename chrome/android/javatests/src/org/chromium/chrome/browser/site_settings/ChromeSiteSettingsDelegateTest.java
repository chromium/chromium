// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;

import android.graphics.drawable.Drawable;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browser_ui.site_settings.BrowsingDataInfo;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.Map;
import java.util.concurrent.TimeoutException;

/** Tests for Chrome's SiteSettingsDelegate implementation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@Batch(SiteSettingsTest.SITE_SETTINGS_BATCH_NAME)
public class ChromeSiteSettingsDelegateTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    ChromeSiteSettingsDelegate mSiteSettingsDelegate;

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
    }

    // Tests that a fallback favicon is generated when a real one isn't found locally.
    // This is a regression test for crbug.com/1077716.
    @Test
    @SmallTest
    public void testFallbackFaviconLoads() throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSiteSettingsDelegate =
                            new ChromeSiteSettingsDelegate(
                                    sActivityTestRule.getActivity(),
                                    ProfileManager.getLastUsedRegularProfile());
                });

        // Hold the Bitmap in an array because it gets assigned to in a closure, and all captured
        // variables have to be effectively final.
        Drawable[] holder = new Drawable[1];
        CallbackHelper helper = new CallbackHelper();
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mSiteSettingsDelegate.getFaviconImageForURL(
                            new GURL("http://url.with.no.favicon"),
                            favicon -> {
                                holder[0] = favicon;
                                helper.notifyCalled();
                            });
                });
        helper.waitForCallback(helper.getCallCount());

        Drawable favicon = holder[0];
        assertThat(favicon).isNotNull();
        assertThat(favicon.getIntrinsicWidth()).isGreaterThan(0);
        assertThat(favicon.getIntrinsicHeight()).isGreaterThan(0);
    }

    // Tests that fetchBrowsingDataInfo returns the correct sample test data in the hashmap.
    @Test
    @SmallTest
    public void testFetchBrowsingDataInfoCookie() throws TimeoutException {

        String url =
                mTestServer.getURLWithHostName(
                        "browsing-data.com", "/content/test/data/browsing_data/site_data.html");
        Tab tab = sActivityTestRule.loadUrlInNewTab(url, /* incognito= */ false);

        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), "setCookie()");

        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSiteSettingsDelegate =
                            new ChromeSiteSettingsDelegate(
                                    sActivityTestRule.getActivity(),
                                    ProfileManager.getLastUsedRegularProfile());

                    // Run browsing data fetcher is required to run on UI thread.
                    mSiteSettingsDelegate.fetchBrowsingDataInfo(
                            result -> {
                                assertEquals(1, result.size());

                                // Ensure that the entry matches the set cookie.
                                var origin = Origin.create(new GURL("http://browsing-data.com"));
                                var entry =
                                        (Map.Entry<Origin, BrowsingDataInfo>)
                                                result.entrySet().iterator().next();
                                assertEquals(origin, entry.getKey());

                                var info = entry.getValue();
                                assertEquals(origin, info.getOrigin());
                                assertEquals(1, info.getCookieCount());
                                assertEquals(0, info.getStorageSize());

                                helper.notifyCalled();
                            });
                });
        helper.waitForFirst();
    }
}
