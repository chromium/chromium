// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelStateProvider;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessorySheetVisualStateProvider;
import org.chromium.chrome.browser.keyboard_accessory.KeyboardAccessoryVisualStateProvider;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentSupplier;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsVisualState;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarStateProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.InsetObserver;

import java.util.Optional;

/**
 * An observer class that listens for changes in UI components that are attached to the bottom of
 * the screen, bordering the navigation bar area. This class then aggregates that information and
 * notifies its own observers of properties of the UI currently bordering ("attached to") the
 * navigation bar.
 */
public class BottomAttachedUiObserver
        implements BrowserControlsStateProvider.Observer,
                SnackbarStateProvider.Observer,
                OverlayPanelStateProvider.Observer,
                BottomSheetObserver,
                AutocompleteCoordinator.OmniboxSuggestionsVisualStateObserver,
                KeyboardAccessoryVisualStateProvider.Observer,
                AccessorySheetVisualStateProvider.Observer,
                InsetObserver.WindowInsetObserver {

    /**
     * An observer to be notified of changes to what kind of UI is currently bordering the bottom of
     * the screen.
     */
    public interface Observer {
        /**
         * Called when the color of the bottom UI that is attached to the bottom of the screen
         * changes.
         *
         * @param color The color of the UI that is attached to the bottom of the screen.
         * @param forceShowDivider Whether the divider should be forced to show.
         * @param disableAnimation Whether the color change animation should be disabled.
         */
        void onBottomAttachedColorChanged(
                @Nullable @ColorInt Integer color,
                boolean forceShowDivider,
                boolean disableAnimation);
    }

    private boolean mBottomNavbarPresent;
    private final ObserverList<Observer> mObservers;
    private @Nullable @ColorInt Integer mBottomAttachedColor;
    private boolean mShouldShowDivider;

    private final BottomSheetController mBottomSheetController;
    private boolean mBottomSheetVisible;
    private @Nullable @ColorInt Integer mBottomSheetColor;

    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private int mBottomControlsHeight;
    private int mBottomControlsMinHeight;
    private @Nullable @ColorInt Integer mBottomControlsColor;
    private boolean mUseBottomControlsColor;

    private final BottomControlsStacker mBottomControlsStacker;

    private final SnackbarStateProvider mSnackbarStateProvider;
    private @Nullable @ColorInt Integer mSnackbarColor;
    private boolean mSnackbarVisible;

    private OverlayPanelStateProvider mOverlayPanelStateProvider;
    private @Nullable @ColorInt Integer mOverlayPanelColor;
    private boolean mOverlayPanelVisible;
    @PanelState private int mOverlayPanelState;

    private final Optional<OmniboxSuggestionsVisualState> mOmniboxSuggestionsVisualState;
    private boolean mOmniboxSuggestionsVisible;
    private @Nullable @ColorInt Integer mOmniboxSuggestionsColor;

    private final InsetObserver mInsetObserver;

    private ObservableSupplier<KeyboardAccessoryVisualStateProvider>
            mKeyboardAccessoryVisualStateProviderSupplier;
    private Callback<KeyboardAccessoryVisualStateProvider>
            mKeyboardAccessoryProviderSupplierObserver;
    private KeyboardAccessoryVisualStateProvider mKeyboardAccessoryVisualStateProvider;
    private boolean mKeyboardAccessoryVisible;
    private @Nullable @ColorInt Integer mKeyboardAccessoryColor;

    private ObservableSupplier<AccessorySheetVisualStateProvider>
            mAccessorySheetVisualStateProviderSupplier;
    private Callback<AccessorySheetVisualStateProvider> mAccessorySheetProviderSupplierObserver;
    private AccessorySheetVisualStateProvider mAccessorySheetVisualStateProvider;
    private boolean mAccessorySheetVisible;
    private @Nullable @ColorInt Integer mAccessorySheetColor;
    private boolean mNonBottomChinBottomControlsVisible;

    /**
     * Build the observer that listens to changes in the UI bordering the bottom.
     *
     * @param bottomControlsStacker The {@link BottomControlsStacker} for interacting with and
     *     checking the state of the bottom browser controls.
     * @param browserControlsStateProvider Supplies a {@link BrowserControlsStateProvider} for the
     *     browser controls.
     * @param snackbarStateProvider Supplies a {@link SnackbarStateProvider} to watch for snackbars
     *     being shown.
     * @param contextualSearchManagerSupplier Supplies a {@link ContextualSearchManager} to watch
     *     for changes to contextual search and the overlay panel.
     * @param bottomSheetController A {@link BottomSheetController} to interact with and watch for
     *     changes to the bottom sheet.
     * @param omniboxSuggestionsVisualState An optional {@link OmniboxSuggestionsVisualState} for
     *     access to the visual state of the omnibox suggestions.
     * @param manualFillingComponentSupplier Supplies the {@link ManualFillingComponent} for
     *     observing the visual state of keyboard accessories.
     * @param insetObserver An {@link InsetObserver} to listen for changes to the window insets.
     */
    public BottomAttachedUiObserver(
            @NonNull BottomControlsStacker bottomControlsStacker,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull SnackbarStateProvider snackbarStateProvider,
            @NonNull ObservableSupplier<ContextualSearchManager> contextualSearchManagerSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull Optional<OmniboxSuggestionsVisualState> omniboxSuggestionsVisualState,
            @NonNull ManualFillingComponentSupplier manualFillingComponentSupplier,
            InsetObserver insetObserver) {
        mObservers = new ObserverList<>();

        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);
        mBottomControlsStacker = bottomControlsStacker;

        mSnackbarStateProvider = snackbarStateProvider;
        if (!SnackbarManager.isFloatingSnackbarEnabled()) {
            // The floating snackbar appears to hover and isn't anchored to bottom UI, and thus
            // should not impact the bottom attached color.
            mSnackbarStateProvider.addObserver(this);
        }

        mBottomSheetController = bottomSheetController;
        mBottomSheetController.addObserver(this);

        mInsetObserver = insetObserver;
        mInsetObserver.addObserver(this);
        checkIfBottomNavbarIsPresent();

        ManualFillingComponent manualFillingComponent = manualFillingComponentSupplier.get();
        if (manualFillingComponent != null) {
            mKeyboardAccessoryVisualStateProviderSupplier =
                    manualFillingComponent.getKeyboardAccessoryVisualStateProvider();
            mKeyboardAccessoryProviderSupplierObserver =
                    (visualStateProvider) -> {
                        if (mKeyboardAccessoryVisualStateProvider != null) {
                            mKeyboardAccessoryVisualStateProvider.removeObserver(this);
                        }
                        mKeyboardAccessoryVisible = false;
                        mKeyboardAccessoryColor = null;
                        mKeyboardAccessoryVisualStateProvider = visualStateProvider;
                        if (mKeyboardAccessoryVisualStateProvider != null) {
                            mKeyboardAccessoryVisualStateProvider.addObserver(this);
                        }
                    };
            mKeyboardAccessoryVisualStateProviderSupplier.addObserver(
                    mKeyboardAccessoryProviderSupplierObserver);

            mAccessorySheetVisualStateProviderSupplier =
                    manualFillingComponent.getAccessorySheetVisualStateProvider();
            mAccessorySheetProviderSupplierObserver =
                    (visualStateProvider) -> {
                        if (mAccessorySheetVisualStateProvider != null) {
                            mAccessorySheetVisualStateProvider.removeObserver(this);
                        }
                        mAccessorySheetVisible = false;
                        mAccessorySheetColor = null;
                        mAccessorySheetVisualStateProvider = visualStateProvider;
                        if (mAccessorySheetVisualStateProvider != null) {
                            mAccessorySheetVisualStateProvider.addObserver(this);
                        }
                    };
            mAccessorySheetVisualStateProviderSupplier.addObserver(
                    mAccessorySheetProviderSupplierObserver);
        }

        contextualSearchManagerSupplier.addObserver(
                (manager) -> {
                    if (manager == null) return;
                    manager.getOverlayPanelStateProviderSupplier()
                            .addObserver(
                                    (provider) -> {
                                        if (mOverlayPanelStateProvider != null) {
                                            mOverlayPanelStateProvider.removeObserver(this);
                                        }
                                        mOverlayPanelVisible = false;
                                        mOverlayPanelColor = null;
                                        mOverlayPanelStateProvider = provider;
                                        if (mOverlayPanelStateProvider != null) {
                                            mOverlayPanelStateProvider.addObserver(this);
                                        }
                                    });
                });

        mOmniboxSuggestionsVisualState = omniboxSuggestionsVisualState;
        mOmniboxSuggestionsVisualState.ifPresent(
                coordinator ->
                        coordinator.setOmniboxSuggestionsVisualStateObserver(Optional.of(this)));
    }

    /**
     * @param observer The observer to add.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * @param observer The observer to remove.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    public void destroy() {
        mOmniboxSuggestionsVisualState.ifPresent(
                autocompleteCoordinator ->
                        autocompleteCoordinator.setOmniboxSuggestionsVisualStateObserver(
                                Optional.empty()));
        if (mAccessorySheetVisualStateProviderSupplier != null) {
            mAccessorySheetVisualStateProviderSupplier.removeObserver(
                    mAccessorySheetProviderSupplierObserver);
        }
        if (mAccessorySheetVisualStateProvider != null) {
            mAccessorySheetVisualStateProvider.removeObserver(this);
        }
        if (mBottomSheetController != null) {
            mBottomSheetController.removeObserver(this);
        }
        if (mOverlayPanelStateProvider != null) {
            mOverlayPanelStateProvider.removeObserver(this);
        }
        if (mBrowserControlsStateProvider != null) {
            mBrowserControlsStateProvider.removeObserver(this);
        }
        if (mSnackbarStateProvider != null) {
            mSnackbarStateProvider.removeObserver(this);
        }
        if (mInsetObserver != null) {
            mInsetObserver.removeObserver(this);
        }
    }

    private void updateBottomAttachedColor() {
        @Nullable
        @ColorInt
        Integer bottomAttachedColor = mBottomNavbarPresent ? calculateBottomAttachedColor() : null;
        boolean shouldShowDivider = mBottomNavbarPresent && shouldShowDivider();
        if (mBottomAttachedColor == null
                && bottomAttachedColor == null
                && shouldShowDivider == mShouldShowDivider) {
            return;
        }
        if (mBottomAttachedColor != null
                && mBottomAttachedColor.equals(bottomAttachedColor)
                && shouldShowDivider == mShouldShowDivider) {
            return;
        }
        mBottomAttachedColor = bottomAttachedColor;
        mShouldShowDivider = shouldShowDivider;
        for (Observer observer : mObservers) {
            observer.onBottomAttachedColorChanged(
                    mBottomAttachedColor, mShouldShowDivider, shouldDisableAnimation());
        }
    }

    private @Nullable @ColorInt Integer calculateBottomAttachedColor() {
        if (mAccessorySheetVisible && mAccessorySheetColor != null) {
            return mAccessorySheetColor;
        }
        if (mOmniboxSuggestionsVisible && mOmniboxSuggestionsColor != null) {
            return mOmniboxSuggestionsColor;
        }

        // A visible bottom toolbar should dictate the color even if there is a bottom sheet or
        // unexpanded overlay panel.
        boolean isBottomToolbarVisible =
                mBrowserControlsStateProvider.getControlsPosition() == ControlsPosition.BOTTOM
                        && !BrowserControlsUtils.areBrowserControlsOffScreen(
                                mBrowserControlsStateProvider);
        boolean isOverlayPanelUnexpanded =
                mOverlayPanelState != OverlayPanel.PanelState.EXPANDED
                        && mOverlayPanelState != OverlayPanel.PanelState.MAXIMIZED;
        if (isBottomToolbarVisible && mUseBottomControlsColor && isOverlayPanelUnexpanded) {
            return mBottomControlsColor;
        }
        if (shouldMatchBottomSheetColor()) {
            // This can cause a null return intentionally to indicate that a bottom sheet is showing
            // a page preview / web content.
            return mBottomSheetColor;
        }
        if (mOverlayPanelVisible
                && (mOverlayPanelStateProvider.isFullWidthSizePanel()
                        || !EdgeToEdgeUtils.isChromeEdgeToEdgeFeatureEnabled())) {
            // Return null if the overlay panel is visible but not peeked - the overlay panel's
            // content will be "bottom attached".
            return mOverlayPanelState == PanelState.PEEKED ? mOverlayPanelColor : null;
        }
        if (mUseBottomControlsColor) {
            return mBottomControlsColor;
        }
        if (mKeyboardAccessoryVisible) {
            return mKeyboardAccessoryColor;
        }
        if (mSnackbarVisible) {
            return mSnackbarColor;
        }
        return null;
    }

    /** The divider should be visible for partial width bottom-attached UI. */
    private boolean shouldShowDivider() {
        if (shouldMatchBottomSheetColor()) {
            return !mBottomSheetController.isFullWidth();
        }
        if (mOverlayPanelVisible && !EdgeToEdgeUtils.isChromeEdgeToEdgeFeatureEnabled()) {
            return !mOverlayPanelStateProvider.isFullWidthSizePanel();
        }
        if (mSnackbarVisible) {
            return !mSnackbarStateProvider.isFullWidth();
        }
        return false;
    }

    /** In certain cases, the animation should be disabled for better visual polish. */
    private boolean shouldDisableAnimation() {
        // The accessory sheet shows after the keyboard has already covered over the bottom UI -
        // animation here would look odd since the previous color is outdated.
        if (mAccessorySheetVisible) {
            return true;
        }

        //  For bottom-anchored UI, we should disable animations on appearance and enable
        // animations on disappearance.
        if (ChromeFeatureList.sNavBarColorAnimation.isEnabled()) {
            // Checks for bottom controls such as bottom tab group tool bar and read aloud mini
            // player.
            boolean nonBottomChinBottomControlsVisible =
                    mBottomControlsHeight > 1
                            && mBottomControlsStacker.hasVisibleLayersOtherThan(
                                    BottomControlsStacker.LayerType.BOTTOM_CHIN);

            // Disable animations on tab group toolbar appearance (toolbar visible false -> true).
            // Enable animations on tab group toolbar disappearance (toolbar visible true -> false).
            // We still want to enable animations when scrolling on/off (toolbar visible false
            // -> false or true -> true).
            boolean disableAnimationsTabGroupToolbar =
                    !mNonBottomChinBottomControlsVisible && nonBottomChinBottomControlsVisible;
            mNonBottomChinBottomControlsVisible = nonBottomChinBottomControlsVisible;

            if (disableAnimationsTabGroupToolbar) {
                return true;
            }

            boolean isBottomToolbarVisible =
                    mBrowserControlsStateProvider.getControlsPosition() == ControlsPosition.BOTTOM
                            && !BrowserControlsUtils.areBrowserControlsOffScreen(
                                    mBrowserControlsStateProvider);

            if (isBottomToolbarVisible) {
                return true;
            }

            if (mBottomSheetVisible) {
                return true;
            }

            if (mOverlayPanelVisible) {
                return true;
            }

            if (mKeyboardAccessoryVisible) {
                return true;
            }

            if (mSnackbarVisible) {
                return true;
            }
        }

        return false;
    }

    private boolean shouldMatchBottomSheetColor() {
        if (!mBottomSheetVisible) return false;

        if (mBottomSheetController.isAnchoredToBottomControls()) {
            // As long as the bottom sheet is anchored to the browser controls, match the sheet's
            // color when there's no other browser controls layer other than the bottom chin.
            // Bottom sheet's width setting does not matter in this case.
            return !mBottomControlsStacker.hasVisibleLayersOtherThan(LayerType.BOTTOM_CHIN);
        } else {
            // When using bottom chin, the chin is covered by the sheet so sheet color could should
            // not be used in partial width. When sheet is in full width, it covers the chin. So the
            // chin's color is not impacted by the bottom sheet in any width setting. When the
            // bottom chin is not in use, the sheet is attached to the nav bar directly, so bottom
            // sheet color should be used.
            return !mBottomControlsStacker.isLayerVisible(LayerType.BOTTOM_CHIN);
        }
    }

    // Browser Controls (Tab group UI, Read Aloud)

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            boolean topControlsMinHeightChanged,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean bottomControlsMinHeightChanged,
            boolean requestNewFrame,
            boolean isVisibilityForced) {
        boolean hasOtherVisibleBottomControls =
                // MiniPlayerMediator#shrinkBottomControls() sets the height to 1 and minHeight to 0
                // when hiding, instead of setting the height to 0.
                // TODO(b/320750931): Clean up once the MiniPlayerMediator has been improved.
                mBottomControlsHeight > 1
                        && mBottomControlsStacker.hasVisibleLayersOtherThan(
                                BottomControlsStacker.LayerType.BOTTOM_CHIN);

        if (!hasOtherVisibleBottomControls) {
            updateUseBottomControlsColor(false);
            return;
        }

        boolean useBrowserControlsColor = bottomOffset < mBottomControlsHeight;

        // When bottom chin constraint exists, the chin will have the same coloring mechanism as
        // the OS navigation bar as if E2E is disabled.
        if (EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.EDGE_TO_EDGE_SAFE_AREA_CONSTRAINT)) {
            boolean hasScrollablePortion =
                    bottomOffset < mBottomControlsHeight - mBottomControlsMinHeight;
            boolean chinNotScrollable =
                    mBottomControlsStacker.isLayerNonScrollable(LayerType.BOTTOM_CHIN);
            boolean hasOtherNonScrollableLayer =
                    mBottomControlsStacker.hasMultipleNonScrollableLayer();
            boolean hasFixedBrowserControlsAttached =
                    chinNotScrollable && hasOtherNonScrollableLayer;

            useBrowserControlsColor = hasScrollablePortion || hasFixedBrowserControlsAttached;
        }

        updateUseBottomControlsColor(useBrowserControlsColor);
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        mBottomControlsHeight = bottomControlsHeight;
        mBottomControlsMinHeight = bottomControlsMinHeight;

        // MiniPlayerMediator#shrinkBottomControls() sets the height to 1 and minHeight to 0 when
        // hiding, instead of setting the height to 0.
        // TODO(b/320750931): Clean up once the MiniPlayerMediator has been improved.
        updateUseBottomControlsColor(
                mBottomControlsHeight > 1
                        && mBottomControlsStacker.hasVisibleLayersOtherThan(
                                BottomControlsStacker.LayerType.BOTTOM_CHIN));

        // BottomChin constraint does not impact this method, since when control's height changes,
        // #hasVisibleLayersOtherThan(BOTTOM_CHIN) already covers whether bottom chin will have
        // a colored layer attached.
    }

    @Override
    public void onBottomControlsBackgroundColorChanged(@ColorInt int color) {
        mBottomControlsColor = color;
        updateBottomAttachedColor();
    }

    private void updateUseBottomControlsColor(boolean useBottomControlsColor) {
        if (useBottomControlsColor == mUseBottomControlsColor) {
            return;
        }
        mUseBottomControlsColor = useBottomControlsColor;
        updateBottomAttachedColor();
    }

    // Snackbar

    @Override
    public void onSnackbarStateChanged(boolean isShowing, Integer color) {
        mSnackbarVisible = isShowing;
        mSnackbarColor = color;
        updateBottomAttachedColor();
    }

    // Overlay Panel

    @Override
    public void onOverlayPanelStateChanged(@OverlayPanel.PanelState int state, int color) {
        mOverlayPanelColor = color;
        mOverlayPanelVisible =
                (state == OverlayPanel.PanelState.PEEKED)
                        || (state == OverlayPanel.PanelState.EXPANDED)
                        || (state == OverlayPanel.PanelState.MAXIMIZED);
        mOverlayPanelState = state;
        updateBottomAttachedColor();
    }

    // Bottom sheet

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        mBottomSheetVisible = false;
        updateBottomAttachedColor();
    }

    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        mBottomSheetVisible = true;
        updateBottomAttachedColor();
    }

    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {
        if (newContent != null) {
            mBottomSheetColor = newContent.getBackgroundColor();
        }
        updateBottomAttachedColor();
    }

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {}

    @Override
    public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {}

    // Omnibox Suggestions

    @Override
    public void onOmniboxSessionStateChange(boolean visible) {
        mOmniboxSuggestionsVisible = visible;
        updateBottomAttachedColor();
    }

    @Override
    public void onOmniboxSuggestionsBackgroundColorChanged(int color) {
        mOmniboxSuggestionsColor = color;
        updateBottomAttachedColor();
    }

    // InsetObserver.WindowInsetObserver

    @Override
    public void onInsetChanged() {
        checkIfBottomNavbarIsPresent();
    }

    /**
     * Observe for changes to the navbar insets - in some situations, the presence of the navbar at
     * the bottom may change (e.g. the 3-button navbar may move from the bottom to the left or right
     * after an orientation change).
     */
    private void checkIfBottomNavbarIsPresent() {
        WindowInsetsCompat windowInsets = mInsetObserver.getLastRawWindowInsets();
        if (windowInsets != null) {
            boolean bottomNavbarPresent =
                    windowInsets.getInsets(WindowInsetsCompat.Type.navigationBars()).bottom > 0;
            if (mBottomNavbarPresent != bottomNavbarPresent) {
                mBottomNavbarPresent = bottomNavbarPresent;
                updateBottomAttachedColor();
            }
        }
    }

    // KeyboardAccessoryVisualStateProvider.Observer

    @Override
    public void onKeyboardAccessoryVisualStateChanged(boolean visible, @ColorInt int color) {
        mKeyboardAccessoryVisible = visible;
        mKeyboardAccessoryColor = color;
        updateBottomAttachedColor();
    }

    // AccessorySheetVisualStateProvider.Observer

    @Override
    public void onAccessorySheetStateChanged(boolean visible, @ColorInt int color) {
        mAccessorySheetVisible = visible;
        mAccessorySheetColor = color;
        updateBottomAttachedColor();
    }
}
