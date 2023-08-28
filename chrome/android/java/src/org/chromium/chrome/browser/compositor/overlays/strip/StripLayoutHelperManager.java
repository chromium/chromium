// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.RectF;
import android.os.Handler;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.CompositorOnClickHandler;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.AreaMotionEventFilter;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.MotionEventHandler;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.url.GURL;

import java.util.List;

/**
 * This class handles managing which {@link StripLayoutHelper} is currently active and dispatches
 * all input and model events to the proper destination.
 */
public class StripLayoutHelperManager implements SceneOverlay, PauseResumeWithNativeObserver {
    /**
     * POD type that contains the necessary tab model info on startup. Used in the startup flicker
     * fix experiment where we create a placeholder tab strip on startup to mitigate jank as tabs
     * are rapidly restored (perceived as a flicker/tab strip scroll).
     */
    public static class TabModelStartupInfo {
        public final int standardCount;
        public final int incognitoCount;
        public final int standardActiveIndex;
        public final int incognitoActiveIndex;
        public final boolean createdStandardTabOnStartup;
        public final boolean createdIncognitoTabOnStartup;

        public TabModelStartupInfo(int standardCount, int incognitoCount, int standardActiveIndex,
                int incognitoActiveIndex, boolean createdStandardTabOnStartup,
                boolean createdIncognitoTabOnStartup) {
            this.standardCount = standardCount;
            this.incognitoCount = incognitoCount;
            this.standardActiveIndex = standardActiveIndex;
            this.incognitoActiveIndex = incognitoActiveIndex;
            this.createdStandardTabOnStartup = createdStandardTabOnStartup;
            this.createdIncognitoTabOnStartup = createdIncognitoTabOnStartup;
        }
    }

    // Model selector buttons constants.
    private static final float MODEL_SELECTOR_BUTTON_Y_OFFSET_DP = 10.f;
    private static final float MODEL_SELECTOR_BUTTON_PADDING_DP = 12.f;
    private static final float MODEL_SELECTOR_BUTTON_WIDTH_DP = 24.f;
    private static final float MODEL_SELECTOR_BUTTON_HEIGHT_DP = 24.f;
    private static final float MODEL_SELECTOR_BUTTON_BACKGROUND_Y_OFFSET_DP_FOLIO = 3.f;
    private static final float MODEL_SELECTOR_BUTTON_BACKGROUND_Y_OFFSET_DP_DETACHED = 5.f;
    private static final float MODEL_SELECTOR_BUTTON_BACKGROUND_WIDTH_DP_TSR = 32.f;
    private static final float MODEL_SELECTOR_BUTTON_BACKGROUND_HEIGHT_DP_TSR = 32.f;
    private static final float MODEL_SELECTOR_BUTTON_CLICK_SLOP_DP = 12.f;
    private static final float BUTTON_DESIRED_TOUCH_TARGET_SIZE = 48.f;

    // Fade constants.
    private static final float FADE_SHORT_WIDTH_DP = 72;
    private static final float FADE_LONG_WIDTH_DP = 120;
    static final float FADE_SHORT_TSR_WIDTH_DP = 60;
    static final float FADE_MEDIUM_TSR_WIDTH_DP = 72;
    static final float FADE_LONG_TSR_WIDTH_DP = 136;

    // Caching Variables
    private final RectF mStripFilterArea = new RectF();

    // External influences
    private TabModelSelector mTabModelSelector;
    private final LayoutUpdateHost mUpdateHost;

    // Event Filters
    private final AreaMotionEventFilter mEventFilter;

    // Internal state
    private boolean mIsIncognito;
    private final StripLayoutHelper mNormalHelper;
    private final StripLayoutHelper mIncognitoHelper;

    // UI State
    private float mWidth;  // in dp units
    private final float mHeight;  // in dp units
    private int mOrientation;
    private CompositorButton mModelSelectorButton;

    private Context mContext;
    private boolean mBrowserScrimShowing;
    private int mTabStripFadeShort;
    private int mTabStripFadeLong;
    private float mTabStripFadeShortWidth;
    private float mTabStripFadeLongWidth;

    private TabStripSceneLayer mTabStripTreeProvider;

    private TabStripEventHandler mTabStripEventHandler;
    private TabSwitcherLayoutObserver mTabSwitcherLayoutObserver;

