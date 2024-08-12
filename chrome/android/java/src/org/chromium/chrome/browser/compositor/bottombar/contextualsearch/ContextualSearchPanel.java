// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.app.Activity;
import android.content.Context;
import android.graphics.RectF;
import android.graphics.drawable.ColorDrawable;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.PanelPriority;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.scene_layer.ContextualSearchSceneLayer;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchUma;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.util.ColorUtils;

import java.util.List;

/**
 * Controls the Contextual Search Panel, primarily the Bar - the {@link ContextualSearchBarControl}
 * - and the content area that shows the Search Result.
 */
public class ContextualSearchPanel extends OverlayPanel {
    /** Allows controls that appear in this panel to call back with requests or notifications. */
    interface ContextualSearchPanelSectionHost {
        /** Returns the current Y position of the panel section. */
        float getYPositionPx();

        /** Notifies the panel that the caller's section is changing its size. */
        void onPanelSectionSizeChange(boolean hasStarted);
    }

    /** The interface that the Opt-in promo uses to communicate with this Panel. */
    interface ContextualSearchPromoHost extends ContextualSearchPanelSectionHost {
        /** Notifies the host that the promo was shown. */
        void onPromoShown();

        /** Notifies the host whether the user enabled the feature via the promotion. */
        void setContextualSearchPromoCardSelection(boolean enabled);
    }

    /** The interface that the Related Searches section uses to communicate with this Panel. */
    interface RelatedSearchesSectionHost extends ContextualSearchPanelSectionHost {
        /**
         * Notifies that the user has clicked on a suggestions in this section of the panel.
         * @param suggestionIndex The 0-based index into the list of suggestions provided by the
         *        panel and presented in the UI. E.g. if the user clicked the second chit this value
         *        would be 1.
         */
        void onSuggestionClicked(int suggestionIndex);
    }

    /** Restricts the maximized panel height to the given fraction of a tab. */
    private static final float MAXIMIZED_HEIGHT_FRACTION = 0.95f;

    /** Used for logging state changes. */
    private final ContextualSearchPanelMetrics mPanelMetrics;

    /** Used to query toolbar state. */
    private final ToolbarManager mToolbarManager;

    /** The distance of the divider from the end of the bar, in dp. */
    private final float mEndButtonWidthDp;

    /** Whether the contextual search panel can be promoted to a new tab. */
    private final boolean mCanPromoteToNewTab;

    /** Supplies a {@link EdgeToEdgeController} that adjusts for more screen-bottom space. */
    private Supplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;

    /** Whether the Panel should be promoted to a new tab after being maximized. */
    private boolean mShouldPromoteToTabAfterMaximizing;

    /** The object for handling global Contextual Search management duties */
    private ContextualSearchManagementDelegate mManagementDelegate;

    /** Whether the content view has been touched. */
    private boolean mHasContentBeenTouched;

    /** The compositor layer used for drawing the panel. */
    private ContextualSearchSceneLayer mSceneLayer;

    /**
     * A ScrimCoordinator for adjusting the Status Bar's brightness when a scrim is present (when
     * the panel is open).
     */
    private ScrimCoordinator mScrimCoordinator;

    /**
     * Params that configure our use of the ScrimCoordinator for adjusting the Status Bar's
     * brightness when a scrim is present (when the panel is open).
     */
    private PropertyModel mScrimProperties;

