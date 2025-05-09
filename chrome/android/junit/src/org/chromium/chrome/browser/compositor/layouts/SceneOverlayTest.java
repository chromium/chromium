// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.RectF;
import android.util.DisplayMetrics;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.layouts.Layout.Orientation;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.ScrollingBottomViewSceneLayer;
import org.chromium.chrome.browser.toolbar.top.TopToolbarOverlayCoordinator;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests for {@link SceneOverlay} interactions. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SceneOverlayTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Mock private Context mContext;

    @Mock private Resources mResources;

    @Mock private LayoutManagerHost mLayoutManagerHost;

    @Mock private ViewGroup mContainerView;

    @Mock private ObservableSupplier<TabContentManager> mTabContentManagerSupplier;

    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;

    // Use different classes so the overlays can be uniquely ordered. Each supported class has an
    // assigned slot; multiple SceneOverlays of the same class are ordered within that slot by time
    // of addition.
    @Mock private SceneOverlay mOverlay1;
    @Mock private TopToolbarOverlayCoordinator mOverlay2;
    @Mock private ScrollingBottomViewSceneLayer mOverlay3;
    @Mock private ContextualSearchPanel mOverlay4;
    @Mock private SceneOverlay mOverlay5;
    @Mock private SceneOverlay mOverlay6;
    @Mock private Layout mLayout;

    private final DisplayMetrics mDisplayMetrics = new DisplayMetrics();
    private LayoutManagerImpl mLayoutManager;

    @Before
    public void setup() {
        mDisplayMetrics.density = 1.0f;
        when(mLayoutManagerHost.getContext()).thenReturn(mContext);
        when(mContext.getResources()).thenReturn(mResources);
        when(mResources.getDisplayMetrics()).thenReturn(mDisplayMetrics);

        doReturn(new ObservableSupplierImpl<>())
                .when(mOverlay1)
                .getHandleBackPressChangedSupplier();
        doReturn(new ObservableSupplierImpl<>())
                .when(mOverlay2)
                .getHandleBackPressChangedSupplier();
        doReturn(new ObservableSupplierImpl<>())
                .when(mOverlay3)
                .getHandleBackPressChangedSupplier();
        doReturn(new ObservableSupplierImpl<>())
                .when(mOverlay4)
                .getHandleBackPressChangedSupplier();
        doReturn(new ObservableSupplierImpl<>())
                .when(mOverlay5)
                .getHandleBackPressChangedSupplier();
        doReturn(new ObservableSupplierImpl<>())
                .when(mOverlay6)
                .getHandleBackPressChangedSupplier();

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

        HashMap<Class, Integer> orderMap = new HashMap<>();
        orderMap.put(mOverlay1.getClass(), 0);
        orderMap.put(mOverlay2.getClass(), 1);
        orderMap.put(mOverlay3.getClass(), 2);
        orderMap.put(mOverlay4.getClass(), 3);
        mLayoutManager.setSceneOverlayOrderForTesting(orderMap);

        // Mix up the addition of each overlay.
        mLayoutManager.addSceneOverlay(mOverlay3);
        mLayoutManager.addSceneOverlay(mOverlay1);
        mLayoutManager.addSceneOverlay(mOverlay4);
        mLayoutManager.addSceneOverlay(mOverlay2);

        assertEquals("Overlay is out of order!", mOverlay1, overlays.get(0));
        assertEquals("Overlay is out of order!", mOverlay2, overlays.get(1));
        assertEquals("Overlay is out of order!", mOverlay3, overlays.get(2));
        assertEquals("Overlay is out of order!", mOverlay4, overlays.get(3));

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

        HashMap<Class, Integer> orderMap = new HashMap<>();
        orderMap.put(mOverlay1.getClass(), 0);
        mLayoutManager.setSceneOverlayOrderForTesting(orderMap);

        // Mix up the addition of each overlay.
        mLayoutManager.addSceneOverlay(mOverlay5);
        mLayoutManager.addSceneOverlay(mOverlay1);
        mLayoutManager.addSceneOverlay(mOverlay6);

        assertEquals("Overlay is out of order!", mOverlay5, overlays.get(0));
        assertEquals("Overlay is out of order!", mOverlay1, overlays.get(1));
        assertEquals("Overlay is out of order!", mOverlay6, overlays.get(2));

        assertEquals("The overlay list should have 3 items.", 3, overlays.size());
    }

    @Test
    public void testSizeSetOnAdd() {
        mLayoutManager.setSceneOverlayOrderForTesting(
                Map.of(mOverlay1.getClass(), 0, mOverlay2.getClass(), 1));

        RectF windowViewport = new RectF(0, 0, 400, 800);
        RectF visibleViewport = new RectF(0, 100, 400, 700);
        Mockito.doAnswer(
                        (Answer<Void>)
                                (invocationOnMock) -> {
                                    invocationOnMock
                                            .getArgument(0, RectF.class)
                                            .set(windowViewport);
                                    return null;
                                })
                .when(mLayoutManagerHost)
                .getWindowViewport(any(RectF.class));
        Mockito.doAnswer(
                        (Answer<Void>)
                                (invocationOnMock) -> {
                                    invocationOnMock
                                            .getArgument(0, RectF.class)
                                            .set(visibleViewport);
                                    return null;
                                })
                .when(mLayoutManagerHost)
                .getVisibleViewport(any(RectF.class));

        mLayoutManager.startShowing(mLayout, false);

        mLayoutManager.addSceneOverlay(mOverlay1);
        mLayoutManager.addSceneOverlay(mOverlay2);

        verify(mOverlay1).onSizeChanged(400, 800, 100, Orientation.PORTRAIT);
        verify(mOverlay2).onSizeChanged(400, 800, 100, Orientation.PORTRAIT);
    }
}
