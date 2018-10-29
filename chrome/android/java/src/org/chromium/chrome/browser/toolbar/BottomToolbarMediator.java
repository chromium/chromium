// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.res.Resources;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.OverlayPanelManagerObserver;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.ToolbarSwipeLayout;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.toolbar.ToolbarButtonSlotData.ToolbarButtonData;
import org.chromium.chrome.browser.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the bottom toolbar, and updating
 * the model accordingly.
 */
class BottomToolbarMediator
        implements FullscreenListener, KeyboardVisibilityDelegate.KeyboardVisibilityListener,
                   OverlayPanelManagerObserver, OverviewModeObserver, SceneChangeObserver {
    /** The amount of time to show the Duet help bubble for. */
    private static final int DUET_IPH_BUBBLE_SHOW_DURATION_MS = 6000;

    /** The model for the bottom toolbar that holds all of its state. */
    private BottomToolbarModel mModel;

    /** The fullscreen manager to observe browser controls events. */
    private ChromeFullscreenManager mFullscreenManager;

    /** The overview mode manager. */
    private OverviewModeBehavior mOverviewModeBehavior;

    /** A {@link WindowAndroid} for watching keyboard visibility events. */
    private WindowAndroid mWindowAndroid;

    /** The previous height of the bottom toolbar. */
    private int mBottomToolbarHeightBeforeHide;

    /** Whether the swipe layout is currently active. */
    private boolean mIsInSwipeLayout;

    /** The data required to fill in the first (leftmost) bottom toolbar button slot.*/
    private final ToolbarButtonSlotData mFirstSlotData;

    /** The data required to fill in the second bottom toolbar button slot.*/
    private final ToolbarButtonSlotData mSecondSlotData;

    /**
     * Build a new mediator that handles events from outside the bottom toolbar.
     * @param model The {@link BottomToolbarModel} that holds all the state for the bottom toolbar.
     * @param fullscreenManager A {@link ChromeFullscreenManager} for events related to the browser
     *                          controls.
     * @param resources Android {@link Resources} to pull dimensions from.
     * @param firstSlotData The data required to fill in the first bottom toolbar button slot.
     * @param secondSlotData The data required to fill in the second bottom toolbar button slot.
     * @param primaryColor The initial color for the bottom toolbar.
     */
    BottomToolbarMediator(BottomToolbarModel model, ChromeFullscreenManager fullscreenManager,
            Resources resources, ToolbarButtonSlotData firstSlotData,
            ToolbarButtonSlotData secondSlotData, int primaryColor) {
        mModel = model;
        mFullscreenManager = fullscreenManager;
        mFullscreenManager.addListener(this);

        // Notify the fullscreen manager that the bottom controls now have a height.
        fullscreenManager.setBottomControlsHeight(
                resources.getDimensionPixelOffset(R.dimen.bottom_toolbar_height));
        fullscreenManager.updateViewportSize();

        mFirstSlotData = firstSlotData;
        mSecondSlotData = secondSlotData;

        mModel.set(BottomToolbarModel.PRIMARY_COLOR, primaryColor);

        mModel.set(BottomToolbarModel.FIRST_BUTTON_DATA, mFirstSlotData.browsingModeButtonData);
        mModel.set(BottomToolbarModel.SECOND_BUTTON_DATA, mSecondSlotData.browsingModeButtonData);
    }

    /**
     * @param swipeHandler The handler that controls the toolbar swipe behavior.
     */
    void setToolbarSwipeHandler(EdgeSwipeHandler swipeHandler) {
        mModel.set(BottomToolbarModel.TOOLBAR_SWIPE_HANDLER, swipeHandler);
    }

    /**
     * Clean up anything that needs to be when the bottom toolbar is destroyed.
     */
    void destroy() {
        mFullscreenManager.removeListener(this);
        if (mOverviewModeBehavior != null) mOverviewModeBehavior.removeOverviewModeObserver(this);
        if (mWindowAndroid != null) {
            mWindowAndroid.getKeyboardDelegate().removeKeyboardVisibilityListener(this);
        }
        if (mModel.get(BottomToolbarModel.LAYOUT_MANAGER) != null) {
            LayoutManager manager = mModel.get(BottomToolbarModel.LAYOUT_MANAGER);
            manager.getOverlayPanelManager().removeObserver(this);
            manager.removeSceneChangeObserver(this);
        }
    }

    @Override
    public void onContentOffsetChanged(float offset) {}

    @Override
    public void onControlsOffsetChanged(float topOffset, float bottomOffset, boolean needsAnimate) {
        mModel.set(BottomToolbarModel.Y_OFFSET, (int) bottomOffset);
        if (bottomOffset > 0 || mFullscreenManager.getBottomControlsHeight() == 0) {
            mModel.set(BottomToolbarModel.ANDROID_VIEW_VISIBLE, false);
        } else {
            tryShowingAndroidView();
        }
    }

    @Override
    public void onToggleOverlayVideoMode(boolean enabled) {}

    @Override
    public void onBottomControlsHeightChanged(int bottomControlsHeight) {}

    @Override
    public void onOverviewModeStartedShowing(boolean showToolbar) {
        mModel.set(BottomToolbarModel.FIRST_BUTTON_DATA, mFirstSlotData.tabSwitcherModeButtonData);
        mModel.set(
                BottomToolbarModel.SECOND_BUTTON_DATA, mSecondSlotData.tabSwitcherModeButtonData);
    }

    @Override
    public void onOverviewModeFinishedShowing() {}

    @Override
    public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
        mModel.set(BottomToolbarModel.FIRST_BUTTON_DATA, mFirstSlotData.browsingModeButtonData);
        mModel.set(BottomToolbarModel.SECOND_BUTTON_DATA, mSecondSlotData.browsingModeButtonData);
    }

    @Override
    public void onOverviewModeFinishedHiding() {}

    @Override
    public void onOverlayPanelShown() {
        mModel.set(BottomToolbarModel.ANDROID_VIEW_VISIBLE, false);
    }

    @Override
    public void onOverlayPanelHidden() {
        tryShowingAndroidView();
    }

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        // The toolbars are force shown when the keyboard is visible, so we can blindly set
        // the bottom toolbar view to visible or invisible regardless of the previous state.
        if (isShowing) {
            mBottomToolbarHeightBeforeHide = mFullscreenManager.getBottomControlsHeight();
            mModel.set(BottomToolbarModel.ANDROID_VIEW_VISIBLE, false);
            mModel.set(BottomToolbarModel.COMPOSITED_VIEW_VISIBLE, false);
            mFullscreenManager.setBottomControlsHeight(0);
        } else {
            mFullscreenManager.setBottomControlsHeight(mBottomToolbarHeightBeforeHide);
            tryShowingAndroidView();
            mModel.set(
                    BottomToolbarModel.Y_OFFSET, (int) mFullscreenManager.getBottomControlOffset());
            mModel.set(BottomToolbarModel.COMPOSITED_VIEW_VISIBLE, true);
        }
    }

    /**
     * Try showing the toolbar's Android view after it has been hidden. This accounts for cases
     * where a browser signal would ordinarily re-show the view, but others still require it to be
     * hidden.
     */
    private void tryShowingAndroidView() {
        if (mFullscreenManager.getBottomControlOffset() > 0) return;
        if (mModel.get(BottomToolbarModel.Y_OFFSET) != 0) return;
        mModel.set(BottomToolbarModel.ANDROID_VIEW_VISIBLE, true);
    }

    void setLayoutManager(LayoutManager layoutManager) {
        mModel.set(BottomToolbarModel.LAYOUT_MANAGER, layoutManager);
        layoutManager.addSceneChangeObserver(this);
        layoutManager.getOverlayPanelManager().addObserver(this);
    }

    @Override
    public void onTabSelectionHinted(int tabId) {}

    @Override
    public void onSceneChange(Layout layout) {
        if (layout instanceof ToolbarSwipeLayout) {
            mIsInSwipeLayout = true;
            mModel.set(BottomToolbarModel.ANDROID_VIEW_VISIBLE, false);
        } else if (mIsInSwipeLayout) {
            // Only change to visible if leaving the swipe layout.
            mIsInSwipeLayout = false;
            mModel.set(BottomToolbarModel.ANDROID_VIEW_VISIBLE, true);
        }
    }

    void setResourceManager(ResourceManager resourceManager) {
        mModel.set(BottomToolbarModel.RESOURCE_MANAGER, resourceManager);
    }

    void setOverviewModeBehavior(OverviewModeBehavior overviewModeBehavior) {
        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeBehavior.addOverviewModeObserver(this);
    }

    void setToolbarSwipeLayout(ToolbarSwipeLayout layout) {
        mModel.set(BottomToolbarModel.TOOLBAR_SWIPE_LAYOUT, layout);
    }

    void setWindowAndroid(WindowAndroid windowAndroid) {
        assert mWindowAndroid == null : "#setWindowAndroid should only be called once per toolbar.";
        // Watch for keyboard events so we can hide the bottom toolbar when the keyboard is showing.
        mWindowAndroid = windowAndroid;
        mWindowAndroid.getKeyboardDelegate().addKeyboardVisibilityListener(this);
    }

    void setTabSwitcherButtonData(
            ToolbarButtonData firstSlotButtonData, ToolbarButtonData secondSlotButtonData) {
        mFirstSlotData.tabSwitcherModeButtonData = firstSlotButtonData;
        mSecondSlotData.tabSwitcherModeButtonData = secondSlotButtonData;

        if (mOverviewModeBehavior.overviewVisible()) {
            mModel.set(
                    BottomToolbarModel.FIRST_BUTTON_DATA, mFirstSlotData.tabSwitcherModeButtonData);
            mModel.set(BottomToolbarModel.SECOND_BUTTON_DATA,
                    mSecondSlotData.tabSwitcherModeButtonData);
        }
    }

    void setPrimaryColor(int color) {
        mModel.set(BottomToolbarModel.PRIMARY_COLOR, color);
    }

    /**
     * Maybe show the IPH bubble for Chrome Duet.
     * @param anchor The view to anchor the IPH to.
     * @param tracker A tracker for IPH.
     */
    void showIPH(View anchor, Tracker tracker) {
        if (tracker.shouldTriggerHelpUI(FeatureConstants.CHROME_DUET_FEATURE)) {
            TextBubble bubble =
                    new TextBubble(anchor.getContext(), anchor, R.string.iph_duet_icons_moved,
                            R.string.iph_duet_icons_moved, true, new ViewRectProvider(anchor));
            bubble.setAutoDismissTimeout(DUET_IPH_BUBBLE_SHOW_DURATION_MS);
            bubble.addOnDismissListener(
                    () -> tracker.dismissed(FeatureConstants.CHROME_DUET_FEATURE));
            bubble.show();
        }
    }
}
