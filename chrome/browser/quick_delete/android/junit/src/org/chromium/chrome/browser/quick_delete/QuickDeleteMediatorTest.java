// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashSet;
import java.util.List;

/**
 * Robolectric tests for {@link QuickDeleteMediator}.
 */

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
@Batch(Batch.UNIT_TESTS)
public class QuickDeleteMediatorTest {
    @Mock
    private IdentityManager mIdentityManagerMock;
    @Mock
    private SyncService mSyncServiceMock;
    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock
    private Profile mProfileMock;
    @Mock
    private QuickDeleteBridge mQuickDeleteBridgeMock;
    @Mock
    private QuickDeleteTabsFilter mQuickDeleteTabsFilterMock;
    @Mock
    private List<Tab> mTabsListMock;

    private PropertyModel mPropertyModel;
    private QuickDeleteMediator mQuickDeleteMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mIdentityServicesProviderMock.getIdentityManager(mProfileMock))
                .thenReturn(mIdentityManagerMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);

        mPropertyModel = new PropertyModel.Builder(QuickDeleteProperties.ALL_KEYS).build();
    }

    private void setSignedInStatus(boolean isSignedIn) {
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(isSignedIn);
    }

    private void setHistorySyncStatus(boolean isSyncing) {
        when(mSyncServiceMock.isSyncFeatureEnabled()).thenReturn(isSyncing);
        when(mSyncServiceMock.getActiveDataTypes())
                .thenReturn(isSyncing
                                ? CollectionUtil.newHashSet(ModelType.HISTORY_DELETE_DIRECTIVES)
                                : new HashSet<Integer>());
    }

    @Test
    @SmallTest
    public void testQuickDeleteMediatorInit_InvokesChanges() {
        setSignedInStatus(true);
        setHistorySyncStatus(true);

        when(mTabsListMock.size()).thenReturn(1);
        when(mQuickDeleteTabsFilterMock.getListOfTabsToBeClosed(eq(TimePeriod.LAST_15_MINUTES)))
                .thenReturn(mTabsListMock);

        mQuickDeleteMediator = new QuickDeleteMediator(
                mPropertyModel, mProfileMock, mQuickDeleteBridgeMock, mQuickDeleteTabsFilterMock);
        assertTrue(mPropertyModel.get(QuickDeleteProperties.IS_SIGNED_IN));
        assertTrue(mPropertyModel.get(QuickDeleteProperties.IS_SYNCING_HISTORY));
        assertEquals(1, mPropertyModel.get(QuickDeleteProperties.CLOSED_TABS_COUNT));
        verify(mQuickDeleteBridgeMock)
                .getLastVisitedDomainAndUniqueDomainCount(eq(TimePeriod.LAST_15_MINUTES), any());
    }
}
