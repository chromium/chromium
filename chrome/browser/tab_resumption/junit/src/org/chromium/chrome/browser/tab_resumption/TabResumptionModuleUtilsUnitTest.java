// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import android.content.res.Resources;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Features.JUnitProcessor;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;

import java.util.concurrent.TimeUnit;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabResumptionModuleUtilsUnitTest {
    @Rule public JUnitProcessor mFeaturesProcessor = new JUnitProcessor();

    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SyncService mSyncService;
    @Mock private IdentityManager mIdentityManager;
    @Mock private Profile mProfile;
    private FeatureList.TestValues mFeatureListValues;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
    }

    @Test
    @SmallTest
    public void testRecencyString() {
        Resources res = ApplicationProvider.getApplicationContext().getResources();
        long dayInMs = TimeUnit.DAYS.toMillis(1);
        Assert.assertEquals("just now", TabResumptionModuleUtils.getRecencyString(res, -1000000L));
        Assert.assertEquals("just now", TabResumptionModuleUtils.getRecencyString(res, 0L));
        Assert.assertEquals("just now", TabResumptionModuleUtils.getRecencyString(res, 59999L));
        Assert.assertEquals("1 minute ago", TabResumptionModuleUtils.getRecencyString(res, 60000L));
        Assert.assertEquals(
                "1 minute ago", TabResumptionModuleUtils.getRecencyString(res, 119999L));
        Assert.assertEquals(
                "2 minutes ago", TabResumptionModuleUtils.getRecencyString(res, 120000L));
        Assert.assertEquals(
                "59 minutes ago", TabResumptionModuleUtils.getRecencyString(res, 3599999L));
        Assert.assertEquals("1 hour ago", TabResumptionModuleUtils.getRecencyString(res, 3600000L));
        Assert.assertEquals("1 hour ago", TabResumptionModuleUtils.getRecencyString(res, 7199999L));
        Assert.assertEquals(
                "2 hours ago", TabResumptionModuleUtils.getRecencyString(res, 7200000L));
        Assert.assertEquals(
                "23 hours ago", TabResumptionModuleUtils.getRecencyString(res, dayInMs - 1));
        Assert.assertEquals("1 day ago", TabResumptionModuleUtils.getRecencyString(res, dayInMs));
        Assert.assertEquals(
                "1 day ago", TabResumptionModuleUtils.getRecencyString(res, dayInMs * 2 - 1));
        Assert.assertEquals(
                "2 days ago", TabResumptionModuleUtils.getRecencyString(res, dayInMs * 2));
        Assert.assertEquals(
                "100 days ago", TabResumptionModuleUtils.getRecencyString(res, dayInMs * 100));
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
    public void testShouldShowTabResumptionModuleWhenDisabled() {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(false);
        Assert.assertFalse(TabResumptionModuleUtils.shouldShowTabResumptionModule(mProfile));

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(true);
        Assert.assertFalse(TabResumptionModuleUtils.shouldShowTabResumptionModule(mProfile));

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(false);
        Assert.assertFalse(TabResumptionModuleUtils.shouldShowTabResumptionModule(mProfile));

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(true);
        Assert.assertFalse(TabResumptionModuleUtils.shouldShowTabResumptionModule(mProfile));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
    public void testShouldShowTabResumptionModuleWhenEnabled() {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(false);
        Assert.assertFalse(TabResumptionModuleUtils.shouldShowTabResumptionModule(mProfile));

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(true);
        Assert.assertFalse(TabResumptionModuleUtils.shouldShowTabResumptionModule(mProfile));

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(false);
        Assert.assertFalse(TabResumptionModuleUtils.shouldShowTabResumptionModule(mProfile));

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(true);
        Assert.assertTrue(TabResumptionModuleUtils.shouldShowTabResumptionModule(mProfile));
    }
}
