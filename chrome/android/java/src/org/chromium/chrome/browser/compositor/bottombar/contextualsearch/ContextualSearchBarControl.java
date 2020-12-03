// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelAnimation;
import org.chromium.chrome.browser.contextualsearch.QuickActionCategory;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Controls the Search Bar in the Contextual Search Panel.
 * This class holds instances of its subcomponents such as the main text, caption, icon
 * and interaction controls such as the close box.
 */
public class ContextualSearchBarControl {
    /** Full opacity -- fully visible. */
    private static final float FULL_OPACITY = 1.0f;

    /** Transparent opacity -- completely transparent (not visible). */
    private static final float TRANSPARENT_OPACITY = 0.0f;

    /**
     * The panel used to get information about the panel layout.
     */
    protected ContextualSearchPanel mContextualSearchPanel;

    /**
     * The {@link ContextualSearchContextControl} used to control the Search Context View.
     */
    private final ContextualSearchContextControl mContextControl;

    /**
     * The {@link ContextualSearchTermControl} used to control the Search Term View.
     */
    private final ContextualSearchTermControl mSearchTermControl;

    /**
    * The {@link ContextualSearchCaptionControl} used to control the Caption View.
    */
    private final ContextualSearchCaptionControl mCaptionControl;

    /**
     * The {@link ContextualSearchQuickActionControl} used to control quick action behavior.
     */
    private final ContextualSearchQuickActionControl mQuickActionControl;

    /**
     * The {@link ContextualSearchCardIconControl} used to control icons for non-action Cards
     * returned by the server.
     */
    private final ContextualSearchCardIconControl mCardIconControl;

    /** The width of our icon, including padding, in pixels. */
    private final float mPaddedIconWidthPx;

    /**
     * The {@link ContextualSearchImageControl} for the panel.
     */
    private ContextualSearchImageControl mImageControl;

    /**
     * The opacity of the Bar's Search Context.
     * This text control may not be initialized until the opacity is set beyond 0.
     */
    private float mSearchBarContextOpacity;

    /**
     * The opacity of the Bar's Search Term.
     * This text control may not be initialized until the opacity is set beyond 0.
     */
    private float mSearchBarTermOpacity;

    // Dimensions used for laying out the search bar.
    private final float mTextLayerMinHeight;
    private final float mTermCaptionSpacing;

    /**
     * The width of the end button in px.
     */
    private final float mEndButtonWidth;

    /**
     * The percentage the panel is expanded. 1.f is fully expanded and 0.f is peeked.
     */
    private float mExpandedPercent;

    /**
     * Converts dp dimensions to pixels.
     */
    private final float mDpToPx;

    /**
     * Whether the panel contents can be promoted to a new tab.
     */
    private final boolean mCanPromoteToNewTab;

    /** The animator that controls the text opacity. */
    private CompositorAnimator mTextOpacityAnimation;

    /** The animator that controls touch highlighting. */
    private CompositorAnimator mTouchHighlightAnimation;

    /**
     * Constructs a new bottom bar control container by inflating views from XML.
     *
     * @param panel     The panel.
     * @param container The parent view for the bottom bar views.
     * @param loader    The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchBarControl(ContextualSearchPanel panel,
                                      Context context,
                                      ViewGroup container,
                                      DynamicResourceLoader loader) {
        mContextualSearchPanel = panel;
        mCanPromoteToNewTab = panel.canPromoteToNewTab();
        mImageControl = new ContextualSearchImageControl(panel);
        mContextControl = new ContextualSearchContextControl(panel, context, container, loader);
        mSearchTermControl = new ContextualSearchTermControl(panel, context, container, loader);
        mCaptionControl = new ContextualSearchCaptionControl(
                panel, context, container, loader, mCanPromoteToNewTab);
        mQuickActionControl = new ContextualSearchQuickActionControl(context, loader);
        mCardIconControl = new ContextualSearchCardIconControl(context, loader);

        mTextLayerMinHeight = context.getResources().getDimension(
                R.dimen.contextual_search_text_layer_min_height);
        mTermCaptionSpacing = context.getResources().getDimension(
                R.dimen.contextual_search_term_caption_spacing);

        // Icon attributes.
        mPaddedIconWidthPx =
                context.getResources().getDimension(R.dimen.contextual_search_padded_button_width);
        mEndButtonWidth = mPaddedIconWidthPx
                + context.getResources().getDimension(R.dimen.overlay_panel_button_padding);
        mDpToPx = context.getResources().getDisplayMetrics().density;
    }

    /**
     * @return The {@link ContextualSearchImageControl} for the panel.
     */
    public ContextualSearchImageControl getImageControl() {
        return mImageControl;
    }

