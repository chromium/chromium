// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.res.ColorStateList;
import android.support.v7.content.res.AppCompatResources;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.HintlessActivityTabObserver;
import org.chromium.chrome.browser.appmenu.AppMenuButtonHelper;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.ToolbarSwipeLayout;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.BottomToolbarViewBinder.ViewHolder;
import org.chromium.chrome.browser.toolbar.ToolbarButtonSlotData.ToolbarButtonData;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.ResourceManager;

/**
 * The coordinator for the bottom toolbar. This class handles all interactions that the bottom
 * toolbar has with the outside world. This class has two primary components, an Android view that
 * handles user actions and a composited texture that draws when the controls are being scrolled
 * off-screen. The Android version does not draw unless the controls offset is 0.
 */
public class BottomToolbarCoordinator {
    /** The mediator that handles events from outside the bottom toolbar. */
    private final BottomToolbarMediator mMediator;

    /** The tab switcher button component that lives in the bottom toolbar. */
    private final TabSwitcherButtonCoordinator mTabSwitcherButtonCoordinator;

    /** The menu button that lives in the bottom toolbar. */
    private final MenuButton mMenuButton;

    /** The light mode tint to be used in bottom toolbar buttons. */
    private final ColorStateList mLightModeTint;

    /** The dark mode tint to be used in bottom toolbar buttons. */
    private final ColorStateList mDarkModeTint;

    /** The primary color to be used in normal mode. */
    private final int mNormalPrimaryColor;

    /** The primary color to be used in incognito mode. */
    private final int mIncognitoPrimaryColor;

    /**
     * Build the coordinator that manages the bottom toolbar.
     * @param fullscreenManager A {@link ChromeFullscreenManager} to update the bottom controls
     *                          height for the renderer.
     * @param root The root {@link ViewGroup} for locating the views to inflate.
     * @param firstSlotData The data required to fill in the leftmost bottom toolbar button slot.
     * @param secondSlotData The data required to fill in the second bottom toolbar button slot.
     */
    public BottomToolbarCoordinator(ChromeFullscreenManager fullscreenManager, ViewGroup root,
            ToolbarButtonSlotData firstSlotData, ToolbarButtonSlotData secondSlotData,
            final ActivityTabProvider tabProvider) {
        BottomToolbarModel model = new BottomToolbarModel();

        int shadowHeight =
                root.getResources().getDimensionPixelOffset(R.dimen.toolbar_shadow_height);

        // This is the Android view component of the views that constitute the bottom toolbar.
        View inflatedView = ((ViewStub) root.findViewById(R.id.bottom_toolbar)).inflate();
        final ScrollingBottomViewResourceFrameLayout toolbarRoot =
                (ScrollingBottomViewResourceFrameLayout) inflatedView;
        toolbarRoot.setTopShadowHeight(shadowHeight);

        PropertyModelChangeProcessor.create(
                model, new ViewHolder(toolbarRoot), new BottomToolbarViewBinder());

        mTabSwitcherButtonCoordinator = new TabSwitcherButtonCoordinator(toolbarRoot);
        mMenuButton = toolbarRoot.findViewById(R.id.menu_button_wrapper);

        mLightModeTint =
                AppCompatResources.getColorStateList(root.getContext(), R.color.light_mode_tint);
        mDarkModeTint =
                AppCompatResources.getColorStateList(root.getContext(), R.color.dark_mode_tint);

        mNormalPrimaryColor =
                ApiCompatibilityUtils.getColor(root.getResources(), R.color.modern_primary_color);
        mIncognitoPrimaryColor = ApiCompatibilityUtils.getColor(
                root.getResources(), R.color.incognito_modern_primary_color);

        mMediator = new BottomToolbarMediator(model, fullscreenManager, root.getResources(),
                firstSlotData, secondSlotData, mNormalPrimaryColor);

        final View iphAnchor = toolbarRoot.findViewById(R.id.bottom_toolbar_container);
        tabProvider.addObserverAndTrigger(new HintlessActivityTabObserver() {
            @Override
            public void onActivityTabChanged(Tab tab) {
                if (tab == null) return;
                mMediator.showIPH(iphAnchor, TrackerFactory.getTrackerForProfile(tab.getProfile()));
                tabProvider.removeObserver(this);
            }
        });
    }

