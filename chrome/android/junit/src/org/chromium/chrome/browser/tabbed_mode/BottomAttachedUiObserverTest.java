// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Color;
import android.os.Looper;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelStateProvider;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessorySheetVisualStateProvider;
import org.chromium.chrome.browser.keyboard_accessory.KeyboardAccessoryVisualStateProvider;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsVisualState;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.ui.insets.InsetObserver;

@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.NAV_BAR_COLOR_ANIMATION)
public class BottomAttachedUiObserverTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int BOTTOM_CONTROLS_HEIGHT = 100;
    private static final int BOTTOM_CONTROLS_MIN_HEIGHT_MULTIPLE_LAYER = 80;
    private static final int BOTTOM_CHIN_HEIGHT = 60;
    private static final int BROWSER_CONTROLS_COLOR = Color.RED;
    private static final int SNACKBAR_COLOR = Color.GREEN;
    private static final int OVERLAY_PANEL_COLOR = Color.BLUE;
    private static final int BOTTOM_SHEET_YELLOW = Color.YELLOW;
    private static final int BOTTOM_SHEET_CYAN = Color.CYAN;
    private static final int OMNIBOX_SUGGESTIONS_COLOR = Color.MAGENTA;
    private static final int OMNIBOX_SUGGESTIONS_COLOR_2 = Color.DKGRAY;
    private static final int KEYBOARD_ACCESSORY_COLOR = 0xFF444400; // dark yellow
    private static final int ACCESSORY_SHEET_COLOR = 0xFF440044; // purple

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
    private TestBottomUiObserver mColorChangeObserver;

    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private SnackbarManager mSnackbarManager;

    private final ObservableSupplierImpl<ContextualSearchManager> mContextualSearchManagerSupplier =
            new ObservableSupplierImpl<>();
    @Mock private ContextualSearchManager mContextualSearchManager;

    private final ObservableSupplierImpl<OverlayPanelStateProvider>
            mOverlayPanelStateProviderSupplier = new ObservableSupplierImpl<>();
    @Mock private OverlayPanelStateProvider mOverlayPanelStateProvider;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private BottomSheetContent mSheetContent;

    @Mock private OmniboxSuggestionsVisualState mOmniboxSuggestionsVisualState;

    @Mock private ManualFillingComponent mManualFillingComponent;
    @Mock private ManualFillingComponentSupplier mManualFillingComponentSupplier;

    @Mock private KeyboardAccessoryVisualStateProvider mKeyboardAccessoryVisualStateProvider;
    private final ObservableSupplierImpl<KeyboardAccessoryVisualStateProvider>
            mKeyboardAccessoryVisualStateSupplier = new ObservableSupplierImpl<>();
    @Mock private AccessorySheetVisualStateProvider mAccessorySheetVisualStateProvider;
    private final ObservableSupplierImpl<AccessorySheetVisualStateProvider>
            mAccessorySheetVisualStateSupplier = new ObservableSupplierImpl<>();

    @Mock private InsetObserver mInsetObserver;

    @Before
    public void setUp() {
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(BOTTOM_NAV_BAR_INSETS);

        when(mContextualSearchManager.getOverlayPanelStateProviderSupplier())
                .thenReturn(mOverlayPanelStateProviderSupplier);

        doReturn(null).when(mBottomSheetController).getSheetBackgroundColor();
        when(mBottomSheetController.isFullWidth()).thenReturn(true);
        when(mSnackbarManager.isFullWidth()).thenReturn(true);

        mContextualSearchManagerSupplier.set(mContextualSearchManager);
        mOverlayPanelStateProviderSupplier.set(mOverlayPanelStateProvider);
        when(mOverlayPanelStateProvider.isFullWidthSizePanel()).thenReturn(true);
        mKeyboardAccessoryVisualStateSupplier.set(mKeyboardAccessoryVisualStateProvider);
        mAccessorySheetVisualStateSupplier.set(mAccessorySheetVisualStateProvider);
        when(mManualFillingComponentSupplier.get()).thenReturn(mManualFillingComponent);
        when(mManualFillingComponent.getKeyboardAccessoryVisualStateProvider())
                .thenReturn(mKeyboardAccessoryVisualStateSupplier);
        when(mManualFillingComponent.getAccessorySheetVisualStateProvider())
                .thenReturn(mAccessorySheetVisualStateSupplier);

        mBottomAttachedUiObserver =
                new BottomAttachedUiObserver(
                        mBottomControlsStacker,
                        mBrowserControlsStateProvider,
                        mSnackbarManager,
                        mContextualSearchManagerSupplier,
                        mBottomSheetController,
                        mOmniboxSuggestionsVisualState,
                        mManualFillingComponentSupplier,
                        mInsetObserver);
        mBottomAttachedUiObserver.onInsetChanged();

        mColorChangeObserver = new TestBottomUiObserver();
        mBottomAttachedUiObserver.addObserver(mColorChangeObserver);

        // Ensure all observer callbacks are run, so that all observables are being properly
        // observed.
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    @Test
    public void testAdaptsColorToBrowserControls() {
        mColorChangeObserver.assertState(null, false, false);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);

        // Show bottom controls.
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(BOTTOM_CONTROLS_HEIGHT, 0);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        // Scroll off bottom controls partway.
        mBottomAttachedUiObserver.onControlsOffsetChanged(
                0, 0, false, BOTTOM_CONTROLS_HEIGHT / 2, 0, false, false, false);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        // Scroll off bottom controls fully.
        mBottomAttachedUiObserver.onControlsOffsetChanged(
                0, 0, false, BOTTOM_CONTROLS_HEIGHT, 0, false, false, false);
        mColorChangeObserver.assertState(null, false, false);

        // Scroll bottom controls back.
        mBottomAttachedUiObserver.onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        // Hide bottom controls.
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(0, 0);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToBrowserControls_ignoresBottomChin() {
        mColorChangeObserver.assertState(null, false, false);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(false);

        // Show bottom controls, but only with the bottom chin.
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(BOTTOM_CHIN_HEIGHT, 0);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToBrowserControls_bottomChinConstraint_bottomChinNonScrollable() {
        mColorChangeObserver.assertState(null, false, false);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);

        when(mBottomControlsStacker.isLayerNonScrollable(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);
        when(mBottomControlsStacker.hasMultipleNonScrollableLayer()).thenReturn(false);

        // Show bottom controls, bottom chin is non-scrollable.
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(
                BOTTOM_CONTROLS_HEIGHT, BOTTOM_CHIN_HEIGHT);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        // Scroll off bottom controls fully. Browser controls should no longer be used.
        mBottomAttachedUiObserver.onControlsOffsetChanged(
                0,
                0,
                false,
                BOTTOM_CONTROLS_HEIGHT - BOTTOM_CHIN_HEIGHT,
                BOTTOM_CHIN_HEIGHT,
                false,
                false,
                false);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToBrowserControls_bottomChinConstraint_multipleNonScrollableLayer() {
        mColorChangeObserver.assertState(null, false, false);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);

        when(mBottomControlsStacker.isLayerNonScrollable(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);
        when(mBottomControlsStacker.hasMultipleNonScrollableLayer()).thenReturn(true);

        // Show bottom controls, but only with the bottom chin. Color should be null.
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(
                BOTTOM_CONTROLS_HEIGHT, BOTTOM_CONTROLS_MIN_HEIGHT_MULTIPLE_LAYER);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        // Scroll off bottom controls fully. Browser controls should still be used.
        mBottomAttachedUiObserver.onControlsOffsetChanged(
                0,
                0,
                false,
                BOTTOM_CONTROLS_HEIGHT - BOTTOM_CONTROLS_MIN_HEIGHT_MULTIPLE_LAYER,
                BOTTOM_CONTROLS_MIN_HEIGHT_MULTIPLE_LAYER,
                false,
                false,
                false);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);
    }

    @Test
    public void testAdaptsColorToBrowserControls_bottomChinConstraint_bottomChinScrollable() {
        mColorChangeObserver.assertState(null, false, false);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);

        when(mBottomControlsStacker.isLayerNonScrollable(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(false);
        when(mBottomControlsStacker.hasMultipleNonScrollableLayer()).thenReturn(false);

        // Show bottom controls. Color should be BROWSER_CONTROLS_COLOR.
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(BOTTOM_CONTROLS_HEIGHT, 0);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        // Scroll off bottom controls fully. Browser controls should no longer be used.
        mBottomAttachedUiObserver.onControlsOffsetChanged(
                0, 0, false, BOTTOM_CONTROLS_HEIGHT, 0, false, false, false);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToBrowserControls_bottomChinConstraint_bottomChinOnly() {
        mColorChangeObserver.assertState(null, false, false);
        when(mBottomControlsStacker.isLayerNonScrollable(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);
        when(mBottomControlsStacker.hasMultipleNonScrollableLayer()).thenReturn(false);

        // Assume some other browser controls were visible, but then is removed.
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(
                BOTTOM_CONTROLS_HEIGHT, BOTTOM_CHIN_HEIGHT);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        // Then the control is removed, the chin is set as the only layer
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(false);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(
                BOTTOM_CHIN_HEIGHT, BOTTOM_CHIN_HEIGHT);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToSnackbars() {
        mColorChangeObserver.assertState(null, false, false);

        // Set only the snackbar color.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(null, false, false);

        // Show the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ true, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, false, false);

        // Hide the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, /* color= */ null);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToSnackbars_doesNotCoverFullWidth() {
        when(mSnackbarManager.isFullWidth()).thenReturn(false);
        mColorChangeObserver.assertState(null, false, false);

        // Set only the snackbar color.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(null, false, false);

        // Show the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ true, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, true, false);

        // Hide the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, /* color= */ null);
        mColorChangeObserver.assertState(null, false, false);
    }

    /*
    Tests that when floating snackbar is enabled, we do not add BottomAttachedUiObserver.
    */
    @Test
    public void testDoesNotAddBottomAttachedUiObserver() {
        verify(mSnackbarManager, never()).addObserver(eq(mBottomAttachedUiObserver));
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
        mColorChangeObserver.assertState(null, false, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, false, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.EXPANDED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.MAXIMIZED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, false, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToOverlayPanel_doesNotCoverFullWidth_drawingEdgeToEdge() {
        when(mOverlayPanelStateProvider.isFullWidthSizePanel()).thenReturn(false, false);
        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToBottomSheet() {
        doReturn(null).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertState(null, false, false);

        openBottomSheet();
        mColorChangeObserver.assertState(null, false, false);
        dismissBottomSheet();
        mColorChangeObserver.assertState(null, false, false);

        doReturn(BOTTOM_SHEET_CYAN).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        openBottomSheet();
        mColorChangeObserver.assertState(BOTTOM_SHEET_CYAN, false, false);
        dismissBottomSheet();
        mColorChangeObserver.assertState(null, false, false);

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertState(null, false, false);

        openBottomSheet();
        mColorChangeObserver.assertState(BOTTOM_SHEET_YELLOW, false, false);
        doReturn(BOTTOM_SHEET_CYAN).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertState(BOTTOM_SHEET_CYAN, false, false);
        dismissBottomSheet();
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToBottomSheet_doesNotCoverFullWidth() {
        when(mBottomSheetController.isFullWidth()).thenReturn(false, false);
        when(mBottomControlsStacker.isLayerVisible(eq(LayerType.BOTTOM_CHIN))).thenReturn(false);

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertState(null, false, false);

        openBottomSheet();
        mColorChangeObserver.assertState(BOTTOM_SHEET_YELLOW, true, false);
        dismissBottomSheet();
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToBottomSheet_doesNotCoverFullWidth_withBottomChin() {
        when(mBottomSheetController.isFullWidth()).thenReturn(false, false);
        when(mBottomControlsStacker.isLayerVisible(eq(LayerType.BOTTOM_CHIN))).thenReturn(true);

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertState(null, false, false);

        openBottomSheet();
        mColorChangeObserver.assertState(null, false, false);
        dismissBottomSheet();
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToBottomSheet_doesNotCoverFullWidth_withoutBottomChin() {
        when(mBottomSheetController.isFullWidth()).thenReturn(false);
        when(mBottomControlsStacker.isLayerVisible(eq(LayerType.BOTTOM_CHIN))).thenReturn(false);

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertState(null, false, false);

        openBottomSheet();
        mColorChangeObserver.assertState(BOTTOM_SHEET_YELLOW, true, false);
        dismissBottomSheet();
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToBottomSheet_anchorToBrowserControls_fullWidthNoControls() {
        when(mBottomSheetController.isFullWidth()).thenReturn(true);
        when(mBottomControlsStacker.isLayerVisible(eq(LayerType.BOTTOM_CHIN))).thenReturn(false);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(eq(LayerType.BOTTOM_CHIN)))
                .thenReturn(false);

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);

        peekBottomSheet();
        mColorChangeObserver.assertColor(BOTTOM_SHEET_YELLOW).assertForceShowDivider(false);
        dismissBottomSheet();
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);
    }

    @Test
    public void testAdaptsColorToBottomSheet_anchorToBrowserControls_fullWidthOnBottomChin() {
        when(mBottomSheetController.isFullWidth()).thenReturn(true);
        when(mBottomControlsStacker.isLayerVisible(eq(LayerType.BOTTOM_CHIN))).thenReturn(true);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(eq(LayerType.BOTTOM_CHIN)))
                .thenReturn(false);

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);

        peekBottomSheet();
        mColorChangeObserver.assertColor(BOTTOM_SHEET_YELLOW).assertForceShowDivider(false);
        dismissBottomSheet();
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);
    }

    @Test
    public void testAdaptsColorToBottomSheet_anchorToBrowserControls_fullWidthOnOtherControls() {
        when(mBottomSheetController.isFullWidth()).thenReturn(true);
        when(mBottomControlsStacker.isLayerVisible(eq(LayerType.BOTTOM_CHIN))).thenReturn(true);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(eq(LayerType.BOTTOM_CHIN)))
                .thenReturn(true);

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);

        peekBottomSheet();
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);
        dismissBottomSheet();
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);
    }

    @Test
    public void testAdaptsColorToBottomSheet_anchorToBrowserControls_notFullWidthNoControls() {
        when(mBottomSheetController.isFullWidth()).thenReturn(false);
        when(mBottomControlsStacker.isLayerVisible(eq(LayerType.BOTTOM_CHIN))).thenReturn(false);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(eq(LayerType.BOTTOM_CHIN)))
                .thenReturn(false);

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);

        peekBottomSheet();
        mColorChangeObserver.assertColor(BOTTOM_SHEET_YELLOW).assertForceShowDivider(true);
        dismissBottomSheet();
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);
    }

    @Test
    public void testAdaptsColorToBottomSheet_anchorToBrowserControls_notFullWidthWithChin() {
        when(mBottomSheetController.isFullWidth()).thenReturn(false);
        when(mBottomControlsStacker.isLayerVisible(eq(LayerType.BOTTOM_CHIN))).thenReturn(true);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(eq(LayerType.BOTTOM_CHIN)))
                .thenReturn(false);

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);

        peekBottomSheet();
        mColorChangeObserver.assertColor(BOTTOM_SHEET_YELLOW).assertForceShowDivider(true);
        dismissBottomSheet();
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);
    }

    @Test
    public void testAdaptsColorToBottomSheet_anchorToBrowserControls_notFullWidthOtherControls() {
        when(mBottomSheetController.isFullWidth()).thenReturn(false);
        when(mBottomControlsStacker.isLayerVisible(eq(LayerType.BOTTOM_CHIN))).thenReturn(true);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(eq(LayerType.BOTTOM_CHIN)))
                .thenReturn(true);

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);

        peekBottomSheet();
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);
        dismissBottomSheet();
        mColorChangeObserver.assertColor(null).assertForceShowDivider(false);
    }

    @Test
    public void testAdaptsToInsetChanges() {
        verify(mInsetObserver).addObserver(eq(mBottomAttachedUiObserver));

        // Navbar is present at the bottom.
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(BOTTOM_NAV_BAR_INSETS);
        mBottomAttachedUiObserver.onInsetChanged();
        mColorChangeObserver.assertState(null, false, false);

        // Show a snackbar to set a color.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ true, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, false, false);

        // Shift navbar to the side.
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(SIDE_NAV_BAR_INSETS);
        mBottomAttachedUiObserver.onInsetChanged();
        mColorChangeObserver.assertState(null, false, false);

        // Return navbar to the bottom.
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(BOTTOM_NAV_BAR_INSETS);
        mBottomAttachedUiObserver.onInsetChanged();
        mColorChangeObserver.assertState(SNACKBAR_COLOR, false, false);

        // Hide the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testAdaptsColorToOmniboxSuggestions() {
        mColorChangeObserver.assertState(null, false, false);

        mBottomAttachedUiObserver.onOmniboxSuggestionsBackgroundColorChanged(
                OMNIBOX_SUGGESTIONS_COLOR);
        mBottomAttachedUiObserver.onOmniboxSessionStateChange(true);
        mColorChangeObserver.assertState(OMNIBOX_SUGGESTIONS_COLOR, false, false);

        mBottomAttachedUiObserver.onOmniboxSessionStateChange(false);
        mColorChangeObserver.assertState(null, false, false);

        mBottomAttachedUiObserver.onOmniboxSuggestionsBackgroundColorChanged(
                OMNIBOX_SUGGESTIONS_COLOR_2);
        mBottomAttachedUiObserver.onOmniboxSessionStateChange(true);
        mColorChangeObserver.assertState(OMNIBOX_SUGGESTIONS_COLOR_2, false, false);

        mBottomAttachedUiObserver.onOmniboxSessionStateChange(false);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testSetAccessorySheetVisualStateObserver() {
        verify(mAccessorySheetVisualStateProvider).addObserver(eq(mBottomAttachedUiObserver));

        mAccessorySheetVisualStateSupplier.set(null);
        verify(mAccessorySheetVisualStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
    }

    @Test
    public void testAdaptsColorToAccessorySheet() {
        mColorChangeObserver.assertState(null, false, false);

        mBottomAttachedUiObserver.onAccessorySheetStateChanged(true, ACCESSORY_SHEET_COLOR);
        mColorChangeObserver.assertState(ACCESSORY_SHEET_COLOR, false, true);

        mBottomAttachedUiObserver.onAccessorySheetStateChanged(false, ACCESSORY_SHEET_COLOR);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testSetKeyboardAccessoryVisualStateObserver() {
        verify(mKeyboardAccessoryVisualStateProvider).addObserver(eq(mBottomAttachedUiObserver));

        mKeyboardAccessoryVisualStateSupplier.set(null);
        verify(mKeyboardAccessoryVisualStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
    }

    @Test
    public void testAdaptsColorToKeyboardAccessory() {
        mColorChangeObserver.assertState(null, false, false);

        mBottomAttachedUiObserver.onKeyboardAccessoryVisualStateChanged(
                true, KEYBOARD_ACCESSORY_COLOR);
        mColorChangeObserver.assertState(KEYBOARD_ACCESSORY_COLOR, false, false);

        mBottomAttachedUiObserver.onKeyboardAccessoryVisualStateChanged(
                false, KEYBOARD_ACCESSORY_COLOR);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    public void testColorPrioritization() {
        mColorChangeObserver.assertState(null, false, false);

        // Show the snackbar.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ true, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, false, false);

        // Show the keyboard accessory.
        mBottomAttachedUiObserver.onKeyboardAccessoryVisualStateChanged(
                /* visible= */ true, KEYBOARD_ACCESSORY_COLOR);
        mColorChangeObserver.assertState(KEYBOARD_ACCESSORY_COLOR, false, false);

        // Show bottom controls.
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(BOTTOM_CONTROLS_HEIGHT, 0);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        // Show overlay panel.
        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, false, false);

        // Show bottom sheet.
        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        openBottomSheet();
        mColorChangeObserver.assertState(BOTTOM_SHEET_YELLOW, false, false);

        // Show omnibox suggestions.
        mBottomAttachedUiObserver.onOmniboxSessionStateChange(true);
        mBottomAttachedUiObserver.onOmniboxSuggestionsBackgroundColorChanged(
                OMNIBOX_SUGGESTIONS_COLOR);
        mColorChangeObserver.assertState(OMNIBOX_SUGGESTIONS_COLOR, false, false);

        // Show accessory sheet.
        mBottomAttachedUiObserver.onAccessorySheetStateChanged(true, ACCESSORY_SHEET_COLOR);
        mColorChangeObserver.assertState(ACCESSORY_SHEET_COLOR, false, true);

        // Hide accessory sheet.
        mBottomAttachedUiObserver.onAccessorySheetStateChanged(false, ACCESSORY_SHEET_COLOR);
        mColorChangeObserver.assertState(OMNIBOX_SUGGESTIONS_COLOR, false, false);

        // Hide omnibox suggestions.
        mBottomAttachedUiObserver.onOmniboxSessionStateChange(false);
        mColorChangeObserver.assertState(BOTTOM_SHEET_YELLOW, false, false);

        // Hide bottom sheet.
        dismissBottomSheet();
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, false, false);

        // Hide overlay panel.
        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        // Hide bottom controls.
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(0, 0);
        mColorChangeObserver.assertState(KEYBOARD_ACCESSORY_COLOR, false, false);

        // Hide keyboard accessory - should fall back to the snackbar color.
        mBottomAttachedUiObserver.onKeyboardAccessoryVisualStateChanged(
                /* visible= */ false, KEYBOARD_ACCESSORY_COLOR);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, false, false);
    }

    @Test
    public void testColorPrioritization_bottomToolbar() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsStateProvider).getControlsPosition();
        doReturn(0.0f).when(mBrowserControlsStateProvider).getBrowserControlHiddenRatio();

        when(mBottomControlsStacker.hasVisibleLayersOtherThan(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(BOTTOM_CONTROLS_HEIGHT, 0);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.EXPANDED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        peekBottomSheet();
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        doReturn(1.0f).when(mBrowserControlsStateProvider).getBrowserControlHiddenRatio();

        doReturn(BOTTOM_SHEET_YELLOW).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        openBottomSheet();
        mColorChangeObserver.assertState(BOTTOM_SHEET_YELLOW, false, false);

        doReturn(0.0f).when(mBrowserControlsStateProvider).getBrowserControlHiddenRatio();
        doReturn(ControlsPosition.TOP).when(mBrowserControlsStateProvider).getControlsPosition();

        dismissBottomSheet();
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, false, false);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION,
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE
    })
    @DisableFeatures({ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN})
    public void testNavBarColorAnimationsOverlayPanel() {
        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false, false);

        // Nav bar color animations disabled on appearance.
        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, false, true);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.EXPANDED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false, true);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.MAXIMIZED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false, true);

        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.PEEKED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(OVERLAY_PANEL_COLOR, false, true);

        // Nav bar color animations enabled on disappearance.
        mBottomAttachedUiObserver.onOverlayPanelStateChanged(
                OverlayPanel.PanelState.CLOSED, OVERLAY_PANEL_COLOR);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION,
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE
    })
    @DisableFeatures({ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN})
    public void testNavBarColorAnimationsBottomSheet() {
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        mColorChangeObserver.assertState(null, false, false);

        openBottomSheet();
        mColorChangeObserver.assertState(null, false, false);
        dismissBottomSheet();
        mColorChangeObserver.assertState(null, false, false);

        doReturn(BOTTOM_SHEET_CYAN).when(mBottomSheetController).getSheetBackgroundColor();
        mBottomAttachedUiObserver.onSheetContentChanged(mSheetContent);
        openBottomSheet();
        // Nav bar color animations disabled on appearance.
        mColorChangeObserver.assertState(BOTTOM_SHEET_CYAN, false, true);

        dismissBottomSheet();
        // Nav bar color animations enabled on disappearance.
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION,
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE
    })
    @DisableFeatures({ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN})
    public void testNavBarColorAnimationsSnackbar() {
        mColorChangeObserver.assertState(null, false, false);

        // Set only the snackbar color.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(null, false, false);

        // Show the snackbar. Nav bar color animations disabled on appearance.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ true, SNACKBAR_COLOR);
        mColorChangeObserver.assertState(SNACKBAR_COLOR, false, true);

        // Hide the snackbar. Nav bar color animations enabled on disappearance.
        mBottomAttachedUiObserver.onSnackbarStateChanged(/* isShowing= */ false, /* color= */ null);
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION,
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE
    })
    @DisableFeatures({ChromeFeatureList.EDGE_TO_EDGE_BOTTOM_CHIN})
    public void testNavBarColorAnimationsBrowserControls() {
        mColorChangeObserver.assertState(null, false, false);
        when(mBottomControlsStacker.hasVisibleLayersOtherThan(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);

        // Show bottom controls.
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(BOTTOM_CONTROLS_HEIGHT, 0);
        // Nav bar color animations disabled on appearance.
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, true);

        // Scroll off bottom controls partway.
        mBottomAttachedUiObserver.onControlsOffsetChanged(
                0, 0, false, BOTTOM_CONTROLS_HEIGHT / 2, 0, false, false, false);
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, true);

        // Scroll off bottom controls fully.
        mBottomAttachedUiObserver.onControlsOffsetChanged(
                0, 0, false, BOTTOM_CONTROLS_HEIGHT, 0, false, false, false);
        // Nav bar color animations enabled when scrolling off.
        mColorChangeObserver.assertState(null, false, false);

        // Scroll bottom controls back.
        mBottomAttachedUiObserver.onControlsOffsetChanged(0, 0, false, 0, 0, false, false, false);
        // Nav bar color animations enabled when scrolling on.
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, false);

        // Hide bottom controls.
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(0, 0);
        // Nav bar color animations enabled on disappearance.
        mColorChangeObserver.assertState(null, false, false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NAV_BAR_COLOR_ANIMATION,
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE
    })
    public void testNavBarColorAnimationsBottomToolbar() {
        doReturn(ControlsPosition.BOTTOM).when(mBrowserControlsStateProvider).getControlsPosition();
        doReturn(0.0f).when(mBrowserControlsStateProvider).getBrowserControlHiddenRatio();

        when(mBottomControlsStacker.hasVisibleLayersOtherThan(
                        eq(BottomControlsStacker.LayerType.BOTTOM_CHIN)))
                .thenReturn(true);
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(BROWSER_CONTROLS_COLOR);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(BOTTOM_CONTROLS_HEIGHT, 0);

        // Nav bar color animations disabled when the bottom toolbar is visible.
        mColorChangeObserver.assertState(BROWSER_CONTROLS_COLOR, false, true);
    }

    @Test
    public void testDestroy() {
        mBottomAttachedUiObserver.destroy();
        verify(mOmniboxSuggestionsVisualState).setOmniboxSuggestionsVisualStateObserver(eq(null));
        verify(mAccessorySheetVisualStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
        verify(mBottomSheetController).removeObserver(eq(mBottomAttachedUiObserver));
        verify(mOverlayPanelStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
        verify(mBrowserControlsStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
        verify(mSnackbarManager).removeObserver(eq(mBottomAttachedUiObserver));
        verify(mInsetObserver).removeObserver(eq(mBottomAttachedUiObserver));
    }

    private void openBottomSheet() {
        doReturn(SheetState.FULL).when(mBottomSheetController).getSheetState();
        when(mBottomSheetController.isAnchoredToBottomControls()).thenReturn(false);

        mBottomAttachedUiObserver.onSheetStateChanged(SheetState.PEEK, 0);
    }

    private void peekBottomSheet() {
        doReturn(SheetState.PEEK).when(mBottomSheetController).getSheetState();
        when(mBottomSheetController.isAnchoredToBottomControls()).thenReturn(true);

        mBottomAttachedUiObserver.onSheetStateChanged(SheetState.PEEK, 0);
    }

    private void dismissBottomSheet() {
        doReturn(SheetState.HIDDEN).when(mBottomSheetController).getSheetState();
        when(mBottomSheetController.isAnchoredToBottomControls()).thenReturn(false);

        mBottomAttachedUiObserver.onSheetStateChanged(SheetState.HIDDEN, 0);
    }

    private static class TestBottomUiObserver implements BottomAttachedUiObserver.Observer {
        private @Nullable @ColorInt Integer mColor;
        private boolean mForceShowDivider;
        private boolean mDisabledAnimation;

        @Override
        public void onBottomAttachedColorChanged(
                @Nullable Integer color, boolean forceShowDivider, boolean disableAnimation) {
            mColor = color;
            mForceShowDivider = forceShowDivider;
            mDisabledAnimation = disableAnimation;
        }

        private void assertState(
                @Nullable @ColorInt Integer expectedColor,
                boolean expectedForceShowDivider,
                boolean expectedDisabledAnimation) {
            assertColor(expectedColor)
                    .assertForceShowDivider(expectedForceShowDivider)
                    .assertDisabledAnimation(expectedDisabledAnimation);
        }

        private TestBottomUiObserver assertColor(@Nullable @ColorInt Integer expectedColor) {
            assertEquals("Incorrect bottom attached color.", expectedColor, mColor);
            return this;
        }

        private TestBottomUiObserver assertForceShowDivider(boolean expectedForceShowDivider) {
            assertEquals(
                    "Incorrect value for forceShowDivider.",
                    expectedForceShowDivider,
                    mForceShowDivider);
            return this;
        }

        private TestBottomUiObserver assertDisabledAnimation(boolean expectedDisabledAnimation) {
            assertEquals(
                    "Incorrect value for disabledAnimation.",
                    expectedDisabledAnimation,
                    mDisabledAnimation);
            return this;
        }
    }
}
