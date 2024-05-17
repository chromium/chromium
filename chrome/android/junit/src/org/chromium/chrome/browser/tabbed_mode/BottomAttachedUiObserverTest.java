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
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

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
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsVisualState;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.InsetObserver;

import java.util.Optional;

@RunWith(BaseRobolectricTestRunner.class)
public class BottomAttachedUiObserverTest {
    private static final int BOTTOM_CONTROLS_HEIGHT = 100;
    private static final int BROWSER_CONTROLS_COLOR = Color.RED;
    private static final int SNACKBAR_COLOR = Color.GREEN;
    private static final int OVERLAY_PANEL_COLOR = Color.BLUE;
    private static final int BOTTOM_SHEET_YELLOW = Color.YELLOW;
    private static final int BOTTOM_SHEET_CYAN = Color.CYAN;
    private static final int OMNIBOX_SUGGESTIONS_COLOR = Color.MAGENTA;
    private static final int OMNIBOX_SUGGESTIONS_COLOR_2 = Color.DKGRAY;

    private static final WindowInsetsCompat BOTTOM_NAV_BAR_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(
                            WindowInsetsCompat.Type.navigationBars(),
                            Insets.of(0, 0, 0, /* bottom= */ 100))
                    .build();
    private static final WindowInsetsCompat SIDE_NAV_BAR_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(
                            WindowInsetsCompat.Type.navigationBars(),
                            Insets.of(0, 0, /* right= */ 100, /* bottom= */ 0))
                    .build();

    private BottomAttachedUiObserver mBottomAttachedUiObserver;
    private MockColorChangeObserver mColorChangeObserver;

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private SnackbarManager mSnackbarManager;

    private final ObservableSupplierImpl<ContextualSearchManager> mContextualSearchManagerSupplier =
            new ObservableSupplierImpl<>();
    @Mock private ContextualSearchManager mContextualSearchManager;

    private final ObservableSupplierImpl<OverlayPanelStateProvider>
            mOverlayPanelStateProviderSupplier = new ObservableSupplierImpl<>();
    @Mock private OverlayPanelStateProvider mOverlayPanelStateProvider;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private BottomSheetContent mBottomSheetContentNullBackground;
    @Mock private BottomSheetContent mBottomSheetContentYellowBackground;
    @Mock private BottomSheetContent mBottomSheetContentCyanBackground;

    @Mock private OmniboxSuggestionsVisualState mOmniboxSuggestionsVisualState;

    @Mock private InsetObserver mInsetObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(BOTTOM_NAV_BAR_INSETS);

        mBottomAttachedUiObserver =
                new BottomAttachedUiObserver(
                        mBrowserControlsStateProvider,
                        mSnackbarManager,
                        mContextualSearchManagerSupplier,
                        mBottomSheetController,
                        Optional.of(mOmniboxSuggestionsVisualState),
                        mInsetObserver);
        mBottomAttachedUiObserver.onInsetChanged(0, 0, 0, 0);

        when(mContextualSearchManager.getOverlayPanelStateProviderSupplier())
                .thenReturn(mOverlayPanelStateProviderSupplier);

        when(mBottomSheetContentNullBackground.getBackgroundColor()).thenReturn(null);
        when(mBottomSheetContentYellowBackground.getBackgroundColor())
                .thenReturn(BOTTOM_SHEET_YELLOW);
        when(mBottomSheetContentCyanBackground.getBackgroundColor()).thenReturn(BOTTOM_SHEET_CYAN);

        when(mBottomSheetController.isFullWidth()).thenReturn(true);
        when(mSnackbarManager.isFullWidth()).thenReturn(true);

        mContextualSearchManagerSupplier.set(mContextualSearchManager);
        mOverlayPanelStateProviderSupplier.set(mOverlayPanelStateProvider);
        when(mOverlayPanelStateProvider.isFullWidthSizePanel()).thenReturn(true);

