// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.junit.Assert.assertEquals;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for WebsitePreferenceBridgeTest. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class WebsitePreferenceBridgeTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    /** Class to parameterize the params for {@link }. */
    public static class EmbargoedParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(true).name("Embargoed"),
                    new ParameterSet().value(false).name("Normal"));
        }
    }

    @After
    public void tearDown() throws TimeoutException {
        // Clean up content settings.
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    helper::notifyCalled,
                                    new int[] {BrowsingDataType.SITE_SETTINGS},
                                    TimePeriod.ALL_TIME);
                });
        helper.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testModifyContentSettings() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowserContextHandle browserContext =
                            ProfileManager.getLastUsedRegularProfile();
                    GURL url = new GURL("https://example.com");
                    assertEquals(
                            ContentSettingValues.ALLOW,
                            WebsitePreferenceBridge.getContentSetting(
                                    browserContext, ContentSettingsType.JAVASCRIPT, url, url));
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            browserContext,
                            ContentSettingsType.JAVASCRIPT,
                            url,
                            url,
                            ContentSettingValues.BLOCK);
                    assertEquals(
                            ContentSettingValues.BLOCK,
                            WebsitePreferenceBridge.getContentSetting(
                                    browserContext, ContentSettingsType.JAVASCRIPT, url, url));
                });
    }

    @Test
    @SmallTest
    @UseMethodParameter(EmbargoedParams.class)
    public void testModifyContentSettingsCustomScope(boolean isEmbargoed) {

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowserContextHandle browserContext =
                            ProfileManager.getLastUsedRegularProfile();
                    String primary = "https://primary.com";
                    String secondary = isEmbargoed ? SITE_WILDCARD : "https://secondary.com";
                    assertEquals(
                            ContentSettingValues.ASK,
                            WebsitePreferenceBridge.getContentSetting(
                                    browserContext,
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL(primary),
                                    new GURL(secondary)));
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            browserContext,
                            ContentSettingsType.STORAGE_ACCESS,
                            primary,
                            secondary,
                            ContentSettingValues.ALLOW);
                    assertEquals(
                            ContentSettingValues.ALLOW,
                            WebsitePreferenceBridge.getContentSetting(
                                    browserContext,
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL(primary),
                                    new GURL(secondary)));
                });
    }
}
