// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;

/** Unit tests for {@link TabStateThemeResourceProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabStateThemeResourceProviderTest {
    @Mock private Context mContext;
    @Mock private LayoutManagerImpl mLayoutManager;
    @Mock private Tab mIncognitoTab;
    @Mock private Tab mRegularTab;

    @Captor private ArgumentCaptor<LayoutStateObserver> mLayoutStateObserverCaptor;

    private final ObservableSupplierImpl<LayoutManagerImpl> mLayoutManagerSupplier =
            new ObservableSupplierImpl<>();
    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();
    private TabStateThemeResourceProvider mProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        doReturn(true).when(mIncognitoTab).isIncognitoBranded();
        doReturn(false).when(mRegularTab).isIncognitoBranded();
    }

    @After
    public void tearDown() {
        if (mProvider != null) {
            mProvider.destroy();
        }
    }

    private void createProvider() {
        mProvider =
                new TabStateThemeResourceProvider(
                        mContext, 0, mActivityTabProvider, mLayoutManagerSupplier);
        mLayoutManagerSupplier.set(mLayoutManager);
        verify(mLayoutManager).addObserver(mLayoutStateObserverCaptor.capture());
    }

    @Test
    public void testIncognitoTabInBrowsingMode() {
        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        doReturn(LayoutType.NONE).when(mLayoutManager).getNextLayoutType();
        mActivityTabProvider.set(mIncognitoTab);

        createProvider();

        assertTrue(
                "Overlay should be used for incognito tab in browsing mode.",
                mProvider.getIsUsingOverlayForTesting());
    }

    @Test
    public void testRegularTabInBrowsingMode() {
        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        doReturn(LayoutType.NONE).when(mLayoutManager).getNextLayoutType();
        mActivityTabProvider.set(mRegularTab);

        createProvider();

        assertFalse(
                "Overlay should not be used for regular tab.",
                mProvider.getIsUsingOverlayForTesting());
    }

    @Test
    public void testLayoutTransitionDisablesOverlay() {
        createProvider();
        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        doReturn(LayoutType.TAB_SWITCHER).when(mLayoutManager).getNextLayoutType(); // Transitioning
        mActivityTabProvider.set(mIncognitoTab);

        assertFalse(
                "Overlay should be disabled during layout transition.",
                mProvider.getIsUsingOverlayForTesting());
    }

    @Test
    public void testNonBrowsingLayoutDisablesOverlay() {

        createProvider();
        doReturn(LayoutType.TAB_SWITCHER).when(mLayoutManager).getActiveLayoutType();
        mActivityTabProvider.set(mIncognitoTab);

        assertFalse(
                "Overlay should be disabled for non-browsing layouts.",
                mProvider.getIsUsingOverlayForTesting());
    }

    @Test
    public void testTabChangeUpdatesOverlay() {
        createProvider();

        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        doReturn(LayoutType.NONE).when(mLayoutManager).getNextLayoutType();
        mActivityTabProvider.set(mRegularTab);
        assertFalse(
                "Overlay should not be used for regular tab.",
                mProvider.getIsUsingOverlayForTesting());

        mActivityTabProvider.set(mIncognitoTab);
        assertTrue(
                "Overlay should be used for incognito tab after tab change.",
                mProvider.getIsUsingOverlayForTesting());
    }

    @Test
    public void testFinishedShowingLayoutUpdatesOverlay() {
        createProvider();
        doReturn(LayoutType.TAB_SWITCHER).when(mLayoutManager).getActiveLayoutType();
        mActivityTabProvider.set(mIncognitoTab);
        assertFalse(
                "Overlay should be disabled for non-browsing layouts.",
                mProvider.getIsUsingOverlayForTesting());

        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        doReturn(LayoutType.BROWSING).when(mLayoutManager).getNextLayoutType();
        mLayoutStateObserverCaptor.getValue().onFinishedShowing(LayoutType.BROWSING);

        assertTrue(
                "Overlay should be enabled when browsing layout is shown.",
                mProvider.getIsUsingOverlayForTesting());
    }
}
