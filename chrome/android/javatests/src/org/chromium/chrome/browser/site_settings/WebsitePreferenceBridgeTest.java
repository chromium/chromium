// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * Tests for WebsitePreferenceBridgeTest.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class WebsitePreferenceBridgeTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @After
    public void tearDown() throws TimeoutException {
        // Clean up content settings.
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(helper::notifyCalled,
                    new int[] {BrowsingDataType.SITE_SETTINGS}, TimePeriod.ALL_TIME);
        });
        helper.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testModifyContentSettings() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowserContextHandle browserContext = Profile.getLastUsedRegularProfile();
            GURL url = new GURL("https://example.com");
            assertEquals(ContentSettingValues.ALLOW,
                    WebsitePreferenceBridge.getContentSetting(
                            browserContext, ContentSettingsType.JAVASCRIPT, url, url));
            WebsitePreferenceBridge.setContentSettingDefaultScope(browserContext,
                    ContentSettingsType.JAVASCRIPT, url, url, ContentSettingValues.BLOCK);
            assertEquals(ContentSettingValues.BLOCK,
                    WebsitePreferenceBridge.getContentSetting(
                            browserContext, ContentSettingsType.JAVASCRIPT, url, url));
        });
    }
}
