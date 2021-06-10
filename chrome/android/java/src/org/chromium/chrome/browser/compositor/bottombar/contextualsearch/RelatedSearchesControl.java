// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelInflater;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.RelatedSearchesSectionHost;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchUma;
import org.chromium.chrome.browser.contextualsearch.RelatedSearchesUma;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.chips.Chip;
import org.chromium.components.browser_ui.widget.chips.ChipsCoordinator;
import org.chromium.components.browser_ui.widget.chips.ChipsProvider;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.widget.ChipView;

import java.util.ArrayList;
import java.util.List;

/**
 * Controls a section of the Panel that provides Related Searches chips.
 * This class is implemented along the lines of {@code ContextualSearchPanelHelp}.
 * TODO(donnd): consider further extracting the common pattern used for this class
 * and the Help and Promo classes.
 */
public class RelatedSearchesControl {
    private static final int INVALID_VIEW_ID = 0;
    private static final int NO_SELECTED_CHIP = -1;

    /**
     * Whether the UI is displaying the TTS query that's based on the selection
     * in the first position of the RS carousel. See https://crbug.com/1216593.
     */
    private static final boolean DO_DISPLAY_DEFAULT_SELECTION_QUERY = false;
    /** The position of the first Related Searches suggestion in the carousel UI. */
    private static final int FIRST_RELATED_SEARCHES_CAROUSEL_INDEX =
            DO_DISPLAY_DEFAULT_SELECTION_QUERY ? 1 : 0;

    private final boolean mIsEnabled;
    private final OverlayPanel mOverlayPanel;
    private final Context mContext;

    /** The pixel density. */
    private final float mDpToPx;

    /**
     * The inflated View, or {@code null} if the associated Feature is not enabled,
     * or {@link #destroy} has been called.
     */
    @Nullable
    private RelatedSearchesControlView mControlView;

    /** The query suggestions for this feature, or {@code null} if we don't have any. */
    private @Nullable List<String> mRelatedSearchesSuggestions;
    private List<Chip> mChips;

    /** Whether the view is visible. */
    private boolean mIsVisible;

    /** The height of the view in pixels. */
    private float mHeightPx;

    /** The height of the content in pixels. */
    private float mContentHeightPx;

    /** Whether the view is showing. */
    private boolean mIsShowingView;

    /** The Y position of the view. */
    private float mViewY;

    /** The reference to the host for this part of the panel (callbacks to the Panel). */
    private RelatedSearchesSectionHost mPanelSectionHost;

    /** The number of chips that have been selected so far. */
    private int mChipsSelected;

    /** Whether any suggestions were actually shown to the user. */
    private boolean mDidShowAnySuggestions;

    /** Which chip is selected (if any) */
    private int mSelectedChip = NO_SELECTED_CHIP;

    /**
     * @param panel             The panel.
     * @param panelSectionHost  A reference to the host of this panel section for notifications.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     */
    RelatedSearchesControl(OverlayPanel panel, RelatedSearchesSectionHost panelSectionHost,
            Context context, ViewGroup container, DynamicResourceLoader resourceLoader) {
        mIsEnabled = ChromeFeatureList.isEnabled(ChromeFeatureList.RELATED_SEARCHES);
        mDpToPx = context.getResources().getDisplayMetrics().density;
        mOverlayPanel = panel;
        mContext = context;

        mControlView = mIsEnabled
                ? new RelatedSearchesControlView(panel, context, container, resourceLoader)
                : null;
        mPanelSectionHost = panelSectionHost;
        // mChipsSelected is default-initialized to 0.
    }

    // ============================================================================================
    // Public API
    // ============================================================================================

    /**
     * @return Whether the View is visible.
     */
    public boolean isVisible() {
        return mIsVisible;
    }

    /**
     * @return The View height in pixels.
     */
    public float getHeightPx() {
        return mIsVisible ? mHeightPx : 0f;
    }

    /** Returns the ID of the view, or {@code INVALID_VIEW_ID} if there's no view. */
    public int getViewId() {
        return mControlView != null ? mControlView.getViewId() : INVALID_VIEW_ID;
    }

