// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.support.annotation.DrawableRes;
import android.support.annotation.Nullable;
import android.support.annotation.StringRes;
import android.support.v4.view.ViewCompat;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.AppCompatImageButton;
import android.util.AttributeSet;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ProgressBar;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.compositor.Invalidator;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.fullscreen.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.PulseDrawable;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.ToolbarProgressBar;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.ui.UiUtils;

/**
 * Layout class that contains the base shared logic for manipulating the toolbar component. For
 * interaction that are not from Views inside Toolbar hierarchy all interactions should be done
 * through {@link Toolbar} rather than using this class directly.
 */
public abstract class ToolbarLayout extends FrameLayout implements Toolbar {
    private Invalidator mInvalidator;

    private final int[] mTempPosition = new int[2];

    /**
     * The app menu button.
     */
    protected AppCompatImageButton mMenuButton;
    private ImageView mMenuBadge;
    private MenuButton mMenuButtonWrapper;
    private AppMenuButtonHelper mAppMenuButtonHelper;

    protected final ColorStateList mDarkModeTint;
    protected final ColorStateList mLightModeTint;

    private ToolbarDataProvider mToolbarDataProvider;
    private ToolbarTabController mToolbarTabController;
    @Nullable
    protected ToolbarProgressBar mProgressBar;

    private boolean mNativeLibraryReady;
    private boolean mUrlHasFocus;

    private long mFirstDrawTimeMs;

    private boolean mFindInPageToolbarShowing;

    protected boolean mHighlightingMenu;
    private PulseDrawable mHighlightDrawable;

    protected boolean mShowMenuBadge;
    private AnimatorSet mMenuBadgeAnimatorSet;
    private boolean mIsMenuBadgeAnimationRunning;

    /**
     * Basic constructor for {@link ToolbarLayout}.
     */
    public ToolbarLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mDarkModeTint = AppCompatResources.getColorStateList(getContext(), R.color.dark_mode_tint);
        mLightModeTint =
                AppCompatResources.getColorStateList(getContext(), R.color.light_mode_tint);
        mProgressBar = createProgressBar();

        addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View view, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (isNativeLibraryReady() && mProgressBar.getParent() != null) {
                    mProgressBar.initializeAnimation();
                }
                mProgressBar.setTopMargin(getProgressBarTopMargin());

