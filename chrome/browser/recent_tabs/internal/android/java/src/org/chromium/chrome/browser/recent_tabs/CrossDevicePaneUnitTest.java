// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.hub.HubLayoutAnimationType;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;

import java.util.function.DoubleConsumer;

/** Unit tests for {@link CrossDevicePane}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CrossDevicePaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private HubContainerView mHubContainerView;
    @Mock private DoubleConsumer mOnToolbarAlphaChange;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    private CrossDevicePane mCrossDevicePane;
    private final ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier =
            new ObservableSupplierImpl<>();

    @Before
    public void setUp() {
        ApplicationProvider.getApplicationContext().setTheme(R.style.Theme_BrowserUI_DayNight);

        mCrossDevicePane =
                new CrossDevicePaneImpl(
                        ApplicationProvider.getApplicationContext(),
                        mOnToolbarAlphaChange,
                        mEdgeToEdgeSupplier);
    }

    @Test
    public void testPaneId() {
        assertEquals(PaneId.CROSS_DEVICE, mCrossDevicePane.getPaneId());
    }

    @Test
    public void testGetRootView() {
        assertNotNull(mCrossDevicePane.getRootView());
    }

    @Test
    public void testDestroy_NoLoadHint() {
        mCrossDevicePane.destroy();
        assertEquals(0, mCrossDevicePane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileHot() {
        mCrossDevicePane.notifyLoadHint(LoadHint.HOT);
        mCrossDevicePane.destroy();
        assertEquals(0, mCrossDevicePane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileCold() {
        mCrossDevicePane.notifyLoadHint(LoadHint.HOT);
        mCrossDevicePane.notifyLoadHint(LoadHint.COLD);
        mCrossDevicePane.destroy();
        assertEquals(0, mCrossDevicePane.getRootView().getChildCount());
    }

    @Test
    public void testNotifyLoadHint() {
        assertEquals(0, mCrossDevicePane.getRootView().getChildCount());

        mCrossDevicePane.notifyLoadHint(LoadHint.HOT);
        assertNotEquals(0, mCrossDevicePane.getRootView().getChildCount());

        mCrossDevicePane.notifyLoadHint(LoadHint.COLD);
        assertEquals(0, mCrossDevicePane.getRootView().getChildCount());
    }

    @Test
    public void testCreateFadeOutAnimatorNoTab() {
        assertEquals(
                HubLayoutAnimationType.FADE_OUT,
                mCrossDevicePane
                        .createHideHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
    public void testCreateFadeInAnimatorNoTab() {
        assertEquals(
                HubLayoutAnimationType.FADE_IN,
                mCrossDevicePane
                        .createShowHubLayoutAnimatorProvider(mHubContainerView)
                        .getPlannedAnimationType());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE
    })
    public void testSetEdgeToEdgeSupplier_BeforeNotifyLoadHint() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        assertFalse(mEdgeToEdgeSupplier.hasObservers());

        mCrossDevicePane.notifyLoadHint(LoadHint.HOT);
        assertTrue(mEdgeToEdgeSupplier.hasObservers());
        ShadowLooper.idleMainLooper();
        verify(mEdgeToEdgeController).registerAdjuster(notNull());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN,
        ChromeFeatureList.DRAW_KEY_NATIVE_EDGE_TO_EDGE
    })
    public void testSetEdgeToEdgeSupplier_AfterNotifyLoadHint() {
        mCrossDevicePane.notifyLoadHint(LoadHint.HOT);
        assertTrue(mEdgeToEdgeSupplier.hasObservers());

        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(notNull());
    }
}
