// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelStateProvider;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

@RunWith(BaseRobolectricTestRunner.class)
public class BottomAttachedUiObserverTest {
    private static final int BOTTOM_CONTROLS_HEIGHT = 100;
    private static final int BROWSER_CONTROLS_COLOR = Color.RED;
    private static final int SNACKBAR_COLOR = Color.GREEN;
    private static final int OVERLAY_PANEL_COLOR = Color.BLUE;

    private BottomAttachedUiObserver mBottomAttachedUiObserver;
    private MockColorChangeObserver mColorChangeObserver;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private SnackbarManager mSnackbarManager;

    private ObservableSupplierImpl<ContextualSearchManager> mContextualSearchManagerSupplier =
            new ObservableSupplierImpl<>();
    @Mock private ContextualSearchManager mContextualSearchManager;
    private ObservableSupplierImpl<OverlayPanelStateProvider> mOverlayPanelStateProviderSupplier =
            new ObservableSupplierImpl<>();
    @Mock private OverlayPanelStateProvider mOverlayPanelStateProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mBottomAttachedUiObserver =
                new BottomAttachedUiObserver(
                        mBrowserControlsStateProvider,
                        mSnackbarManager,
                        mContextualSearchManagerSupplier);

        when(mContextualSearchManager.getOverlayPanelStateProviderSupplier())
                .thenReturn(mOverlayPanelStateProviderSupplier);

        mColorChangeObserver = new MockColorChangeObserver();
        mBottomAttachedUiObserver.addObserver(mColorChangeObserver);
    }

    @Test
    public void testAdaptsColorToBrowserControls() {
        mColorChangeObserver.assertColor(null);

        // Show bottom controls.
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(BOTTOM_CONTROLS_HEIGHT, 0);
        mColorChangeObserver.assertColor(BROWSER_CONTROLS_COLOR);

        // Scroll off bottom controls partway.
        mBottomAttachedUiObserver.onControlsOffsetChanged(
                0, 0, BOTTOM_CONTROLS_HEIGHT / 2, 0, false);
        mColorChangeObserver.assertColor(BROWSER_CONTROLS_COLOR);

        // Scroll off bottom controls fully.
        mBottomAttachedUiObserver.onControlsOffsetChanged(0, 0, BOTTOM_CONTROLS_HEIGHT, 0, false);
        mColorChangeObserver.assertColor(null);

        // Scroll bottom controls back.
        mBottomAttachedUiObserver.onControlsOffsetChanged(0, 0, 0, 0, false);
        mColorChangeObserver.assertColor(BROWSER_CONTROLS_COLOR);

        // Hide bottom controls.
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(0, 0);
        mColorChangeObserver.assertColor(null);
    }

    @Test
    public void testAdaptsColorToSnackbars() {
        mColorChangeObserver.assertColor(null);

        // Set only the snackbar color.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, SNACKBAR_COLOR);
        mColorChangeObserver.assertColor(null);

        // Show the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ true, SNACKBAR_COLOR);
        mColorChangeObserver.assertColor(SNACKBAR_COLOR);

        // Hide the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, /* color= */ null);
        mColorChangeObserver.assertColor(null);
    }

    @Test
    public void testSetOverlayPanelObserver() {
        setOverlayPanelObserver();
        verify(mOverlayPanelStateProvider).addObserver(eq(mBottomAttachedUiObserver));

        mOverlayPanelStateProviderSupplier.set(null);
        verify(mOverlayPanelStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
    }

    private void setOverlayPanelObserver() {
        mContextualSearchManagerSupplier.set(mContextualSearchManager);
        mOverlayPanelStateProviderSupplier.set(mOverlayPanelStateProvider);
    }

    @Test
    public void testAdaptsColorToOverlayPanel() {
        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertColor(null);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertColor(OVERLAY_PANEL_COLOR);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.EXPANDED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertColor(null);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.MAXIMIZED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertColor(null);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertColor(OVERLAY_PANEL_COLOR);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertColor(null);
    }

    @Test
    public void testColorPrioritization() {
        mColorChangeObserver.assertColor(null);

        // Show the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ true, SNACKBAR_COLOR);
        mColorChangeObserver.assertColor(SNACKBAR_COLOR);

        // Show bottom controls.
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(BOTTOM_CONTROLS_HEIGHT, 0);
        mColorChangeObserver.assertColor(BROWSER_CONTROLS_COLOR);

        // Hide bottom controls - should fall back to the snackbar color.
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(0, 0);
        mColorChangeObserver.assertColor(SNACKBAR_COLOR);
    }

    @Test
    public void testDestroy() {
        setOverlayPanelObserver();

        mBottomAttachedUiObserver.destroy();
        verify(mBrowserControlsStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
        verify(mSnackbarManager).removeObserver(eq(mBottomAttachedUiObserver));
        verify(mOverlayPanelStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
    }

    private static class MockColorChangeObserver implements BottomAttachedUiObserver.Observer {
        private @Nullable @ColorInt Integer mColor;

        @Override
        public void onBottomAttachedColorChanged(@Nullable Integer color) {
            mColor = color;
        }

        public void assertColor(@Nullable @ColorInt Integer expectedColor) {
            assertEquals("Incorrect bottom attached color.", expectedColor, mColor);
        }
    }
}