    /** Returns whether the SERP is showing due to a Related Searches suggestion. */
    public boolean isShowingRelatedSearchSerp() {
        if (!mIsEnabled) return false;
        return mSelectedChip >= FIRST_RELATED_SEARCHES_CAROUSEL_INDEX;
    }

    // ============================================================================================
    // Package-private API
    // ============================================================================================

    /**
     * Shows the View. This includes inflating the View and setting its initial state.
     */
    void show() {
        if (mIsVisible || !hasReleatedSearchesToShow()) return;

        // Invalidates the View in order to generate a snapshot, but do not show the View yet.
        if (mControlView != null) mControlView.invalidate();

        mIsVisible = true;
        mHeightPx = mContentHeightPx;
    }

    /**
     * Hides the View
     */
    void hide() {
        if (!mIsVisible) return;

        hideView();

        mIsVisible = false;
        mHeightPx = 0.f;
    }

    /**
     * Sets the Related Searches suggestions to show in this view.
     * @param relatedSearches An {@code List} of suggested queries or {@code null} when none.
     */
    void setRelatedSearchesSuggestions(@Nullable List<String> relatedSearches) {
        mRelatedSearchesSuggestions = relatedSearches;
        mChips = null;
        show();
        calculateHeight();
        assert mChipsSelected == 0;
        mSelectedChip = NO_SELECTED_CHIP;
    }

    @VisibleForTesting
    @Nullable
    List<String> getRelatedSearchesSuggestions() {
        return mRelatedSearchesSuggestions;
    }

    // ============================================================================================
    // Test support API
    // ============================================================================================

    @VisibleForTesting
    public void selectChipForTest(int chipIndex) {
        mControlView.selectChipForTest(chipIndex);
    }

    // ============================================================================================
    // Panel Motion Handling
    // ============================================================================================

    /**
     * Interpolates the UI from states Closed to Peeked.
     *
     * @param percentage The completion percentage.
     */
    void onUpdateFromCloseToPeek(float percentage) {
        if (!isVisible()) return;

        // The View snapshot should be fully visible here.
        updateAppearance(1.0f);

        // The View should not be visible in this state.
        hideView();
    }

    /**
     * Interpolates the UI from states Peeked to Expanded.
     *
     * @param percentage The completion percentage.
     */
    void onUpdateFromPeekToExpand(float percentage) {
        updateViewAndNativeAppearance(percentage);
    }

    /**
     * Interpolates the UI from states Expanded to Maximized.
     *
     * @param percentage The completion percentage.
     */
    void onUpdateFromExpandToMaximize(float percentage) {
        updateViewAndNativeAppearance(percentage);
    }

    /**
     * Destroys as much of this instance as possible.
     */
    void destroy() {
        if (hasReleatedSearchesToShow()) {
            RelatedSearchesUma.logNumberOfSuggestionsClicked(mChipsSelected);
            if (mDidShowAnySuggestions) RelatedSearchesUma.logCtr(mChipsSelected > 0);
        }
        if (mControlView != null) mControlView.destroy();
        mControlView = null;
    }

    /** Invalidates the view. */
    void invalidate(boolean didViewSizeChange) {
        if (mControlView != null) mControlView.invalidate(didViewSizeChange);
    }

    /**
     * Updates the Android View's hidden state and the native appearance attributes.
     * This hides the Android View when transitioning between states and instead shows the
     * native snapshot (by setting the native attributes to do so).
     * @param percentage The completion percentage.
     */
    private void updateViewAndNativeAppearance(float percentage) {
        if (!isVisible()) return;

        // The panel stays full sized during this size transition.
        updateAppearance(1.0f);

        // We should show the View only when the Panel has reached the exact height.
        // If not exact then the non-interactive native code draws the Panel during the transition.
        if (percentage == 1.f) {
            showView();
        } else {
            // If the View is still showing, capture a new snapshot in case anything changed.
            // TODO(donnd): grab snapshots when the view changes instead.
            // See https://crbug.com/1184308 for details.
            if (mIsShowingView) {
                invalidate(false);
            }
            hideView();
        }
    }