    private float mModelSelectorWidth;
    // 3-dots menu button with tab strip end padding
    private float mMenuButtonPadding;
    private TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver =
            new TabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    tabModelSwitched(newModel.isIncognito());
                }
            };

    private TabModelObserver mTabModelObserver;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;

    private final String mDefaultTitle;
    private final Supplier<LayerTitleCache> mLayerTitleCacheSupplier;

    private class TabStripEventHandler implements MotionEventHandler {
        @Override
        public void onDown(float x, float y, boolean fromMouse, int buttons) {
            if (mModelSelectorButton.onDown(x, y)) return;
            getActiveStripLayoutHelper().onDown(time(), x, y, fromMouse, buttons);
        }

        @Override
        public void onUpOrCancel() {
            if (mModelSelectorButton.onUpOrCancel() && mTabModelSelector != null) {
                getActiveStripLayoutHelper().finishAnimationsAndPushTabUpdates();
                if (!mModelSelectorButton.isVisible()) return;
                mTabModelSelector.selectModel(!mTabModelSelector.isIncognitoSelected());
                return;
            }
            getActiveStripLayoutHelper().onUpOrCancel(time());
        }

        @Override
        public void drag(float x, float y, float dx, float dy, float tx, float ty) {
            mModelSelectorButton.drag(x, y);
            getActiveStripLayoutHelper().drag(time(), x, y, dx, dy, tx, ty);
        }

        @Override
        public void click(float x, float y, boolean fromMouse, int buttons) {
            long time = time();
            if (mModelSelectorButton.click(x, y)) {
                mModelSelectorButton.handleClick(time);
                return;
            }
            getActiveStripLayoutHelper().click(time(), x, y, fromMouse, buttons);
        }

        @Override
        public void fling(float x, float y, float velocityX, float velocityY) {
            getActiveStripLayoutHelper().fling(time(), x, y, velocityX, velocityY);
        }

        @Override
        public void onLongPress(float x, float y) {
            getActiveStripLayoutHelper().onLongPress(time(), x, y);
        }

        @Override
        public void onPinch(float x0, float y0, float x1, float y1, boolean firstEvent) {
            // Not implemented.
        }

        @Override
        public void onHoverEnter(float x, float y) {
            getActiveStripLayoutHelper().onHoverEnter(x, y);
        }

        @Override
        public void onHoverMove(float x, float y) {
            getActiveStripLayoutHelper().onHoverMove(x, y);
        }

        @Override
        public void onHoverExit() {
            getActiveStripLayoutHelper().onHoverExit();
        }

        private long time() {
            return LayoutManagerImpl.time();
        }
    }

    /**
     * Observer for Tab Switcher layout events.
     */
    class TabSwitcherLayoutObserver implements LayoutStateObserver {
        @Override
        public void onStartedShowing(@LayoutType int layoutType) {
            if (layoutType != LayoutType.TAB_SWITCHER) return;
            mBrowserScrimShowing = true;
        }

        @Override
        public void onStartedHiding(@LayoutType int layoutType) {
            if (layoutType != LayoutType.TAB_SWITCHER) return;
            mBrowserScrimShowing = false;
        }

        @Override
        public void onTabSelectionHinted(int tabId) {
            LayoutStateObserver.super.onTabSelectionHinted(tabId);
        }
    }

    /**
     * @return Returns layout observer for tab switcher.
     */
    public TabSwitcherLayoutObserver getTabSwitcherObserver() {
        return mTabSwitcherLayoutObserver;
    }

    /**
     * Creates an instance of the {@link StripLayoutHelperManager}.
     * @param context The current Android {@link Context}.
     * @param managerHost The parent {@link LayoutManagerHost}.
     * @param updateHost The parent {@link LayoutUpdateHost}.
     * @param renderHost The {@link LayoutRenderHost}.
     * @param layerTitleCacheSupplier A supplier of the cache that holds the title textures.
     * @param tabModelStartupInfoSupplier A supplier for the {@link TabModelStartupInfo}.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for registering this class
     *         to lifecycle events.
     * @param multiInstanceManager @{link MultiInstanceManager} passed to @{link TabDragSource} for
     *         drag and drop.
     * @param toolbarContainerView @{link View} passed to @{link TabDragSource} for drag and drop.
     */
    public StripLayoutHelperManager(Context context, LayoutManagerHost managerHost,
            LayoutUpdateHost updateHost, LayoutRenderHost renderHost,
            Supplier<LayerTitleCache> layerTitleCacheSupplier,
            ObservableSupplier<TabModelStartupInfo> tabModelStartupInfoSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MultiInstanceManager multiInstanceManager, View toolbarContainerView) {
        mUpdateHost = updateHost;
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mTabStripTreeProvider = new TabStripSceneLayer(context);
        mTabStripEventHandler = new TabStripEventHandler();
        mTabSwitcherLayoutObserver = new TabSwitcherLayoutObserver();
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mDefaultTitle = context.getString(R.string.tab_loading_default_title);
        mEventFilter =
                new AreaMotionEventFilter(context, mTabStripEventHandler, null, false, false);

        CompositorOnClickHandler selectorClickHandler = new CompositorOnClickHandler() {
            @Override
            public void onClick(long time) {
                handleModelSelectorButtonClick();
            }
        };
        if (ChromeFeatureList.sTabStripRedesign.isEnabled()) {
            createModelSelectorButtonForTsr(context, selectorClickHandler);

            // Model selector button background color.
            // Default bg color is surface inverse.
            int backgroundDefaultColor =
                    context.getResources().getColor(R.color.model_selector_button_bg_color);

            // Incognito bg color is surface 1 baseline for folio, surface 2 baseline for detached.
            int backgroundIncognitoColor = TabManagementFieldTrial.isTabStripFolioEnabled()
                    ? context.getResources().getColor(R.color.default_bg_color_dark_elev_1_baseline)
                    : context.getResources().getColor(
                            R.color.default_bg_color_dark_elev_2_baseline);

            // Model selector button icon color
            // @Todo(zheliooo crbugs.com/1447285) may need to update color using GM3 and update MSB
            // icon per UX suggestion. Temporarily set MSB color match NTB color in normal mode
            int iconDefaultColor = TabUiFeatureUtilities.isTabStripButtonStyleDisabled()
                    ? AppCompatResources
                              .getColorStateList(context, R.color.default_icon_color_tint_list)
                              .getDefaultColor()
                    : context.getResources().getColor(R.color.model_selector_button_icon_color);
            int iconIncognitoColor =
                    context.getResources().getColor(R.color.default_icon_color_secondary_light);

            ((TintedCompositorButton) mModelSelectorButton)
                    .setTint(iconDefaultColor, iconDefaultColor, iconIncognitoColor,
                            iconIncognitoColor);

            ((TintedCompositorButton) mModelSelectorButton)
                    .setBackgroundTint(backgroundDefaultColor, backgroundDefaultColor,
                            backgroundIncognitoColor, backgroundIncognitoColor);

            if (TabManagementFieldTrial.isTabStripFolioEnabled()) {
                // y-offset for folio = lowered tab container + (tab container size - bg size)/2 -
                // folio tab title y-offset = 2 + (38 - 32)/2 - 2 = 3dp
                mModelSelectorButton.setY(MODEL_SELECTOR_BUTTON_BACKGROUND_Y_OFFSET_DP_FOLIO);
            } else if (TabManagementFieldTrial.isTabStripDetachedEnabled()) {
                // y-offset for detached = lowered tab container + (tab container size - bg size)/2
                // = 2 + (38 - 32)/2 = 5dp
                mModelSelectorButton.setY(MODEL_SELECTOR_BUTTON_BACKGROUND_Y_OFFSET_DP_DETACHED);
            }
            mTabStripFadeShort = R.drawable.tab_strip_fade_short_tsr;
            mTabStripFadeLong = R.drawable.tab_strip_fade_long_tsr;
            mTabStripFadeShortWidth = FADE_SHORT_TSR_WIDTH_DP;
            mTabStripFadeLongWidth = FADE_LONG_TSR_WIDTH_DP;

            // Use toolbar menu button padding to align MSB with menu button.
            mMenuButtonPadding = context.getResources().getDimension(R.dimen.button_end_padding)
                    / context.getResources().getDisplayMetrics().density;

        } else {
            mModelSelectorButton = new CompositorButton(context, MODEL_SELECTOR_BUTTON_WIDTH_DP,
                    MODEL_SELECTOR_BUTTON_HEIGHT_DP, selectorClickHandler);
            mModelSelectorButton.setResources(R.drawable.btn_tabstrip_switch_normal,
                    R.drawable.btn_tabstrip_switch_normal, R.drawable.location_bar_incognito_badge,
                    R.drawable.location_bar_incognito_badge);
            mModelSelectorButton.setY(MODEL_SELECTOR_BUTTON_Y_OFFSET_DP);
            mModelSelectorWidth = MODEL_SELECTOR_BUTTON_WIDTH_DP;
            mTabStripFadeShort = R.drawable.tab_strip_fade_short;
            mTabStripFadeLong = R.drawable.tab_strip_fade_long;
            mTabStripFadeShortWidth = FADE_SHORT_WIDTH_DP;
            mTabStripFadeLongWidth = FADE_LONG_WIDTH_DP;
        }
        mModelSelectorButton.setIncognito(false);
        mModelSelectorButton.setVisible(false);
        // Pressed resources are the same as the unpressed resources.
        mModelSelectorButton.setClickSlop(MODEL_SELECTOR_BUTTON_CLICK_SLOP_DP);

        Resources res = context.getResources();
        mHeight = res.getDimension(R.dimen.tab_strip_height) / res.getDisplayMetrics().density;
        mModelSelectorButton.setAccessibilityDescription(
                res.getString(R.string.accessibility_tabstrip_btn_incognito_toggle_standard),
                res.getString(R.string.accessibility_tabstrip_btn_incognito_toggle_incognito));

        mBrowserScrimShowing = false;

        mNormalHelper = new StripLayoutHelper(context, managerHost, updateHost, renderHost, false,
                mModelSelectorButton, multiInstanceManager, toolbarContainerView);
        mIncognitoHelper = new StripLayoutHelper(context, managerHost, updateHost, renderHost, true,
                mModelSelectorButton, multiInstanceManager, toolbarContainerView);

        if (tabModelStartupInfoSupplier != null) {
            if (tabModelStartupInfoSupplier.hasValue()) {
                setTabModelStartupInfo(tabModelStartupInfoSupplier.get());
            } else {
                tabModelStartupInfoSupplier.addObserver(this::setTabModelStartupInfo);
            }
        }

        mNormalHelper.prepareForDragDrop();

        onContextChanged(context);
    }

    private void setTabModelStartupInfo(TabModelStartupInfo startupInfo) {
        mNormalHelper.setTabModelStartupInfo(startupInfo.standardCount,
                startupInfo.standardActiveIndex, startupInfo.createdStandardTabOnStartup);
        mIncognitoHelper.setTabModelStartupInfo(startupInfo.incognitoCount,
                startupInfo.incognitoActiveIndex, startupInfo.createdIncognitoTabOnStartup);
    }

    // Incognito button for Tab Strip Redesign.
    private void createModelSelectorButtonForTsr(
            Context context, CompositorOnClickHandler selectorClickHandler) {
        mModelSelectorButton =
                new TintedCompositorButton(context, MODEL_SELECTOR_BUTTON_BACKGROUND_WIDTH_DP_TSR,
                        MODEL_SELECTOR_BUTTON_BACKGROUND_HEIGHT_DP_TSR, selectorClickHandler,
                        R.drawable.ic_incognito);

        // Tab strip redesign button bg size is 32 * 32.
        ((TintedCompositorButton) mModelSelectorButton)
                .setBackgroundResourceId(R.drawable.bg_circle_tab_strip_button);
        mModelSelectorWidth = MODEL_SELECTOR_BUTTON_BACKGROUND_WIDTH_DP_TSR;
    }

    /**
     * Cleans up internal state.
     */
    public void destroy() {
        mTabStripTreeProvider.destroy();
        mTabStripTreeProvider = null;
        mIncognitoHelper.destroy();
        mNormalHelper.destroy();
        mLifecycleDispatcher.unregister(this);
        if (mTabModelSelector != null) {
            mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                    mTabModelObserver);

            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
            mTabModelSelectorTabModelObserver.destroy();
            mTabModelSelectorTabObserver.destroy();
        }
    }

    @Override
    public void onResumeWithNative() {
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab == null) return;
        getStripLayoutHelper(currentTab.isIncognito())
                .scrollTabToView(LayoutManagerImpl.time(), true);
    }

    @Override
    public void onPauseWithNative() {}

    private void handleModelSelectorButtonClick() {
        if (mTabModelSelector == null) return;
        getActiveStripLayoutHelper().finishAnimationsAndPushTabUpdates();
        if (!mModelSelectorButton.isVisible()) return;
        mTabModelSelector.selectModel(!mTabModelSelector.isIncognitoSelected());
    }

    @VisibleForTesting
    public void simulateClick(float x, float y, boolean fromMouse, int buttons) {
        mTabStripEventHandler.click(x, y, fromMouse, buttons);
    }

    @VisibleForTesting
    public void simulateLongPress(float x, float y) {
        mTabStripEventHandler.onLongPress(x, y);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset) {
        assert mTabStripTreeProvider != null;

        Tab selectedTab = mTabModelSelector.getCurrentModel().getTabAt(
                mTabModelSelector.getCurrentModel().index());
        int selectedTabId = selectedTab == null ? TabModel.INVALID_TAB_INDEX : selectedTab.getId();
        int hoveredTabId = getActiveStripLayoutHelper().getLastHoveredTab() == null
                ? TabModel.INVALID_TAB_INDEX
                : getActiveStripLayoutHelper().getLastHoveredTab().getId();
        mTabStripTreeProvider.pushAndUpdateStrip(this, mLayerTitleCacheSupplier.get(),
                resourceManager, getActiveStripLayoutHelper().getStripLayoutTabsToRender(), yOffset,
                selectedTabId, hoveredTabId);
        return mTabStripTreeProvider;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        // TODO(mdjones): This matches existing behavior but can be improved to return false if
        // the browser controls offset is equal to the browser controls height.
        return true;
    }

    @Override
    public EventFilter getEventFilter() {
        return mEventFilter;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {
        mWidth = width;
        boolean orientationChanged = false;
        if (mOrientation != orientation) {
            mOrientation = orientation;
            orientationChanged = true;
        }
        if (!LocalizationUtils.isLayoutRtl()) {
            if (ChromeFeatureList.sTabStripRedesign.isEnabled()) {
                mModelSelectorButton.setX(mWidth - getModelSelectorButtonWidthWithPadding());
            } else {
                mModelSelectorButton.setX(
                        mWidth - mModelSelectorWidth - MODEL_SELECTOR_BUTTON_PADDING_DP);
            }
        } else {
            if (ChromeFeatureList.sTabStripRedesign.isEnabled()) {
                mModelSelectorButton.setX(
                        getModelSelectorButtonWidthWithPadding() - mModelSelectorWidth);
            } else {
                mModelSelectorButton.setX(MODEL_SELECTOR_BUTTON_PADDING_DP);
            }
        }

        mNormalHelper.onSizeChanged(mWidth, mHeight, orientationChanged, LayoutManagerImpl.time());
        mIncognitoHelper.onSizeChanged(
                mWidth, mHeight, orientationChanged, LayoutManagerImpl.time());

        mStripFilterArea.set(0, 0, mWidth, Math.min(getHeight(), visibleViewportOffsetY));
        mEventFilter.setEventArea(mStripFilterArea);
    }

    private float getModelSelectorButtonWidthWithPadding() {
        if (ChromeFeatureList.sTabStripRedesign.isEnabled()) {
            float modelSelectorWithStripEndPadding =
                    (BUTTON_DESIRED_TOUCH_TARGET_SIZE - mModelSelectorWidth - mMenuButtonPadding)
                            / 2
                    + mMenuButtonPadding;
            return mModelSelectorWidth + modelSelectorWithStripEndPadding;
        } else {
            return mModelSelectorWidth + (MODEL_SELECTOR_BUTTON_PADDING_DP * 2);
        }
    }

    public TintedCompositorButton getNewTabButton() {
        return getActiveStripLayoutHelper().getNewTabButton();
    }

    /**
     * @return The touch target offset to be applied to the new tab button.
     */
    public float getNewTabBtnTouchTargetOffset() {
        return getActiveStripLayoutHelper().getNewTabButtonTouchTargetOffset();
    }

    public CompositorButton getModelSelectorButton() {
        return mModelSelectorButton;
    }

    @Override
    public void getVirtualViews(List<VirtualView> views) {
        if (mBrowserScrimShowing) return;

        getActiveStripLayoutHelper().getVirtualViews(views);
        if (mModelSelectorButton.isVisible()) views.add(mModelSelectorButton);
    }

    @Override
    public boolean shouldHideAndroidBrowserControls() {
        return false;
    }

    /**
     * @return The opacity to use for the fade on the left side of the tab strip.
     */
    public float getLeftFadeOpacity() {
        return getActiveStripLayoutHelper().getLeftFadeOpacity();
    }

    /**
     * @return The opacity to use for the fade on the right side of the tab strip.
     */
    public float getRightFadeOpacity() {
        return getActiveStripLayoutHelper().getRightFadeOpacity();
    }

    public int getLeftFadeDrawable() {
        int leftFadeDrawable;
        if (mModelSelectorButton.isVisible() && LocalizationUtils.isLayoutRtl()) {
            leftFadeDrawable = mTabStripFadeLong;
            mNormalHelper.setLeftFadeWidth(mTabStripFadeLongWidth);
            mIncognitoHelper.setLeftFadeWidth(mTabStripFadeLongWidth);
        } else if (ChromeFeatureList.sTabStripRedesign.isEnabled()
                && !mModelSelectorButton.isVisible() && LocalizationUtils.isLayoutRtl()) {
            // Use fade_medium for TSR left fade when RTL and model selector button not
            // visible.
            leftFadeDrawable = R.drawable.tab_strip_fade_medium_tsr;
            mNormalHelper.setLeftFadeWidth(FADE_MEDIUM_TSR_WIDTH_DP);
            mIncognitoHelper.setLeftFadeWidth(FADE_MEDIUM_TSR_WIDTH_DP);
        } else {
            leftFadeDrawable = mTabStripFadeShort;
            mNormalHelper.setLeftFadeWidth(mTabStripFadeShortWidth);
            mIncognitoHelper.setLeftFadeWidth(mTabStripFadeShortWidth);
        }
        return leftFadeDrawable;
    }

    public int getRightFadeDrawable() {
        int rightFadeDrawable;
        if (mModelSelectorButton.isVisible() && !LocalizationUtils.isLayoutRtl()) {
            rightFadeDrawable = mTabStripFadeLong;
            mNormalHelper.setRightFadeWidth(mTabStripFadeLongWidth);
            mIncognitoHelper.setRightFadeWidth(mTabStripFadeLongWidth);
        } else if (ChromeFeatureList.sTabStripRedesign.isEnabled()
                && !mModelSelectorButton.isVisible() && !LocalizationUtils.isLayoutRtl()) {
            // Use fade_medium for TSR right fade when model selector button not visible.
            rightFadeDrawable = R.drawable.tab_strip_fade_medium_tsr;
            mNormalHelper.setRightFadeWidth(FADE_MEDIUM_TSR_WIDTH_DP);
            mIncognitoHelper.setRightFadeWidth(FADE_MEDIUM_TSR_WIDTH_DP);
        } else {
            rightFadeDrawable = mTabStripFadeShort;
            mNormalHelper.setRightFadeWidth(mTabStripFadeShortWidth);
            mIncognitoHelper.setRightFadeWidth(mTabStripFadeShortWidth);
        }
        return rightFadeDrawable;
    }

    void setModelSelectorButtonVisibleForTesting(boolean isVisible) {
        mModelSelectorButton.setVisible(isVisible);
    }

    /** Update the title cache for the available tabs in the model. */
    private void updateTitleCacheForInit() {
        LayerTitleCache titleCache = mLayerTitleCacheSupplier.get();
        if (mTabModelSelector == null || titleCache == null) return;

        // Make sure any tabs already restored get loaded into the title cache.
        List<TabModel> models = mTabModelSelector.getModels();
        for (int i = 0; i < models.size(); i++) {
            TabModel model = models.get(i);
            for (int j = 0; j < model.getCount(); j++) {
                Tab tab = model.getTabAt(j);
                if (tab != null) {
                    titleCache.getUpdatedTitle(
                            tab, tab.getContext().getString(R.string.tab_loading_default_title));
                }
            }
        }
    }

    /**
     * Sets the {@link TabModelSelector} that this {@link StripLayoutHelperManager} will visually
     * represent, and various objects associated with it.
     * @param modelSelector The {@link TabModelSelector} to visually represent.
     * @param tabCreatorManager The {@link TabCreatorManager}, used to create new tabs.
     */
    public void setTabModelSelector(TabModelSelector modelSelector,
            TabCreatorManager tabCreatorManager) {
        if (mTabModelSelector == modelSelector) return;

        mTabModelObserver = new TabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int launchType,
                    @TabCreationState int creationState, boolean markedForSelection) {
                updateTitleForTab(tab);
            }
        };
        modelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        mTabModelSelector = modelSelector;

        updateTitleCacheForInit();

        if (mTabModelSelector.isTabStateInitialized()) {
            updateModelSwitcherButton();
        } else {
            mTabModelSelector.addObserver(new TabModelSelectorObserver() {
                @Override
                public void onTabStateInitialized() {
                    updateModelSwitcherButton();
                    new Handler().post(() -> mTabModelSelector.removeObserver(this));

                    mNormalHelper.onTabStateInitialized();
                    mIncognitoHelper.onTabStateInitialized();
                }
            });
        }

        boolean tabStateInitialized = mTabModelSelector.isTabStateInitialized();
        mNormalHelper.setTabModel(mTabModelSelector.getModel(false),
                tabCreatorManager.getTabCreator(false), tabStateInitialized);
        mIncognitoHelper.setTabModel(mTabModelSelector.getModel(true),
                tabCreatorManager.getTabCreator(true), tabStateInitialized);
        TabModelFilterProvider provider = mTabModelSelector.getTabModelFilterProvider();
        mNormalHelper.setTabGroupModelFilter(
                (TabGroupModelFilter) provider.getTabModelFilter(false));
        mIncognitoHelper.setTabGroupModelFilter(
                (TabGroupModelFilter) provider.getTabModelFilter(true));
        tabModelSwitched(mTabModelSelector.isIncognitoSelected());

        mTabModelSelectorTabModelObserver = new TabModelSelectorTabModelObserver(modelSelector) {
            /**
             * @return The actual current time of the app in ms.
             */
            public long time() {
                return SystemClock.uptimeMillis();
            }

            @Override
            public void tabRemoved(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabClosed(time(), tab.getId());
                updateModelSwitcherButton();
            }

            @Override
            public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                // For right-direction move, layout helper re-ordering logic
                // expects destination index = position + 1
                getStripLayoutHelper(tab.isIncognito())
                        .tabMoved(time(), tab.getId(), curIndex,
                                newIndex > curIndex ? newIndex + 1 : newIndex);
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabClosureCancelled(time(), tab.getId());
                updateModelSwitcherButton();
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                if (mLayerTitleCacheSupplier.hasValue()) {
                    mLayerTitleCacheSupplier.get().remove(tab.getId());
                }
            }

            @Override
            public void tabPendingClosure(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabClosed(time(), tab.getId());
                updateModelSwitcherButton();
            }

            @Override
            public void onFinishingTabClosure(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabClosed(time(), tab.getId());
                updateModelSwitcherButton();
            }

            @Override
            public void willCloseAllTabs(boolean incognito) {
                getStripLayoutHelper(incognito).willCloseAllTabs();
                updateModelSwitcherButton();
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                if (tab.getId() == lastId) return;
                getStripLayoutHelper(tab.isIncognito())
                        .tabSelected(time(), tab.getId(), lastId, false);
            }

            @Override
            public void didAddTab(
                    Tab tab, int type, int creationState, boolean markedForSelection) {
                boolean onStartup = type == TabLaunchType.FROM_RESTORE;
                getStripLayoutHelper(tab.isIncognito())
                        .tabCreated(time(), tab.getId(), mTabModelSelector.getCurrentTabId(),
                                markedForSelection, false, onStartup);
            }
        };

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(modelSelector) {
            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                if (params.getTransitionType() == PageTransition.HOME_PAGE
                        || (params.getTransitionType() & PageTransition.FROM_ADDRESS_BAR)
                                == PageTransition.FROM_ADDRESS_BAR) {
                    getStripLayoutHelper(tab.isIncognito())
                            .scrollTabToView(LayoutManagerImpl.time(), false);
                }
            }

            @Override
            public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                getStripLayoutHelper(tab.isIncognito()).tabLoadStarted(tab.getId());
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                getStripLayoutHelper(tab.isIncognito()).tabLoadFinished(tab.getId());
            }

            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadStarted(tab.getId());
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadFinished(tab.getId());
            }

            @Override
            public void onPageLoadFailed(Tab tab, int errorCode) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadFinished(tab.getId());
            }

            @Override
            public void onCrash(Tab tab) {
                getStripLayoutHelper(tab.isIncognito()).tabPageLoadFinished(tab.getId());
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                updateTitleForTab(tab);
            }

            @Override
            public void onFaviconUpdated(Tab tab, Bitmap icon, GURL iconUrl) {
                updateTitleForTab(tab);
            }
        };

        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    private void updateTitleForTab(Tab tab) {
        if (mLayerTitleCacheSupplier.get() == null) return;

        String title = mLayerTitleCacheSupplier.get().getUpdatedTitle(tab, mDefaultTitle);
        getStripLayoutHelper(tab.isIncognito()).tabTitleChanged(tab.getId(), title);
        mUpdateHost.requestUpdate();
    }

    public float getHeight() {
        return mHeight;
    }

    public float getWidth() {
        return mWidth;
    }

    public @ColorInt int getBackgroundColor() {
        return TabUiThemeUtil.getTabStripBackgroundColor(mContext, mIsIncognito);
    }

    /**
     * Updates all internal resources and dimensions.
     * @param context The current Android {@link Context}.
     */
    public void onContextChanged(Context context) {
        mContext = context;
        mNormalHelper.onContextChanged(context);
        mIncognitoHelper.onContextChanged(context);
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        getInactiveStripLayoutHelper().finishAnimationsAndPushTabUpdates();
        return getActiveStripLayoutHelper().updateLayout(time);
    }

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public boolean handlesTabCreating() {
        return false;
    }

    private void tabModelSwitched(boolean incognito) {
        if (incognito == mIsIncognito) return;
        mIsIncognito = incognito;

        mIncognitoHelper.tabModelSelected(mIsIncognito);
        mNormalHelper.tabModelSelected(!mIsIncognito);

        updateModelSwitcherButton();

        mUpdateHost.requestUpdate();
    }

    private void updateModelSwitcherButton() {
        mModelSelectorButton.setIncognito(mIsIncognito);
        if (mTabModelSelector != null) {
            boolean isVisible = mTabModelSelector.getModel(true).getCount() != 0;
            mModelSelectorButton.setVisible(isVisible);

            float endMargin = isVisible ? getModelSelectorButtonWidthWithPadding() : 0.0f;

            mNormalHelper.setEndMargin(endMargin, isVisible);
            mIncognitoHelper.setEndMargin(endMargin, true);
        }
    }

    /**
     * @param incognito Whether or not you want the incognito StripLayoutHelper
     * @return The requested StripLayoutHelper.
     */
    @VisibleForTesting
    public StripLayoutHelper getStripLayoutHelper(boolean incognito) {
        return incognito ? mIncognitoHelper : mNormalHelper;
    }

    /**
     * @return The currently visible strip layout helper.
     */
    @VisibleForTesting
    public StripLayoutHelper getActiveStripLayoutHelper() {
        return getStripLayoutHelper(mIsIncognito);
    }

    private StripLayoutHelper getInactiveStripLayoutHelper() {
        return mIsIncognito ? mNormalHelper : mIncognitoHelper;
    }

    void simulateHoverEventForTesting(int event, float x, float y) {
        if (event == MotionEvent.ACTION_HOVER_ENTER) {
            mTabStripEventHandler.onHoverEnter(x, y);
        } else if (event == MotionEvent.ACTION_HOVER_MOVE) {
            mTabStripEventHandler.onHoverMove(x, y);
        } else if (event == MotionEvent.ACTION_HOVER_EXIT) {
            mTabStripEventHandler.onHoverExit();
        }
    }

    void setTabStripTreeProviderForTesting(TabStripSceneLayer tabStripTreeProvider) {
        mTabStripTreeProvider = tabStripTreeProvider;
    }
}
