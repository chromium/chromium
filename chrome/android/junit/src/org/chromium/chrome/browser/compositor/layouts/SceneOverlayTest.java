// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.ScrollingBottomViewSceneLayer;
import org.chromium.chrome.browser.toolbar.top.TopToolbarOverlayCoordinator;

import java.util.HashMap;
import java.util.List;

/** Tests for {@link SceneOverlay} interactions. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SceneOverlayTest {
    @Mock private Context mContext;

    @Mock private Resources mResources;

    @Mock private DisplayMetrics mDisplayMetrics;

    @Mock private LayoutManagerHost mLayoutManagerHost;

    @Mock private ViewGroup mContainerView;

    @Mock private ObservableSupplier<TabContentManager> mTabContentManagerSupplier;

    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;

    private LayoutManagerImpl mLayoutManager;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        when(mLayoutManagerHost.getContext()).thenReturn(mContext);
        when(mContext.getResources()).thenReturn(mResources);
        when(mResources.getDisplayMetrics()).thenReturn(mDisplayMetrics);

        mLayoutManager =
                new LayoutManagerImpl(
                        mLayoutManagerHost,
                        mContainerView,
                        mTabContentManagerSupplier,
                        () -> mTopUiThemeColorProvider);
    }

    @Test
    public void testSceneOverlayPositions() {
        List<SceneOverlay> overlays = mLayoutManager.getSceneOverlaysForTesting();
        assertEquals("The overlay list should be empty.", 0, overlays.size());

        // Use different class names so the overlays can be uniquely identified.
        SceneOverlay overlay1 = Mockito.mock(SceneOverlay.class);
        when(overlay1.getHandleBackPressChangedSupplier())
                .thenReturn(new ObservableSupplierImpl<>());
        SceneOverlay overlay2 = Mockito.mock(TopToolbarOverlayCoordinator.class);
        when(overlay2.getHandleBackPressChangedSupplier())
                .thenReturn(new ObservableSupplierImpl<>());
        SceneOverlay overlay3 = Mockito.mock(ScrollingBottomViewSceneLayer.class);
        when(overlay3.getHandleBackPressChangedSupplier())
                .thenReturn(new ObservableSupplierImpl<>());
        SceneOverlay overlay4 = Mockito.mock(ContextualSearchPanel.class);
        when(overlay4.getHandleBackPressChangedSupplier())
                .thenReturn(new ObservableSupplierImpl<>());

        HashMap<Class, Integer> orderMap = new HashMap<>();
        orderMap.put(overlay1.getClass(), 0);
        orderMap.put(overlay2.getClass(), 1);
        orderMap.put(overlay3.getClass(), 2);
        orderMap.put(overlay4.getClass(), 3);
        mLayoutManager.setSceneOverlayOrderForTesting(orderMap);

        // Mix up the addition of each overlay.
        mLayoutManager.addSceneOverlay(overlay3);
        mLayoutManager.addSceneOverlay(overlay1);
        mLayoutManager.addSceneOverlay(overlay4);
        mLayoutManager.addSceneOverlay(overlay2);

        assertEquals("Overlay is out of order!", overlay1, overlays.get(0));
        assertEquals("Overlay is out of order!", overlay2, overlays.get(1));
        assertEquals("Overlay is out of order!", overlay3, overlays.get(2));
        assertEquals("Overlay is out of order!", overlay4, overlays.get(3));

        assertEquals("The overlay list should have 4 items.", 4, overlays.size());
    }

    /**
     * Ensure the ordering in LayoutManager is order-of-addition for multiple instances of the same
     * SceneOverlay.
     */
    @Test
    public void testSceneOverlayPositions_multipleInstances() {
        List<SceneOverlay> overlays = mLayoutManager.getSceneOverlaysForTesting();
        assertEquals("The overlay list should be empty.", 0, overlays.size());

        SceneOverlay overlay1 = Mockito.mock(SceneOverlay.class);
        when(overlay1.getHandleBackPressChangedSupplier())
                .thenReturn(new ObservableSupplierImpl<>());
        SceneOverlay overlay2 = Mockito.mock(SceneOverlay.class);
        when(overlay2.getHandleBackPressChangedSupplier())
                .thenReturn(new ObservableSupplierImpl<>());
        SceneOverlay overlay3 = Mockito.mock(SceneOverlay.class);
        when(overlay3.getHandleBackPressChangedSupplier())
                .thenReturn(new ObservableSupplierImpl<>());

        HashMap<Class, Integer> orderMap = new HashMap<>();
        orderMap.put(overlay1.getClass(), 0);
        mLayoutManager.setSceneOverlayOrderForTesting(orderMap);

        // Mix up the addition of each overlay.
        mLayoutManager.addSceneOverlay(overlay3);
        mLayoutManager.addSceneOverlay(overlay1);
        mLayoutManager.addSceneOverlay(overlay2);

        assertEquals("Overlay is out of order!", overlay3, overlays.get(0));
        assertEquals("Overlay is out of order!", overlay1, overlays.get(1));
        assertEquals("Overlay is out of order!", overlay2, overlays.get(2));

        assertEquals("The overlay list should have 3 items.", 3, overlays.size());
    }
}