    /**
     * Updates the appearance of the View.
     * @param percentage The completion percentage. 0.f means the view is fully collapsed and
     *        transparent. 1.f means the view is fully expanded and opaque.
     */
    private void updateAppearance(float percentage) {
        if (mIsVisible) {
            mHeightPx = Math.round(
                    MathUtils.clamp(percentage * mContentHeightPx, 0.f, mContentHeightPx));
        } else {
            mHeightPx = 0.f;
        }
    }

    // ============================================================================================
    // Helpers
    // ============================================================================================

    /**
     * Shows the Android View. By making the Android View visible, we are allowing the
     * View to be interactive. Since snapshots are not interactive (they are just a bitmap),
     * we need to temporarily show the Android View on top of the snapshot, so the user will
     * be able to click in the View button.
     */
    private void showView() {
        if (mControlView == null) return;

        float y = mPanelSectionHost.getYPositionPx();
        View view = mControlView.getControlView();
        if (view == null || !mIsVisible || (mIsShowingView && mViewY == y) || mHeightPx == 0.f) {
            return;
        }

        float offsetX = mOverlayPanel.getOffsetX() * mDpToPx;
        if (LocalizationUtils.isLayoutRtl()) {
            offsetX = -offsetX;
        }

        view.setTranslationX(offsetX);
        view.setTranslationY(y);
        view.setVisibility(View.VISIBLE);

        // NOTE: We need to call requestLayout, otherwise the View will not become visible.
        view.requestLayout();

        mIsShowingView = true;
        mViewY = y;
    }

    /**
     * Hides the Android View. See {@link #showView()}.
     */
    private void hideView() {
        if (mControlView == null) return;

        View view = mControlView.getControlView();
        if (view == null || !mIsVisible || !mIsShowingView) {
            return;
        }

        view.setVisibility(View.INVISIBLE);
        mIsShowingView = false;
    }

    /**
     * Calculates the content height of the View, and adjusts the height of the View while
     * preserving the proportion of the height with the content height. This should be called
     * whenever the the size of the View changes.
     */
    private void calculateHeight() {
        if (mControlView == null || !hasReleatedSearchesToShow()) return;

        mControlView.updateRelatedSearchesToShow();
        mControlView.layoutView();

        final float previousContentHeight = mContentHeightPx;
        mContentHeightPx = mControlView.getMeasuredHeight();

        if (mIsVisible) {
            // Calculates the ratio between the current height and the previous content height,
            // and uses it to calculate the new height, while preserving the ratio.
            final float ratio = mHeightPx / previousContentHeight;
            mHeightPx = Math.round(mContentHeightPx * ratio);
        }
    }

    /**
     * Notifies that the user has clicked on a suggestions in this section of the panel.
     * See https://crbug.com/1216593.
     * @param suggestionIndex The 0-based index into the list of suggestions provided by the panel
     *        and presented in the UI. E.g. if the user clicked the second chip this
     *        value would be 1.
     */
    private void onSuggestionClicked(int suggestionIndex) {
        mPanelSectionHost.onSuggestionClicked(suggestionIndex);
        RelatedSearchesUma.logSelectedCarouselIndex(suggestionIndex);
        // TODO(donnd): check the computation once we're showing the default query. That will
        // not need to be logged using the call below since it's not an RS suggestion.
        // See https://crbug.com/1216593.
        RelatedSearchesUma.logSelectedSuggestionIndex(
                suggestionIndex + (DO_DISPLAY_DEFAULT_SELECTION_QUERY ? 0 : 1));
        mChipsSelected++;
        ContextualSearchUma.logAllSearches(/* isRelatedSearches */ true);
    }

    /** Returns whether we have Related Searches to show or not.  */
    private boolean hasReleatedSearchesToShow() {
        return mIsEnabled && mRelatedSearchesSuggestions != null
                && mRelatedSearchesSuggestions.size() > 0;
    }

    // ============================================================================================
    // Chips
    // ============================================================================================

    private class RelatedSearchesChipsProvider implements ChipsProvider {
        private final ObserverList<Observer> mObservers = new ObserverList<>();

        @Override
        public void addObserver(Observer observer) {
            mObservers.addObserver(observer);
        }

        @Override
        public void removeObserver(Observer observer) {
            mObservers.removeObserver(observer);
        }