        mColorChangeObserver = new MockColorChangeObserver();
        mBottomAttachedUiObserver.addObserver(mColorChangeObserver);
    }

    @Test
    public void testAdaptsColorToBrowserControls() {
        mColorChangeObserver.assertState(null, false);

        // Show bottom controls.
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(BOTTOM_CONTROLS_HEIGHT, 0);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false);

        // Scroll off bottom controls partway.
        mBottomAttachedUiObserver.onControlsOffsetChanged(
                0, 0, BOTTOM_CONTROLS_HEIGHT / 2, 0, false);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false);

        // Scroll off bottom controls fully.
        mBottomAttachedUiObserver.onControlsOffsetChanged(0, 0, BOTTOM_CONTROLS_HEIGHT, 0, false);
        mColorChangeObserver.assertState(null, false);

        // Scroll bottom controls back.
        mBottomAttachedUiObserver.onControlsOffsetChanged(0, 0, 0, 0, false);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false);

        // Hide bottom controls.
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(0, 0);
        mColorChangeObserver.assertState(null, false);
    }

    @Test
    public void testAdaptsColorToSnackbars() {
        mColorChangeObserver.assertState(null, false);

        // Set only the snackbar color.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(null, false);

        // Show the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ true, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, false);

        // Hide the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, /* color= */ null);
        mColorChangeObserver.assertState(null, false);
    }

    @Test
    public void testAdaptsColorToSnackbars_doesNotCoverFullWidth() {
        when(mSnackbarManager.isFullWidth()).thenReturn(false);
        mColorChangeObserver.assertState(null, false);

        // Set only the snackbar color.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(null, false);

        // Show the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ true, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, true);

        // Hide the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, /* color= */ null);
        mColorChangeObserver.assertState(null, false);
    }

    @Test
    public void testSetOverlayPanelObserver() {
        verify(mOverlayPanelStateProvider).addObserver(eq(mBottomAttachedUiObserver));

        mOverlayPanelStateProviderSupplier.set(null);
        verify(mOverlayPanelStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
    }

    @Test
    public void testAdaptsColorToOverlayPanel() {
        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.EXPANDED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.MAXIMIZED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false);
    }

    @Test
    public void testAdaptsColorToOverlayPanel_doesNotCoverFullWidth() {
        when(mOverlayPanelStateProvider.isFullWidthSizePanel()).thenReturn(false);
        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, true);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false);
    }

    @Test
    public void testAdaptsColorToBottomSheet() {
        mBottomAttachedUiObserver.onSheetContentChanged(mBottomSheetContentNullBackground);
        mColorChangeObserver.assertState(null, false);

        mBottomAttachedUiObserver.onSheetOpened(0);
        mColorChangeObserver.assertState(null, false);
        mBottomAttachedUiObserver.onSheetClosed(0);
        mColorChangeObserver.assertState(null, false);

        mBottomAttachedUiObserver.onSheetContentChanged(mBottomSheetContentCyanBackground);
        mBottomAttachedUiObserver.onSheetOpened(0);
        mColorChangeObserver.assertState(BOTTOM_SHEET_CYAN, false);
        mBottomAttachedUiObserver.onSheetClosed(0);
        mColorChangeObserver.assertState(null, false);

        mBottomAttachedUiObserver.onSheetContentChanged(mBottomSheetContentYellowBackground);
        mColorChangeObserver.assertState(null, false);

        mBottomAttachedUiObserver.onSheetOpened(0);
        mColorChangeObserver.assertState(BOTTOM_SHEET_YELLOW, false);
        mBottomAttachedUiObserver.onSheetContentChanged(mBottomSheetContentCyanBackground);
        mColorChangeObserver.assertState(BOTTOM_SHEET_CYAN, false);
        mBottomAttachedUiObserver.onSheetClosed(0);
        mColorChangeObserver.assertState(null, false);
    }

    @Test
    public void testAdaptsColorToBottomSheet_doesNotCoverFullWidth() {
        when(mBottomSheetController.isFullWidth()).thenReturn(false);

        mBottomAttachedUiObserver.onSheetContentChanged(mBottomSheetContentYellowBackground);
        mColorChangeObserver.assertState(null, false);

        mBottomAttachedUiObserver.onSheetOpened(0);
        mColorChangeObserver.assertState(BOTTOM_SHEET_YELLOW, true);
        mBottomAttachedUiObserver.onSheetClosed(0);
        mColorChangeObserver.assertState(null, false);
    }

    @Test
    public void testAdaptsToInsetChanges() {
        verify(mInsetObserver).addObserver(eq(mBottomAttachedUiObserver));

        // Navbar is present at the bottom.
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(BOTTOM_NAV_BAR_INSETS);
        mBottomAttachedUiObserver.onInsetChanged(0, 0, 0, 0);
        mColorChangeObserver.assertState(null, false);

        // Show a snackbar to set a color.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ true, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, false);

        // Shift navbar to the side.
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(SIDE_NAV_BAR_INSETS);
        mBottomAttachedUiObserver.onInsetChanged(0, 0, 0, 0);
        mColorChangeObserver.assertState(null, false);

        // Return navbar to the bottom.
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(BOTTOM_NAV_BAR_INSETS);
        mBottomAttachedUiObserver.onInsetChanged(0, 0, 0, 0);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, false);

        // Hide the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(null, false);
    }

    @Test
    public void testAdaptsColorToOmniboxSuggestions() {
        mColorChangeObserver.assertState(null, false);

        mBottomAttachedUiObserver.onOmniboxSuggestionsBackgroundColorChanged(
                OMNIBOX_SUGGESTIONS_COLOR);
        mBottomAttachedUiObserver.onOmniboxSuggestionsVisibilityChanged(true);
        mColorChangeObserver.assertState(OMNIBOX_SUGGESTIONS_COLOR, false);

        mBottomAttachedUiObserver.onOmniboxSuggestionsVisibilityChanged(false);
        mColorChangeObserver.assertState(null, false);

        mBottomAttachedUiObserver.onOmniboxSuggestionsBackgroundColorChanged(
                OMNIBOX_SUGGESTIONS_COLOR_2);
        mBottomAttachedUiObserver.onOmniboxSuggestionsVisibilityChanged(true);
        mColorChangeObserver.assertState(OMNIBOX_SUGGESTIONS_COLOR_2, false);

        mBottomAttachedUiObserver.onOmniboxSuggestionsVisibilityChanged(false);
        mColorChangeObserver.assertState(null, false);
    }

    @Test
    public void testColorPrioritization() {
        mColorChangeObserver.assertState(null, false);

        // Show the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ true, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, false);

        // Show bottom controls.
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(BOTTOM_CONTROLS_HEIGHT, 0);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false);

        // Show overlay panel.
        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, false);

        // Show bottom sheet.
        mBottomAttachedUiObserver.onSheetContentChanged(mBottomSheetContentYellowBackground);
        mBottomAttachedUiObserver.onSheetOpened(0);
        mColorChangeObserver.assertState(BOTTOM_SHEET_YELLOW, false);

        // Show omnibox suggestions.
        mBottomAttachedUiObserver.onOmniboxSuggestionsBackgroundColorChanged(
                OMNIBOX_SUGGESTIONS_COLOR);
        mBottomAttachedUiObserver.onOmniboxSuggestionsVisibilityChanged(true);
        mColorChangeObserver.assertState(OMNIBOX_SUGGESTIONS_COLOR, false);

        // Hide omnibox suggestions.
        mBottomAttachedUiObserver.onOmniboxSuggestionsVisibilityChanged(false);
        mColorChangeObserver.assertState(BOTTOM_SHEET_YELLOW, false);

        // Hide bottom sheet.
        mBottomAttachedUiObserver.onSheetClosed(0);
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, false);

        // Hide overlay panel.
        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false);

        // Hide bottom controls - should fall back to the snackbar color.
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(0, 0);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, false);
    }

    @Test
    public void testDestroy() {
        mBottomAttachedUiObserver.destroy();
        verify(mOmniboxSuggestionsVisualState)
                .setOmniboxSuggestionsVisualStateObserver(eq(Optional.empty()));
        verify(mBottomSheetController).removeObserver(eq(mBottomAttachedUiObserver));
        verify(mOverlayPanelStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
        verify(mBrowserControlsStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
        verify(mSnackbarManager).removeObserver(eq(mBottomAttachedUiObserver));
        verify(mInsetObserver).removeObserver(eq(mBottomAttachedUiObserver));
    }

    private static class MockColorChangeObserver implements BottomAttachedUiObserver.Observer {
        private @Nullable @ColorInt Integer mColor;
        private boolean mForceShowDivider;

        @Override
        public void onBottomAttachedColorChanged(
                @Nullable Integer color, boolean forceShowDivider) {
            mColor = color;
            mForceShowDivider = forceShowDivider;
        }

        private void assertState(
                @Nullable @ColorInt Integer expectedColor, boolean expectedForceShowDivider) {
            assertEquals("Incorrect bottom attached color.", expectedColor, mColor);
            assertEquals(
                    "Incorrect value for forceShowDivider.",
                    expectedForceShowDivider,
                    mForceShowDivider);
        }
    }
}
