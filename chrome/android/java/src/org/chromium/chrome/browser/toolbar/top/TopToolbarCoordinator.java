// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.widget.ImageButton;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.Invalidator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.findinpage.FindToolbar;
import org.chromium.chrome.browser.fullscreen.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.MenuButton;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;

/**
 * A coordinator for the top toolbar component.
 */
public class TopToolbarCoordinator implements Toolbar {
    static final int TAB_SWITCHER_MODE_NORMAL_ANIMATION_DURATION_MS = 200;
    static final int TAB_SWITCHER_MODE_GTS_ANIMATION_DURATION_MS = 150;

    /**
     * Observes toolbar URL expansion percentage change.
     */
    public interface UrlExpansionObserver {
        /**
         * Notified when toolbar URL expansion percentage changes.
         * @param percentage The toolbar expansion percentage. 0 indicates that the URL bar is not
         *                   expanded. 1 indicates that the URL bar is expanded to the maximum
         *                   width.
         */
        void onUrlExpansionPercentageChanged(float percentage);
    }

    private final ToolbarLayout mToolbarLayout;

    /**
     * The coordinator for the tab switcher mode toolbar (phones only). This will be lazily created
     * after ToolbarLayout is inflated.
     */
    private @Nullable TabSwitcherModeTTCoordinatorPhone mTabSwitcherModeCoordinatorPhone;

    private HomepageManager.HomepageStateListener mHomepageStateListener =
            new HomepageManager.HomepageStateListener() {
                @Override
                public void onHomepageStateUpdated() {
                    mToolbarLayout.onHomeButtonUpdate(HomepageManager.isHomepageEnabled());
                }
            };

    /**
     * Creates a new {@link TopToolbarCoordinator}.
     * @param controlContainer The {@link ToolbarControlContainer} for the containing activity.
     * @param toolbarLayout The {@link ToolbarLayout}.
     */
    public TopToolbarCoordinator(
            ToolbarControlContainer controlContainer, ToolbarLayout toolbarLayout) {
        mToolbarLayout = toolbarLayout;
        if (mToolbarLayout instanceof ToolbarPhone) {
            mTabSwitcherModeCoordinatorPhone = new TabSwitcherModeTTCoordinatorPhone(
                    controlContainer.getRootView().findViewById(R.id.tab_switcher_toolbar_stub));
        }
        controlContainer.setToolbar(this);
        HomepageManager.getInstance().addListener(mHomepageStateListener);
    }

    /**
     * Initialize the external dependencies required for view interaction.
     * @param toolbarDataProvider The provider for toolbar data.
     * @param tabController       The controller that handles interactions with the tab.
     */
    public void initialize(
            ToolbarDataProvider toolbarDataProvider, ToolbarTabController tabController) {
        mToolbarLayout.initialize(toolbarDataProvider, tabController);
    }

