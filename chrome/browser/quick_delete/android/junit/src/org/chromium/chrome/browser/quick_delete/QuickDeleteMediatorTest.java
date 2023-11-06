// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Robolectric tests for {@link QuickDeleteMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
@Batch(Batch.UNIT_TESTS)
public class QuickDeleteMediatorTest {
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock private Profile mProfileMock;
    @Mock private QuickDeleteBridge mQuickDeleteBridgeMock;
    @Mock private QuickDeleteTabsFilter mQuickDeleteTabsFilterMock;
    @Mock private List<Tab> mTabsListMock;

    private PropertyModel mPropertyModel;
    private QuickDeleteMediator mQuickDeleteMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mIdentityServicesProviderMock.getIdentityManager(mProfileMock))
                .thenReturn(mIdentityManagerMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);

        mPropertyModel = new PropertyModel.Builder(QuickDeleteProperties.ALL_KEYS).build();
    }

    private void setSignedInStatus(boolean isSignedIn) {
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(isSignedIn);
    }

    @Test
    @SmallTest
    public void testQuickDeleteMediatorInit_InvokesChanges() {
        setSignedInStatus(true);

        when(mTabsListMock.size()).thenReturn(1);
        when(mQuickDeleteTabsFilterMock.getListOfTabsToBeClosed(eq(TimePeriod.LAST_15_MINUTES)))
                .thenReturn(mTabsListMock);

        mQuickDeleteMediator =
                new QuickDeleteMediator(
                        mPropertyModel,
                        mProfileMock,
                        mQuickDeleteBridgeMock,
                        mQuickDeleteTabsFilterMock);
        mQuickDeleteMediator.onTimePeriodChanged(TimePeriod.LAST_15_MINUTES);

        assertTrue(mPropertyModel.get(QuickDeleteProperties.IS_SIGNED_IN));
        assertEquals(1, mPropertyModel.get(QuickDeleteProperties.CLOSED_TABS_COUNT));
        assertEquals(
                TimePeriod.LAST_15_MINUTES, mPropertyModel.get(QuickDeleteProperties.TIME_PERIOD));
        assertTrue(mPropertyModel.get(QuickDeleteProperties.IS_DOMAIN_VISITED_DATA_PENDING));
        assertFalse(mPropertyModel.get(QuickDeleteProperties.IS_SYNCING_HISTORY));
        verify(mQuickDeleteBridgeMock)
                .getLastVisitedDomainAndUniqueDomainCount(eq(TimePeriod.LAST_15_MINUTES), any());
    }
}
