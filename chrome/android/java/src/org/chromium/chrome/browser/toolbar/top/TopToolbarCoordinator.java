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
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.Invalidator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.findinpage.FindToolbar;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.identity_disc.IdentityDiscController;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.TabCountProvider;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.chrome.browser.toolbar.ToolbarTabController;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButton;
import org.chromium.chrome.browser.toolbar.menu_button.MenuButtonCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;

import java.util.List;

/**
 * A coordinator for the top toolbar component.
 */
public class TopToolbarCoordinator implements Toolbar {
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

    public static final int TAB_SWITCHER_MODE_NORMAL_ANIMATION_DURATION_MS = 200;
    public static final int TAB_SWITCHER_MODE_GTS_ANIMATION_DURATION_MS = 150;

    private final ToolbarLayout mToolbarLayout;

    /**
     * The coordinator for the tab switcher mode toolbar (phones only). This will be lazily created
     * after ToolbarLayout is inflated.
     */
    private @Nullable TabSwitcherModeTTCoordinatorPhone mTabSwitcherModeCoordinatorPhone;
    /**
     * The coordinator for the start surface mode toolbar (phones only) if the StartSurface is
     * enabled. This will be lazily created after ToolbarLayout is inflated.
     */
    private @Nullable StartSurfaceToolbarCoordinator mStartSurfaceToolbarCoordinator;

    private final IdentityDiscController mIdentityDiscController;
    private OptionalBrowsingModeButtonController mOptionalButtonController;

    private Callback<OverviewModeBehavior> mOverviewModeBehaviorSupplierObserver;
    private ObservableSupplier<OverviewModeBehavior> mOverviewModeBehaviorSupplier;
    private MenuButtonCoordinator mMenuButtonCoordinator;
    private ObservableSupplier<AppMenuButtonHelper> mAppMenuButtonHelperSupplier;

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
     * @param identityDiscController Class that controls the state of the identity disc.
     * @param userEducationHelper Helper class for showing in-product help text bubbles.
     * @param buttonDataProviders List of classes that wish to display an optional button in the
     *         browsing mode toolbar.
     * @param overviewModeBehaviorSupplier Supplier of the overview mode manager for the current
     *                                     profile.
     * @param normalThemeColorProvider The {@link ThemeColorProvider} for normal mode.
     * @param overviewThemeColorProvider The {@link ThemeColorProvider} for overview mode.
     */
    public TopToolbarCoordinator(ToolbarControlContainer controlContainer,
            ToolbarLayout toolbarLayout, IdentityDiscController identityDiscController,
            ToolbarDataProvider toolbarDataProvider, ToolbarTabController tabController,
            UserEducationHelper userEducationHelper, List<ButtonDataProvider> buttonDataProviders,
            ObservableSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier,
            ThemeColorProvider normalThemeColorProvider,
            ThemeColorProvider overviewThemeColorProvider,
            MenuButtonCoordinator menuButtonCoordinator,
            ObservableSupplier<AppMenuButtonHelper> appMenuButtonHelperSupplier) {
        mToolbarLayout = toolbarLayout;
        mIdentityDiscController = identityDiscController;
        mMenuButtonCoordinator = menuButtonCoordinator;
        mOptionalButtonController = new OptionalBrowsingModeButtonController(buttonDataProviders,
                userEducationHelper, mToolbarLayout, () -> toolbarDataProvider.getTab());

        mOverviewModeBehaviorSupplier = overviewModeBehaviorSupplier;
        mOverviewModeBehaviorSupplierObserver = this::setOverviewModeBehavior;
        mOverviewModeBehaviorSupplier.addObserver(mOverviewModeBehaviorSupplierObserver);

        if (mToolbarLayout instanceof ToolbarPhone) {
            if (StartSurfaceConfiguration.isStartSurfaceEnabled()) {
                mStartSurfaceToolbarCoordinator = new StartSurfaceToolbarCoordinator(
                        controlContainer.getRootView().findViewById(R.id.tab_switcher_toolbar_stub),
                        mIdentityDiscController, userEducationHelper, mOverviewModeBehaviorSupplier,
                        overviewThemeColorProvider);
            } else {
                mTabSwitcherModeCoordinatorPhone = new TabSwitcherModeTTCoordinatorPhone(
                        controlContainer.getRootView().findViewById(
                                R.id.tab_switcher_toolbar_stub));
            }
        }
        controlContainer.setToolbar(this);
        HomepageManager.getInstance().addListener(mHomepageStateListener);
        mToolbarLayout.initialize(toolbarDataProvider, tabController);

        final MenuButton menuButtonWrapper = getMenuButtonWrapper();
        if (menuButtonWrapper != null) {
            menuButtonWrapper.setThemeColorProvider(normalThemeColorProvider);
        }
        mToolbarLayout.setThemeColorProvider(normalThemeColorProvider);
        mAppMenuButtonHelperSupplier = appMenuButtonHelperSupplier;
        new OneShotCallback<>(mAppMenuButtonHelperSupplier, this::setAppMenuButtonHelper);
    }