    /**
     * @param appMenuButtonHelper The helper for managing menu button interactions.
     */
    public void setAppMenuButtonHelper(AppMenuButtonHelper appMenuButtonHelper) {
        mToolbarLayout.setAppMenuButtonHelper(appMenuButtonHelper);
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.setAppMenuButtonHelper(appMenuButtonHelper);
        }
    }

    /**
     * Initialize the coordinator with the components that have native initialization dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     *
     * @param tabModelSelector The selector that handles tab management.
     * @param controlsVisibilityDelegate The delegate to handle visibility of browser controls.
     * @param layoutManager A {@link LayoutManager} instance used to watch for scene changes.
     * @param tabSwitcherClickHandler The click handler for the tab switcher button.
     * @param tabSwitcherLongClickHandler The long click handler for the tab switcher button.
     * @param newTabClickHandler The click handler for the new tab button.
     * @param bookmarkClickHandler The click handler for the bookmarks button.
     * @param customTabsBackClickHandler The click handler for the custom tabs back button.
     */
    public void initializeWithNative(TabModelSelector tabModelSelector,
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate,
            LayoutManager layoutManager, OnClickListener tabSwitcherClickHandler,
            OnLongClickListener tabSwitcherLongClickHandler, OnClickListener newTabClickHandler,
            OnClickListener bookmarkClickHandler, OnClickListener customTabsBackClickHandler) {
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.setOnTabSwitcherClickHandler(tabSwitcherClickHandler);
            mTabSwitcherModeCoordinatorPhone.setOnNewTabClickHandler(newTabClickHandler);
            mTabSwitcherModeCoordinatorPhone.setTabModelSelector(tabModelSelector);
        }

        mToolbarLayout.setTabModelSelector(tabModelSelector);
        getLocationBar().updateVisualsForState();
        getLocationBar().setUrlToPageUrl();
        mToolbarLayout.setBrowserControlsVisibilityDelegate(controlsVisibilityDelegate);
        mToolbarLayout.setOnTabSwitcherClickHandler(tabSwitcherClickHandler);
        mToolbarLayout.setOnTabSwitcherLongClickHandler(tabSwitcherLongClickHandler);
        mToolbarLayout.setBookmarkClickHandler(bookmarkClickHandler);
        mToolbarLayout.setCustomTabCloseClickHandler(customTabsBackClickHandler);
        mToolbarLayout.setLayoutUpdateHost(layoutManager);

        mToolbarLayout.onNativeLibraryReady();
    }

    /**
     * @param urlExpansionObserver The observer that observes URL expansion percentage change.
     */
    public void addUrlExpansionObserver(UrlExpansionObserver urlExpansionObserver) {
        mToolbarLayout.addUrlExpansionObserver(urlExpansionObserver);
    }

    /**
     * @param urlExpansionObserver The observer that observes URL expansion percentage change.
     */
    public void removeUrlExpansionObserver(UrlExpansionObserver urlExpansionObserver) {
        mToolbarLayout.removeUrlExpansionObserver(urlExpansionObserver);
    }

    /**
     * @see View#addOnAttachStateChangeListener(View.OnAttachStateChangeListener)
     */
    public void addOnAttachStateChangeListener(View.OnAttachStateChangeListener listener) {
        mToolbarLayout.addOnAttachStateChangeListener(listener);
    }

    /**
     * Cleans up any code as necessary.
     */
    public void destroy() {
        HomepageManager.getInstance().removeListener(mHomepageStateListener);
        mToolbarLayout.destroy();
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.destroy();
        }
    }

    @Override
    public void disableMenuButton() {
        mToolbarLayout.disableMenuButton();
    }

    /** Notified that the menu was shown. */
    public void onMenuShown() {
        mToolbarLayout.onMenuShown();
    }

    /**
     * @return The wrapper for the browsing mode toolbar's menu button.
     */
    public MenuButton getMenuButtonWrapper() {
        View menuButtonWrapper = mToolbarLayout.getMenuButtonWrapper();
        if (menuButtonWrapper instanceof MenuButton) return (MenuButton) menuButtonWrapper;
        return null;
    }

    /**
     * @return The {@link ImageButton} containing the menu button.
     */
    public @Nullable ImageButton getMenuButton() {
        return mToolbarLayout.getMenuButton();
    }

    @Override
    public ToolbarProgressBar getProgressBar() {
        return mToolbarLayout.getProgressBar();
    }

    @Override
    public int getPrimaryColor() {
        return mToolbarLayout.getToolbarDataProvider().getPrimaryColor();
    }

    @Override
    public void updateTabSwitcherToolbarState(boolean requestToShow) {
        if (mTabSwitcherModeCoordinatorPhone == null
                || mToolbarLayout.getToolbarDataProvider() == null
                || !mToolbarLayout.getToolbarDataProvider().isInOverviewAndShowingOmnibox()) {
            return;
        }

        mTabSwitcherModeCoordinatorPhone.setTabSwitcherToolbarVisibility(requestToShow);
    }

    @Override
    public void getPositionRelativeToContainer(View containerView, int[] position) {
        mToolbarLayout.getPositionRelativeToContainer(containerView, position);
    }

    /**
     * Sets the {@link Invalidator} that will be called when the toolbar attempts to invalidate the
     * drawing surface.  This will give the object that registers as the host for the
     * {@link Invalidator} a chance to defer the actual invalidate to sync drawing.
     * @param invalidator An {@link Invalidator} instance.
     */
    public void setPaintInvalidator(Invalidator invalidator) {
        mToolbarLayout.setPaintInvalidator(invalidator);
    }

    /**
     * Gives inheriting classes the chance to respond to
     * {@link FindToolbar} state changes.
     * @param showing Whether or not the {@code FindToolbar} will be showing.
     */
    public void handleFindLocationBarStateChange(boolean showing) {
        mToolbarLayout.handleFindLocationBarStateChange(showing);
    }

    /**
     * Sets whether the urlbar should be hidden on first page load.
     */
    public void setUrlBarHidden(boolean hidden) {
        mToolbarLayout.setUrlBarHidden(hidden);
    }

    /**
     * @return The name of the publisher of the content if it can be reliably extracted, or null
     *         otherwise.
     */
    public String getContentPublisher() {
        return mToolbarLayout.getContentPublisher();
    }

    /**
     * Tells the Toolbar to update what buttons it is currently displaying.
     */
    public void updateButtonVisibility() {
        mToolbarLayout.updateButtonVisibility();
    }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * back button.
     * @param canGoBack Whether or not the current tab has any history to go back to.
     */
    public void updateBackButtonVisibility(boolean canGoBack) {
        mToolbarLayout.updateBackButtonVisibility(canGoBack);
    }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * forward button.
     * @param canGoForward Whether or not the current tab has any history to go forward to.
     */
    public void updateForwardButtonVisibility(boolean canGoForward) {
        mToolbarLayout.updateForwardButtonVisibility(canGoForward);
    }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * reload button.
     * @param isReloading Whether or not the current tab is loading.
     */
    public void updateReloadButtonVisibility(boolean isReloading) {
        mToolbarLayout.updateReloadButtonVisibility(isReloading);
    }

    /**
     * Gives inheriting classes the chance to update the visual status of the
     * bookmark button.
     * @param isBookmarked Whether or not the current tab is already bookmarked.
     * @param editingAllowed Whether or not bookmarks can be modified (added, edited, or removed).
     */
    public void updateBookmarkButton(boolean isBookmarked, boolean editingAllowed) {
        mToolbarLayout.updateBookmarkButton(isBookmarked, editingAllowed);
    }

    /**
     * Gives inheriting classes the chance to respond to accessibility state changes.
     * @param enabled Whether or not accessibility is enabled.
     */
    public void onAccessibilityStatusChanged(boolean enabled) {
        mToolbarLayout.onAccessibilityStatusChanged(enabled);
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.onAccessibilityStatusChanged(enabled);
        }
    }

    /**
     * Gives inheriting classes the chance to do the necessary UI operations after Chrome is
     * restored to a previously saved state.
     */
    public void onStateRestored() {
        mToolbarLayout.onStateRestored();
    }

    /**
     * Triggered when the current tab or model has changed.
     * <p>
     * As there are cases where you can select a model with no tabs (i.e. having incognito
     * tabs but no normal tabs will still allow you to select the normal model), this should
     * not guarantee that the model's current tab is non-null.
     */
    public void onTabOrModelChanged() {
        mToolbarLayout.onTabOrModelChanged();
    }

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    public void onPrimaryColorChanged(boolean shouldAnimate) {
        mToolbarLayout.onPrimaryColorChanged(shouldAnimate);
    }

    /**
     * Sets whether a title should be shown within the Toolbar.
     * @param showTitle Whether a title should be shown.
     */
    public void setShowTitle(boolean showTitle) {
        getLocationBar().setShowTitle(showTitle);
    }

    /**
     * Sets the icon drawable that the close button in the toolbar (if any) should show, or hides
     * it if {@code drawable} is {@code null}.
     */
    public void setCloseButtonImageResource(@Nullable Drawable drawable) {
        mToolbarLayout.setCloseButtonImageResource(drawable);
    }

    /**
     * Adds a custom action button to the toolbar layout, if it is supported.
     * @param drawable The icon for the button.
     * @param description The content description for the button.
     * @param listener The {@link View.OnClickListener} to use for clicks to the button.
     */
    public void addCustomActionButton(
            Drawable drawable, String description, View.OnClickListener listener) {
        mToolbarLayout.addCustomActionButton(drawable, description, listener);
    }

    /**
     * Updates the visual appearance of a custom action button in the toolbar layout,
     * if it is supported.
     * @param index The index of the button.
     * @param drawable The icon for the button.
     * @param description The content description for the button.
     */
    public void updateCustomActionButton(int index, Drawable drawable, String description) {
        mToolbarLayout.updateCustomActionButton(index, drawable, description);
    }

    @Override
    public int getTabStripHeight() {
        return mToolbarLayout.getTabStripHeight();
    }

    /**
     * Triggered when the content view for the specified tab has changed.
     */
    public void onTabContentViewChanged() {
        mToolbarLayout.onTabContentViewChanged();
    }

    @Override
    public boolean isReadyForTextureCapture() {
        return mToolbarLayout.isReadyForTextureCapture();
    }

    @Override
    public boolean setForceTextureCapture(boolean forceTextureCapture) {
        return mToolbarLayout.setForceTextureCapture(forceTextureCapture);
    }

    /**
     * @param attached Whether or not the web content is attached to the view heirarchy.
     */
    public void setContentAttached(boolean attached) {
        mToolbarLayout.setContentAttached(attached);
    }

    /**
     * Gives inheriting classes the chance to show or hide the TabSwitcher mode of this toolbar.
     * @param inTabSwitcherMode Whether or not TabSwitcher mode should be shown or hidden.
     * @param showToolbar    Whether or not to show the normal toolbar while animating.
     * @param delayAnimation Whether or not to delay the animation until after the transition has
     *                       finished (which can be detected by a call to
     *                       {@link #onTabSwitcherTransitionFinished()}).
     */
    public void setTabSwitcherMode(
            boolean inTabSwitcherMode, boolean showToolbar, boolean delayAnimation) {
        mToolbarLayout.setTabSwitcherMode(inTabSwitcherMode, showToolbar, delayAnimation);
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.setTabSwitcherMode(inTabSwitcherMode);
        }
    }

    /**
     * Gives inheriting classes the chance to update their state when the TabSwitcher transition has
     * finished.
     */
    public void onTabSwitcherTransitionFinished() {
        mToolbarLayout.onTabSwitcherTransitionFinished();
    }

    /**
     * Gives inheriting classes the chance to observe tab count changes.
     * @param tabCountProvider The {@link TabCountProvider} subclasses can observe.
     */
    public void setTabCountProvider(TabCountProvider tabCountProvider) {
        mToolbarLayout.setTabCountProvider(tabCountProvider);
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.setTabCountProvider(tabCountProvider);
        }
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    public void setIncognitoStateProvider(IncognitoStateProvider provider) {
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.setIncognitoStateProvider(provider);
        }
    }

    /**
     * @param provider The provider used to determine theme color.
     */
    public void setThemeColorProvider(ThemeColorProvider provider) {
        final MenuButton menuButtonWrapper = getMenuButtonWrapper();
        if (menuButtonWrapper != null) {
            menuButtonWrapper.setThemeColorProvider(provider);
        }
        mToolbarLayout.setThemeColorProvider(provider);
    }

    /**
     * Gives inheriting classes the chance to update themselves based on default search engine
     * changes.
     */
    public void onDefaultSearchEngineChanged() {
        mToolbarLayout.onDefaultSearchEngineChanged();
    }

    @Override
    public void getLocationBarContentRect(Rect outRect) {
        mToolbarLayout.getLocationBarContentRect(outRect);
    }

    @Override
    public void setTextureCaptureMode(boolean textureMode) {
        mToolbarLayout.setTextureCaptureMode(textureMode);
    }

    @Override
    public boolean shouldIgnoreSwipeGesture() {
        return mToolbarLayout.shouldIgnoreSwipeGesture();
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    public void onUrlFocusChange(boolean hasFocus) {
        mToolbarLayout.onUrlFocusChange(hasFocus);
    }

    /**
     * Returns the elapsed realtime in ms of the time at which first draw for the toolbar occurred.
     */
    public long getFirstDrawTime() {
        return mToolbarLayout.getFirstDrawTime();
    }

    /**
     * Notified when a navigation to a different page has occurred.
     */
    public void onNavigatedToDifferentPage() {
        mToolbarLayout.onNavigatedToDifferentPage();
    }

    /**
     * @param enabled Whether the progress bar is enabled.
     */
    public void setProgressBarEnabled(boolean enabled) {
        getProgressBar().setVisibility(enabled ? View.VISIBLE : View.GONE);
    }

    /**
     * @param anchor The view to use as an anchor.
     */
    public void setProgressBarAnchorView(@Nullable View anchor) {
        getProgressBar().setAnchorView(anchor);
    }

    /**
     * Starts load progress.
     */
    public void startLoadProgress() {
        mToolbarLayout.startLoadProgress();
    }

    /**
     * Sets load progress.
     * @param progress The load progress between 0 and 1.
     */
    public void setLoadProgress(float progress) {
        mToolbarLayout.setLoadProgress(progress);
    }

    /**
     * Finishes load progress.
     * @param delayed Whether hiding progress bar should be delayed to give enough time for user to
     *                        recognize the last state.
     */
    public void finishLoadProgress(boolean delayed) {
        mToolbarLayout.finishLoadProgress(delayed);
    }

    /**
     * @return True if the progress bar is started.
     */
    public boolean isProgressStarted() {
        return mToolbarLayout.isProgressStarted();
    }

    /**
     * Finish any toolbar animations.
     */
    public void finishAnimations() {
        mToolbarLayout.finishAnimations();
    }

    /**
     * @return {@link LocationBar} object this {@link ToolbarLayout} contains.
     */
    public LocationBar getLocationBar() {
        return mToolbarLayout.getLocationBar();
    }

    @Override
    public void setMenuButtonHighlight(boolean highlight) {
        mToolbarLayout.setMenuButtonHighlight(highlight);
    }

    @Override
    public void showAppMenuUpdateBadge() {
        mToolbarLayout.showAppMenuUpdateBadge(true);
    }

    @Override
    public boolean isShowingAppMenuUpdateBadge() {
        return mToolbarLayout.isShowingAppMenuUpdateBadge();
    }

    @Override
    public void removeAppMenuUpdateBadge(boolean animate) {
        mToolbarLayout.removeAppMenuUpdateBadge(animate);
    }

    /**
     * Enable the experimental toolbar button.
     * @param onClickListener The {@link View.OnClickListener} to be called when the button is
     *                        clicked.
     * @param image The drawable to display for the button.
     * @param contentDescriptionResId The resource id of the content description for the button.
     */
    public void enableExperimentalButton(View.OnClickListener onClickListener, Drawable image,
            @StringRes int contentDescriptionResId) {
        mToolbarLayout.enableExperimentalButton(onClickListener, image, contentDescriptionResId);
    }

    /**
     * @param isVisible Whether the bottom toolbar is visible.
     */
    public void onBottomToolbarVisibilityChanged(boolean isVisible) {
        mToolbarLayout.onBottomToolbarVisibilityChanged(isVisible);
    }

    /**
     * @return The experimental toolbar button if it exists.
     */
    public void updateExperimentalButtonImage(Drawable image) {
        mToolbarLayout.updateExperimentalButtonImage(image);
    }

    /**
     * Disable the experimental toolbar button.
     */
    public void disableExperimentalButton() {
        mToolbarLayout.disableExperimentalButton();
    }

    /**
     * Displays in-product help for experimental button.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param dismissedCallback The callback that will be called when in-product help is dismissed.
     */
    public void showIPHOnExperimentalButton(@StringRes int stringId,
            @StringRes int accessibilityStringId, Runnable dismissedCallback) {
        mToolbarLayout.showIPHOnExperimentalButton(
                stringId, accessibilityStringId, dismissedCallback);
    }

    @Override
    public int getHeight() {
        return mToolbarLayout.getHeight();
    }

    /**
     * @return The {@link ToolbarLayout} that constitutes the toolbar.
     */
    @VisibleForTesting
    public ToolbarLayout getToolbarLayoutForTesting() {
        return mToolbarLayout;
    }
}
