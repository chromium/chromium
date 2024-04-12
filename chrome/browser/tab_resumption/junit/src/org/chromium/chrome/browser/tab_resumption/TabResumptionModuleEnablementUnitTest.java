// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import androidx.annotation.Nullable;
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
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleNotShownReason;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabResumptionModuleEnablementUnitTest {
    @Rule public JUnitProcessor mFeaturesProcessor = new JUnitProcessor();

    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SyncService mSyncService;
    @Mock private IdentityManager mIdentityManager;
    @Mock private Profile mProfile;
    @Mock private ModuleDelegate mModuleDelegate;
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
    @DisableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
    public void testEnablementWithoutForeignSessionFeature() {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(false);
        Assert.assertEquals(ModuleNotShownReason.FEATURE_DISABLED, getNotShownReason().intValue());
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(true);
        Assert.assertEquals(ModuleNotShownReason.FEATURE_DISABLED, getNotShownReason().intValue());

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(false);
        Assert.assertEquals(ModuleNotShownReason.FEATURE_DISABLED, getNotShownReason().intValue());
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(true);
        Assert.assertEquals(ModuleNotShownReason.FEATURE_DISABLED, getNotShownReason().intValue());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
    public void testEnablementWithForeignSessionFeature() {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(false);
        Assert.assertEquals(ModuleNotShownReason.NOT_SIGNED_IN, getNotShownReason().intValue());
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(true);
        Assert.assertEquals(ModuleNotShownReason.NOT_SIGNED_IN, getNotShownReason().intValue());

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(false);
        Assert.assertEquals(ModuleNotShownReason.NOT_SYNC, getNotShownReason().intValue());
        when(mSyncService.hasKeepEverythingSynced()).thenReturn(true);
        Assert.assertNull(getNotShownReason());
    }

    private @Nullable Integer getNotShownReason() {
        return TabResumptionModuleEnablement.computeModuleNotShownReason(mModuleDelegate, mProfile);
    }
}
