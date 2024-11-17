// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Unit test for {@link AutofillVcnEnrollBottomSheetLifecycle}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillVcnEnrollBottomSheetLifecycleTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabModel mNewTabModel;
    @Mock private Tab mTab;

    private boolean mObserverWasNotifiedAboutLifecycleEnd;
    private AutofillVcnEnrollBottomSheetLifecycle mLifecycle;

    @Before
    public void setUp() {
        mObserverWasNotifiedAboutLifecycleEnd = false;
        mLifecycle =
                new AutofillVcnEnrollBottomSheetLifecycle(
                        mLayoutStateProvider, mTabModelSelectorSupplier);
    }

    @Test
    public void canBeginInBrowsingMode() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.BROWSING)).thenReturn(true);

        assertTrue(mLifecycle.canBegin());
    }

    @Test
    public void cannotBeginNotInBrowsingMode() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.BROWSING)).thenReturn(false);

        assertFalse(mLifecycle.canBegin());
    }

    @Test
    public void cannotBeginTwice() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.BROWSING)).thenReturn(true);
        mLifecycle.begin(/* onEndOfLifecycle= */ CallbackUtils.emptyRunnable());

        assertFalse(mLifecycle.canBegin());
    }

    @Test
    public void testBeginStartsObservingLayoutAndTabModelSelectorSupplier() {
        mLifecycle.begin(/* onEndOfLifecycle= */ CallbackUtils.emptyRunnable());

        verify(mLayoutStateProvider).addObserver(eq(mLifecycle));
        verify(mTabModelSelectorSupplier).addObserver(eq(mLifecycle));
    }

    @Test
    public void testEndStopsObservingLayoutAndTabModelSelectorSupplier() {
        mLifecycle.end();

        verify(mLayoutStateProvider).removeObserver(eq(mLifecycle));
        verify(mTabModelSelectorSupplier).removeObserver(eq(mLifecycle));
    }

    @Test
    public void testLayoutStateChangeEndsLifecycleAndNotifiesObserver() {
        mLifecycle.begin(this::notifyObserverAboutLifecycleEnd);

        ((LayoutStateObserver) mLifecycle).onStartedShowing(/* layoutType= */ -1);

        verify(mLayoutStateProvider).removeObserver(eq(mLifecycle));
        verify(mTabModelSelectorSupplier).removeObserver(eq(mLifecycle));
        assertTrue(mObserverWasNotifiedAboutLifecycleEnd);
    }

    @Test
    public void testEndLifecycleAfterTabModelSelectionWithCurrentModel() {
        mLifecycle.begin(this::notifyObserverAboutLifecycleEnd);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.index()).thenReturn(0);
        mLifecycle.onResult(mTabModelSelector);

        mLifecycle.onTabModelSelected(/* newModel= */ mNewTabModel, /* oldModel= */ mTabModel);

        assertTrue(mObserverWasNotifiedAboutLifecycleEnd);
        verifyNoInteractions(mNewTabModel);
    }

    @Test
    public void testEndLifecycleAfterTabModelSelectionWithoutCurrentModel() {
        mLifecycle.begin(this::notifyObserverAboutLifecycleEnd);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.index()).thenReturn(-1);
        mLifecycle.onResult(mTabModelSelector);

        mLifecycle.onTabModelSelected(/* newModel= */ mNewTabModel, /* oldModel= */ mTabModel);

        assertFalse(mObserverWasNotifiedAboutLifecycleEnd);
        verify(mNewTabModel).addObserver(eq(mLifecycle));
    }

    @Test
    public void testNullTabSelectionEndsLifecycle() {
        mLifecycle.begin(this::notifyObserverAboutLifecycleEnd);
        mLifecycle.didSelectTab(/* tab= */ null, TabSelectionType.FROM_USER, /* lastId= */ -1);

        assertTrue(mObserverWasNotifiedAboutLifecycleEnd);
    }

    @Test
    public void testDifferentTabSelectionEndsLifecycle() {
        mLifecycle.begin(this::notifyObserverAboutLifecycleEnd);
        when(mTab.getId()).thenReturn(1);
        mLifecycle.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 0);

        assertTrue(mObserverWasNotifiedAboutLifecycleEnd);
    }

    @Test
    public void testSameTabSelectionDoesNotEndLifecycle() {
        mLifecycle.begin(this::notifyObserverAboutLifecycleEnd);
        when(mTab.getId()).thenReturn(1);
        mLifecycle.didSelectTab(mTab, TabSelectionType.FROM_USER, /* lastId= */ 1);

        assertFalse(mObserverWasNotifiedAboutLifecycleEnd);
    }

    private void notifyObserverAboutLifecycleEnd() {
        mObserverWasNotifiedAboutLifecycleEnd = true;
    }
}