    /**
     * Returns the minimum height that the text layer (containing the Search Context, Term and
     * Caption) should be.
     */
    public float getTextLayerMinHeight() {
        return mTextLayerMinHeight;
    }

    /**
     * Returns the spacing that should be placed between the Search Term and Caption.
     */
    public float getSearchTermCaptionSpacing() {
        return mTermCaptionSpacing;
    }

    /**
     * Removes the bottom bar views from the parent container.
     */
    public void destroy() {
        mContextControl.destroy();
        mSearchTermControl.destroy();
        mCaptionControl.destroy();
        mQuickActionControl.destroy();
        mCardIconControl.destroy();
    }

    /**
     * Updates this bar when in transition between closed to peeked states.
     * @param percentage The percentage to the more opened state.
     */
    public void onUpdateFromCloseToPeek(float percentage) {
        // #onUpdateFromPeekToExpanded() never reaches the 0.f value because this method is called
        // instead. If the panel is fully peeked, call #onUpdateFromPeekToExpanded().
        if (percentage == FULL_OPACITY) onUpdateFromPeekToExpand(TRANSPARENT_OPACITY);

        // When the panel is completely closed the caption and custom image should be hidden.
        if (percentage == TRANSPARENT_OPACITY) {
            mQuickActionControl.reset();
            mCaptionControl.hide();
            getImageControl().hideCustomImage(false);
        }
    }

    /**
     * Updates this bar when in transition between peeked to expanded states.
     * @param percentage The percentage to the more opened state.
     */
    public void onUpdateFromPeekToExpand(float percentage) {
        mExpandedPercent = percentage;

        getImageControl().onUpdateFromPeekToExpand(percentage);
        mCaptionControl.onUpdateFromPeekToExpand(percentage);
        mSearchTermControl.onUpdateFromPeekToExpand(percentage);
        mContextControl.onUpdateFromPeekToExpand(percentage);
    }

    /**
     * Sets the details of the context to display in the control.
     * @param selection The portion of the context that represents the user's selection.
     * @param end The portion of the context after the selection.
     */
    public void setContextDetails(String selection, String end) {
        cancelSearchTermResolutionAnimation();
        hideCaption();
        mQuickActionControl.reset();
        mContextControl.setContextDetails(selection, end);
        resetSearchBarContextOpacity();
    }

    /**
     * Updates the Bar to display a dictionary definition card.
     * @param searchTerm The string that represents the search term to display.
     * @param cardTagEnum Which kind of card is being shown in this update.
     */
    void updateForDictionaryDefinition(String searchTerm, @CardTag int cardTagEnum) {
        if (!mCardIconControl.didUpdateControlsForDefinition(
                    mContextControl, mImageControl, searchTerm, cardTagEnum)) {
            // Can't style, just update with the text to display.
            setSearchTerm(searchTerm);
            animateSearchTermResolution();
        }
    }

    /**
     * Sets the search term to display in the control.
     * @param searchTerm The string that represents the search term.
     */
    public void setSearchTerm(String searchTerm) {
        cancelSearchTermResolutionAnimation();
        hideCaption();
        mQuickActionControl.reset();
        mSearchTermControl.setSearchTerm(searchTerm);
        resetSearchBarTermOpacity();
    }

    /**
     * Sets the caption to display in the control and sets the caption visible.
     * @param caption The caption to display.
     */
    public void setCaption(String caption) {
        mCaptionControl.setCaption(caption);
    }

    /**
     * Gets the current animation percentage for the Caption control, which guides the vertical
     * position and opacity of the caption.
     * @return The animation percentage ranging from 0.0 to 1.0.
     *
     */
    public float getCaptionAnimationPercentage() {
        return mCaptionControl.getAnimationPercentage();
    }

    /**
     * @return Whether the caption control is visible.
     */
    public boolean getCaptionVisible() {
        return mCaptionControl.getIsVisible();
    }

    /**
     * @return The Id of the Search Context View.
     */
    public int getSearchContextViewId() {
        return mContextControl.getViewId();
    }

    /**
     * @return The Id of the Search Term View.
     */
    public int getSearchTermViewId() {
        return mSearchTermControl.getViewId();
    }

