// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelInflater;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.RelatedSearchesSectionHost;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchUma;
import org.chromium.chrome.browser.contextualsearch.RelatedSearchesUma;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.browser_ui.widget.chips.ChipsCoordinator;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

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
     * In the carousel UI, the first chip is the default search, so the related search start from
     * the index 1.
     */
    public static final int INDEX_OF_THE_FIRST_RELATED_SEARCHES = 1;

    /** The Android {@link Context} used to inflate the View. */
    private final Context mContext;

    /** The container View used to inflate the View. */
    private final ViewGroup mViewContainer;

    /** The resource loader that will handle the snapshot capturing. */
    private final DynamicResourceLoader mResourceLoader;

    private final OverlayPanel mOverlayPanel;

    /** The pixel density. */
    private final float mDpToPx;

    /** The chip models being displayed for related searches. */
    private final ModelList mChips;

    /**
     * The inflated View, or {@code null} if the associated Feature is not enabled,
     * or {@link #destroy} has been called.
     */
    @Nullable private RelatedSearchesControlView mControlView;

    /** The query suggestions for this feature, or {@code null} if we don't have any. */
    private @Nullable List<String> mRelatedSearchesSuggestions;

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

    /** Whether the carousel is scrolled. */
    private boolean mScrolled;

    /**
     * @param panel             The panel.
     * @param panelSectionHost  A reference to the host of this panel section for notifications.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     */
    RelatedSearchesControl(
            OverlayPanel panel,
            RelatedSearchesSectionHost panelSectionHost,
            Context context,
            ViewGroup container,
            DynamicResourceLoader resourceLoader) {
        mContext = context;
        mViewContainer = container;
        mResourceLoader = resourceLoader;
        mDpToPx = context.getResources().getDisplayMetrics().density;
        mOverlayPanel = panel;
        mPanelSectionHost = panelSectionHost;
        mChips = new ModelList();
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
        return !mIsVisible || mControlView == null ? 0f : mHeightPx;
    }

    /** Returns the maximum height of the control. */
    float getMaximumHeightPx() {
        return mContentHeightPx;
    }

    /**
     * Returns the amount of padding that is redundant between the Related Searches carousel that is
     * shown in the Bar with the content above it. The content above has its own padding that
     * provides a space between it and the bottom of the Bar. So when the Bar grows to include the
     * Related Searches (which has its own padding above and below) there is redundant padding.
     * @return The amount of overlap of padding values that can be removed (in pixels).
     */
    public float getRedundantPadding() {
        return !mIsVisible || mControlView == null
                ? 0f
                : mContext.getResources()
                        .getDimension(R.dimen.related_searches_in_bar_redundant_padding);
    }

    /** Returns the ID of the view, or {@code INVALID_VIEW_ID} if there's no view. */
    public int getViewId() {
        return mControlView != null ? mControlView.getViewId() : INVALID_VIEW_ID;
    }

    /** Returns whether the SERP is showing due to a Related Searches suggestion. */
    public boolean isShowingRelatedSearchSerp() {
        return mSelectedChip >= INDEX_OF_THE_FIRST_RELATED_SEARCHES;
    }

    // ============================================================================================
    // Package-private API
    // ============================================================================================

    /** Shows the View. This includes inflating the View and setting its initial state. */
    void show() {
        if (mIsVisible || !hasReleatedSearchesToShow()) return;

        // Invalidates the View in order to generate a snapshot, but do not show the View yet.
        if (mControlView != null) mControlView.invalidate();

        mIsVisible = true;
        mHeightPx = mContentHeightPx;
    }

    /** Hides the View */
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
        if (mControlView == null) {
            mControlView =
                    new RelatedSearchesControlView(
                            mOverlayPanel,
                            mContext,
                            mViewContainer,
                            mResourceLoader,
                            R.layout.contextual_search_related_searches_view,
                            R.id.contextual_search_related_searches_view_id,
                            R.id.contextual_search_related_searches_view_control_id);
        }
        assert mChipsSelected == 0 || hasReleatedSearchesToShow();
        mRelatedSearchesSuggestions = relatedSearches;
        mChips.clear();
        if (hasReleatedSearchesToShow()) {
            show();
        } else {
            hide();
        }
        calculateHeight();
        mSelectedChip = NO_SELECTED_CHIP;
    }

    void onPanelCollapsing() {
        clearSelectedSuggestions();
    }

    /** Un-selects any currently selected chip. */
    void clearSelectedSuggestions() {
        if (mSelectedChip != NO_SELECTED_CHIP) {
            mChips.get(mSelectedChip).model.set(ChipProperties.SELECTED, false);
        }
        mSelectedChip = NO_SELECTED_CHIP;
    }

    /** Returns whether we have Related Searches to show or not.  */
    boolean hasReleatedSearchesToShow() {
        return mRelatedSearchesSuggestions != null && mRelatedSearchesSuggestions.size() > 0;
    }

    @VisibleForTesting
    @Nullable
    List<String> getRelatedSearchesSuggestions() {
        return mRelatedSearchesSuggestions;
    }

    // ============================================================================================
    // Test support API
    // ============================================================================================

    public void selectChipForTest(int chipIndex) {
        handleChipTapped(mChips.get(chipIndex).model);
    }

    public ModelList getChipsForTest() {
        return mControlView.getChipsForTest(); // IN-TEST
    }

    public int getSelectedChipForTest() {
        return mSelectedChip;
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

        showView(true);
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

    /** Destroys as much of this instance as possible. */
    void destroy() {
        if (hasReleatedSearchesToShow()) {
            RelatedSearchesUma.logNumberOfSuggestionsClicked(mChipsSelected);
            if (mDidShowAnySuggestions) RelatedSearchesUma.logCtr(mChipsSelected > 0);
        }

        if (mControlView != null) {
            if (mDidShowAnySuggestions) {
                RelatedSearchesUma.logCarouselScrolled(mScrolled);
                RelatedSearchesUma.logCarouselScrollAndClickStatus(mScrolled, mChipsSelected > 0);
            }
            mControlView.destroy();
            mControlView = null;
        }
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
            showView(false);
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
            mHeightPx =
                    Math.round(
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
     * @param fromCloseToPeek Whether the view is shown from states Closed to Peeked.
     */
    private void showView(boolean fromCloseToPeek) {
        if (mControlView == null) return;

        float y = mPanelSectionHost.getYPositionPx();
        View view = mControlView.getView();
        if (view == null || !mIsVisible || (mIsShowingView && mViewY == y) || mHeightPx == 0.f) {
            return;
        }

        float offsetX = mOverlayPanel.getOffsetX() * mDpToPx;
        if (LocalizationUtils.isLayoutRtl()) {
            offsetX = -offsetX;
        }

        if (mSelectedChip == NO_SELECTED_CHIP && !fromCloseToPeek) {
            mSelectedChip = 0;
            mChips.get(mSelectedChip).model.set(ChipProperties.SELECTED, true);
        }

        view.setTranslationX(offsetX);
        view.setTranslationY(y - mOverlayPanel.getBarMarginBottomPx());
        view.setVisibility(View.VISIBLE);

        // NOTE: We need to call requestLayout, otherwise the View will not become visible.
        ViewUtils.requestLayout(view, "RelatedSearchesControl.showView");

        mIsShowingView = true;
        mViewY = y;
    }

    /** Hides the Android View. See {@link #showView()}. */
    private void hideView() {
        if (mControlView == null) return;

        View view = mControlView.getView();
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
     * Notifies that the user has clicked on a suggestion in this section of the panel.
     * See https://crbug.com/1216593.
     * @param suggestionIndex The 0-based index into the list of suggestions provided by the panel
     *        and presented in the UI. E.g. if the user clicked the second chip this
     *        value would be 1.
     */
    private void onSuggestionClicked(int suggestionIndex) {
        mPanelSectionHost.onSuggestionClicked(suggestionIndex);
        // TODO(donnd): add infrastructure to check if the suggestion is an RS before logging.
        RelatedSearchesUma.logSelectedCarouselIndex(suggestionIndex);
        RelatedSearchesUma.logSelectedSuggestionIndex(suggestionIndex);
        mChipsSelected++;
        boolean isRelatedSearchesSuggestion =
                suggestionIndex >= INDEX_OF_THE_FIRST_RELATED_SEARCHES;
        ContextualSearchUma.logAllSearches(isRelatedSearchesSuggestion);

        mControlView.smoothScrollToPosition(suggestionIndex);
    }

    // ============================================================================================
    // Chips
    // ============================================================================================

    public void updateChips() {
        Callback<PropertyModel> selectedCallback = (model) -> handleChipTapped(model);

        if (mChips.size() == 0 && hasReleatedSearchesToShow()) {
            for (String suggestion : mRelatedSearchesSuggestions) {
                final int index = mChips.size();
                ListItem chip =
                        ChipsCoordinator.buildChipListItem(index, suggestion, selectedCallback);

                if (index < INDEX_OF_THE_FIRST_RELATED_SEARCHES) {
                    chip.model.set(
                            ChipProperties.TEXT_MAX_WIDTH_PX,
                            mContext.getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.contextual_search_chip_max_width));
                }
                mChips.add(chip);
            }
            mDidShowAnySuggestions = true;
        }
    }

    private void handleChipTapped(PropertyModel tappedChip) {
        if (mControlView == null) return;

        onSuggestionClicked(tappedChip.get(ChipProperties.ID));
        if (mSelectedChip != NO_SELECTED_CHIP) {
            mChips.get(mSelectedChip).model.set(ChipProperties.SELECTED, false);
        }
        mSelectedChip = tappedChip.get(ChipProperties.ID);
        tappedChip.set(ChipProperties.SELECTED, true);
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
        // TODO(donnd): track the offset of the carousel here, so we can use it for snapshotting
        // and log that the user has scrolled it.
        private float mLastOffset;
        private final int mControlId;

        /**
         * Constructs a view that can be shown in the panel.
         * @param panel             The panel.
         * @param context           The Android Context used to inflate the View.
         * @param container         The container View used to inflate the View.
         * @param resourceLoader    The resource loader that will handle the snapshot capturing.
         * @param layoutId          The XML Layout that declares the View.
         * @param viewId            The id of the root View of the Layout.
         * @param controlId         The id of the control View.
         */
        RelatedSearchesControlView(
                OverlayPanel panel,
                Context context,
                ViewGroup container,
                DynamicResourceLoader resourceLoader,
                int layoutId,
                int viewId,
                int controlId) {
            super(panel, layoutId, viewId, context, container, resourceLoader);
            mControlId = controlId;

            // Setup Chips handling
            mChipsCoordinator = new ChipsCoordinator(context, mChips);
            mChipsCoordinator.setSpaceItemDecoration(
                    context.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.contextual_search_chip_list_chip_spacing),
                    context.getResources()
                            .getDimensionPixelSize(
                                    R.dimen.contextual_search_chip_list_side_padding));

            RecyclerView recyclerView = (RecyclerView) mChipsCoordinator.getView();
            recyclerView.addOnScrollListener(
                    new OnScrollListener() {
                        @Override
                        public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                            if (newState == RecyclerView.SCROLL_STATE_DRAGGING) mScrolled = true;
                            if (newState == RecyclerView.SCROLL_STATE_IDLE) invalidate(false);
                        }
                    });
        }

        /**
         * Smoothly scroll to the view in the position.
         * @param position the position of the view to scroll to.
         */
        private void smoothScrollToPosition(int position) {
            RecyclerView recyclerView = (RecyclerView) mChipsCoordinator.getView();
            recyclerView.smoothScrollToPosition(position);
        }

        /** Returns the view for this control. */
        View getControlView() {
            View view = getView();
            if (view == null) return null;

            return view.findViewById(mControlId);
        }

        @Override
        public View getView() {
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
            updateChips();
            View coordinatorView = mChipsCoordinator.getView();
            if (coordinatorView == null) return;

            // Put the chip coordinator view into our control view.
            ViewGroup parent = (ViewGroup) coordinatorView.getParent();
            if (parent != null) parent.removeView(coordinatorView);
            relatedSearchesViewGroup.addView(coordinatorView);
            invalidate(false);

            // Log carousel visible item position
            RecyclerView recyclerView = (RecyclerView) mChipsCoordinator.getView();
            LinearLayoutManager layoutManager =
                    (LinearLayoutManager) recyclerView.getLayoutManager();
            int lastVisibleItemPosition = layoutManager.findLastVisibleItemPosition();
            if (lastVisibleItemPosition != RecyclerView.NO_POSITION) {
                RelatedSearchesUma.logCarouselLastVisibleItemPosition(lastVisibleItemPosition);
            }
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

            calculateHeight();
        }

        @Override
        protected boolean shouldDetachViewAfterCapturing() {
            return false;
        }

        ModelList getChipsForTest() {
            updateChips();
            return mChips;
        }
    }
}
