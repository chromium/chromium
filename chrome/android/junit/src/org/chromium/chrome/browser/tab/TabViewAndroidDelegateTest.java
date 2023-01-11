// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Build;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for the TabViewAndroidDelegate. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
@Features.EnableFeatures(ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU)
@Features.DisableFeatures(ChromeFeatureList.ANIMATED_IMAGE_DRAG_SHADOW)
public class TabViewAndroidDelegateTest {
    private final ArgumentCaptor<TabObserver> mTabObserverCaptor =
            ArgumentCaptor.forClass(TabObserver.class);

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Mock
    private TabImpl mTab;

    @Mock
    private WebContents mWebContents;

    @Mock
    private WindowAndroid mWindowAndroid;

    @Mock
    private ContentView mContentView;

    private ApplicationViewportInsetSupplier mApplicationInsetSupplier;
    private ObservableSupplierImpl<Integer> mFeatureInsetSupplier;
    private TabViewAndroidDelegate mViewAndroidDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mFeatureInsetSupplier = new ObservableSupplierImpl<>();

        mApplicationInsetSupplier = ApplicationViewportInsetSupplier.createForTests();
        mApplicationInsetSupplier.addOverlappingSupplier(mFeatureInsetSupplier);

        when(mWindowAndroid.getApplicationBottomInsetProvider())
                .thenReturn(mApplicationInsetSupplier);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab.getWebContents()).thenReturn(mWebContents);

        mViewAndroidDelegate = new TabViewAndroidDelegate(mTab, mContentView);
        verify(mTab).addObserver(mTabObserverCaptor.capture());
    }

    @Test
    public void testInset() {
        mFeatureInsetSupplier.set(10);
        assertEquals("The bottom inset for the tab should be non-zero.", 10,
                mViewAndroidDelegate.getViewportInsetBottom());
    }

    @Test
    public void testInset_afterHidden() {
        mFeatureInsetSupplier.set(10);

        when(mTab.isHidden()).thenReturn(true);

        mTabObserverCaptor.getValue().onHidden(mTab, 0);
        assertEquals("The bottom inset for the tab should be zero.", 0,
                mViewAndroidDelegate.getViewportInsetBottom());
    }

    @Test
    public void testInset_afterDetachAndAttach() {
        mFeatureInsetSupplier.set(10);

        assertEquals("The bottom inset for the tab should be non-zero.", 10,
                mViewAndroidDelegate.getViewportInsetBottom());

        mTabObserverCaptor.getValue().onActivityAttachmentChanged(mTab, null);

        assertEquals("The bottom inset for the tab should be zero.", 0,
                mViewAndroidDelegate.getViewportInsetBottom());

        WindowAndroid window = Mockito.mock(WindowAndroid.class);
        mTabObserverCaptor.getValue().onActivityAttachmentChanged(mTab, window);
        assertEquals("The bottom inset for the tab should be non-zero.", 10,
                mViewAndroidDelegate.getViewportInsetBottom());
    }

    @Test
    public void testCreateDragAndDropBrowserDelegate() {
        assertNotNull("DragAndDropBrowserDelegate should not null when feature enabled.",
                mViewAndroidDelegate.getDragAndDropBrowserDelegateForTesting());
        mViewAndroidDelegate.destroy();
        assertNull("DragAndDropBrowserDelegate should be removed once destroyed.",
                mViewAndroidDelegate.getDragAndDropBrowserDelegateForTesting());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.N)
    public void testCreateDragAndDropBrowserDelegate_N() {
        assertNull("DragAndDropBrowserDelegate be null on N.",
                mViewAndroidDelegate.getDragAndDropBrowserDelegateForTesting());
    }
}
