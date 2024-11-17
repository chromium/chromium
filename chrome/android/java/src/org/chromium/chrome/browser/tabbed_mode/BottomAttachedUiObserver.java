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
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelStateProvider;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.keyboard_accessory.AccessorySheetVisualStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsVisualState;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarStateProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.InsetObserver;

import java.util.Optional;

/**
 * An observer class that listens for changes in UI components that are attached to the bottom of
 * the screen, bordering the OS navigation bar. This class then aggregates that information and
 * notifies its own observers of properties of the UI currently bordering ("attached to") the
 * navigation bar.
 */
public class BottomAttachedUiObserver
        implements BrowserControlsStateProvider.Observer,
                SnackbarStateProvider.Observer,
                OverlayPanelStateProvider.Observer,
                BottomSheetObserver,
                AutocompleteCoordinator.OmniboxSuggestionsVisualStateObserver,
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
    private @Nullable @ColorInt Integer mBottomControlsColor;
    private boolean mUseBottomControlsColor;

    private final BottomControlsStacker mBottomControlsStacker;

    private final SnackbarStateProvider mSnackbarStateProvider;
    private @Nullable @ColorInt Integer mSnackbarColor;
    private boolean mSnackbarVisible;

    private OverlayPanelStateProvider mOverlayPanelStateProvider;
    private @Nullable @ColorInt Integer mOverlayPanelColor;
    private boolean mOverlayPanelVisible;
    private boolean mOverlayPanelPeeked;

    private Optional<OmniboxSuggestionsVisualState> mOmniboxSuggestionsVisualState;
    private boolean mOmniboxSuggestionsVisible;
    private @Nullable @ColorInt Integer mOmniboxSuggestionsColor;

    private final InsetObserver mInsetObserver;

    private ObservableSupplier<AccessorySheetVisualStateProvider>
            mAccessorySheetVisualStateProviderSupplier;
    private Callback<AccessorySheetVisualStateProvider> mAccessorySheetProviderSupplierObserver;
    private AccessorySheetVisualStateProvider mAccessorySheetVisualStateProvider;
    private boolean mAccessorySheetVisible;
    private @Nullable @ColorInt Integer mAccessorySheetColor;

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
     * @param accessorySheetVisualStateProviderSupplier Supplies an {@link
     *     AccessorySheetVisualStateProvider} to watch for visual changes to the keyboard accessory
     *     sheet.
     * @param insetObserver An {@link InsetObserver} to listen for changes to the window insets.
     */
    public BottomAttachedUiObserver(
            @NonNull BottomControlsStacker bottomControlsStacker,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull SnackbarStateProvider snackbarStateProvider,
            @NonNull ObservableSupplier<ContextualSearchManager> contextualSearchManagerSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull Optional<OmniboxSuggestionsVisualState> omniboxSuggestionsVisualState,
            @NonNull
                    ObservableSupplier<AccessorySheetVisualStateProvider>
                            accessorySheetVisualStateProviderSupplier,
            InsetObserver insetObserver) {
        mObservers = new ObserverList<>();

        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);
        mBottomControlsStacker = bottomControlsStacker;

        mSnackbarStateProvider = snackbarStateProvider;
        mSnackbarStateProvider.addObserver(this);

        mBottomSheetController = bottomSheetController;
        mBottomSheetController.addObserver(this);

        mInsetObserver = insetObserver;
        mInsetObserver.addObserver(this);
        checkIfBottomNavbarIsPresent();

        mAccessorySheetVisualStateProviderSupplier = accessorySheetVisualStateProviderSupplier;
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
        // If drawing edge-to-edge only match the bottom sheet color if the bottom sheet extends
        // across the full width. Since the bottom sheet shows in the front, if it doesn't extend
        // across the entire width, it looks nicer to match the color of other components behind /
        // to the side of the bottom sheet.
        if (mBottomSheetVisible
                && (mBottomSheetController.isFullWidth() || !EdgeToEdgeUtils.isEnabled())) {
            // This can cause a null return intentionally to indicate that a bottom sheet is showing
            // a page preview / web content.
            return mBottomSheetColor;
        }
        if (mOverlayPanelVisible
                && (mOverlayPanelStateProvider.isFullWidthSizePanel()
                        || !EdgeToEdgeUtils.isEnabled())) {
            // Return null if the overlay panel is visible but not peeked - the overlay panel's
            // content will be "bottom attached".
            return mOverlayPanelPeeked ? mOverlayPanelColor : null;
        }
        if (mUseBottomControlsColor) {
            return mBottomControlsColor;
        }
        if (mSnackbarVisible) {
            return mSnackbarColor;
        }
        return null;
    }

    /** The divider should be visible for partial width bottom-attached UI. */
    private boolean shouldShowDivider() {
        if (mBottomSheetVisible) {
            return !mBottomSheetController.isFullWidth() && !EdgeToEdgeUtils.isEnabled();
        }
        if (mOverlayPanelVisible && !EdgeToEdgeUtils.isEnabled()) {
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
        return mAccessorySheetVisible;
    }

    // Browser Controls (Tab group UI, Read Aloud)

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
            boolean isVisibilityForced) {
        updateBrowserControlsVisibility(
                // MiniPlayerMediator#shrinkBottomControls() sets the height to 1 and minHeight to 0
                // when hiding, instead of setting the height to 0.
                // TODO(b/320750931): Clean up once the MiniPlayerMediator has been improved.
                mBottomControlsHeight > 1
                        && bottomOffset < mBottomControlsHeight
                        && mBottomControlsStacker.hasVisibleLayersOtherThan(
                                BottomControlsStacker.LayerType.BOTTOM_CHIN));
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        mBottomControlsHeight = bottomControlsHeight;

        // MiniPlayerMediator#shrinkBottomControls() sets the height to 1 and minHeight to 0 when
        // hiding, instead of setting the height to 0.
        // TODO(b/320750931): Clean up once the MiniPlayerMediator has been improved.
        updateBrowserControlsVisibility(
                mBottomControlsHeight > 1
                        && mBottomControlsStacker.hasVisibleLayersOtherThan(
                                BottomControlsStacker.LayerType.BOTTOM_CHIN));
    }

    @Override
    public void onBottomControlsBackgroundColorChanged(@ColorInt int color) {
        mBottomControlsColor = color;
        updateBottomAttachedColor();
    }

    private void updateBrowserControlsVisibility(boolean useBottomControlsColor) {
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
        mOverlayPanelPeeked = (state == OverlayPanel.PanelState.PEEKED);
        updateBottomAttachedColor();
    }

    // Bottom sheet

    @Override
    public void onSheetClosed(int reason) {
        mBottomSheetVisible = false;
        updateBottomAttachedColor();
    }

    @Override
    public void onSheetOpened(int reason) {
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
    public void onSheetStateChanged(int newState, int reason) {}

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
    public void onInsetChanged(int left, int top, int right, int bottom) {
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

    // AccessorySheetVisualStateProvider.Observer

    @Override
    public void onAccessorySheetStateChanged(boolean visible, @ColorInt int color) {
        mAccessorySheetVisible = visible;
        mAccessorySheetColor = color;
        updateBottomAttachedColor();
    }
}
