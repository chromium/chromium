// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.overlay_panel.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.overlay_panel.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.overlay_panel.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.scene_layer.ContextualSearchSceneLayer;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager.ContextualSearchTabPromotionDelegate;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

/** Unit tests for {@link ContextualSearchManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextualSearchManagerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ContextualSearchTabPromotionDelegate mTabPromotionDelegate;
    @Mock private ScrimManager mScrimManager;
    @Mock private Tab mTab;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ContextualSearchNetworkCommunicator mNetworkCommunicator;
    @Mock private ContextualSearchInternalStateController mInternalStateController;
    @Mock private LayoutManagerImpl mLayoutManager;
    @Mock private OverlayPanelManager mPanelManager;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private ContextualSearchSceneLayer mSceneLayer;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Captor private ArgumentCaptor<EdgeToEdgePadAdjuster> mAdjusterCaptor;

    private final SettableMonotonicObservableSupplier<EdgeToEdgeController>
            mEdgeToEdgeControllerSupplier = ObservableSuppliers.createMonotonic();
    private Activity mActivity;
    private ContextualSearchManager mManager;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mManager =
                new ContextualSearchManager(
                        mActivity,
                        mProfile,
                        mTabPromotionDelegate,
                        mScrimManager,
                        () -> mTab,
                        mFullscreenManager,
                        mBrowserControlsStateProvider,
                        mWindowAndroid,
                        mTabModelSelector,
                        mEdgeToEdgeControllerSupplier);
        mManager.setNetworkCommunicator(mNetworkCommunicator);
        mManager.setContextualSearchInternalStateController(mInternalStateController);
    }

    @Test
    @Feature({"ContextualSearch"})
    public void testResolveSearchTermWithNullSelectionDoesNotAssert() {
        mManager.getSelectionController().setSelectedText(null);
        assertNull(mManager.getSelectionController().getSelectedText());

        ContextualSearchInternalStateHandler handler =
                mManager.getContextualSearchInternalStateHandler();
        handler.resolveSearchTerm();

        verify(mNetworkCommunicator, never())
                .startSearchTermResolutionRequest(anyString(), anyBoolean(), any());
        verify(mInternalStateController).reset(StateChangeReason.UNKNOWN);
    }

    @Test
    @Feature({"ContextualSearch"})
    public void testResolveSearchTermWithEmptySelectionDoesNotAssert() {
        mManager.getSelectionController().setSelectedText("");
        assertEquals("", mManager.getSelectionController().getSelectedText());

        ContextualSearchInternalStateHandler handler =
                mManager.getContextualSearchInternalStateHandler();
        handler.resolveSearchTerm();

        verify(mNetworkCommunicator, never())
                .startSearchTermResolutionRequest(anyString(), anyBoolean(), any());
        verify(mInternalStateController).reset(StateChangeReason.UNKNOWN);
    }

    @Test
    @Feature({"ContextualSearch"})
    public void testEdgeToEdgeTopInsetForwardedToSearchPanel() {
        ContextualSearchPanel panel =
                new ContextualSearchPanel(
                        mActivity,
                        mLayoutManager,
                        mPanelManager,
                        mBrowserControlsStateProvider,
                        mWindowAndroid,
                        mProfile,
                        /* compositorViewHolder= */ null,
                        /* toolbarHeightDp= */ 56.f,
                        mToolbarManager,
                        /* canPromoteToNewTab= */ true,
                        () -> mTab,
                        mEdgeToEdgeControllerSupplier,
                        /* desktopWindowStateManager= */ null,
                        mBottomControlsStacker) {
                    @Override
                    protected ContextualSearchSceneLayer createNewContextualSearchSceneLayer() {
                        return mSceneLayer;
                    }
                };
        panel.onLayoutChanged(400.f, 1000.f, 0.f);
        mEdgeToEdgeControllerSupplier.set(mEdgeToEdgeController);

        verify(mEdgeToEdgeController).registerAdjuster(mAdjusterCaptor.capture());

        int topInsetPx = 64;
        float topInsetDp = topInsetPx / mActivity.getResources().getDisplayMetrics().density;
        mAdjusterCaptor.getValue().overrideTopInset(topInsetPx);
        assertEquals(1000.f - topInsetDp, panel.getTabHeightWithoutTopOffset(), MathUtils.EPSILON);

        panel.destroy();
        verify(mEdgeToEdgeController).unregisterAdjuster(mAdjusterCaptor.getValue());
    }
}
