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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.overlay_panel.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager.ContextualSearchTabPromotionDelegate;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.base.WindowAndroid;

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

    private final SettableMonotonicObservableSupplier<EdgeToEdgeController>
            mEdgeToEdgeControllerSupplier = ObservableSuppliers.createMonotonic();
    private Activity mActivity;
    private ContextualSearchManager mManager;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
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
}
