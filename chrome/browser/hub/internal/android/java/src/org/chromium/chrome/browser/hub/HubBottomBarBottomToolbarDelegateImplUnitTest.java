// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.bottombar.BottomBarView;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
public class HubBottomBarBottomToolbarDelegateImplUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PaneManager mPaneManager;
    @Mock private HubColorMixer mHubColorMixer;
    @Mock private BottomBarView mBottomBarView;

    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private ViewGroup mContainer;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        mActivity = mActivityController.get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContainer = new FrameLayout(mActivity);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    public void testIsBottomToolbarEnabled() {
        HubBottomBarBottomToolbarDelegateImpl delegate =
                new HubBottomBarBottomToolbarDelegateImpl();
        assertTrue(delegate.isBottomToolbarEnabled());
        delegate.destroy();
    }

    @Test
    public void testGetBottomToolbarVisibilitySupplier() {
        HubBottomBarBottomToolbarDelegateImpl delegate =
                new HubBottomBarBottomToolbarDelegateImpl();
        assertTrue(delegate.getBottomToolbarVisibilitySupplier().get());
        delegate.destroy();
    }

    @Test
    public void testInitializeBottomToolbarView() {
        HubBottomBarBottomToolbarDelegateImpl delegate =
                new HubBottomBarBottomToolbarDelegateImpl();
        HubBottomToolbarView view =
                delegate.initializeBottomToolbarView(
                        mActivity, mContainer, mPaneManager, mHubColorMixer);

        assertNotNull(view);
        assertEquals(1, mContainer.getChildCount());
        assertEquals(view, mContainer.getChildAt(0));

        delegate.destroy();
    }

    @Test
    public void testAttachBottomBarView() {
        HubBottomBarBottomToolbarDelegateImpl delegate =
                new HubBottomBarBottomToolbarDelegateImpl();
        HubBottomToolbarView parentView =
                delegate.initializeBottomToolbarView(
                        mActivity, mContainer, mPaneManager, mHubColorMixer);

        View childView = new View(mActivity);
        delegate.attachBottomBarView(childView);

        assertEquals(1, parentView.getChildCount());
        assertEquals(childView, parentView.getChildAt(0));

        delegate.destroy();
    }

    @Test
    public void testAttachBottomBarView_bottomBarView_createsAndDestroysAdapter() {
        HubBottomBarBottomToolbarDelegateImpl delegate =
                new HubBottomBarBottomToolbarDelegateImpl();
        HubBottomToolbarView parentView =
                delegate.initializeBottomToolbarView(
                        mActivity, mContainer, mPaneManager, mHubColorMixer);

        when(mBottomBarView.getContext()).thenReturn(mActivity);
        delegate.attachBottomBarView(mBottomBarView);

        assertEquals(1, parentView.getChildCount());
        assertEquals(mBottomBarView, parentView.getChildAt(0));
        assertNotNull(delegate.getBottomBarColorMixerAdapterForTesting());

        delegate.destroy();
        assertNull(delegate.getBottomBarColorMixerAdapterForTesting());
    }

    @Test
    public void testAttachBottomBarView_WithSuppliers_CreatesAdapter() {
        SettableNonNullObservableSupplier<Boolean> isHidingSupplier =
                ObservableSuppliers.createNonNull(false);
        SettableNullableObservableSupplier<Tab> currentTabSupplier =
                ObservableSuppliers.createNullable();
        HubBottomBarBottomToolbarDelegateImpl delegate =
                new HubBottomBarBottomToolbarDelegateImpl(currentTabSupplier, isHidingSupplier);
        delegate.initializeBottomToolbarView(mActivity, mContainer, mPaneManager, mHubColorMixer);

        when(mBottomBarView.getContext()).thenReturn(mActivity);
        delegate.attachBottomBarView(mBottomBarView);

        assertNotNull(delegate.getBottomBarColorMixerAdapterForTesting());

        delegate.destroy();
        assertNull(delegate.getBottomBarColorMixerAdapterForTesting());
    }
}