    /**
     * @return The Id of the Search Caption View.
     */
    public int getCaptionViewId() {
        return mCaptionControl.getViewId();
    }

    /**
     * @return The text currently showing in the caption view.
     */
    @VisibleForTesting
    public CharSequence getCaptionText() {
        return mCaptionControl.getCaptionText();
    }

    /**
     * @return The opacity of the SearchBar's search context.
     */
    public float getSearchBarContextOpacity() {
        return mSearchBarContextOpacity;
    }

    /**
     * @return The opacity of the SearchBar's search term.
     */
    public float getSearchBarTermOpacity() {
        return mSearchBarTermOpacity;
    }

    /**
     * Sets the quick action if one is available.
     * @param quickActionUri The URI for the intent associated with the quick action.
     * @param quickActionCategory The {@link QuickActionCategory} for the quick action.
     * @param toolbarBackgroundColor The current toolbar background color. This may be used for
     *                               icon tinting.
     */
    public void setQuickAction(String quickActionUri, @QuickActionCategory int quickActionCategory,
            int toolbarBackgroundColor) {
        mQuickActionControl.setQuickAction(
                quickActionUri, quickActionCategory, toolbarBackgroundColor);
        if (mQuickActionControl.hasQuickAction()) {
            // TODO(twellington): should the quick action caption be stored separately from the
            // regular caption?
            mCaptionControl.setCaption(mQuickActionControl.getCaption());
            mImageControl.setCardIconResourceId(mQuickActionControl.getIconResId());
        }
    }

    /**
     * @return The {@link ContextualSearchQuickActionControl} for the panel.
     */
    public ContextualSearchQuickActionControl getQuickActionControl() {
        return mQuickActionControl;
    }

    /**
     * Resets the SearchBar text opacity when a new search context is set. The search
     * context is made visible and the search term invisible.
     */
    private void resetSearchBarContextOpacity() {
        mSearchBarContextOpacity = FULL_OPACITY;
        mSearchBarTermOpacity = TRANSPARENT_OPACITY;
    }

    /**
     * Resets the SearchBar text opacity when a new search term is set. The search
     * term is made visible and the search context invisible.
     */
    private void resetSearchBarTermOpacity() {
        mSearchBarContextOpacity = TRANSPARENT_OPACITY;
        mSearchBarTermOpacity = FULL_OPACITY;
    }

    /**
     * Hides the caption so it will not be displayed in the control.
     */
    private void hideCaption() {
        mCaptionControl.hide();
    }

    // ============================================================================================
    // Touch Highlight
    // ============================================================================================

    /**
     * Whether the touch highlight is visible.
     */
    private boolean mTouchHighlightVisible;

    /** Where the touch highlight should start, in pixels. */
    private float mTouchHighlightXOffsetPx;

    /** The width of the touch highlight, in pixels. */
    private float mTouchHighlightWidthPx;

    /**
     * @return Whether the touch highlight is visible.
     */
    public boolean getTouchHighlightVisible() {
        return mTouchHighlightVisible;
    }

    /**
     * @return The x-offset of the touch highlight in pixels.
     */
    public float getTouchHighlightXOffsetPx() {
        return mTouchHighlightXOffsetPx;
    }

    /**
     * @return The width of the touch highlight in pixels.
     */
    public float getTouchHighlightWidthPx() {
        return mTouchHighlightWidthPx;
    }

    /**
     * Should be called when the Bar is clicked.
     * @param xDps The x-position of the click in DPs.
     */
    public void onSearchBarClick(float xDps) {
        showTouchHighlight(xDps * mDpToPx);
    }

    /**
     * Should be called when an onShowPress() event occurs on the Bar.
     * See {@code GestureDetector.SimpleOnGestureListener#onShowPress()}.
     * @param xDps The x-position of the touch in DPs.
     */
    public void onShowPress(float xDps) {
        showTouchHighlight(xDps * mDpToPx);
    }