    /** Whether we have started collapsing the panel. */
    private boolean mDidStartCollapsing;

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param context The current Android {@link Context}.
     * @param layoutManager A layout manager for observing scene changes.
     * @param panelManager The object managing the how different panels are shown.
     * @param browserControlsStateProvider Used to measure the browser controls.
     * @param windowAndroid The {@link WindowAndroid} for the current activity.
     * @param profile The Profile this ContextualSearchPanel is associated with.
     * @param compositorViewHolder The {@link CompositorViewHolder} for the current activity.
     * @param toolbarHeightDp The height of the toolbar in dp.
     * @param toolbarManager The {@link ToolbarManager}, used to query for colors.
     * @param canPromoteToNewTab Whether the panel can be promoted to a new tab.
     * @param currentTabSupplier Supplies the current activity tab.
     * @param edgeToEdgeControllerSupplier Controller for edge-to-edge drawing.
     */
    public ContextualSearchPanel(
            @NonNull Context context,
            @NonNull LayoutManagerImpl layoutManager,
            @NonNull OverlayPanelManager panelManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull WindowAndroid windowAndroid,
            @NonNull Profile profile,
            @NonNull CompositorViewHolder compositorViewHolder,
            float toolbarHeightDp,
            @NonNull ToolbarManager toolbarManager,
            boolean canPromoteToNewTab,
            @NonNull Supplier<Tab> currentTabSupplier,
            @NonNull Supplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        super(
                context,
                layoutManager,
                panelManager,
                browserControlsStateProvider,
                windowAndroid,
                profile,
                compositorViewHolder,
                toolbarHeightDp,
                currentTabSupplier);
        mSceneLayer = createNewContextualSearchSceneLayer();
        mPanelMetrics = new ContextualSearchPanelMetrics();
        mToolbarManager = toolbarManager;
        mCanPromoteToNewTab = canPromoteToNewTab;
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;

        mEndButtonWidthDp =
                mContext.getResources()
                                .getDimensionPixelSize(
                                        R.dimen.contextual_search_padded_button_width)
                        * mPxToDp;
    }

    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        return new OverlayPanelContent(
                mManagementDelegate.getOverlayPanelContentDelegate(),
                new PanelProgressObserver(),
                mActivity,
                getProfile(),
                getBarHeight(),
                getCompositorViewHolder(),
                getWindowAndroid(),
                getCurrentTabSupplier());
    }

    // ============================================================================================
    // Scene Overlay
    // ============================================================================================

    /** Create a new scene layer for this panel. This should be overridden by tests as necessary. */
    protected ContextualSearchSceneLayer createNewContextualSearchSceneLayer() {
        return new ContextualSearchSceneLayer(
                getProfile(), mContext.getResources().getDisplayMetrics().density);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset) {
        super.getUpdatedSceneOverlayTree(viewport, visibleViewport, resourceManager, yOffset);
        mSceneLayer.update(
                resourceManager,
                this,
                getSearchBarControl(),
                getPromoControl(),
                getRelatedSearchesInBarControl(),
                getImageControl());

        return mSceneLayer;
    }

    // ============================================================================================
    // Contextual Search Manager Integration
    // ============================================================================================

    /**
     * Sets the {@code ContextualSearchManagementDelegate} associated with this panel.
     *
     * @param delegate The {@code ContextualSearchManagementDelegate}.
     */
    public void setManagementDelegate(ContextualSearchManagementDelegate delegate) {
        if (mManagementDelegate != delegate) {
            mManagementDelegate = delegate;
            if (delegate != null) {
                setActivity(mManagementDelegate.getActivity());
            }
        }
    }

    /**
     * Notifies that the preference state has changed.
     *
     * @param isEnabled Whether the feature is enabled.
     */
    public void onContextualSearchPrefChanged(boolean isEnabled) {
        if (!isShowing()) return;

        getPromoControl().onContextualSearchPrefChanged(isEnabled);
    }

    // ============================================================================================
    // Panel State
    // ============================================================================================

    @Override
    public void setPanelState(@PanelState int toState, @StateChangeReason int reason) {
        @PanelState int fromState = getPanelState();

        mPanelMetrics.onPanelStateChanged(fromState, toState, reason, getProfile());

        if (toState == PanelState.CLOSED || toState == PanelState.UNDEFINED) {
            mManagementDelegate.onPanelFinishedShowing();
        }

        super.setPanelState(toState, reason);
        mDidStartCollapsing = false;
    }

    @Override
    protected @PanelState int getProjectedState(float velocity) {
        @PanelState int projectedState = super.getProjectedState(velocity);

        // Prevent the fling gesture from moving the Panel from PEEKED to MAXIMIZED. This is to
        // make sure the Promo will be visible, considering that the EXPANDED state is the only
        // one that will show the Promo.
        if (getPromoControl().isVisible()
                && projectedState == PanelState.MAXIMIZED
                && getPanelState() == PanelState.PEEKED) {
            projectedState = PanelState.EXPANDED;
        }

        // If we're swiping the panel down from MAXIMIZED skip the EXPANDED state and go all the
        // way to PEEKED.
        if (getPanelState() == PanelState.MAXIMIZED && projectedState == PanelState.EXPANDED) {
            projectedState = PanelState.PEEKED;
        }

        return projectedState;
    }

    @Override
    public boolean onBackPressed() {
        if (!isShowing()) return false;
        mManagementDelegate.hideContextualSearch(StateChangeReason.BACK_PRESS);
        return true;
    }

    // ============================================================================================
    // Contextual Search Manager Integration
    // ============================================================================================

    @Override
    protected void onClosed(@StateChangeReason int reason) {
        // Must be called before destroying Content because unseen visits should be removed from
        // history, and if the Content gets destroyed there won't be a Webcontents to do that.
        mManagementDelegate.onCloseContextualSearch(reason);

        setProgressBarCompletion(0);
        setProgressBarVisible(false);
        getImageControl().hideCustomImage(false);

        super.onClosed(reason);

        if (mSceneLayer != null) mSceneLayer.hideTree();
        if (mScrimCoordinator != null) mScrimCoordinator.hideScrim(false);

        mDidStartCollapsing = false;
    }

    // ============================================================================================
    // Generic Event Handling
    // ============================================================================================

    private boolean isCoordinateInsideActionTarget(float x) {
        if (LocalizationUtils.isLayoutRtl()) {
            return x >= getContentX() + mEndButtonWidthDp;
        } else {
            return x <= getContentX() + getWidth() - mEndButtonWidthDp;
        }
    }

    /** Handles a bar click. The position is given in dp. */
    @Override
    public void handleBarClick(float x, float y) {
        getSearchBarControl().onSearchBarClick(x);

        if (isPeeking()) {
            if (getSearchBarControl().getQuickActionControl().hasQuickAction()
                    && isCoordinateInsideActionTarget(x)) {
                getSearchBarControl()
                        .getQuickActionControl()
                        .sendIntent(getCurrentTabSupplier().get());
            } else {
                // super takes care of expanding the Panel when peeking.
                super.handleBarClick(x, y);
            }
        } else if (isExpanded() || isMaximized()) {
            if (canPromoteToNewTab() && isCoordinateInsideOpenTabButton(x)) {
                mManagementDelegate.promoteToTab();
            } else {
                peekPanel(StateChangeReason.UNKNOWN);
            }
        }
    }

    @Override
    public boolean onInterceptBarClick() {
        return onInterceptOpeningPanel();
    }

    @Override
    public boolean onInterceptBarSwipe() {
        return onInterceptOpeningPanel();
    }

    /**
     * @return True if the event on the bar was intercepted.
     */
    private boolean onInterceptOpeningPanel() {
        if (mManagementDelegate.isRunningInCompatibilityMode()) {
            mManagementDelegate.openResolvedSearchUrlInNewTab();
            return true;
        }
        return false;
    }

    @Override
    public void onShowPress(float x, float y) {
        if (isCoordinateInsideBar(x, y)) getSearchBarControl().onShowPress(x);
        super.onShowPress(x, y);
    }

    // ============================================================================================
    // Panel base methods
    // ============================================================================================

    @Override
    protected void destroyComponents() {
        super.destroyComponents();
        destroyPromoControl();
        destroyInBarRelatedSearchesControl();
        destroySearchBarControl();
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        super.onActivityStateChange(activity, newState);
        if (newState == ActivityState.PAUSED) {
            mManagementDelegate.logCurrentState();
        }
    }

    @Override
    public @PanelPriority int getPriority() {
        return PanelPriority.HIGH;
    }

    @Override
    public boolean canBeSuppressed() {
        // The selected text on the page is lost when the panel is closed, thus, this panel cannot
        // be restored if it is suppressed.
        return false;
    }

    @Override
    public void notifyBarTouched(float x) {
        getOverlayPanelContent().showContent();
    }

    @Override
    public float getOpenTabIconX() {
        if (LocalizationUtils.isLayoutRtl()) {
            return getOffsetX() + getBarMarginSide();
        } else {
            return getOffsetX() + getWidth() - getBarMarginSide() - getCloseIconDimension();
        }
    }

    @Override
    protected boolean isCoordinateInsideCloseButton(float x) {
        return false;
    }

    @Override
    protected boolean isCoordinateInsideOpenTabButton(float x) {
        return getOpenTabIconX() - getButtonPaddingDps() <= x
                && x <= getOpenTabIconX() + getOpenTabIconDimension() + getButtonPaddingDps();
    }

    @Override
    public float getContentY() {
        return getOffsetY() + getBarContainerHeight() + getPromoHeightPx() * mPxToDp;
    }

    @Override
    public float getBarContainerHeight() {
        return getBarHeight();
    }

    @Override
    protected float getPeekedHeight() {
        return getBarHeight();
    }

    @Override
    protected float getMaximizedHeight() {
        // Max height does not cover the entire content screen.
        return getTabHeight() * MAXIMIZED_HEIGHT_FRACTION;
    }

    @Override
    public float getBarMarginBottomPx() {
        // When Edge To Edge is enabled and drawing to the bottom edge, pass in the bottom inset
        // to pad the search bar (specifically, the caption's bottom padding). Use 0 otherwise.
        // TODO(crbug.com/332543636) Remove padding when it's no longer needed in EXPANDED and
        //  MAXIMIZED states
        @Nullable EdgeToEdgeController edgeToEdgeController = mEdgeToEdgeControllerSupplier.get();
        return edgeToEdgeController != null ? edgeToEdgeController.getBottomInsetPx() : 0;
    }

    @Override
    public float getBarHeight() {
        // If the font is scaled, the preset bar height obtained from super.getBarHeight() may be
        // smaller than the height required to display the bar's content. In such cases, it is
        // necessary to select the larger value between the preset height and the actual content
        // height.
        float baseBarHeight = super.getBarHeight();

        // When Edge To Edge is enabled and drawing to the bottom edge, increase the base bar height
        // to properly account for the extra bottom inset when positioning the peek height. The
        // padding will appear in the search bar control min height after a delay, once the view has
        // inflated, but that's too late for initial positioning.
        if (mEdgeToEdgeControllerSupplier.get() != null) {
            baseBarHeight += mEdgeToEdgeControllerSupplier.get().getBottomInset();
        }
        return Math.max(baseBarHeight, getSearchBarControlMinHeightDps())
                + getInBarRelatedSearchesAnimatedHeightDps();
    }

    @Override
    public void setClampedPanelHeight(float height) {
        super.setClampedPanelHeight(height);
    }

    // ============================================================================================
    // Animation Handling
    // ============================================================================================

    @Override
    protected void onHeightAnimationFinished() {
        super.onHeightAnimationFinished();

        if (mShouldPromoteToTabAfterMaximizing && getPanelState() == PanelState.MAXIMIZED) {
            mShouldPromoteToTabAfterMaximizing = false;
            mManagementDelegate.promoteToTab();
        }
    }

    @Override
    @VisibleForTesting
    public void animatePanelToState(
            @Nullable @PanelState Integer state, @StateChangeReason int reason, long duration) {
        // If the in bar chip showing animation is running, do not run the new panel animation
        // unless it needs to animate to a different state.
        if (state == getPanelState()
                && haveSearchBarControl()
                && getSearchBarControl().inBarRelatedSearchesAnimationIsRunning()) {
            return;
        }

        if (state == PanelState.PEEKED
                && (getPanelState() == PanelState.EXPANDED
                        || getPanelState() == PanelState.MAXIMIZED)) {
            mManagementDelegate.onPanelCollapsing();
            getRelatedSearchesInBarControl().onPanelCollapsing();
        }

        super.animatePanelToState(state, reason, duration);
    }

    // ============================================================================================
    // Contextual Search Panel API
    // ============================================================================================

    /** Notify the panel that the content was seen. */
    public void setWasSearchContentViewSeen() {
        mPanelMetrics.setWasSearchContentViewSeen();
    }

    /**
     * @param isActive Whether the promo is active.
     */
    public void setIsPromoActive(boolean isActive) {
        if (isActive) {
            getPromoControl().show();
        } else {
            getPromoControl().hide();
        }

        mPanelMetrics.setIsPromoActive(isActive);
    }

    public void clearRelatedSearches() {
        getRelatedSearchesInBarControl().hide();
    }

    /**
     * Maximizes the Contextual Search Panel.
     * @param reason The {@code StateChangeReason} behind the maximization.
     */
    @Override
    public void maximizePanel(@StateChangeReason int reason) {
        mShouldPromoteToTabAfterMaximizing = false;
        super.maximizePanel(reason);
    }

    /**
     * Maximizes the Contextual Search Panel, then promotes it to a regular Tab.
     *
     * @param reason The {@code StateChangeReason} behind the maximization and promotion to tab.
     */
    public void maximizePanelThenPromoteToTab(@StateChangeReason int reason) {
        mShouldPromoteToTabAfterMaximizing = true;
        super.maximizePanel(reason);
        if (reason == StateChangeReason.SERP_NAVIGATION) {
            RelatedSearchesControl activeRelatedSearches = getRelatedSearchesInBarControl();
            ContextualSearchUma.logSerpResultClicked(
                    activeRelatedSearches.isShowingRelatedSearchSerp());
        }
    }

    @Override
    public void peekPanel(@StateChangeReason int reason) {
        super.peekPanel(reason);

        if (getPanelState() == PanelState.CLOSED || getPanelState() == PanelState.PEEKED) {
            mHasContentBeenTouched = false;
        }

        if ((getPanelState() == PanelState.UNDEFINED || getPanelState() == PanelState.CLOSED)
                && reason == StateChangeReason.TEXT_SELECT_TAP) {
            mPanelMetrics.onPanelTriggeredFromTap();
        }
    }

    @Override
    public void closePanel(@StateChangeReason int reason, boolean animate) {
        super.closePanel(reason, animate);
        mHasContentBeenTouched = false;
        if (reason == StateChangeReason.TAB_PROMOTION) {
            RelatedSearchesControl activeRelatedSearches = getRelatedSearchesInBarControl();
            ContextualSearchUma.logTabPromotion(activeRelatedSearches.isShowingRelatedSearchSerp());
        }
    }

    @Override
    public void expandPanel(@StateChangeReason int reason) {
        super.expandPanel(reason);
    }

    @Override
    public void requestPanelShow(@StateChangeReason int reason) {
        // If a re-tap is causing the panel to show when already shown, the superclass may ignore
        // that, but we want to be sure to capture search metrics for each tap.
        if (isShowing() && getPanelState() == PanelState.PEEKED) {
            peekPanel(reason);
        }
        super.requestPanelShow(reason);
    }

    /** Gets whether a touch on the content view has been done yet or not. */
    public boolean didTouchContent() {
        return mHasContentBeenTouched;
    }

    /**
     * Sets the search term to display in the SearchBar. This should be called when the search term
     * is set without search term resolution.
     *
     * @param searchTerm The string that represents the search term.
     */
    public void setSearchTerm(String searchTerm) {
        setSearchTerm(searchTerm, null);
    }

    /**
     * Sets the search term to display in the SearchBar. This should be called when the search term
     * is set after search term resolution completed.
     *
     * @param searchTerm The string that represents the search term.
     * @param pronunciation A string for the pronunciation when a Definition is shown.
     */
    public void setSearchTerm(String searchTerm, @Nullable String pronunciation) {
        getImageControl().hideCustomImage(true);
        getSearchBarControl().setSearchTerm(searchTerm, pronunciation);
        mPanelMetrics.onSearchRequestStarted();
        // Make sure the new Search Term draws.
        requestUpdate();
    }

    /**
     * Sets the search context details to display in the SearchBar.
     *
     * @param selection The portion of the context that represents the user's selection.
     * @param end The portion of the context from the selection to its end.
     */
    public void setContextDetails(String selection, String end) {
        getImageControl().hideCustomImage(true);
        getSearchBarControl().setContextDetails(selection, end);
        mPanelMetrics.onSearchRequestStarted();
        // Make sure the new Context draws.
        requestUpdate();
    }

    /**
     * Sets the caption to display in the SearchBar. When the caption is displayed, the Search Term
     * is pushed up and the caption shows below.
     *
     * @param caption The string to show in as the caption.
     */
    public void setCaption(String caption) {
        getSearchBarControl().setCaption(caption);
    }

    /** Ensures that we have a Caption to display in the SearchBar. */
    public void ensureCaption() {
        if (getSearchBarControl().hasCaption()) return;
        getSearchBarControl()
                .setCaption(
                        mContext.getResources()
                                .getString(R.string.contextual_search_default_caption));
    }

    /** Hides the caption. */
    public void hideCaption() {
        getSearchBarControl().hideCaption();
    }

    /**
     * Handles showing the resolved search term in the SearchBar.
     *
     * @param searchTerm The string that represents the search term.
     * @param thumbnailUrl The URL of the thumbnail to display.
     * @param quickActionUri The URI for the intent associated with the quick action.
     * @param quickActionCategory The {@code QuickActionCategory} for the quick action.
     * @param cardTagEnum The {@link CardTag} that the server returned if there was a card, or
     *     {@code 0}.
     * @param relatedSearchesInBar Related Searches suggestions to be displayed in the Bar.
     */
    @VisibleForTesting
    public void onSearchTermResolved(
            String searchTerm,
            String thumbnailUrl,
            String quickActionUri,
            int quickActionCategory,
            @CardTag int cardTagEnum,
            @Nullable List<String> relatedSearchesInBar) {
        onSearchTermResolved(
                searchTerm,
                null,
                thumbnailUrl,
                quickActionUri,
                quickActionCategory,
                cardTagEnum,
                relatedSearchesInBar);
    }

    /**
     * Handles showing the resolved search term in the SearchBar.
     *
     * @param searchTerm The string that represents the search term.
     * @param pronunciation A string for the pronunciation when a Definition is shown.
     * @param thumbnailUrl The URL of the thumbnail to display.
     * @param quickActionUri The URI for the intent associated with the quick action.
     * @param quickActionCategory The {@code QuickActionCategory} for the quick action.
     * @param cardTagEnum The {@link CardTag} that the server returned if there was a card, or
     *     {@code 0}.
     * @param relatedSearchesInBar Related Searches suggestions to be displayed in the Bar.
     */
    public void onSearchTermResolved(
            String searchTerm,
            @Nullable String pronunciation,
            String thumbnailUrl,
            String quickActionUri,
            int quickActionCategory,
            @CardTag int cardTagEnum,
            @Nullable List<String> relatedSearchesInBar) {
        boolean hadInBarSuggestions = getRelatedSearchesInBarControl().hasReleatedSearchesToShow();
        getRelatedSearchesInBarControl().setRelatedSearchesSuggestions(relatedSearchesInBar);
        if (getRelatedSearchesInBarControl().hasReleatedSearchesToShow() != hadInBarSuggestions) {
            getSearchBarControl().animateInBarRelatedSearches(!hadInBarSuggestions);
        }

        if (cardTagEnum == CardTag.CT_DEFINITION
                || cardTagEnum == CardTag.CT_CONTEXTUAL_DEFINITION) {
            getSearchBarControl().setVectorDrawableDefinitionIcon();
        } else {
            getImageControl().setThumbnailUrl(thumbnailUrl);
        }

        getSearchBarControl().setSearchTerm(searchTerm, pronunciation);
        getSearchBarControl().animateSearchTermResolution();
        // TODO(donnd): this can probably be removed or changed to an assert.
        if (mActivity == null || mToolbarManager == null) return;

        getSearchBarControl()
                .setQuickAction(
                        quickActionUri, quickActionCategory, mToolbarManager.getPrimaryColor());
    }

    /**
     * @return The padding used for each side of the button in the Bar.
     */
    public float getButtonPaddingDps() {
        return mButtonPaddingDps;
    }

    // ============================================================================================
    // Panel Metrics
    // ============================================================================================

    // TODO(pedrosimonetti): replace proxy methods with direct PanelMetrics usage

    /**
     * @return The {@link ContextualSearchPanelMetrics}.
     */
    public ContextualSearchPanelMetrics getPanelMetrics() {
        return mPanelMetrics;
    }

    /** Sets that the contextual search involved the promo. */
    public void setDidSearchInvolvePromo() {
        mPanelMetrics.setDidSearchInvolvePromo();
    }

    // ============================================================================================
    // Panel Rendering
    // ============================================================================================

    // TODO(pedrosimonetti): generalize the dispatching of panel updates.

    @Override
    protected void updatePanelForCloseOrPeek(float percentage) {
        super.updatePanelForCloseOrPeek(percentage);

        getPromoControl().onUpdateFromCloseToPeek(percentage);
        getRelatedSearchesInBarControl().onUpdateFromCloseToPeek(percentage);
        getSearchBarControl().onUpdateFromCloseToPeek(percentage);
        mDidStartCollapsing = false;
    }

    @Override
    protected void updatePanelForExpansion(float percentage) {
        super.updatePanelForExpansion(percentage);

        if (getPanelState() == PanelState.EXPANDED && !mDidStartCollapsing && percentage < 0.5f) {
            mDidStartCollapsing = true;
            mManagementDelegate.onPanelCollapsing();
            getRelatedSearchesInBarControl().onPanelCollapsing();
        }

        getPromoControl().onUpdateFromPeekToExpand(percentage);
        getRelatedSearchesInBarControl().onUpdateFromPeekToExpand(percentage);
        getSearchBarControl().onUpdateFromPeekToExpand(percentage);
    }

    @Override
    protected void updatePanelForMaximization(float percentage) {
        super.updatePanelForMaximization(percentage);

        getPromoControl().onUpdateFromExpandToMaximize(percentage);
        getRelatedSearchesInBarControl().onUpdateFromExpandToMaximize(percentage);
    }

    @Override
    protected void updatePanelForSizeChange() {
        if (getPromoControl().isVisible()) {
            getPromoControl().invalidate(true);
        }
        if (getRelatedSearchesInBarControl().isVisible()) {
            getRelatedSearchesInBarControl().invalidate(true);
        }

        // NOTE(pedrosimonetti): We cannot tell where the selection will be after the
        // orientation change, so we are setting the selection position to zero, which
        // means the base page will be positioned in its original state and we won't
        // try to keep the selection in view.
        updateBasePageSelectionYPx(0.f);
        updateBasePageTargetY();

        super.updatePanelForSizeChange();
    }

    @Override
    protected void updateStatusBar() {
        float maxBrightness = getMaxBasePageBrightness();
        float minBrightness = getMinBasePageBrightness();
        float basePageBrightness = getBasePageBrightness();
        // Compute Status Bar alpha based on the base-page brightness range applied by the Overlay.
        // TODO(donnd): Create a full-screen sized view and apply the black_alpha_65 color to get
        // an exact match between the scrim and the status bar colors instead of adjusting the
        // status bar alpha to approximate the native overlay brightness filter.
        // Details in https://crbug.com/848922.
        float statusBarAlpha =
                (maxBrightness - basePageBrightness) / (maxBrightness - minBrightness);
        if (!getCanHideAndroidBrowserControls()) scrimAndroidToolbar(statusBarAlpha);
        if (statusBarAlpha == 0.0) {
            if (mScrimCoordinator != null) mScrimCoordinator.hideScrim(false);
            mScrimProperties = null;
            mScrimCoordinator = null;
            return;

        } else {
            mScrimCoordinator = mManagementDelegate.getScrimCoordinator();
            if (mScrimProperties == null) {
                mScrimProperties =
                        new PropertyModel.Builder(ScrimProperties.REQUIRED_KEYS)
                                .with(ScrimProperties.TOP_MARGIN, 0)
                                .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                                .with(ScrimProperties.ANCHOR_VIEW, getCompositorViewHolder())
                                .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                                .with(ScrimProperties.VISIBILITY_CALLBACK, null)
                                .with(ScrimProperties.CLICK_DELEGATE, null)
                                .build();
                mScrimCoordinator.showScrim(mScrimProperties);
            }
            mScrimCoordinator.setAlpha(statusBarAlpha);
        }
    }

    private void scrimAndroidToolbar(float scrimFraction) {
        int toolbarColor = mToolbarManager.getToolbar().getPrimaryColor();
        if (scrimFraction > 0.f) {
            toolbarColor = getScrimmedColor(mActivity, toolbarColor, scrimFraction);
        }
        ToolbarLayout toolbarLayout = mActivity.findViewById(R.id.toolbar);
        ColorDrawable toolbarBackground = (ColorDrawable) toolbarLayout.getBackground();
        toolbarBackground.setColor(toolbarColor);

        scrimImage(R.id.drag_handlebar, R.color.drag_handlebar_color_baseline, scrimFraction);
        scrimImage(R.id.toolbar_hairline, R.color.divider_line_bg_color_baseline, scrimFraction);
    }

    private void scrimImage(int viewId, int colorId, float scrimFraction) {
        ImageView view = mActivity.findViewById(viewId);
        if (view == null) return;
        int baseColor = mActivity.getColor(colorId);
        if (scrimFraction > 0.f) {
            view.setColorFilter(getScrimmedColor(mActivity, baseColor, scrimFraction));
        } else {
            view.clearColorFilter();
        }
    }

    private static @ColorInt int getScrimmedColor(
            Context context, @ColorInt int baseColor, float scrimFraction) {
        @ColorInt int scrimColor = context.getColor(R.color.default_scrim_color);
        return ColorUtils.overlayColor(baseColor, scrimColor, scrimFraction);
    }

    // ============================================================================================
    // Selection position
    // ============================================================================================

    /** The approximate Y coordinate of the selection in pixels. */
    private float mBasePageSelectionYPx = -1.f;

    /**
     * Updates the coordinate of the existing selection.
     *
     * @param y The y coordinate of the selection in pixels.
     */
    public void updateBasePageSelectionYPx(float y) {
        mBasePageSelectionYPx = y;
    }

    @Override
    protected float calculateBasePageDesiredOffset() {
        float offset = 0.f;
        if (mBasePageSelectionYPx > 0.f) {
            // Convert from px to dp.
            final float selectionY = mBasePageSelectionYPx * mPxToDp;

            // Calculate the offset to center the selection on the available area.
            final float availableHeight = getTabHeight() - getExpandedHeight();
            offset = -selectionY + availableHeight / 2;
            offset += getLayoutOffsetYDps();
        }
        return offset;
    }

    // ============================================================================================
    // ContextualSearchBarControl
    // ============================================================================================

    private ContextualSearchBarControl mSearchBarControl;

    /**
     * Creates the ContextualSearchBarControl, if needed. The Views are set to INVISIBLE, because
     * they won't actually be displayed on the screen (their snapshots will be displayed instead).
     */
    public ContextualSearchBarControl getSearchBarControl() {
        if (mSearchBarControl == null) {
            mSearchBarControl =
                    new ContextualSearchBarControl(this, mContext, mContainerView, mResourceLoader);
        }
        return mSearchBarControl;
    }

    /** Destroys the ContextualSearchBarControl. */
    protected void destroySearchBarControl() {
        if (mSearchBarControl != null) {
            mSearchBarControl.destroy();
            mSearchBarControl = null;
        }
    }

    /** Returns whether we currently have a Search Bar created. */
    private boolean haveSearchBarControl() {
        return mSearchBarControl != null;
    }

    /** Returns the search bar's minimum required height. */
    private float getSearchBarControlMinHeightDps() {
        return mSearchBarControl == null ? 0 : mSearchBarControl.getMinHeightDps();
    }

    // ============================================================================================
    // Image Control
    // ============================================================================================
    /**
     * @return The {@link ContextualSearchImageControl} for the panel.
     */
    public ContextualSearchImageControl getImageControl() {
        return getSearchBarControl().getImageControl();
    }

    // ============================================================================================
    // Promo
    // ============================================================================================

    private ContextualSearchPromoControl mPromoControl;
    private ContextualSearchPromoHost mPromoHost;

    /**
     * @return Height of the promo in pixels.
     */
    private float getPromoHeightPx() {
        return getPromoControl().getHeightPx();
    }

    /** Creates the ContextualSearchPromoControl, if needed. */
    private ContextualSearchPromoControl getPromoControl() {
        if (mPromoControl == null) {
            mPromoControl =
                    new ContextualSearchPromoControl(
                            this,
                            getContextualSearchPromoHost(),
                            mContext,
                            getCoordinatorView(),
                            mResourceLoader);
        }
        return mPromoControl;
    }

    /** Destroys the ContextualSearchPromoControl. */
    private void destroyPromoControl() {
        if (mPromoControl != null) {
            mPromoControl.destroy();
            mPromoControl = null;
        }
    }

    /**
     * @return An implementation of {@link ContextualSearchPromoHost}.
     */
    private ContextualSearchPromoHost getContextualSearchPromoHost() {
        if (mPromoHost == null) {
            // Create a handler for callbacks from the Opt-in promo.
            mPromoHost =
                    new ContextualSearchPromoHost() {
                        @Override
                        public float getYPositionPx() {
                            // Needs to enumerate anything that can appear above it in the panel.
                            return Math.round((getOffsetY() + getBarContainerHeight()) / mPxToDp);
                        }

                        @Override
                        public void onPanelSectionSizeChange(boolean hasStarted) {
                            // The promo section is causing movement, but since there's nothing
                            // below it we don't need to do anything.
                        }

                        @Override
                        public void onPromoShown() {
                            mManagementDelegate.onPromoShown();
                        }

                        @Override
                        public void setContextualSearchPromoCardSelection(boolean enabled) {
                            mManagementDelegate.setContextualSearchPromoCardSelection(enabled);
                        }
                    };
        }

        return mPromoHost;
    }

    private ViewGroup getCoordinatorView() {
        ViewGroup result = mContainerView;
        // Use the coordinator inside of the container if we can get it. See crbug.com/1258902.
        ViewGroup coordinator = mContainerView.findViewById(org.chromium.chrome.R.id.coordinator);
        // Returns null in tests. TODO(donnd): figure out why - tests should have the same views.
        if (coordinator != null) result = coordinator;
        return result;
    }

    // ============================================================================================
    // The Related Searches Control that appears in the Bar
    // ============================================================================================

    private RelatedSearchesControl mRelatedSearchesInBarControl;
    private RelatedSearchesSectionHost mRelatedSearchesInBarHost;

    /** Creates the RelatedSearchesControl to be shown in the Bar, if needed. */
    @VisibleForTesting
    public RelatedSearchesControl getRelatedSearchesInBarControl() {
        if (mRelatedSearchesInBarControl == null) {
            mRelatedSearchesInBarControl =
                    new RelatedSearchesControl(
                            this,
                            getRelatedSearchesInBarHost(),
                            mContext,
                            getCoordinatorView(),
                            mResourceLoader);
        }
        return mRelatedSearchesInBarControl;
    }

    /**
     * @return Height of the Related Searches UI as currently show right inside the Bar, in DPs.
     */
    public float getInBarRelatedSearchesAnimatedHeightDps() {
        return haveSearchBarControl()
                ? getSearchBarControl().getInBarRelatedSearchesAnimatedHeightDps()
                : 0.f;
    }

    /**
     * Returns the amount of padding that is redundant between the Related Searches carousel that is
     * shown in the Bar with the content above it. The content above has its own padding that
     * provides a space between it and the bottom of the Bar. So when the Bar grows to include the
     * Related Searches (which has its own padding above and below) there is redundant padding.
     * @return The amount of overlap of padding values that can be removed (in pixels).
     */
    public float getInBarRelatedSearchesRedundantPadding() {
        return getRelatedSearchesInBarControl().getRedundantPadding();
    }

    /**
     * @return Total height of this section of the Bar in DPs (once fully exposed by animation).
     */
    float getInBarRelatedSearchesMaximumHeightDps() {
        return getRelatedSearchesInBarControl().getMaximumHeightPx() * mPxToDp;
    }

    /** Destroys the RelatedSearchesControl. */
    private void destroyInBarRelatedSearchesControl() {
        if (mRelatedSearchesInBarControl != null) {
            mRelatedSearchesInBarControl.destroy();
            mRelatedSearchesInBarControl = null;
        }
    }

    /**
     * @return An implementation of {@link RelatedSearchesSectionHost}.
     */
    private RelatedSearchesSectionHost getRelatedSearchesInBarHost() {
        if (mRelatedSearchesInBarHost == null) {
            mRelatedSearchesInBarHost =
                    new RelatedSearchesSectionHost() {
                        @Override
                        public float getYPositionPx() {
                            // Position the carousel at the bottom part of the bar as it animates to
                            // a taller size.
                            return Math.round(
                                    (getOffsetY()
                                                    + getBarContainerHeight()
                                                    - getInBarRelatedSearchesAnimatedHeightDps())
                                            / mPxToDp);
                        }

                        @Override
                        public void onPanelSectionSizeChange(boolean hasStarted) {
                            // This section currently doesn't change size, so we can ignore this.
                        }

                        @Override
                        public void onSuggestionClicked(int selectionIndex) {
                            mManagementDelegate.onRelatedSearchesSuggestionClicked(selectionIndex);
                        }
                    };
        }
        return mRelatedSearchesInBarHost;
    }

    // ============================================================================================
    // Panel Content
    // ============================================================================================

    @Override
    public void onTouchSearchContentViewAck() {
        mHasContentBeenTouched = true;
    }

    /**
     * Destroy the current content in the panel. NOTE(mdjones): This should not be exposed. The only
     * use is in ContextualSearchManager for a bug related to loading new panel content.
     */
    public void destroyContent() {
        super.destroyOverlayPanelContent();
    }

    /**
     * @return Whether the panel content can be displayed in a new tab.
     */
    public boolean canPromoteToNewTab() {
        return mCanPromoteToNewTab;
    }

    // ============================================================================================
    // Testing Support
    // ============================================================================================

    /** Simulates a tap on the panel's end button. */
    @VisibleForTesting
    public void simulateTapOnEndButton() {
        endHeightAnimation();

        // Determine the x-position for the simulated tap.
        float xPosition;
        if (LocalizationUtils.isLayoutRtl()) {
            xPosition = getContentX() + (mEndButtonWidthDp / 2);
        } else {
            xPosition = getContentX() + getWidth() - (mEndButtonWidthDp / 2);
        }

        // Determine the y-position for the simulated tap.
        float yPosition = getOffsetY() + (getHeight() / 2);

        // Simulate the tap.
        handleClick(xPosition, yPosition);
    }

    /**
     * Updates the panel as if a transition from one state to the given state has just been
     * completed. The caller should first set the panel to the supplied "to" state. This method just
     * makes the panel notify its subcomponents that the transition has been completed.
     * @param panelState The "to" state that has just been completed by the test.
     */
    public void updatePanelToStateForTest(@PanelState int panelState) {
        // Use a switch to just support the implemented state(s) and fail if others are attempted.
        switch (panelState) {
            case PanelState.EXPANDED:
                updatePanelForExpansion(1.0f);
                break;
        }
    }

    @Override
    @VisibleForTesting
    public boolean getCanHideAndroidBrowserControls() {
        return super.getCanHideAndroidBrowserControls();
    }

    @Override
    @VisibleForTesting
    public OverlayPanelContent getOverlayPanelContent() {
        return super.getOverlayPanelContent();
    }

    public void setEdgeToEdgeControllerSupplierForTesting(
            Supplier<EdgeToEdgeController> edgeToEdgeControllerSupplier) {
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;
    }
}