        @Override
        public List<Chip> getChips() {
            if (mChips == null) mChips = new ArrayList<>();
            if (mChips.size() == 0 && hasReleatedSearchesToShow()) {
                for (String suggestion : mRelatedSearchesSuggestions) {
                    final int index = mChips.size();
                    Chip chip = new Chip(index, suggestion, ChipView.INVALID_ICON_ID,
                            () -> handleChipTapped(index));
                    chip.enabled = true;
                    mChips.add(chip);
                }
                mDidShowAnySuggestions = true;
            }
            return mChips;
        }

        private void handleChipTapped(int tappedChipIndex) {
            onSuggestionClicked(tappedChipIndex);
            if (mSelectedChip != NO_SELECTED_CHIP) mChips.get(mSelectedChip).selected = false;
            mSelectedChip = tappedChipIndex;
            mChips.get(tappedChipIndex).selected = true;
            for (Observer observer : mObservers) observer.onChipsChanged();
        }

        @VisibleForTesting
        void selectChipForTest(int chipIndex) {
            handleChipTapped(chipIndex);
        }
    }

    // ============================================================================================
    // ControlView - the Android View that this class controls.
    // ============================================================================================

    /**
     * The {@code RelatedSearchesControlView} is an {@link OverlayPanelInflater} controlled view
     * that renders the actual View and can be created and destroyed under the control of the
     * enclosing Panel section class. The enclosing class delegates several public methods to this
     * class, e.g. {@link #invalidate}.
     */
    private class RelatedSearchesControlView extends OverlayPanelInflater {
        private final ChipsCoordinator mChipsCoordinator;
        private final RelatedSearchesChipsProvider mChipsProvider;
        // TODO(donnd): track the offset of the carousel here, so we can use it for snapshotting
        // and log that the user has scrolled it.
        private float mLastOffset;

        /**
         * Constructs a view that can be shown in the panel.
         * @param panel             The panel.
         * @param context           The Android Context used to inflate the View.
         * @param container         The container View used to inflate the View.
         * @param resourceLoader    The resource loader that will handle the snapshot capturing.
         */
        RelatedSearchesControlView(OverlayPanel panel, Context context, ViewGroup container,
                DynamicResourceLoader resourceLoader) {
            super(panel, R.layout.contextual_search_related_searches_view,
                    R.id.contextual_search_related_searches_layout, context, container,
                    resourceLoader);

            // Setup Chips handling
            mChipsProvider = new RelatedSearchesChipsProvider();
            mChipsCoordinator = new ChipsCoordinator(context, mChipsProvider);
        }

        /** Returns the view for this control. */
        View getControlView() {
            return super.getView();
        }

        /** Triggers a view layout. */
        void layoutView() {
            super.layout();
        }

        /** Tells the control to update the Related Searches to display in the view. */
        void updateRelatedSearchesToShow() {
            View relatedSearchesView = getControlView();
            ViewGroup relatedSearchesViewGroup = (ViewGroup) relatedSearchesView;
            if (relatedSearchesViewGroup == null) return;

            // Notify the coordinator that the chips need to change
            mChipsCoordinator.onChipsChanged();
            View coordinatorView = mChipsCoordinator.getView();
            if (coordinatorView == null) return;

            ViewGroup parent = (ViewGroup) coordinatorView.getParent();
            if (parent != null) parent.removeView(coordinatorView);
            relatedSearchesViewGroup.addView(coordinatorView);
            invalidate(false);
        }

        @Override
        public void destroy() {
            hide();
            super.destroy();
        }

        @Override
        public void invalidate(boolean didViewSizeChange) {
            super.invalidate(didViewSizeChange);

            if (didViewSizeChange) {
                calculateHeight();
            }
        }

        @Override
        protected void onFinishInflate() {
            super.onFinishInflate();

            View view = getControlView();

            calculateHeight();
        }

        @Override
        protected boolean shouldDetachViewAfterCapturing() {
            return false;
        }

        @VisibleForTesting
        void selectChipForTest(int chipIndex) {
            mChipsProvider.selectChipForTest(chipIndex);
        }
    }
}
