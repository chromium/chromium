// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.toolbar;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for the top toolbar overlay's mediator (composited version of the top toolbar). */
@RunWith(BaseRobolectricTestRunner.class)
public class TopToolbarOverlayMediatorTest {
    private TopToolbarOverlayMediator mMediator;
    private PropertyModel mModel;

    @Mock
    private Context mContext;

    @Mock
    private LayoutManager mLayoutManager;

    @Mock
    private ControlContainer mControlContainer;

    @Mock
    private BrowserControlsStateProvider mBrowserControlsProvider;

    @Mock
    private TabImpl mTab;

    @Mock
    private TabImpl mTab2;

    @Captor
    private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer> mBrowserControlsObserverCaptor;

    @Mock
    private ActivityTabProvider mTabSupplier;

    @Captor
    private ArgumentCaptor<ActivityTabProvider.ActivityTabObserver> mActivityTabObserverCaptor;

    private ObservableSupplierImpl<Boolean> mAndroidViewShownSupplier;

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);

        mAndroidViewShownSupplier = new ObservableSupplierImpl<>();
        mAndroidViewShownSupplier.set(true);

        TopToolbarOverlayMediator.setToolbarBackgroundColorForTesting(Color.RED);
        TopToolbarOverlayMediator.setUrlBarColorForTesting(Color.BLUE);
        TopToolbarOverlayMediator.setIsTabletForTesting(false);

        mModel = new PropertyModel.Builder(TopToolbarOverlayProperties.ALL_KEYS)
                         .with(TopToolbarOverlayProperties.RESOURCE_ID, 0)
                         .with(TopToolbarOverlayProperties.URL_BAR_RESOURCE_ID, 0)
                         .with(TopToolbarOverlayProperties.CONTENT_OFFSET, 0)
                         .with(TopToolbarOverlayProperties.SHOW_SHADOW, true)
                         .with(TopToolbarOverlayProperties.TOOLBAR_BACKGROUND_COLOR,
                                 Color.TRANSPARENT)
                         .with(TopToolbarOverlayProperties.URL_BAR_COLOR, Color.TRANSPARENT)
                         .with(TopToolbarOverlayProperties.PROGRESS_BAR_INFO, null)
                         .build();

        mMediator =
                new TopToolbarOverlayMediator(mModel, mContext, mLayoutManager, mControlContainer,
                        mTabSupplier, mBrowserControlsProvider, mAndroidViewShownSupplier);

        // Ensure the observer is added to the initial tab.
        verify(mTabSupplier).addObserverAndTrigger(mActivityTabObserverCaptor.capture());
        setTabSupplierTab(mTab);

        verify(mTab).addObserver(mTabObserverCaptor.capture());

        verify(mBrowserControlsProvider).addObserver(mBrowserControlsObserverCaptor.capture());
    }

    /** Set the tab that will be returned by the supplier and trigger the observer event. */
    private void setTabSupplierTab(Tab tab) {
        when(mTabSupplier.get()).thenReturn(tab);
        mActivityTabObserverCaptor.getValue().onActivityTabChanged(tab, false);
    }

    @After
    public void afterTest() {
        // Unset any testing state the tests may have set.
        TopToolbarOverlayMediator.setIsTabletForTesting(null);
    }

    @Test
    public void testTabObserverAfterTabSwitch() {
        setTabSupplierTab(mTab2);

        // Make sure the tab observer for this overlay is only observing the "current" tab.
        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
        verify(mTab2).addObserver(mTabObserverCaptor.getValue());
    }

    @Test
    public void testShadowVisibility_browserControlsOffsets() {
        when(mBrowserControlsProvider.getBrowserControlHiddenRatio()).thenReturn(0.0f);
        mBrowserControlsObserverCaptor.getValue().onControlsOffsetChanged(0, 0, 0, 0, false);

        Assert.assertFalse(
                "Shadow should be invisible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));

        when(mBrowserControlsProvider.getBrowserControlHiddenRatio()).thenReturn(0.5f);
        mBrowserControlsObserverCaptor.getValue().onControlsOffsetChanged(100, 0, 0, 0, false);

        Assert.assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
    }

    @Test
    public void testShadowVisibility_androidViewForceHidden() {
        mAndroidViewShownSupplier.set(true);

        Assert.assertFalse(
                "Shadow should be invisible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));

        mAndroidViewShownSupplier.set(false);

        Assert.assertTrue(
                "Shadow should be visible.", mModel.get(TopToolbarOverlayProperties.SHOW_SHADOW));
    }

    @Test
    public void testProgressUpdate_phone() {
        mModel.set(TopToolbarOverlayProperties.PROGRESS_BAR_INFO, null);

        mTabObserverCaptor.getValue().onLoadProgressChanged(mTab, 0.25f);

        Assert.assertNotNull("The progress bar data should be populated.",
                mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));

        // Ensure the progress is correct on tab switch.
        mTabObserverCaptor.getValue().onLoadProgressChanged(mTab, 0.f);
        setTabSupplierTab(mTab2);
    }

    @Test
    public void testProgressUpdate_tablet() {
        TopToolbarOverlayMediator.setIsTabletForTesting(true);
        mModel.set(TopToolbarOverlayProperties.PROGRESS_BAR_INFO, null);

        mTabObserverCaptor.getValue().onLoadProgressChanged(mTab, 0.25f);

        Assert.assertNull("The progress bar data should be still be empty.",
                mModel.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO));
    }
}
