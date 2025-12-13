// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.junit.Assert.assertEquals;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
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
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
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
                            ContentSetting.ALLOW,
                            WebsitePreferenceBridge.getContentSetting(
                                    browserContext, ContentSettingsType.JAVASCRIPT, url, url));
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            browserContext,
                            ContentSettingsType.JAVASCRIPT,
                            url,
                            url,
                            ContentSetting.BLOCK);
                    assertEquals(
                            ContentSetting.BLOCK,
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
                            ContentSetting.ASK,
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
                            ContentSetting.ALLOW);
                    assertEquals(
                            ContentSetting.ALLOW,
                            WebsitePreferenceBridge.getContentSetting(
                                    browserContext,
                                    ContentSettingsType.STORAGE_ACCESS,
                                    new GURL(primary),
                                    new GURL(secondary)));
                });
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({
        PermissionsAndroidFeatureList.GEOLOCATION_ELEMENT,
        PermissionsAndroidFeatureList.PERMISSION_HEURISTIC_AUTO_GRANT
    })
    @Features.DisableFeatures({PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION})
    public void testHeuristicDataCleared() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowserContextHandle browserContext =
                            ProfileManager.getLastUsedRegularProfile();
                    GURL url = new GURL("https://example.com");
                    int type = ContentSettingsType.GEOLOCATION;

                    // Set content setting to ALLOW and record some heuristic data.
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            browserContext, type, url, url, ContentSetting.ALLOW);
                    WebsitePreferenceBridge.recordHeuristicActionForTesting(
                            browserContext, url.getSpec(), type, 1);
                    Assert.assertTrue(
                            "Heuristic data should exist.",
                            WebsitePreferenceBridge.hasHeuristicDataForTesting(
                                    browserContext, url.getSpec(), type));

                    // Change content setting to BLOCK.
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            browserContext, type, url, url, ContentSetting.BLOCK);
                    Assert.assertFalse(
                            "Heuristic data should be cleared when setting is changed to BLOCK.",
                            WebsitePreferenceBridge.hasHeuristicDataForTesting(
                                    browserContext, url.getSpec(), type));

                    // Set content setting to ALLOW and record some heuristic data again.
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            browserContext, type, url, url, ContentSetting.ALLOW);
                    WebsitePreferenceBridge.recordHeuristicActionForTesting(
                            browserContext, url.getSpec(), type, 1);
                    Assert.assertTrue(
                            "Heuristic data should exist.",
                            WebsitePreferenceBridge.hasHeuristicDataForTesting(
                                    browserContext, url.getSpec(), type));

                    // Change content setting to ASK.
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            browserContext, type, url, url, ContentSetting.ASK);
                    Assert.assertFalse(
                            "Heuristic data should be cleared when setting is changed to ASK.",
                            WebsitePreferenceBridge.hasHeuristicDataForTesting(
                                    browserContext, url.getSpec(), type));

                    // Set content setting to ALLOW and record some heuristic data again.
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            browserContext, type, url, url, ContentSetting.ALLOW);
                    WebsitePreferenceBridge.recordHeuristicActionForTesting(
                            browserContext, url.getSpec(), type, 1);
                    Assert.assertTrue(
                            "Heuristic data should exist.",
                            WebsitePreferenceBridge.hasHeuristicDataForTesting(
                                    browserContext, url.getSpec(), type));

                    // Set content setting to ALLOW again, should not clear data.
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            browserContext, type, url, url, ContentSetting.ALLOW);
                    Assert.assertTrue(
                            "Heuristic data should not be cleared when setting is set to ALLOW"
                                    + " again.",
                            WebsitePreferenceBridge.hasHeuristicDataForTesting(
                                    browserContext, url.getSpec(), type));
                });
    }
}