    /**
     * Classifies the give x position in pixels and computes the highlight offset and width.
     * @param xPx The x-coordinate of a touch location, in pixels.
     */
    private void classifyTouchLocation(float xPx) {
        // There are 3 cases:
        // 1) The whole Bar (without any icons)
        // 2) The Bar minus icon (when the icon is present)
        // 3) The icon
        int panelWidth = mContextualSearchPanel.getContentViewWidthPx();
        if (mContextualSearchPanel.isPeeking()) {
            // Case 1 - whole Bar.
            mTouchHighlightXOffsetPx = 0;
            mTouchHighlightWidthPx = panelWidth;
        } else {
            // The open-tab-icon is on the right (on the left in RTL).
            boolean isRtl = LocalizationUtils.isLayoutRtl();
            float paddedIconWithMarginWidth =
                    (mContextualSearchPanel.getBarMarginSide()
                            + mContextualSearchPanel.getOpenTabIconDimension()
                            + mContextualSearchPanel.getButtonPaddingDps())
                    * mDpToPx;
            float contentWidth = panelWidth - paddedIconWithMarginWidth;
            // Adjust the touch point to panel coordinates.
            xPx -= mContextualSearchPanel.getOffsetX() * mDpToPx;
            if (isRtl && xPx > paddedIconWithMarginWidth || !isRtl && xPx < contentWidth) {
                // Case 2 - Bar minus icon.
                mTouchHighlightXOffsetPx = isRtl ? paddedIconWithMarginWidth : 0;
                mTouchHighlightWidthPx = contentWidth;
            } else {
                // Case 3 - the icon.
                mTouchHighlightXOffsetPx = isRtl ? 0 : contentWidth;
                mTouchHighlightWidthPx = paddedIconWithMarginWidth;
            }
        }
    }

    /**
     * Shows the touch highlight if it is not already visible.
     * @param x The x-position of the touch in px.
     */
    private void showTouchHighlight(float x) {
        if (mTouchHighlightVisible) return;

        // If the panel is expanded or maximized and the panel content cannot be promoted to a new
        // tab, then tapping anywhere besides the end buttons does nothing. In this case, the touch
        // highlight should not be shown.
        if (!mContextualSearchPanel.isPeeking() && !mCanPromoteToNewTab) return;

        classifyTouchLocation(x);
        mTouchHighlightVisible = true;

        // The touch highlight animation is used to ensure the touch highlight is visible for at
        // least OverlayPanelAnimation.BASE_ANIMATION_DURATION_MS.
        // TODO(donnd): Add a material ripple to this animation.
        if (mTouchHighlightAnimation == null) {
            mTouchHighlightAnimation =
                    new CompositorAnimator(mContextualSearchPanel.getAnimationHandler());
            mTouchHighlightAnimation.setDuration(OverlayPanelAnimation.BASE_ANIMATION_DURATION_MS);
            mTouchHighlightAnimation.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    mTouchHighlightVisible = false;
                }
            });
        }
        mTouchHighlightAnimation.cancel();
        mTouchHighlightAnimation.start();
    }

    // ============================================================================================
    // Search Bar Animation
    // ============================================================================================

    /**
     * Animates the search term resolution.
     */
    public void animateSearchTermResolution() {
        if (mTextOpacityAnimation == null) {
            mTextOpacityAnimation = CompositorAnimator.ofFloat(
                    mContextualSearchPanel.getAnimationHandler(), TRANSPARENT_OPACITY, FULL_OPACITY,
                    OverlayPanelAnimation.BASE_ANIMATION_DURATION_MS, null);
            mTextOpacityAnimation.addUpdateListener(
                    animator -> updateSearchBarTextOpacity(animator.getAnimatedValue()));
        }
        mTextOpacityAnimation.cancel();
        mTextOpacityAnimation.start();
    }

    /**
     * Cancels the search term resolution animation if it is in progress.
     */
    public void cancelSearchTermResolutionAnimation() {
        if (mTextOpacityAnimation != null) mTextOpacityAnimation.cancel();
    }

    /**
     * Updates the UI state for the SearchBar text. The search context view will fade out
     * while the search term fades in.
     *
     * @param percentage The visibility percentage of the search term view.
     */
    private void updateSearchBarTextOpacity(float percentage) {
        // The search context will start fading out before the search term starts fading in.
        // They will both be partially visible for overlapPercentage of the animation duration.
        float overlapPercentage = .75f;
        float fadingOutPercentage =
                Math.max(1 - (percentage / overlapPercentage), TRANSPARENT_OPACITY);
        float fadingInPercentage =
                Math.max(percentage - (1 - overlapPercentage), TRANSPARENT_OPACITY)
                / overlapPercentage;

        mSearchBarContextOpacity = fadingOutPercentage;
        mSearchBarTermOpacity = fadingInPercentage;
    }
}
