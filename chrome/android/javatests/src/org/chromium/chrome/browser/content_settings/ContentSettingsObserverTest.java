// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_settings;

import android.text.TextUtils;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsObserver;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.ContentSettingsTypeSet;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Browser tests for {@link ContentSettingsObserver}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ContentSettingsObserverTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private CallbackHelper mCallbackHelper = new CallbackHelper();

    private String mLastPrimaryPattern;
    private String mLastSecondaryPattern;
    private ContentSettingsTypeSet mLastTypeSet;

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
    @UiThreadTest
    public void testContentSettingChanges() throws TimeoutException {
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        ContentSettingsObserver observer =
                new ContentSettingsObserver(profile) {
                    @Override
                    protected void onContentSettingChanged(
                            String primaryPattern,
                            String secondaryPattern,
                            ContentSettingsTypeSet lastTypeSet) {
                        mCallbackHelper.notifyCalled();

                        mLastPrimaryPattern = primaryPattern;
                        mLastSecondaryPattern = secondaryPattern;
                        mLastTypeSet = lastTypeSet;
                    }
                };

        GURL url = new GURL("https://www.chromium.org");
        WebsitePreferenceBridge.setContentSettingDefaultScope(
                profile, ContentSettingsType.JAVASCRIPT, url, url, ContentSettingValues.BLOCK);

        mCallbackHelper.waitForOnly();
        Assert.assertNotNull("ContentSettingsTypeSet should not be null.", mLastTypeSet);
        Assert.assertTrue(
                "ContentSettingsType.JAVASCRIPT should be the latest changed content type.",
                mLastTypeSet.contains(ContentSettingsType.JAVASCRIPT));
        Assert.assertFalse(
                "The primary pattern should not be empty.", TextUtils.isEmpty(mLastPrimaryPattern));
        Assert.assertFalse(
                "The secondary pattern should not be empty.",
                TextUtils.isEmpty(mLastSecondaryPattern));

        // Destroy the observer and no updates should be posted for mCallbackHelper.
        observer.destroy();
        WebsitePreferenceBridge.setContentSettingDefaultScope(
                profile, ContentSettingsType.JAVASCRIPT, url, url, ContentSettingValues.DEFAULT);
        Assert.assertEquals(
                "Content settings should be updated for URL.",
                ContentSettingValues.ALLOW,
                WebsitePreferenceBridge.getContentSetting(
                        profile, ContentSettingsType.JAVASCRIPT, url, url));
        Assert.assertEquals(
                "Updates should no longer notify ContentSettingsObserver.",
                1,
                mCallbackHelper.getCallCount());
    }
}
