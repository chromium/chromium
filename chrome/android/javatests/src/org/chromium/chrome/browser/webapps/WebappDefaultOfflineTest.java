// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;

import android.content.Intent;
import android.graphics.Color;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflineTestUtil;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

/** Tests for the Default Offline behavior. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappDefaultOfflineTest {
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    /** Simulates what happens when you launch a web app when the network is down. */
    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testDefaultOffline() throws Exception {
        // Make sure the navigations to the test app result in a 404 error.
        final String testAppUrl =
                WebappTestPage.getServiceWorkerUrl(mActivityTestRule.getTestServer());
        OfflineTestUtil.interceptWithOfflineError(testAppUrl);

        WebApkDataProvider.setWebappInfoForTesting(getDefaultWebappInfo(testAppUrl));

        // Launch the test app.
        runWebappActivityAndWaitForIdle(mActivityTestRule.createIntent());

        // Ensure that web_app_default_offline.html is showing the correct values.
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        assertEquals(
                "\"shortname\"",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.title;"));
        assertEquals(
                "\"You're offline\"",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(),
                        "document.getElementById('default-web-app-msg').textContent;"));
        assertEquals(
                "\"data:image/png;base64," + WebappActivityTestRule.TEST_ICON + "\"",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.getElementById('icon').src;"));
        assertEquals(
                "\"inline\"",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(),
                        "document.getElementById('offlineIcon').style.display;"));
    }

    private WebappInfo getDefaultWebappInfo(String url) {
        String id = "webapp_id";
        String name = "longName";
        String shortName = "shortname";
        long backgroundColor = Color.argb(0xff, 0x0, 0xff, 0x0);
        long themeColor = Color.argb(0xff, 0, 0, 0xff);

        Intent intent = new Intent();
        intent.putExtra(WebappConstants.EXTRA_ID, id);
        intent.putExtra(WebappConstants.EXTRA_NAME, name);
        intent.putExtra(WebappConstants.EXTRA_SHORT_NAME, shortName);
        intent.putExtra(WebappConstants.EXTRA_URL, url);
        intent.putExtra(WebappConstants.EXTRA_ICON, WebappActivityTestRule.TEST_ICON);
        intent.putExtra(WebappConstants.EXTRA_BACKGROUND_COLOR, backgroundColor);
        intent.putExtra(WebappConstants.EXTRA_THEME_COLOR, themeColor);

        return WebappInfo.create(WebappIntentDataProviderFactory.create(intent));
    }

    private WebappActivity runWebappActivityAndWaitForIdle(Intent intent) {
        return runWebappActivityAndWaitForIdleWithUrl(
                intent, WebappTestPage.getServiceWorkerUrl(mActivityTestRule.getTestServer()));
    }

    private WebappActivity runWebappActivityAndWaitForIdleWithUrl(Intent intent, String url) {
        mActivityTestRule.startWebappActivity(intent.putExtra(WebappConstants.EXTRA_URL, url));
        return mActivityTestRule.getActivity();
    }
}
