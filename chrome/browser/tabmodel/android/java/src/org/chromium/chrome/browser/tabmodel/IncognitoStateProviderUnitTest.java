// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;

/** Unit tests for {@link IncognitoStateProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class IncognitoStateProviderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private IncognitoStateProvider.IncognitoStateObserver mObserver1;
    @Mock private IncognitoStateProvider.IncognitoStateObserver mObserver2;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Profile mStandardProfile;
    @Mock private Profile mIncognitoProfile;

    private MockTabModel mStandardTabModel;
    private MockTabModel mIncognitoTabModel;
    private SettableMonotonicObservableSupplier<TabModel> mTabModelSupplier;
    private IncognitoStateProvider mProvider;

    @Before
    public void setUp() {
        when(mStandardProfile.isOffTheRecord()).thenReturn(false);
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        mStandardTabModel = new MockTabModel(mStandardProfile, null);
        mIncognitoTabModel = new MockTabModel(mIncognitoProfile, null);

        mTabModelSupplier = ObservableSuppliers.createMonotonic();
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);
        mProvider = new IncognitoStateProvider();
    }

    @Test
    public void testAddObserverAndTrigger_notifiesInitialState() {
        assertEquals(0, mProvider.getObserverCountForTesting());
        mProvider.addIncognitoStateObserverAndTrigger(mObserver1);
        assertEquals(1, mProvider.getObserverCountForTesting());
        verify(mObserver1).onIncognitoStateChanged(false);
    }

    @Test
    public void testRemoveObserver_stopsNotifications() {
        mProvider.addIncognitoStateObserverAndTrigger(mObserver1);
        verify(mObserver1).onIncognitoStateChanged(false);

        mProvider.removeObserver(mObserver1);
        assertEquals(0, mProvider.getObserverCountForTesting());

        mProvider.setIncognitoStateForTesting(true);
        verify(mObserver1, never()).onIncognitoStateChanged(true);
    }

    @Test
    public void testMultipleObservers_allNotifiedOnStateChange() {
        mProvider.addIncognitoStateObserverAndTrigger(mObserver1);
        mProvider.addIncognitoStateObserverAndTrigger(mObserver2);
        assertEquals(2, mProvider.getObserverCountForTesting());

        verify(mObserver1).onIncognitoStateChanged(false);
        verify(mObserver2).onIncognitoStateChanged(false);

        mProvider.setIncognitoStateForTesting(true);
        verify(mObserver1).onIncognitoStateChanged(true);
        verify(mObserver2).onIncognitoStateChanged(true);
    }

    @Test
    public void testSetTabModelSelector_observesCurrentTabModelAndEmits() {
        InOrder inOrder = inOrder(mObserver1);
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        mProvider.addIncognitoStateObserverAndTrigger(mObserver1);
        inOrder.verify(mObserver1).onIncognitoStateChanged(false);

        mProvider.setTabModelSelector(mTabModelSelector);
        inOrder.verify(mObserver1).onIncognitoStateChanged(true);

        mTabModelSupplier.set(mStandardTabModel);
        inOrder.verify(mObserver1).onIncognitoStateChanged(false);
    }

    @Test
    public void testDestroy_unregistersObserverAndClearsList() {
        mProvider.setTabModelSelector(mTabModelSelector);
        mProvider.addIncognitoStateObserverAndTrigger(mObserver1);
        assertEquals(1, mProvider.getObserverCountForTesting());

        mProvider.destroy();
        assertEquals(0, mProvider.getObserverCountForTesting());

        mProvider.setIncognitoStateForTesting(true);
        verify(mObserver1, never()).onIncognitoStateChanged(true);
    }

    @Test
    public void testShortCircuitDuplicateEmissions() {
        mProvider.addIncognitoStateObserverAndTrigger(mObserver1);
        verify(mObserver1).onIncognitoStateChanged(false);

        // Duplicate emission to false should be short-circuited
        mProvider.setIncognitoStateForTesting(false);
        verify(mObserver1, times(1)).onIncognitoStateChanged(false);

        // Toggle to true
        mProvider.setIncognitoStateForTesting(true);
        verify(mObserver1).onIncognitoStateChanged(true);

        // Duplicate call to true should be short-circuited
        mProvider.setIncognitoStateForTesting(true);
        verify(mObserver1, times(1)).onIncognitoStateChanged(true);
    }

    @Test
    public void testDestroyResetsStateTracking() {
        mProvider.addIncognitoStateObserverAndTrigger(mObserver1);
        mProvider.setIncognitoStateForTesting(true);
        verify(mObserver1).onIncognitoStateChanged(true);

        mProvider.destroy();
        mProvider.addIncognitoStateObserverAndTrigger(mObserver2);

        // Re-emitting true after destroy should notify new observers
        mProvider.setIncognitoStateForTesting(true);
        verify(mObserver2).onIncognitoStateChanged(true);
    }

    @Test
    public void testShortCircuitWithTabModelSelector() {
        mProvider.setTabModelSelector(mTabModelSelector);
        mProvider.addIncognitoStateObserverAndTrigger(mObserver1);
        verify(mObserver1).onIncognitoStateChanged(false);

        // Setting standard tab model (incognito = false) should short-circuit since state was
        // already false
        mTabModelSupplier.set(mStandardTabModel);
        verify(mObserver1, times(1)).onIncognitoStateChanged(false);

        // Transitioning to incognito tab model (incognito = true) should notify
        mTabModelSupplier.set(mIncognitoTabModel);
        verify(mObserver1).onIncognitoStateChanged(true);
    }
}
