// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;

import android.graphics.drawable.Drawable;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browsing_data.content.BrowsingDataInfo;
import org.chromium.components.browsing_data.content.BrowsingDataModel;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.Map;
import java.util.concurrent.TimeoutException;
import java.util.stream.Collectors;

/** Tests for Chrome's SiteSettingsDelegate implementation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@Batch(SiteSettingsTest.SITE_SETTINGS_BATCH_NAME)
public class ChromeSiteSettingsDelegateTest {

    public static final String BROWSING_DATA_HOST = "browsing-data.com";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    ChromeSiteSettingsDelegate mSiteSettingsDelegate;

    @Before
    public void setUp() throws Exception {
        clearBrowsingData(BrowsingDataType.SITE_DATA, TimePeriod.LAST_HOUR);
    }

    // Tests that a fallback favicon is generated when a real one isn't found locally.
    // This is a regression test for crbug.com/1077716.
    @Test
    @SmallTest
    public void testFallbackFaviconLoads() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
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

    // Tests that getBrowsingDataInfo returns the correct sample test data in the hashmap.
    @Test
    @SmallTest
    public void testGetBrowsingDataInfoCookie() throws TimeoutException {
        setCookie(Scheme.HTTP, BROWSING_DATA_HOST, "'foo1=bar1'");
        setCookie(Scheme.HTTPS, BROWSING_DATA_HOST, "'foo2=bar2'");

        BrowsingDataModel[] browsingDataModel = {null};
        Map<Origin, BrowsingDataInfo> result = getBrowsingDataInfo(browsingDataModel);
        assertEquals(2, result.size());

        // Ensure that the entry matches the set cookie.
        var https_origin = Origin.create(new GURL("https://browsing-data.com"));
        var http_origin = Origin.create(new GURL("http://browsing-data.com"));

        var entries = result.entrySet().stream().collect(Collectors.toList());
        assertEquals(https_origin, entries.get(0).getKey());

        BrowsingDataInfo info = entries.get(0).getValue();
        assertEquals(https_origin, info.getOrigin());
        assertEquals(1, info.getCookieCount());
        assertEquals(0, info.getStorageSize());

        assertEquals(http_origin, entries.get(1).getKey());
    }

    // Tests that removeBrowsingData removes data correctly for a given host.
    @Test
    @SmallTest
    public void testRemoveBrowsingData() throws TimeoutException {
        setCookie(Scheme.HTTP, BROWSING_DATA_HOST, null);

        // Validate the model is populated with one entry.
        BrowsingDataModel[] browsingDataModel = {null};
        Map<Origin, BrowsingDataInfo> result = getBrowsingDataInfo(browsingDataModel);
        assertEquals(1, result.size());

        CallbackHelper helper = new CallbackHelper();

        // Remove browsing-data.com host data.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    browsingDataModel[0].removeBrowsingData(
                            /* host= */ BROWSING_DATA_HOST, helper::notifyCalled);
                });

        helper.waitForNext();

        // Validate model is empty after removal.
        result = getBrowsingDataInfo(browsingDataModel);
        assertEquals(0, result.size());
    }

    private static void setCookie(Scheme scheme, String hostname, String data)
            throws TimeoutException {
        EmbeddedTestServer server =
                scheme == Scheme.HTTPS
                        ? EmbeddedTestServer.createAndStartHTTPSServer(
                                InstrumentationRegistry.getInstrumentation().getContext(),
                                ServerCertificate.CERT_OK)
                        : EmbeddedTestServer.createAndStartServer(
                                ApplicationProvider.getApplicationContext());

        String url =
                server.getURLWithHostName(
                        hostname, "/content/test/data/browsing_data/site_data.html");
        Tab tab = sActivityTestRule.loadUrlInNewTab(url, /* incognito= */ false);

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "setCookie(" + data + ")");

        server.stopAndDestroyServer();
    }

    private enum Scheme {
        HTTP,
        HTTPS
    }

    private void clearBrowsingData(int dataType, int timePeriod) throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    helper::notifyCalled, new int[] {dataType}, timePeriod);
                });
        helper.waitForCallback(0);
    }

    private Map<Origin, BrowsingDataInfo> getBrowsingDataInfo(BrowsingDataModel[] browsingDataModel)
            throws TimeoutException {

        CallbackHelper helper = new CallbackHelper();

        // Run browsing data methods require running on UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSiteSettingsDelegate =
                            new ChromeSiteSettingsDelegate(
                                    sActivityTestRule.getActivity(),
                                    ProfileManager.getLastUsedRegularProfile());
                    mSiteSettingsDelegate.getBrowsingDataModel(
                            model -> {
                                browsingDataModel[0] = model;
                                helper.notifyCalled();
                            });
                });

        helper.waitForNext();

        Map<Origin, BrowsingDataInfo> result =
                browsingDataModel[0].getBrowsingDataInfo(
                        mSiteSettingsDelegate.getBrowserContextHandle(), false);
        return result;
    }
}