                // Since this only needs to happen once, remove this listener from the view.
                removeOnLayoutChangeListener(this);
            }
        });
    }

    /**
     * Get the top margin of the progress bar relative to the toolbar layout. This is used to set
     * the position of the progress bar (either top or bottom of the toolbar).
     * @return The top margin of the progress bar.
     */
    protected int getProgressBarTopMargin() {
        return getHeight()
                - getResources().getDimensionPixelSize(R.dimen.toolbar_progress_bar_height);
    }

    /**
     * Set the height that the progress bar should be.
     * @return The progress bar height in px.
     */
    protected int getProgressBarHeight() {
        return getResources().getDimensionPixelSize(R.dimen.toolbar_progress_bar_height);
    }

    /**
     * @return A progress bar for Chrome to use.
     */
    protected ToolbarProgressBar createProgressBar() {
        return new ToolbarProgressBar(
                getContext(), getProgressBarHeight(), getProgressBarTopMargin(), false);
    }

    /**
     * Disable the menu button. This removes the view from the hierarchy and nulls the related
     * instance vars.
     */
    public void disableMenuButton() {
        UiUtils.removeViewFromParent(getMenuButtonWrapper());
        mMenuButtonWrapper = null;
        mMenuButton = null;
        mMenuBadge = null;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mMenuButton = (AppCompatImageButton) findViewById(R.id.menu_button);
        mMenuBadge = (ImageView) findViewById(R.id.menu_badge);
        mMenuButtonWrapper = findViewById(R.id.menu_button_wrapper);

        // Initialize the provider to an empty version to avoid null checking everywhere.
        mToolbarDataProvider = new ToolbarDataProvider() {
            @Override
            public boolean isIncognito() {
                return false;
            }

            @Override
            public Profile getProfile() {
                return null;
            }

            @Override
            public Tab getTab() {
                return null;
            }

            @Override
            public boolean hasTab() {
                return false;
            }

            @Override
            public String getCurrentUrl() {
                return "";
            }

            @Override
            public UrlBarData getUrlBarData() {
                return UrlBarData.EMPTY;
            }

            @Override
            public String getTitle() {
                return "";
            }

            @Override
            public NewTabPage getNewTabPageForCurrentTab() {
                return null;
            }

            @Override
            public int getPrimaryColor() {
                return 0;
            }

            @Override
            public boolean isUsingBrandColor() {
                return false;
            }

            @Override
            public boolean isOfflinePage() {
                return false;
            }

            @Override
            public boolean isPreview() {
                return false;
            }

            @Override
            public boolean shouldShowVerboseStatus() {
                return false;
            }

            @Override
            public int getSecurityLevel() {
                return ConnectionSecurityLevel.NONE;
            }

            @Override
            public int getSecurityIconResource(boolean isTablet) {
                return 0;
            }

            @Override
            public ColorStateList getSecurityIconColorStateList() {
                return null;
            }

            @Override
            public boolean shouldDisplaySearchTerms() {
                return false;
            }
        };

        // Set menu button background in case it was previously called before inflation
        // finished (i.e. mMenuButtonWrapper == null)
        setMenuButtonHighlightDrawable(mHighlightingMenu);
    }

    /**
     * Quick getter for LayoutParams for a View inside a FrameLayout.
     * @param view {@link View} to fetch the layout params for.
     * @return {@link LayoutParams} the given {@link View} is currently using.
     */
    protected FrameLayout.LayoutParams getFrameLayoutParams(View view) {
        return ((FrameLayout.LayoutParams) view.getLayoutParams());
    }

    /**
     * Initialize the external dependencies required for view interaction.
     * @param toolbarDataProvider The provider for toolbar data.
     * @param tabController       The controller that handles interactions with the tab.
     * @param appMenuButtonHelper The helper for managing menu button interactions.
     */
    public void initialize(ToolbarDataProvider toolbarDataProvider,
            ToolbarTabController tabController, AppMenuButtonHelper appMenuButtonHelper) {
        mToolbarDataProvider = toolbarDataProvider;
        mToolbarTabController = tabController;

        mAppMenuButtonHelper = appMenuButtonHelper;

        if (mMenuButton != null) {
            mMenuButton.setOnTouchListener(mAppMenuButtonHelper);
            mMenuButton.setAccessibilityDelegate(mAppMenuButtonHelper);
        }
    }

    /** Notified that the menu was shown. */
    protected void onMenuShown() {}

    /**
     *  This function handles native dependent initialization for this class
     */
    public void onNativeLibraryReady() {
        mNativeLibraryReady = true;
        if (mProgressBar.getParent() != null) mProgressBar.initializeAnimation();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        addProgressBarToHierarchy();
    }

    /**
     * @return The view containing the menu button and menu button badge.
     */
    protected View getMenuButtonWrapper() {
        return mMenuButtonWrapper;
    }

    /**
     * @return The {@link AppCompatImageButton} containing the menu button.
     */
    protected AppCompatImageButton getMenuButton() {
        return mMenuButton;
    }

    /**
     * @return The view containing the menu badge.
     */
    protected View getMenuBadge() {
        return mMenuBadge;
    }

    /**
     * @return The {@link ProgressBar} this layout uses.
     */
    @VisibleForTesting
    public ToolbarProgressBar getProgressBar() {
        return mProgressBar;
    }

    @Override
    public void getPositionRelativeToContainer(View containerView, int[] position) {
        ViewUtils.getRelativeDrawPosition(containerView, this, position);
    }

    /**
     * @return The helper for menu button UI interactions.
     */
    protected AppMenuButtonHelper getMenuButtonHelper() {
        return mAppMenuButtonHelper;
    }

    /**
     * @return Whether or not the native library is loaded and ready.
     */
    protected boolean isNativeLibraryReady() {
        return mNativeLibraryReady;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        recordFirstDrawTime();
    }

    /**
     * Add the toolbar's progress bar to the view hierarchy.
     */
    protected void addProgressBarToHierarchy() {
        ViewGroup controlContainer =
                (ViewGroup) getRootView().findViewById(R.id.control_container);
        int progressBarPosition = UiUtils.insertAfter(
                controlContainer, mProgressBar, (View) getParent());
        assert progressBarPosition >= 0;
        mProgressBar.setProgressBarContainer(controlContainer);
    }

    /**
     * @return The provider for toolbar related data.
     */
    protected ToolbarDataProvider getToolbarDataProvider() {
        return mToolbarDataProvider;
    }

    /**
     * Sets the {@link Invalidator} that will be called when the toolbar attempts to invalidate the
     * drawing surface.  This will give the object that registers as the host for the
     * {@link Invalidator} a chance to defer the actual invalidate to sync drawing.
     * @param invalidator An {@link Invalidator} instance.
     */
    public void setPaintInvalidator(Invalidator invalidator) {
        mInvalidator = invalidator;
    }

    /**
     * Triggers a paint but allows the {@link Invalidator} set by
     * {@link #setPaintInvalidator(Invalidator)} to decide when to actually invalidate.
     * @param client A {@link Invalidator.Client} instance that wants to be invalidated.
     */
    protected void triggerPaintInvalidate(Invalidator.Client client) {
        if (mInvalidator == null) {
            client.doInvalidate();
        } else {
            mInvalidator.invalidate(client);
        }
    }

    /**
     * Gives inheriting classes the chance to respond to
     * {@link org.chromium.chrome.browser.widget.findinpage.FindToolbar} state changes.
     * @param showing Whether or not the {@code FindToolbar} will be showing.
     */
    protected void handleFindToolbarStateChange(boolean showing) {
        mFindInPageToolbarShowing = showing;
    }

    /**
     * Cleans up any code as necessary.
     */
    public void destroy() { }

    /**
     * Sets the delegate to handle visibility of browser controls.
     */
    public void setBrowserControlsVisibilityDelegate(
            BrowserStateBrowserControlsVisibilityDelegate controlsVisibilityDelegate) { }

    /**
     * Sets the OnClickListener that will be notified when the TabSwitcher button is pressed.
     * @param listener The callback that will be notified when the TabSwitcher button is pressed.
     */
    public void setOnTabSwitcherClickHandler(OnClickListener listener) { }

    /**
     * Sets the OnClickListener that will be notified when the New Tab button is pressed.
     * @param listener The callback that will be notified when the New Tab button is pressed.
     */
    public void setOnNewTabClickHandler(OnClickListener listener) { }

    /**
     * Sets the OnClickListener that will be notified when the bookmark button is pressed.
     * @param listener The callback that will be notified when the bookmark button is pressed.
     */
    public void setBookmarkClickHandler(OnClickListener listener) { }

    /**
     * Sets the OnClickListener to notify when the close button is pressed in a custom tab.
     * @param listener The callback that will be notified when the close button is pressed.
     */
    public void setCustomTabCloseClickHandler(OnClickListener listener) { }

    /**
     * Sets the OnClickListener to notify when the incognito button is pressed.
     * @param listener The callback that will be notifed when the incognito button is pressed.
     */
    public void setIncognitoClickHandler(OnClickListener listener) {}

    /**
     * Sets whether the urlbar should be hidden on first page load.
     */
    public void setUrlBarHidden(boolean hide) { }

    /**
     * @return The name of the publisher of the content if it can be reliably extracted, or null
     *         otherwise.
     */
    public String getContentPublisher() {
        return null;
    }

    /**
     * Tells the Toolbar to update what buttons it is currently displaying.
     */
    public void updateButtonVisibility() { }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * back button.
     * @param canGoBack Whether or not the current tab has any history to go back to.
     */
    protected void updateBackButtonVisibility(boolean canGoBack) { }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * forward button.
     * @param canGoForward Whether or not the current tab has any history to go forward to.
     */
    protected void updateForwardButtonVisibility(boolean canGoForward) { }

    /**
     * Gives inheriting classes the chance to update the visibility of the
     * reload button.
     * @param isReloading Whether or not the current tab is loading.
     */
    protected void updateReloadButtonVisibility(boolean isReloading) { }

    /**
     * Gives inheriting classes the chance to update the visual status of the
     * bookmark button.
     * @param isBookmarked Whether or not the current tab is already bookmarked.
     * @param editingAllowed Whether or not bookmarks can be modified (added, edited, or removed).
     */
    protected void updateBookmarkButton(boolean isBookmarked, boolean editingAllowed) { }

    /**
     * Gives inheriting classes the chance to respond to accessibility state changes.
     * @param enabled Whether or not accessibility is enabled.
     */
    protected void onAccessibilityStatusChanged(boolean enabled) { }

    /**
     * Gives inheriting classes the chance to do the necessary UI operations after Chrome is
     * restored to a previously saved state.
     */
    protected void onStateRestored() { }

    /**
     * Gives inheriting classes the chance to update home button UI if home button preference is
     * changed.
     * @param homeButtonEnabled Whether or not home button is enabled in preference.
     */
    protected void onHomeButtonUpdate(boolean homeButtonEnabled) { }

    /**
     * Triggered when the current tab or model has changed.
     * <p>
     * As there are cases where you can select a model with no tabs (i.e. having incognito
     * tabs but no normal tabs will still allow you to select the normal model), this should
     * not guarantee that the model's current tab is non-null.
     */
    protected void onTabOrModelChanged() {
        NewTabPage ntp = getToolbarDataProvider().getNewTabPageForCurrentTab();
        if (ntp != null) {
            getLocationBar().onTabLoadingNTP(ntp);
        }

        getLocationBar().updateMicButtonState();
    }

    /**
     * For extending classes to override and carry out the changes related with the primary color
     * for the current tab changing.
     */
    protected void onPrimaryColorChanged(boolean shouldAnimate) { }

    /**
     * Sets the icon drawable that the close button in the toolbar (if any) should show, or hides
     * it if {@code drawable} is {@code null}.
     */
    public void setCloseButtonImageResource(@Nullable Drawable drawable) { }

    /**
     * Adds a custom action button to the toolbar layout, if it is supported.
     * @param drawable The icon for the button.
     * @param description The content description for the button.
     * @param listener The {@link OnClickListener} to use for clicks to the button.
     */
    public void addCustomActionButton(
            Drawable drawable, String description, OnClickListener listener) {
        // This method should only be called for subclasses that override it.
        assert false;
    }

    /**
     * Updates the visual appearance of a custom action button in the toolbar layout,
     * if it is supported.
     * @param index The index of the button.
     * @param drawable The icon for the button.
     * @param description The content description for the button.
     */
    public void updateCustomActionButton(int index, Drawable drawable, String description) {
        // This method should only be called for subclasses that override it.
        assert false;
    }

    /**
     * @return The height of the tab strip. Return 0 for toolbars that do not have a tabstrip.
     */
    @Override
    public int getTabStripHeight() {
        return getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
    }

    /**
     * Triggered when the content view for the specified tab has changed.
     */
    protected void onTabContentViewChanged() {
        NewTabPage ntp = getToolbarDataProvider().getNewTabPageForCurrentTab();
        if (ntp != null) getLocationBar().onTabLoadingNTP(ntp);
    }

    @Override
    public boolean isReadyForTextureCapture() {
        return true;
    }

    @Override
    public boolean setForceTextureCapture(boolean forceTextureCapture) {
        return false;
    }

    @Override
    public void setLayoutUpdateHost(LayoutUpdateHost layoutUpdateHost) { }

    /**
     * @param attached Whether or not the web content is attached to the view heirarchy.
     */
    protected void setContentAttached(boolean attached) { }

    /**
     * Gives inheriting classes the chance to show or hide the TabSwitcher mode of this toolbar.
     * @param inTabSwitcherMode Whether or not TabSwitcher mode should be shown or hidden.
     * @param showToolbar    Whether or not to show the normal toolbar while animating.
     * @param delayAnimation Whether or not to delay the animation until after the transition has
     *                       finished (which can be detected by a call to
     *                       {@link #onTabSwitcherTransitionFinished()}).
     */
    protected void setTabSwitcherMode(
            boolean inTabSwitcherMode, boolean showToolbar, boolean delayAnimation) { }

    /**
     * Gives inheriting classes the chance to update their state when the TabSwitcher transition has
     * finished.
     */
    protected void onTabSwitcherTransitionFinished() { }

    /**
     * Gives inheriting classes the chance to update themselves based on the
     * number of tabs in the current TabModel.
     * @param numberOfTabs The number of tabs in the current model.
     */
    protected void updateTabCountVisuals(int numberOfTabs) { }

    /**
     * Gives inheriting classes the chance to update themselves based on default search engine
     * changes.
     */
    protected void onDefaultSearchEngineChanged() { }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        // Consumes mouse button events on toolbar so they don't get leaked to content layer.
        // See https://crbug.com/740855.
        if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0
                && event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_BUTTON_PRESS
                    || action == MotionEvent.ACTION_BUTTON_RELEASE) {
                return true;
            }
        }
        return super.onGenericMotionEvent(event);
    }

    @Override
    public void getLocationBarContentRect(Rect outRect) {
        View container = getLocationBar().getContainerView();
        outRect.set(container.getPaddingLeft(), container.getPaddingTop(),
                container.getWidth() - container.getPaddingRight(),
                container.getHeight() - container.getPaddingBottom());
        ViewUtils.getRelativeDrawPosition(
                this, getLocationBar().getContainerView(), mTempPosition);
        outRect.offset(mTempPosition[0], mTempPosition[1]);
    }

    @Override
    public void setTextureCaptureMode(boolean textureMode) { }

    @Override
    public boolean shouldIgnoreSwipeGesture() {
        return mUrlHasFocus
                || (mAppMenuButtonHelper != null && mAppMenuButtonHelper.isAppMenuActive())
                || mFindInPageToolbarShowing;
    }

    /**
     * @return Whether or not the url bar has focus.
     */
    protected boolean urlHasFocus() {
        return mUrlHasFocus;
    }

    /**
     * Triggered when the URL input field has gained or lost focus.
     * @param hasFocus Whether the URL field has gained focus.
     */
    protected void onUrlFocusChange(boolean hasFocus) {
        mUrlHasFocus = hasFocus;
    }

    /**
     * Keeps track of the first time the toolbar is drawn.
     */
    private void recordFirstDrawTime() {
        if (mFirstDrawTimeMs == 0) mFirstDrawTimeMs = SystemClock.elapsedRealtime();
    }

    /**
     * Returns the elapsed realtime in ms of the time at which first draw for the toolbar occurred.
     */
    public long getFirstDrawTime() {
        return mFirstDrawTimeMs;
    }

    /**
     * Notified when a navigation to a different page has occurred.
     */
    protected void onNavigatedToDifferentPage() {
    }

    /**
     * Starts load progress.
     */
    protected void startLoadProgress() {
        mProgressBar.start();
    }

    /**
     * Sets load progress.
     * @param progress The load progress between 0 and 1.
     */
    protected void setLoadProgress(float progress) {
        mProgressBar.setProgress(progress);
    }

    /**
     * Finishes load progress.
     * @param delayed Whether hiding progress bar should be delayed to give enough time for user to
     *                        recognize the last state.
     */
    protected void finishLoadProgress(boolean delayed) {
        mProgressBar.finish(delayed);
    }

    /**
     * @return True if the progress bar is started.
     */
    protected boolean isProgressStarted() {
        return mProgressBar.isStarted();
    }

    /**
     * Finish any toolbar animations.
     */
    public void finishAnimations() { }

    /**
     * @return The current View showing in the Tab.
     */
    protected View getCurrentTabView() {
        Tab tab = mToolbarDataProvider.getTab();
        if (tab != null) {
            return tab.getView();
        }
        return null;
    }

    /**
     * @return Whether or not the toolbar is incognito.
     */
    protected boolean isIncognito() {
        return mToolbarDataProvider.isIncognito();
    }

    /**
     * @return {@link LocationBar} object this {@link ToolbarLayout} contains.
     */
    public abstract LocationBar getLocationBar();

    /**
     * @return Whether or not this toolbar should use light or dark assets based on the theme.
     */
    public abstract boolean useLightDrawables();

    /**
     * Navigates the current Tab back.
     * @return Whether or not the current Tab did go back.
     */
    protected boolean back() {
        if (getLocationBar() != null) getLocationBar().setUrlBarFocus(false);
        return mToolbarTabController != null ? mToolbarTabController.back() : false;
    }

    /**
     * Navigates the current Tab forward.
     * @return Whether or not the current Tab did go forward.
     */
    protected boolean forward() {
        if (getLocationBar() != null) getLocationBar().setUrlBarFocus(false);
        return mToolbarTabController != null ? mToolbarTabController.forward() : false;
    }

    /**
     * If the page is currently loading, this will trigger the tab to stop.  If the page is fully
     * loaded, this will trigger a refresh.
     *
     * <p>The buttons of the toolbar will be updated as a result of making this call.
     */
    protected void stopOrReloadCurrentTab() {
        if (getLocationBar() != null) getLocationBar().setUrlBarFocus(false);
        if (mToolbarTabController != null) mToolbarTabController.stopOrReloadCurrentTab();
    }

    /**
     * Opens hompage in the current tab.
     */
    protected void openHomepage() {
        if (getLocationBar() != null) getLocationBar().setUrlBarFocus(false);
        if (mToolbarTabController != null) mToolbarTabController.openHomepage();
    }

    /**
     * Opens the Memex UI in the current tab.
     */
    protected void openMemexUI() {
        if (mToolbarTabController != null) mToolbarTabController.openMemexUI();
    }

    @Override
    public void setMenuButtonHighlight(boolean highlight) {
        mHighlightingMenu = highlight;
        setMenuButtonHighlightDrawable(mHighlightingMenu);
    }

    @Override
    public void showAppMenuUpdateBadge() {
        mShowMenuBadge = true;
        mMenuButtonWrapper.updateImageResources();
    }

    @Override
    public boolean isShowingAppMenuUpdateBadge() {
        return mShowMenuBadge;
    }

    @Override
    public void removeAppMenuUpdateBadge(boolean animate) {
        if (mMenuBadge == null) return;
        boolean wasShowingMenuBadge = mShowMenuBadge;
        mShowMenuBadge = false;
        setMenuButtonContentDescription();

        if (!animate || !wasShowingMenuBadge) {
            mMenuButtonWrapper.setUpdateBadgeVisibilityIfValidState(false);
            return;
        }

        if (mIsMenuBadgeAnimationRunning && mMenuBadgeAnimatorSet != null) {
            mMenuBadgeAnimatorSet.cancel();
        }

        // Set initial states.
        mMenuButton.setAlpha(0.f);

        mMenuBadgeAnimatorSet = UpdateMenuItemHelper.createHideUpdateBadgeAnimation(
                mMenuButton, mMenuBadge);

        mMenuBadgeAnimatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mIsMenuBadgeAnimationRunning = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mIsMenuBadgeAnimationRunning = false;
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                mIsMenuBadgeAnimationRunning = false;
            }
        });

        mMenuBadgeAnimatorSet.start();
    }

    /**
     * Enable the experimental toolbar button.
     * @param onClickListener The {@link OnClickListener} to be called when the button is clicked.
     * @param drawableResId The resource id of the drawable to display for the button.
     * @param contentDescriptionResId The resource id of the content description for the button.
     */
    public void enableExperimentalButton(OnClickListener onClickListener,
            @DrawableRes int drawableResId, @StringRes int contentDescriptionResId) {}

    /**
     * @return The experimental toolbar button if it exists.
     */
    public @Nullable View getExperimentalButtonView() {
        return null;
    }

    /**
     * Disable the experimental toolbar button.
     */
    public void disableExperimentalButton() {}

    /**
     * Sets the update badge visibility to VISIBLE and sets the menu button image to the badged
     * bitmap.
     */
    protected void setAppMenuUpdateBadgeToVisible(boolean animate) {
        if (mMenuBadge == null || mMenuButton == null || mMenuButtonWrapper == null) return;
        setMenuButtonContentDescription();
        if (!animate || mIsMenuBadgeAnimationRunning) {
            mMenuButtonWrapper.setUpdateBadgeVisibilityIfValidState(true);
            return;
        }

        // Set initial states.
        mMenuBadge.setAlpha(0.f);
        mMenuBadge.setVisibility(View.VISIBLE);

        mMenuBadgeAnimatorSet = UpdateMenuItemHelper.createShowUpdateBadgeAnimation(
                mMenuButton, mMenuBadge);

        mMenuBadgeAnimatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mIsMenuBadgeAnimationRunning = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mIsMenuBadgeAnimationRunning = false;
            }

            @Override
            public void onAnimationCancel(Animator animation) {
                mIsMenuBadgeAnimationRunning = false;
            }
        });

        mMenuBadgeAnimatorSet.start();
    }

    protected void cancelAppMenuUpdateBadgeAnimation() {
        if (mIsMenuBadgeAnimationRunning && mMenuBadgeAnimatorSet != null) {
            mMenuBadgeAnimatorSet.cancel();
        }
    }

    /**
     * Sets the update menu badge drawable to the light or dark asset.
     * @param useLightDrawable Whether the light drawable should be used.
     */
    protected void setAppMenuUpdateBadgeDrawable(boolean useLightDrawable) {
        if (mMenuButtonWrapper == null) return;
        mMenuButtonWrapper.setUseLightDrawables(useLightDrawable);
    }

    /**
     * Sets the menu button's background depending on whether or not we are highlighting and whether
     * or not we are using light or dark assets.
     * @param highlighting Whether or not the menu button should be highlighted.
     */
    protected void setMenuButtonHighlightDrawable(boolean highlighting) {
        // Return if onFinishInflate didn't finish
        if (mMenuButtonWrapper == null || mMenuButton == null) return;

        if (highlighting) {
            if (mHighlightDrawable == null) {
                mHighlightDrawable = PulseDrawable.createCircle(getContext());
                mHighlightDrawable.setInset(ViewCompat.getPaddingStart(mMenuButton),
                        mMenuButton.getPaddingTop(), ViewCompat.getPaddingEnd(mMenuButton),
                        mMenuButton.getPaddingBottom());
            }
            mHighlightDrawable.setUseLightPulseColor(useLightDrawables());
            mMenuButtonWrapper.setBackground(mHighlightDrawable);
            mHighlightDrawable.start();
        } else {
            mMenuButtonWrapper.setBackground(null);
        }
    }

    /**
     * Sets the content description for the menu button.
     */
    protected void setMenuButtonContentDescription() {
        if (mMenuButtonWrapper == null) return;
        mMenuButtonWrapper.updateContentDescription();
    }

    /**
     * Sets the current TabModelSelector so the toolbar can pass it into buttons that need access to
     * it.
     */
    public void setTabModelSelector(TabModelSelector selector) {}

    /**
     * Sets the icon drawable for the ntp button if the ntp button feature is enabled.
     * Note: This method is called twice in ToolbarLayout's children - once in
     * #onNativeLibraryReady() & once in #onFinishInflate() (see https://crbug.com/862887).
     * @param ntpButton The button that needs to be changed.
     */
    protected void changeIconToNTPIcon(AppCompatImageButton ntpButton) {
        if (FeatureUtilities.isNewTabPageButtonEnabled())
            ntpButton.setImageResource(R.drawable.ic_home);
    }

    @Override
    public void setScrim(ScrimView scrim) {
        getLocationBar().setScrim(scrim);
    }
}
