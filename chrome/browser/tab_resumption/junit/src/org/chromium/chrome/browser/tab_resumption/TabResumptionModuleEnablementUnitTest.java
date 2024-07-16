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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.CollectionUtil;
import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleNotShownReason;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.HashSet;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabResumptionModuleEnablementUnitTest extends TestSupportExtended {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private SyncService mSyncService;
    @Mock private IdentityManager mIdentityManager;
    @Mock private Profile mProfile;
    private FeatureList.TestValues mFeatureListValues;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID,
        ChromeFeatureList.VISITED_URL_RANKING_SERVICE
    })
    public void testEnablementWithoutSignInOrSync() {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(new HashSet<>());
        Assert.assertEquals(ModuleNotShownReason.FEATURE_DISABLED, getNotShownReason().intValue());
        when(mSyncService.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.TABS));
        Assert.assertEquals(ModuleNotShownReason.FEATURE_DISABLED, getNotShownReason().intValue());

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mSyncService.getSelectedTypes()).thenReturn(new HashSet<>());
        Assert.assertEquals(ModuleNotShownReason.FEATURE_DISABLED, getNotShownReason().intValue());
        when(mSyncService.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.TABS));
        Assert.assertEquals(ModuleNotShownReason.FEATURE_DISABLED, getNotShownReason().intValue());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
    @DisableFeatures({ChromeFeatureList.VISITED_URL_RANKING_SERVICE})
    public void testEnablementWithSignInOrSync() {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(new HashSet<>());
        Assert.assertEquals(ModuleNotShownReason.NOT_SIGNED_IN, getNotShownReason().intValue());
        when(mSyncService.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.TABS));
        Assert.assertEquals(ModuleNotShownReason.NOT_SIGNED_IN, getNotShownReason().intValue());

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mSyncService.getSelectedTypes()).thenReturn(new HashSet<>());
        Assert.assertEquals(ModuleNotShownReason.NOT_SYNC, getNotShownReason().intValue());
        when(mSyncService.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.TABS));
        Assert.assertNull(getNotShownReason());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID,
        ChromeFeatureList.VISITED_URL_RANKING_SERVICE
    })
    public void testEnablementWithVisitedUrlRankingFeature() {
        TabResumptionModuleUtils.TAB_RESUMPTION_V2.setForTesting(true);

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(new HashSet<>());
        Assert.assertNull(getNotShownReason());
        when(mSyncService.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.TABS));
        Assert.assertNull(getNotShownReason());

        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mSyncService.getSelectedTypes()).thenReturn(new HashSet<>());
        Assert.assertNull(getNotShownReason());
        when(mSyncService.getSelectedTypes())
                .thenReturn(CollectionUtil.newHashSet(UserSelectableType.TABS));
        Assert.assertNull(getNotShownReason());
    }

    private @Nullable Integer getNotShownReason() {
        return TabResumptionModuleEnablement.computeModuleNotShownReason(mModuleDelegate, mProfile);
    }
}
