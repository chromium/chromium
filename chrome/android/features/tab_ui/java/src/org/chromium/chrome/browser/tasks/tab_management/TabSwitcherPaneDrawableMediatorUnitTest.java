// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherPaneDrawableProperties.TAB_COUNT;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for the {@link TabSwitcherPaneDrawableMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherPaneDrawableMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;

    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;

    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>();

    private Context mContext;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mModel = new PropertyModel.Builder(TabSwitcherPaneDrawableProperties.ALL_KEYS).build();

        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getTabCountSupplier()).thenReturn(mTabCountSupplier);
        when(mTabModel.isIncognito()).thenReturn(false);

        mTabCountSupplier.set(1);
    }

    @After
    public void tearDown() {
        assertFalse(mTabCountSupplier.hasObservers());
    }

    @Test
    @SmallTest
    public void testMediatorEarlyTabModelSelectorInit() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        var mediator = new TabSwitcherPaneDrawableMediator(mTabModelSelector, mModel);
        verify(mTabModelSelector, never()).addObserver(any());

        assertTrue(mTabCountSupplier.hasObservers());

        ShadowLooper.runUiThreadTasks();

        assertEquals(mTabCountSupplier.get().intValue(), mModel.get(TAB_COUNT));

        mTabCountSupplier.set(50);
        assertEquals(mTabCountSupplier.get().intValue(), mModel.get(TAB_COUNT));

        mediator.destroy();
    }

    @Test
    @SmallTest
    public void testMediatorLateTabModelSelectorInit() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        var mediator = new TabSwitcherPaneDrawableMediator(mTabModelSelector, mModel);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        assertTrue(mTabCountSupplier.hasObservers());
        verify(mTabModelSelector).removeObserver(any());

        ShadowLooper.runUiThreadTasks();

        assertEquals(mTabCountSupplier.get().intValue(), mModel.get(TAB_COUNT));

        mTabCountSupplier.set(30);
        assertEquals(mTabCountSupplier.get().intValue(), mModel.get(TAB_COUNT));

        mediator.destroy();
    }

    @Test
    @SmallTest
    public void testDestroyBeforeInitAvoidsLeak() {
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        var mediator = new TabSwitcherPaneDrawableMediator(mTabModelSelector, mModel);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        mediator.destroy();

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        assertFalse(mTabCountSupplier.hasObservers());
    }
}