    /**
     * Initialize the bottom toolbar with the components that had native initialization
     * dependencies.
     * <p>
     * Calling this must occur after the native library have completely loaded.
     * @param resourceManager A {@link ResourceManager} for loading textures into the compositor.
     * @param layoutManager A {@link LayoutManager} to attach overlays to.
     * @param tabSwitcherListener An {@link OnClickListener} that is triggered when the
     *                                  tab switcher button is clicked.
     * @param menuButtonHelper An {@link AppMenuButtonHelper} that is triggered when the
     *                           menu button is clicked.
     * @param tabModelSelector A {@link TabModelSelector} that the tab switcher button uses to
     *                         keep its tab count updated.
     * @param overviewModeBehavior The overview mode manager.
     * @param firstSlotTabSwitcherButtonData The button to be shown in the first slot when in tab
     *                                       switcher mode.
     * @param secondSlotTabSwitcherButtonData The button to be shown in the second slot when in tab
     *                                        switcher mode.
     */
    public void initializeWithNative(ResourceManager resourceManager, LayoutManager layoutManager,
            OnClickListener tabSwitcherListener, AppMenuButtonHelper menuButtonHelper,
            TabModelSelector tabModelSelector, OverviewModeBehavior overviewModeBehavior,
            WindowAndroid windowAndroid, ToolbarButtonData firstSlotTabSwitcherButtonData,
            ToolbarButtonData secondSlotTabSwitcherButtonData, boolean isIncognito) {
        mMediator.setLayoutManager(layoutManager);
        mMediator.setResourceManager(resourceManager);
        mMediator.setOverviewModeBehavior(overviewModeBehavior);
        mMediator.setToolbarSwipeHandler(layoutManager.getToolbarSwipeHandler());
        mMediator.setWindowAndroid(windowAndroid);
        setIncognito(isIncognito);
        mMediator.setTabSwitcherButtonData(
                firstSlotTabSwitcherButtonData, secondSlotTabSwitcherButtonData);

        mTabSwitcherButtonCoordinator.setTabSwitcherListener(tabSwitcherListener);
        mTabSwitcherButtonCoordinator.setTabModelSelector(tabModelSelector);

        mMenuButton.setTouchListener(menuButtonHelper);
        mMenuButton.setAccessibilityDelegate(menuButtonHelper);
    }

    /**
     * Show the update badge over the bottom toolbar's app menu.
     */
    public void showAppMenuUpdateBadge() {
        mMenuButton.setUpdateBadgeVisibilityIfValidState(true);
    }

    /**
     * Remove the update badge.
     */
    public void removeAppMenuUpdateBadge() {
        mMenuButton.setUpdateBadgeVisibilityIfValidState(false);
    }

    /**
     * @return Whether the update badge is showing.
     */
    public boolean isShowingAppMenuUpdateBadge() {
        return mMenuButton.isShowingAppMenuUpdateBadge();
    }

    /**
     * @param layout The {@link ToolbarSwipeLayout} that the bottom toolbar will hook into. This
     *               allows the bottom toolbar to provide the layout with scene layers with the
     *               bottom toolbar's texture.
     */
    public void setToolbarSwipeLayout(ToolbarSwipeLayout layout) {
        mMediator.setToolbarSwipeLayout(layout);
    }

    /**
     * @return The wrapper for the app menu button.
     */
    public MenuButton getMenuButtonWrapper() {
        return mMenuButton;
    }

    public void setIncognito(boolean isIncognito) {
        mMediator.setPrimaryColor(isIncognito ? mIncognitoPrimaryColor : mNormalPrimaryColor);

        final ColorStateList tint = isIncognito ? mLightModeTint : mDarkModeTint;
        mTabSwitcherButtonCoordinator.setTint(tint);
        mMenuButton.setTint(tint);
        mMenuButton.setUseLightDrawables(isIncognito);
    }

    /**
     * Clean up any state when the bottom toolbar is destroyed.
     */
    public void destroy() {
        mMediator.destroy();
        mTabSwitcherButtonCoordinator.destroy();
    }
}