    /**
     * @param appMenuButtonHelper The helper for managing menu button interactions.
     */
    public void setAppMenuButtonHelper(AppMenuButtonHelper appMenuButtonHelper) {
        mToolbarLayout.setAppMenuButtonHelper(appMenuButtonHelper);
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.setAppMenuButtonHelper(appMenuButtonHelper);
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.setAppMenuButtonHelper(appMenuButtonHelper);
        }
    }

    /**
     * Initialize the coordinator with the components that have native initialization dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     *
     * @param tabModelSelector The selector that handles tab management.
     * @param layoutManager A {@link LayoutManager} instance used to watch for scene changes.
     * @param tabSwitcherClickHandler The click handler for the tab switcher button.
     * @param tabSwitcherLongClickHandler The long click handler for the tab switcher button.
     * @param newTabClickHandler The click handler for the new tab button.
     * @param bookmarkClickHandler The click handler for the bookmarks button.
     * @param customTabsBackClickHandler The click handler for the custom tabs back button.
     */
    public void initializeWithNative(TabModelSelector tabModelSelector, LayoutManager layoutManager,
            OnClickListener tabSwitcherClickHandler,
            OnLongClickListener tabSwitcherLongClickHandler, OnClickListener newTabClickHandler,
            OnClickListener bookmarkClickHandler, OnClickListener customTabsBackClickHandler) {
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.setOnTabSwitcherClickHandler(tabSwitcherClickHandler);
            mTabSwitcherModeCoordinatorPhone.setOnNewTabClickHandler(newTabClickHandler);
            mTabSwitcherModeCoordinatorPhone.setTabModelSelector(tabModelSelector);
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.setOnNewTabClickHandler(newTabClickHandler);
            mStartSurfaceToolbarCoordinator.setTabModelSelector(tabModelSelector);
            mStartSurfaceToolbarCoordinator.setTabSwitcherListener(tabSwitcherClickHandler);
            mStartSurfaceToolbarCoordinator.setOnTabSwitcherLongClickHandler(
                    tabSwitcherLongClickHandler);
            mStartSurfaceToolbarCoordinator.onNativeLibraryReady();
        }

        mToolbarLayout.setTabModelSelector(tabModelSelector);
        getLocationBar().updateVisualsForState();
        getLocationBar().setUrlToPageUrl();
        mToolbarLayout.setOnTabSwitcherClickHandler(tabSwitcherClickHandler);
        mToolbarLayout.setOnTabSwitcherLongClickHandler(tabSwitcherLongClickHandler);
        mToolbarLayout.setBookmarkClickHandler(bookmarkClickHandler);
        mToolbarLayout.setCustomTabCloseClickHandler(customTabsBackClickHandler);
        mToolbarLayout.setLayoutUpdateHost(layoutManager);

        mToolbarLayout.onNativeLibraryReady();
    }

    private void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        assert overviewModeBehavior != null;
        mToolbarLayout.setOverviewModeBehavior(overviewModeBehavior);
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
        if (mOverviewModeBehaviorSupplier != null) {
            mOverviewModeBehaviorSupplier.removeObserver(mOverviewModeBehaviorSupplierObserver);
            mOverviewModeBehaviorSupplier = null;
            mOverviewModeBehaviorSupplierObserver = null;
        }
        mToolbarLayout.destroy();
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.destroy();
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.destroy();
        }

        if (mOptionalButtonController != null) {
            mOptionalButtonController.destroy();
            mOptionalButtonController = null;
        }

        if (mAppMenuButtonHelperSupplier != null) {
            mAppMenuButtonHelperSupplier = null;
        }
    }

    @Override
    public void disableMenuButton() {
        mMenuButtonCoordinator.disableMenuButton();
        mToolbarLayout.onMenuButtonDisabled();
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
        // TODO(https://crbug.com/1041123): Investigate whether isInOverviewAndShowingOmnibox check
        // is needed.
        if (mStartSurfaceToolbarCoordinator == null
                || mToolbarLayout.getToolbarDataProvider() == null
                || !mToolbarLayout.getToolbarDataProvider().isInOverviewAndShowingOmnibox()) {
            return;
        }
        mStartSurfaceToolbarCoordinator.setStartSurfaceToolbarVisibility(requestToShow);
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
        mOptionalButtonController.updateButtonVisibility();
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
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.onAccessibilityStatusChanged(enabled);
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
        mToolbarLayout.setTabSwitcherMode(
                inTabSwitcherMode, showToolbar, delayAnimation, mMenuButtonCoordinator);
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.setTabSwitcherMode(inTabSwitcherMode);
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.setStartSurfaceMode(inTabSwitcherMode);
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
        if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.setTabCountProvider(tabCountProvider);
        }
    }

    /**
     * @param provider The provider used to determine incognito state.
     */
    public void setIncognitoStateProvider(IncognitoStateProvider provider) {
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.setIncognitoStateProvider(provider);
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.setIncognitoStateProvider(provider);
        }
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

    /**
     * @param isVisible Whether the bottom toolbar is visible.
     */
    public void onBottomToolbarVisibilityChanged(boolean isVisible) {
        mToolbarLayout.onBottomToolbarVisibilityChanged(isVisible);
        if (mTabSwitcherModeCoordinatorPhone != null) {
            mTabSwitcherModeCoordinatorPhone.onBottomToolbarVisibilityChanged(isVisible);
        } else if (mStartSurfaceToolbarCoordinator != null) {
            mStartSurfaceToolbarCoordinator.onBottomToolbarVisibilityChanged(isVisible);
        }
        mOptionalButtonController.updateButtonVisibility();
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

    @VisibleForTesting
    public StartSurfaceToolbarCoordinator getStartSurfaceToolbarForTesting() {
        return mStartSurfaceToolbarCoordinator;
    }
}
