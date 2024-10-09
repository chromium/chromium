// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Handler;
import android.os.SystemClock;
import android.util.FloatProperty;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnDragListener;
import android.view.ViewStub;
import android.view.animation.Interpolator;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.cc.input.BrowserControlsOffsetTagsInfo;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.ButtonType;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.AreaMotionEventFilter;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.MotionEventHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnClickHandler;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayer;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.TopResumedActivityChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.top.tab_strip.TabStripTransitionCoordinator.TabStripTransitionDelegate;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider.AppHeaderObserver;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * This class handles managing which StripLayoutHelper is currently active and dispatches all input
 * and model events to the proper destination.
 */
public class StripLayoutHelperManager
        implements SceneOverlay,
                PauseResumeWithNativeObserver,
                TabStripTransitionDelegate,
                TopResumedActivityChangedObserver,
                AppHeaderObserver {

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

        public TabModelStartupInfo(
                int standardCount,
                int incognitoCount,
                int standardActiveIndex,
                int incognitoActiveIndex,
                boolean createdStandardTabOnStartup,
                boolean createdIncognitoTabOnStartup) {
            this.standardCount = standardCount;
            this.incognitoCount = incognitoCount;
            this.standardActiveIndex = standardActiveIndex;
            this.incognitoActiveIndex = incognitoActiveIndex;
            this.createdStandardTabOnStartup = createdStandardTabOnStartup;
            this.createdIncognitoTabOnStartup = createdIncognitoTabOnStartup;
        }
    }

    /** Defines if the strip is visible or hidden. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        StripVisibilityState.UNKNOWN,
        StripVisibilityState.VISIBLE,
        StripVisibilityState.GONE,
        StripVisibilityState.INVISIBLE,
    })
    @interface StripVisibilityState {
        int UNKNOWN = 0; // Strip visibility is unknown.
        int VISIBLE = 1; // Strip is visible.
        int GONE = 2; // Strip is hidden by a height transition.
        int INVISIBLE = 3; // Strip is hidden by an in-place fade transition.
    }

    private static final FloatProperty<StripLayoutHelperManager> SCRIM_OPACITY =
            new FloatProperty<>("scrimOpacity") {
                @Override
                public void setValue(StripLayoutHelperManager object, float value) {
                    object.mStripTransitionScrimOpacity = value;
                }

                @Override
                public Float get(StripLayoutHelperManager object) {
                    return object.mStripTransitionScrimOpacity;
                }
            };

    // Model selector buttons constants.
    private static final float MODEL_SELECTOR_BUTTON_BACKGROUND_Y_OFFSET_DP = 3.f;
    private static final float MODEL_SELECTOR_BUTTON_BACKGROUND_WIDTH_DP = 32.f;
    private static final float MODEL_SELECTOR_BUTTON_BACKGROUND_HEIGHT_DP = 32.f;
    private static final float MODEL_SELECTOR_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY = 0.12f;
    private static final float MODEL_SELECTOR_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY = 0.08f;
    private static final float MODEL_SELECTOR_BUTTON_CLICK_SLOP_DP = 12.f;
    private static final float BUTTON_DESIRED_TOUCH_TARGET_SIZE = 48.f;

    // Tab strip transition constants.
    @VisibleForTesting
    static final Interpolator TAB_STRIP_TRANSITION_INTERPOLATOR =
            Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR;

    // Fade constants.
    static final float FADE_SHORT_WIDTH_DP = 60;
    static final float FADE_MEDIUM_WIDTH_DP = 72;
    static final float FADE_LONG_WIDTH_DP = 136;

    // Caching Variables
    private final RectF mStripFilterArea = new RectF();
    private final boolean mIsLayoutOptimizationsEnabled;

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
    private float mWidth; // in dp units
    private float mHeight; // Height of the entire tab strip compositor layer in DP.
    private final float mScrollableStripHeight; // Height of the scrollable tab strip layer in DP.
    private boolean mIsVerticalScrollInProgress; // Is the tab strip is being scrolled by a gesture.

    // Padding regions that tabs should remain untouchable.
    private float mLeftPadding; // in dp units
    private float mRightPadding; // in dp units
    private float mTopPadding; // in dp units
    private final float mDensity;
    private int mOrientation;
    @Nullable private TintedCompositorButton mModelSelectorButton;
    private Context mContext;
    private boolean mTabStripObscured;
    private float mStripTransitionScrimOpacity;
    private Animator mFadeTransitionAnimator;
    private boolean mIsHeightTransitioning;
    private final ToolbarManager mToolbarManager;
    private final StatusBarColorController mStatusBarColorController;
    private TabStripSceneLayer mTabStripTreeProvider;
    private TabStripEventHandler mTabStripEventHandler;
    private TabSwitcherLayoutObserver mTabSwitcherLayoutObserver;
    private final View mToolbarControlContainer;
    private final ViewStub mTabHoverCardViewStub;
    private float mModelSelectorWidth;
    private float mLastVisibleViewportOffsetY;

    /**
     * Whether the current activity is the top resumed activity. This is only relevant for use in
     * the desktop windowing mode, to determine the tab strip background color.
     */
    private boolean mIsTopResumedActivity;

    private final DesktopWindowStateProvider mDesktopWindowStateProvider;

    // 3-dots menu button with tab strip end padding
    private float mStripEndPadding;
    private TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private final Callback<TabModel> mCurrentTabModelObserver =
            (tabModel) -> {
                tabModelSwitched(tabModel.isIncognito());
            };

    private TabModelObserver mTabModelObserver;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final String mDefaultTitle;
    private final ObservableSupplier<LayerTitleCache> mLayerTitleCacheSupplier;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final Callback<Integer> mStripVisibilityStateObserver;
    private ObservableSupplierImpl<Integer> mStripVisibilityStateSupplier;
    private boolean mAnimationsDisabledForTesting;

    // Drag-Drop
    @Nullable private TabDragSource mTabDragSource;

    private class TabStripEventHandler implements MotionEventHandler {
        @Override
        public void onDown(float x, float y, boolean fromMouse, int buttons) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
            if (mModelSelectorButton != null
                    && mModelSelectorButton.onDown(x, y, fromMouse, buttons)) {
                return;
            }
            getActiveStripLayoutHelper().onDown(time(), x, y, fromMouse, buttons);
        }

        @Override
        public void onUpOrCancel() {
            if (mModelSelectorButton != null
                    && mModelSelectorButton.onUpOrCancel()
                    && mTabModelSelector != null) {
                getActiveStripLayoutHelper().finishAnimationsAndPushTabUpdates();
                if (!mModelSelectorButton.isVisible()) return;
                mTabModelSelector.selectModel(!mTabModelSelector.isIncognitoSelected());
                return;
            }
            getActiveStripLayoutHelper().onUpOrCancel(time());
        }

        @Override
        public void drag(float x, float y, float dx, float dy, float tx, float ty) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
            if (mModelSelectorButton != null) {
                mModelSelectorButton.drag(x, y);
            }
            getActiveStripLayoutHelper().drag(time(), x, y, dx);
        }

        @Override
        public void click(float x, float y, boolean fromMouse, int buttons) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
            long time = time();
            if (mModelSelectorButton != null
                    && mModelSelectorButton.click(x, y, fromMouse, buttons)) {
                mModelSelectorButton.handleClick(time);
                return;
            }
            getActiveStripLayoutHelper().click(time(), x, y, fromMouse, buttons);
        }

        @Override
        public void fling(float x, float y, float velocityX, float velocityY) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
            getActiveStripLayoutHelper().fling(time(), x, y, velocityX, velocityY);
        }

        @Override
        public void onLongPress(float x, float y) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
            getActiveStripLayoutHelper().onLongPress(time(), x, y);
        }

        @Override
        public void onPinch(float x0, float y0, float x1, float y1, boolean firstEvent) {
            // Not implemented.
        }

        @Override
        public void onHoverEnter(float x, float y) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }

            // Inflate the hover card ViewStub if not already inflated.
            if (mTabHoverCardViewStub.getParent() != null) {
                mTabHoverCardViewStub.inflate();
            }
            getActiveStripLayoutHelper().onHoverEnter(x, y);
        }

        @Override
        public void onHoverMove(float x, float y) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
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

    /** Observer for Tab Switcher layout events. */
    class TabSwitcherLayoutObserver implements LayoutStateObserver {
        @Override
        public void onFinishedShowing(@LayoutType int layoutType) {
            if (layoutType != LayoutType.TAB_SWITCHER) return;
            mTabStripObscured = true;
        }

        @Override
        public void onStartedHiding(@LayoutType int layoutType) {
            if (layoutType != LayoutType.TAB_SWITCHER) return;
            mTabStripObscured = false;

            // Expand tab group on GTS exit.
            mNormalHelper.expandGroupOnGtsExit();
            mIncognitoHelper.expandGroupOnGtsExit();
        }
    }

    /**
     * @return Returns layout observer for tab switcher.
     */
    public TabSwitcherLayoutObserver getTabSwitcherObserver() {
        return mTabSwitcherLayoutObserver;
    }

    /**
     * Creates an instance of the StripLayoutHelperManager.
     *
     * @param context The current Android Context.
     * @param managerHost The parent LayoutManagerHost.
     * @param updateHost The parent LayoutUpdateHost.
     * @param renderHost The LayoutRenderHost.
     * @param layerTitleCacheSupplier A supplier of the cache that holds the title textures.
     * @param tabModelStartupInfoSupplier A supplier for the TabModelStartupInfo.
     * @param lifecycleDispatcher The ActivityLifecycleDispatcher for registering this class to
     *     lifecycle events.
     * @param multiInstanceManager MultiInstanceManager passed to TabDragSource for drag and drop.
     * @param dragDropDelegate DragAndDropDelegate passed to TabDragSource to initiate tab drag and
     *     drop.
     * @param toolbarContainerView View passed to TabDragSource for drag and drop.
     * @param tabHoverCardViewStub The ViewStub representing the strip tab hover card.
     * @param tabContentManagerSupplier Supplier of the TabContentManager instance.
     * @param browserControlsStateProvider BrowserControlsStateProvider for drag drop.
     * @param toolbarManager The ToolbarManager instance.
     * @param desktopWindowStateProvider The DesktopWindowStateProvider for the app header.
     */
    public StripLayoutHelperManager(
            Context context,
            LayoutManagerHost managerHost,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            ObservableSupplier<LayerTitleCache> layerTitleCacheSupplier,
            ObservableSupplier<TabModelStartupInfo> tabModelStartupInfoSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MultiInstanceManager multiInstanceManager,
            DragAndDropDelegate dragDropDelegate,
            View toolbarContainerView,
            @NonNull ViewStub tabHoverCardViewStub,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull WindowAndroid windowAndroid,
            // TODO(crbug.com/40939440): Avoid passing the ToolbarManager instance. Potentially
            // implement an interface to manage strip transition states.
            @NonNull ToolbarManager toolbarManager,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider,
            ActionConfirmationManager actionConfirmationManager,
            DataSharingTabManager dataSharingTabManager) {
        Resources res = context.getResources();
        mUpdateHost = updateHost;
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mDensity = res.getDisplayMetrics().density;
        mTabStripTreeProvider = new TabStripSceneLayer(mDensity);
        mTabStripEventHandler = new TabStripEventHandler();
        mTabSwitcherLayoutObserver = new TabSwitcherLayoutObserver();
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mDefaultTitle = context.getString(R.string.tab_loading_default_title);
        mToolbarControlContainer = toolbarContainerView;
        mEventFilter =
                new AreaMotionEventFilter(context, mTabStripEventHandler, null, false, false);

        mIsLayoutOptimizationsEnabled =
                ToolbarFeatures.isTabStripWindowLayoutOptimizationEnabled(true);
        mScrollableStripHeight = res.getDimension(R.dimen.tab_strip_height) / mDensity;
        mHeight =
                mIsLayoutOptimizationsEnabled
                        ? toolbarManager.getTabStripHeightSupplier().get() / mDensity
                        : mScrollableStripHeight;
        mTopPadding = mHeight - mScrollableStripHeight;
        mDesktopWindowStateProvider = desktopWindowStateProvider;
        mStripVisibilityStateSupplier = new ObservableSupplierImpl<>(StripVisibilityState.UNKNOWN);
        mStripVisibilityStateObserver =
                state -> {
                    // Consume motion events only on a visible strip.
                    mEventFilter.setEventArea(
                            state == StripVisibilityState.VISIBLE ? mStripFilterArea : null);
                };
        mStripVisibilityStateSupplier.addObserver(mStripVisibilityStateObserver);

        if (!ChromeFeatureList.sTabStripIncognitoMigration.isEnabled()) {
            StripLayoutViewOnClickHandler selectorClickHandler =
                    (time, view) -> handleModelSelectorButtonClick();
            createModelSelectorButton(context, selectorClickHandler);
        }
        // Use toolbar menu button padding to align MSB with menu button.
        mStripEndPadding = res.getDimension(R.dimen.button_end_padding) / mDensity;

        mTabStripObscured = false;

        mTabHoverCardViewStub = tabHoverCardViewStub;
        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            mTabDragSource =
                    new TabDragSource(
                            context,
                            this::getActiveStripLayoutHelper,
                            () -> !mTabStripObscured,
                            tabContentManagerSupplier,
                            mLayerTitleCacheSupplier,
                            multiInstanceManager,
                            dragDropDelegate,
                            browserControlsStateProvider,
                            windowAndroid,
                            toolbarManager.getTabStripHeightSupplier());
        }

        mToolbarManager = toolbarManager;
        mStatusBarColorController = mToolbarManager.getStatusBarColorController();

        mNormalHelper =
                new StripLayoutHelper(
                        context,
                        managerHost,
                        updateHost,
                        renderHost,
                        false,
                        mModelSelectorButton,
                        mTabDragSource,
                        toolbarContainerView,
                        windowAndroid,
                        actionConfirmationManager,
                        dataSharingTabManager,
                        toolbarManager.getTabStripHeightSupplier().get(),
                        () ->
                                !mTabStripObscured
                                        && getStripVisibilityState()
                                                == StripVisibilityState.VISIBLE);
        mIncognitoHelper =
                new StripLayoutHelper(
                        context,
                        managerHost,
                        updateHost,
                        renderHost,
                        true,
                        mModelSelectorButton,
                        mTabDragSource,
                        toolbarContainerView,
                        windowAndroid,
                        actionConfirmationManager,
                        dataSharingTabManager,
                        toolbarManager.getTabStripHeightSupplier().get(),
                        () ->
                                !mTabStripObscured
                                        && getStripVisibilityState()
                                                == StripVisibilityState.VISIBLE);

        tabHoverCardViewStub.setOnInflateListener(
                (viewStub, view) -> {
                    var hoverCardView = (StripTabHoverCardView) view;
                    hoverCardView.initialize(mTabModelSelector, tabContentManagerSupplier);
                    mNormalHelper.setTabHoverCardView(hoverCardView);
                    mIncognitoHelper.setTabHoverCardView(hoverCardView);
                });

        if (tabModelStartupInfoSupplier != null) {
            if (tabModelStartupInfoSupplier.hasValue()) {
                setTabModelStartupInfo(tabModelStartupInfoSupplier.get());
            } else {
                tabModelStartupInfoSupplier.addObserver(this::setTabModelStartupInfo);
            }
        }

        mLayerTitleCacheSupplier.addObserver(
                (LayerTitleCache layerTitleCache) -> {
                    mNormalHelper.setLayerTitleCache(layerTitleCache);
                    mIncognitoHelper.setLayerTitleCache(layerTitleCache);
                });

        onContextChanged(context);
        if (mDesktopWindowStateProvider != null) {
            mDesktopWindowStateProvider.addObserver(this);
            mIsTopResumedActivity = !mDesktopWindowStateProvider.isInUnfocusedDesktopWindow();
        } else {
            mIsTopResumedActivity = AppHeaderUtils.isActivityFocusedAtStartup(lifecycleDispatcher);
        }
        if (AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateProvider)) {
            onAppHeaderStateChanged(mDesktopWindowStateProvider.getAppHeaderState());
        }
    }

    private void setTabModelStartupInfo(TabModelStartupInfo startupInfo) {
        mNormalHelper.setTabModelStartupInfo(
                startupInfo.standardCount,
                startupInfo.standardActiveIndex,
                startupInfo.createdStandardTabOnStartup);
        mIncognitoHelper.setTabModelStartupInfo(
                startupInfo.incognitoCount,
                startupInfo.incognitoActiveIndex,
                startupInfo.createdIncognitoTabOnStartup);
    }

    // Incognito button for Tab Strip Redesign.
    private void createModelSelectorButton(
            Context context, StripLayoutViewOnClickHandler selectorClickHandler) {
        mModelSelectorButton =
                new TintedCompositorButton(
                        context,
                        ButtonType.INCOGNITO_SWITCHER,
                        null,
                        MODEL_SELECTOR_BUTTON_BACKGROUND_WIDTH_DP,
                        MODEL_SELECTOR_BUTTON_BACKGROUND_HEIGHT_DP,
                        selectorClickHandler,
                        R.drawable.ic_incognito);

        // Tab strip redesign button bg size is 32 * 32.
        mModelSelectorButton.setBackgroundResourceId(R.drawable.bg_circle_tab_strip_button);

        mModelSelectorWidth = MODEL_SELECTOR_BUTTON_BACKGROUND_WIDTH_DP;

        // Model selector button background color.
        // Default bg color is surface inverse.
        @ColorInt
        int backgroundDefaultColor = context.getColor(R.color.model_selector_button_bg_color);

        // Incognito bg color is surface 1 baseline.
        @ColorInt
        int backgroundIncognitoColor =
                context.getColor(R.color.default_bg_color_dark_elev_1_baseline);

        @ColorInt
        int apsBackgroundHoveredColor =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultTextColor(context),
                        MODEL_SELECTOR_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY);
        @ColorInt
        int apsBackgroundPressedColor =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultTextColor(context),
                        MODEL_SELECTOR_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY);
        @ColorInt
        int apsBackgroundHoveredIncognitoColor =
                ColorUtils.setAlphaComponentWithFloat(
                        context.getColor(R.color.tab_strip_button_hover_bg_color),
                        MODEL_SELECTOR_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY);
        @ColorInt
        int apsBackgroundPressedIncognitoColor =
                ColorUtils.setAlphaComponentWithFloat(
                        context.getColor(R.color.tab_strip_button_hover_bg_color),
                        MODEL_SELECTOR_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY);

        @ColorInt
        int iconDefaultColor =
                AppCompatResources.getColorStateList(context, R.color.default_icon_color_tint_list)
                        .getDefaultColor();
        @ColorInt
        int iconIncognitoColor = context.getColor(R.color.default_icon_color_secondary_light);

        mModelSelectorButton.setTint(
                iconDefaultColor, iconDefaultColor, iconIncognitoColor, iconIncognitoColor);

        mModelSelectorButton.setBackgroundTint(
                backgroundDefaultColor,
                backgroundDefaultColor,
                backgroundIncognitoColor,
                backgroundIncognitoColor,
                apsBackgroundHoveredColor,
                apsBackgroundPressedColor,
                apsBackgroundHoveredIncognitoColor,
                apsBackgroundPressedIncognitoColor);

        // y-offset for folio = lowered tab container + (tab container size - bg size)/2 -
        // folio tab title y-offset = 2 + (38 - 32)/2 - 2 = 3dp
        mModelSelectorButton.setDrawY(MODEL_SELECTOR_BUTTON_BACKGROUND_Y_OFFSET_DP + mTopPadding);

        mModelSelectorButton.setIncognito(false);
        mModelSelectorButton.setVisible(false);
        // Pressed resources are the same as the unpressed resources.
        mModelSelectorButton.setClickSlop(MODEL_SELECTOR_BUTTON_CLICK_SLOP_DP);

        mModelSelectorButton.setAccessibilityDescription(
                context.getResources()
                        .getString(R.string.accessibility_tabstrip_btn_incognito_toggle_standard),
                context.getResources()
                        .getString(R.string.accessibility_tabstrip_btn_incognito_toggle_incognito));
    }

    /** Cleans up internal state. */
    public void destroy() {
        mTabStripTreeProvider.destroy();
        mTabStripTreeProvider = null;
        mIncognitoHelper.destroy();
        mNormalHelper.destroy();
        mLifecycleDispatcher.unregister(this);
        if (mTabModelSelector != null) {
            mTabModelSelector
                    .getTabModelFilterProvider()
                    .removeTabModelFilterObserver(mTabModelObserver);

            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
            mTabModelSelectorTabModelObserver.destroy();
            mTabModelSelectorTabObserver.destroy();
        }
        mTabDragSource = null;
        if (mDesktopWindowStateProvider != null) {
            mDesktopWindowStateProvider.removeObserver(this);
        }
        mStripVisibilityStateSupplier.removeObserver(mStripVisibilityStateObserver);
    }

    /** Mark whether tab strip is hidden by a height transition. */
    public void setIsTabStripHidden(boolean isHidden) {
        mStripVisibilityStateSupplier.set(
                isHidden ? StripVisibilityState.GONE : StripVisibilityState.VISIBLE);
        mStatusBarColorController.setTabStripHiddenOnTablet(isHidden);
    }

    @Override
    public void onResumeWithNative() {
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab == null) return;
        getStripLayoutHelper(currentTab.isIncognito())
                .scrollTabToView(LayoutManagerImpl.time(), true);
    }

    @Override
    public void onPauseWithNative() {
        // Clear any persisting tab strip hover state when the activity is paused.
        getActiveStripLayoutHelper().onHoverExit();
    }

    private void handleModelSelectorButtonClick() {
        if (mTabModelSelector == null) return;
        getActiveStripLayoutHelper().finishAnimationsAndPushTabUpdates();
        if (!mModelSelectorButton.isVisible()) return;
        mTabModelSelector.selectModel(!mTabModelSelector.isIncognitoSelected());
        RecordUserAction.record("MobileToolbarModelSelected");
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

        Tab selectedTab =
                mTabModelSelector
                        .getCurrentModel()
                        .getTabAt(mTabModelSelector.getCurrentModel().index());
        int selectedTabId = selectedTab == null ? TabModel.INVALID_TAB_INDEX : selectedTab.getId();
        int hoveredTabId =
                getActiveStripLayoutHelper().getLastHoveredTab() == null
                        ? TabModel.INVALID_TAB_INDEX
                        : getActiveStripLayoutHelper().getLastHoveredTab().getTabId();

        // When tab strip is hiding, animation will trigger the toolbar moving up and tab
        // strip fade-out in place. In this case the tab strip should not move at all.
        if (duringTabStripHeightTransition()) {
            // During tab strip transition, make the yOffset stick to the top of the browser
            // controls. This assumes on tablet there are no other components on top of the control
            // container.
            float visibleHeight = yOffset;
            if (visibleHeight < 0) visibleHeight += getHeight();

            // The fade-out is implemented by adding a scrim layer on top of the tab strip, with the
            // same bg as the toolbar background color.
            calculateScrimOpacityDuringHeightTransition(visibleHeight);
            mStatusBarColorController.setTabStripColorOverlay(
                    getStripTransitionScrimColor(), mStripTransitionScrimOpacity);

            yOffset = 0;
        } else if (ToolbarFeatures.isBrowserControlsInVizEnabled(
                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext))
                && mIsVerticalScrollInProgress) {
            // With bciv, we don't want anything else controlling the offset while scrolling.
            // Tabstrip currently has no min height, so setting to 0 is ok.
            yOffset = 0;
        } else if (getStripVisibilityState() == StripVisibilityState.GONE) {
            // When the tab strip is hidden by a height transition, the stable offset of this scene
            // layer should be a negative value.
            yOffset -= getHeight();
        }
        mTabStripTreeProvider.pushAndUpdateStrip(
                this,
                mLayerTitleCacheSupplier.get(),
                resourceManager,
                getActiveStripLayoutHelper().getStripLayoutTabsToRender(),
                getActiveStripLayoutHelper().getStripLayoutGroupTitlesToRender(),
                yOffset,
                selectedTabId,
                hoveredTabId,
                getStripTransitionScrimColor(),
                mStripTransitionScrimOpacity,
                mLeftPadding,
                mRightPadding,
                mTopPadding);
        return mTabStripTreeProvider;
    }

    private int getStripTransitionScrimColor() {
        return mToolbarManager.getPrimaryColor();
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        // TODO(mdjones): This matches existing behavior but can be improved to return false if
        // the browser controls offset is equal to the browser controls height.
        return !mTabStripObscured;
    }

    @Override
    public EventFilter getEventFilter() {
        return mEventFilter;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {
        mWidth = width;
        mLastVisibleViewportOffsetY = visibleViewportOffsetY;
        boolean orientationChanged = false;
        if (mOrientation != orientation) {
            mOrientation = orientation;
            orientationChanged = true;
        }
        if (mModelSelectorButton != null) {
            mModelSelectorButton.setDrawY(
                    mTopPadding + MODEL_SELECTOR_BUTTON_BACKGROUND_Y_OFFSET_DP);
            if (!LocalizationUtils.isLayoutRtl()) {
                mModelSelectorButton.setDrawX(
                        mWidth - mRightPadding - getModelSelectorButtonWidthWithEndPadding());
            } else {
                mModelSelectorButton.setDrawX(
                        mLeftPadding
                                + getModelSelectorButtonWidthWithEndPadding()
                                - mModelSelectorWidth);
            }
        }

        mNormalHelper.onSizeChanged(
                mWidth,
                mScrollableStripHeight,
                orientationChanged,
                LayoutManagerImpl.time(),
                mLeftPadding,
                mRightPadding);
        mIncognitoHelper.onSizeChanged(
                mWidth,
                mScrollableStripHeight,
                orientationChanged,
                LayoutManagerImpl.time(),
                mLeftPadding,
                mRightPadding);

        mStripFilterArea.set(
                mLeftPadding,
                mTopPadding,
                mWidth - mRightPadding,
                Math.min(getHeight(), visibleViewportOffsetY));
        // Avoid handling motion events when invisible strip state persists after a size change.
        if (getStripVisibilityState() == StripVisibilityState.VISIBLE) {
            mEventFilter.setEventArea(mStripFilterArea);
        }
    }

    // Implements TabStripTransitionDelegate.

    @Override
    public void onHeightChanged(int newHeightPx) {
        mIsHeightTransitioning = true;
        boolean hideStrip = newHeightPx == 0;
        mStripVisibilityStateSupplier.set(
                hideStrip ? StripVisibilityState.GONE : StripVisibilityState.VISIBLE);
        mStripTransitionScrimOpacity = hideStrip ? 0f : 1f;
        // Update the strip visibility state in StatusBarController just after the margins are
        // updated during a hide->show transition so that the status bar assumes the base tab strip
        // color for the remaining duration of the transition while a scrim is applied.
        if (!hideStrip) {
            mStatusBarColorController.setTabStripHiddenOnTablet(false);
        }
        // Set the status bar color and scrim overlay at the start of the transition.
        mStatusBarColorController.setTabStripColorOverlay(
                getStripTransitionScrimColor(), mStripTransitionScrimOpacity);

        if (mIsLayoutOptimizationsEnabled) {
            // Convert the input HeightPx to Dp.
            mHeight = newHeightPx / mDensity;

            // TODO(crbug/331490430): Revisit how we position the scrollable strip.
            mTopPadding = Math.max(0, mHeight - mScrollableStripHeight);
            onSizeChanged(mWidth, mHeight, mLastVisibleViewportOffsetY, mOrientation);
        }
    }

    @Override
    public void onFadeTransitionRequested(float startOpacity, float endOpacity, int durationMs) {
        boolean showStrip = endOpacity == 0f;
        if (mAnimationsDisabledForTesting) {
            onFadeTransitionEnd(showStrip);
            return;
        }
        if (mFadeTransitionAnimator != null && mFadeTransitionAnimator.isRunning()) {
            mFadeTransitionAnimator.cancel();
        }
        mFadeTransitionAnimator =
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        this,
                        StripLayoutHelperManager.SCRIM_OPACITY,
                        startOpacity,
                        endOpacity,
                        durationMs);
        mFadeTransitionAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(@NonNull Animator animation) {
                        onFadeTransitionEnd(showStrip);
                    }
                });
        mFadeTransitionAnimator.start();
    }

    private void onFadeTransitionEnd(boolean showStrip) {
        mFadeTransitionAnimator = null;
        mStripVisibilityStateSupplier.set(
                showStrip ? StripVisibilityState.VISIBLE : StripVisibilityState.INVISIBLE);
    }

    @Override
    public void onHeightTransitionFinished() {
        mIsHeightTransitioning = false;
        mStripTransitionScrimOpacity = 0f;
        //  Update the strip visibility state in StatusBarColorController only after a show->hide
        // transition, so that the status bar assumes the toolbar color when the strip is hidden.
        if (getStripVisibilityState() == StripVisibilityState.GONE) {
            mStatusBarColorController.setTabStripHiddenOnTablet(true);
        }
        mStatusBarColorController.setTabStripColorOverlay(
                ScrimProperties.INVALID_COLOR, mStripTransitionScrimOpacity);
    }

    private boolean duringTabStripHeightTransition() {
        return mIsHeightTransitioning;
    }

    @VisibleForTesting
    float calculateScrimOpacityDuringHeightTransition(float visibleHeight) {
        if (!duringTabStripHeightTransition()) {
            return 0.0f;
        }

        // Stop any running fade transition animation that is updating the scrim opacity.
        if (mFadeTransitionAnimator != null && mFadeTransitionAnimator.isRunning()) {
            mFadeTransitionAnimator.cancel();
        }

        // Otherwise, the alpha fraction is based on the percent of the tab strip visibility.
        float ratio = 1 - visibleHeight / mHeight;
        float newOpacity = TAB_STRIP_TRANSITION_INTERPOLATOR.getInterpolation(ratio);

        boolean isHidden = getStripVisibilityState() == StripVisibilityState.GONE;

        // There is a known issue where the scrim opacity for a hide->show transition incorrectly
        // gets updated to 1f (when yOffset = 0) in concluding frame updates during the transition,
        // thereby making the transition janky (b/324130906). This could be due to frame updates
        // initiated potentially by other sources before a timely dispatch of #onTransitionFinished.
        // The following logic is to prevent such jank from surfacing in both directions of
        // transition.
        // If the tab strip is hiding, new opacity should be >= current opacity; if the tab strip is
        // showing, new opacity should be <= current opacity. Otherwise, ignore the new value and
        // use the current value.
        if ((isHidden && newOpacity >= mStripTransitionScrimOpacity)
                || (!isHidden && newOpacity <= mStripTransitionScrimOpacity)) {
            mStripTransitionScrimOpacity = newOpacity;
        }

        return mStripTransitionScrimOpacity;
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        // TODO (crbug/328055199): Check if losing focus to a non-Chrome task.
        if (!mIsLayoutOptimizationsEnabled) return;
        mIsTopResumedActivity = isTopResumedActivity;
        mUpdateHost.requestUpdate();
    }

    private float getModelSelectorButtonWidthWithEndPadding() {
        return mModelSelectorWidth + mStripEndPadding;
    }

    /**
     * @return The start padding needed for model selector button to ensure there is enough space
     *     for touch target.
     */
    private float getButtonStartPaddingForTouchTarget() {
        if (mModelSelectorButton != null && mModelSelectorButton.isVisible()) {
            return BUTTON_DESIRED_TOUCH_TARGET_SIZE
                    - mModelSelectorButton.getWidth()
                    - mStripEndPadding;
        } else {
            return 0.f;
        }
    }

    public TintedCompositorButton getNewTabButton() {
        return getActiveStripLayoutHelper().getNewTabButton();
    }

    public @ColorInt int getSelectedOutlineGroupTint(int id, boolean shouldShowOutline) {
        return getActiveStripLayoutHelper().getSelectedOutlineGroupTint(id, shouldShowOutline);
    }

    public boolean shouldShowTabOutline(StripLayoutTab tab) {
        return getActiveStripLayoutHelper().shouldShowTabOutline(tab);
    }

    /**
     * @return The touch target offset to be applied to the new tab button.
     */
    public float getNewTabBtnVisualOffset() {
        return getActiveStripLayoutHelper().getNewTabButtonVisualOffset();
    }

    public CompositorButton getModelSelectorButton() {
        return mModelSelectorButton;
    }

    @Override
    public void getVirtualViews(List<VirtualView> views) {
        if (mTabStripObscured) return;
        if (duringTabStripHeightTransition()
                || getStripVisibilityState() == StripVisibilityState.GONE) {
            return;
        }
        // Remove the a11y views when top controls is partially invisible.
        if (mBrowserControlsStateProvider.getTopControlOffset() < 0) return;

        getActiveStripLayoutHelper().getVirtualViews(views);
        if (mModelSelectorButton != null && mModelSelectorButton.isVisible()) {
            views.add(mModelSelectorButton);
        }
    }

    @Override
    public boolean shouldHideAndroidBrowserControls() {
        return false;
    }

    /** Allow / disallow system gestures on touchable areas on the strip. */
    private void updateTouchableAreas() {
        // #setSystemGestureExclusionRects requires API Q.
        if (VERSION.SDK_INT < VERSION_CODES.Q || !mIsLayoutOptimizationsEnabled) return;

        if (getStripVisibilityState() == StripVisibilityState.INVISIBLE) {
            // Reset the system gesture exclusion rects to allow system gestures on the tab strip
            // area.
            mToolbarControlContainer.setSystemGestureExclusionRects(List.of(new Rect(0, 0, 0, 0)));
            return;
        }

        // #setSystemGestureExclusionRects allows Chrome to receive touch events on the tab strip
        // when it is drawn under the system gesture area so that the strip remains accessible.
        List<Rect> rects = new ArrayList<>();
        RectF tabStripRectDp = new RectF(getActiveStripLayoutHelper().getTouchableRect());
        tabStripRectDp.top = mTopPadding;
        tabStripRectDp.bottom = mHeight;

        Rect tabStripRect =
                new Rect(
                        (int) Math.floor(tabStripRectDp.left * mDensity),
                        (int) Math.floor(tabStripRectDp.top * mDensity),
                        (int) Math.ceil(tabStripRectDp.right * mDensity),
                        (int) Math.ceil(tabStripRectDp.bottom * mDensity));
        rects.add(tabStripRect);

        if (mModelSelectorButton != null && mModelSelectorButton.isVisible()) {
            var msbTouchRect = new RectF();
            mModelSelectorButton.getTouchTarget(msbTouchRect);
            Rect msbRect =
                    new Rect(
                            (int) Math.floor(msbTouchRect.left * mDensity),
                            (int) Math.max(Math.floor(msbTouchRect.top * mDensity), mTopPadding),
                            (int) Math.ceil(msbTouchRect.right * mDensity),
                            (int) Math.min(Math.ceil(msbTouchRect.bottom * mDensity), mHeight));
            rects.add(msbRect);
        }
        mToolbarControlContainer.setSystemGestureExclusionRects(rects);
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
        if (LocalizationUtils.isLayoutRtl()) {
            if (mModelSelectorButton != null && mModelSelectorButton.isVisible()) {
                leftFadeDrawable = R.drawable.tab_strip_fade_long;
                mNormalHelper.setLeftFadeWidth(FADE_LONG_WIDTH_DP);
                mIncognitoHelper.setLeftFadeWidth(FADE_LONG_WIDTH_DP);
            } else {
                // Use fade_medium for left fade when RTL and model selector button not
                // visible.
                leftFadeDrawable = R.drawable.tab_strip_fade_medium;
                mNormalHelper.setLeftFadeWidth(FADE_MEDIUM_WIDTH_DP);
                mIncognitoHelper.setLeftFadeWidth(FADE_MEDIUM_WIDTH_DP);
            }
        } else {
            leftFadeDrawable = R.drawable.tab_strip_fade_short;
            mNormalHelper.setLeftFadeWidth(FADE_SHORT_WIDTH_DP);
            mIncognitoHelper.setLeftFadeWidth(FADE_SHORT_WIDTH_DP);
        }
        return leftFadeDrawable;
    }

    public @DrawableRes int getRightFadeDrawable() {
        @DrawableRes int rightFadeDrawable;
        if (!LocalizationUtils.isLayoutRtl()) {
            if (mModelSelectorButton != null && mModelSelectorButton.isVisible()) {
                rightFadeDrawable = R.drawable.tab_strip_fade_long;
                mNormalHelper.setRightFadeWidth(FADE_LONG_WIDTH_DP);
                mIncognitoHelper.setRightFadeWidth(FADE_LONG_WIDTH_DP);
            } else {
                // Use fade_medium for right fade when model selector button not visible.
                rightFadeDrawable = R.drawable.tab_strip_fade_medium;
                mNormalHelper.setRightFadeWidth(FADE_MEDIUM_WIDTH_DP);
                mIncognitoHelper.setRightFadeWidth(FADE_MEDIUM_WIDTH_DP);
            }
        } else {
            rightFadeDrawable = R.drawable.tab_strip_fade_short;
            mNormalHelper.setRightFadeWidth(FADE_SHORT_WIDTH_DP);
            mIncognitoHelper.setRightFadeWidth(FADE_SHORT_WIDTH_DP);
        }
        return rightFadeDrawable;
    }

    /** Returns drag listener for tab strip. */
    public OnDragListener getDragListener() {
        return mTabDragSource;
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
     * Sets the TabModelSelector that this StripLayoutHelperManager will visually represent, and
     * various objects associated with it.
     *
     * @param modelSelector The TabModelSelector to visually represent.
     * @param tabCreatorManager The TabCreatorManager, used to create new tabs.
     */
    public void setTabModelSelector(
            TabModelSelector modelSelector, TabCreatorManager tabCreatorManager) {
        if (mTabModelSelector == modelSelector) return;

        mTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int launchType,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        updateTitleForTab(tab);
                    }
                };
        modelSelector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);

        mTabModelSelector = modelSelector;

        updateTitleCacheForInit();

        if (mTabModelSelector.isTabStateInitialized()) {
            updateModelSwitcherButton();
        } else {
            mTabModelSelector.addObserver(
                    new TabModelSelectorObserver() {
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
        mNormalHelper.setTabModel(
                mTabModelSelector.getModel(false),
                tabCreatorManager.getTabCreator(false),
                tabStateInitialized);
        mIncognitoHelper.setTabModel(
                mTabModelSelector.getModel(true),
                tabCreatorManager.getTabCreator(true),
                tabStateInitialized);
        TabModelFilterProvider provider = mTabModelSelector.getTabModelFilterProvider();
        mNormalHelper.setTabGroupModelFilter(
                (TabGroupModelFilter) provider.getTabModelFilter(false));
        mIncognitoHelper.setTabGroupModelFilter(
                (TabGroupModelFilter) provider.getTabModelFilter(true));
        tabModelSwitched(mTabModelSelector.isIncognitoSelected());

        mTabModelSelectorTabModelObserver =
                new TabModelSelectorTabModelObserver(modelSelector) {
                    /**
                     * @return The actual current time of the app in ms.
                     */
                    public long time() {
                        return SystemClock.uptimeMillis();
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        getStripLayoutHelper(tab.isIncognitoBranded()).willCloseTab(time(), tab);
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        getStripLayoutHelper(tab.isIncognitoBranded())
                                .tabClosed(time(), tab.getId());
                        updateModelSwitcherButton();
                    }

                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                        // For right-direction move, layout helper re-ordering logic
                        // expects destination index = position + 1
                        getStripLayoutHelper(tab.isIncognitoBranded())
                                .tabMoved(
                                        time(),
                                        tab.getId(),
                                        curIndex,
                                        newIndex > curIndex ? newIndex + 1 : newIndex);
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        getStripLayoutHelper(tab.isIncognitoBranded())
                                .tabClosureCancelled(time(), tab.getId());
                        updateModelSwitcherButton();
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        if (mLayerTitleCacheSupplier.hasValue()) {
                            mLayerTitleCacheSupplier.get().removeTabTitle(tab.getId());
                        }
                    }

                    @Override
                    public void tabPendingClosure(Tab tab) {
                        getStripLayoutHelper(tab.isIncognitoBranded())
                                .tabClosed(time(), tab.getId());
                        updateModelSwitcherButton();
                    }

                    @Override
                    public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                        if (tabs.isEmpty()) return;
                        getStripLayoutHelper(tabs.get(0).isIncognitoBranded())
                                .multipleTabsClosed(tabs);
                        updateModelSwitcherButton();
                    }

                    @Override
                    public void onFinishingTabClosure(Tab tab) {
                        getStripLayoutHelper(tab.isIncognitoBranded())
                                .tabClosed(time(), tab.getId());
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
                        getStripLayoutHelper(tab.isIncognitoBranded())
                                .tabSelected(time(), tab.getId(), lastId);
                    }

                    @Override
                    public void didAddTab(
                            Tab tab, int type, int creationState, boolean markedForSelection) {
                        boolean onStartup = type == TabLaunchType.FROM_RESTORE;
                        getStripLayoutHelper(tab.isIncognitoBranded())
                                .tabCreated(
                                        time(),
                                        tab.getId(),
                                        mTabModelSelector.getCurrentTabId(),
                                        markedForSelection,
                                        false,
                                        onStartup);
                    }
                };

        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(modelSelector) {
                    @Override
                    public void onLoadUrl(
                            Tab tab, LoadUrlParams params, LoadUrlResult loadUrlResult) {
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
                    public void onCrash(Tab tab) {
                        getStripLayoutHelper(tab.isIncognito()).tabLoadFinished(tab.getId());
                    }

                    @Override
                    public void onTitleUpdated(Tab tab) {
                        updateTitleForTab(tab);
                    }

                    @Override
                    public void onFaviconUpdated(Tab tab, Bitmap icon, GURL iconUrl) {
                        updateTitleForTab(tab);
                    }

                    @Override
                    public void onBrowserControlsConstraintsChanged(
                            Tab tab,
                            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
                            BrowserControlsOffsetTagsInfo offsetTagsInfo,
                            @BrowserControlsState int constraints) {
                        if (ToolbarFeatures.isBrowserControlsInVizEnabled(
                                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext))) {
                            mTabStripTreeProvider.updateOffsetTag(
                                    offsetTagsInfo.getTopControlsOffsetTag());
                        }
                    }

                    @Override
                    public void onContentViewScrollingStateChanged(boolean scrolling) {
                        mIsVerticalScrollInProgress = scrolling;
                    }
                };

        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);
        if (mTabDragSource != null) {
            mTabDragSource.setTabModelSelector(mTabModelSelector);
        }
    }

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        assert mDesktopWindowStateProvider != null;
        // We do not update the layer's height in this method. The height adjustment will be
        // triggered by #onHeightChanged.

        mDesktopWindowStateProvider.updateForegroundColor(getBackgroundColor());
        updateHorizontalPaddings(newState.getLeftPadding(), newState.getRightPadding());
    }

    /**
     * Update the start / end padding for the tab strip.
     *
     * @param leftPaddingPx Left padding for the tab strip in px.
     * @param rightPaddingPx Right padding for the tab strip in px.
     */
    private void updateHorizontalPaddings(int leftPaddingPx, int rightPaddingPx) {
        mLeftPadding = leftPaddingPx / mDensity;
        mRightPadding = rightPaddingPx / mDensity;

        onSizeChanged(mWidth, mHeight, mLastVisibleViewportOffsetY, mOrientation);
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
        return AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateProvider)
                ? TabUiThemeUtil.getTabStripBackgroundColorForActivityState(
                        mContext, mIsIncognito, mIsTopResumedActivity)
                : TabUiThemeUtil.getTabStripBackgroundColor(mContext, mIsIncognito);
    }

    /**
     * Updates all internal resources and dimensions.
     *
     * @param context The current Android Context.
     */
    public void onContextChanged(Context context) {
        mContext = context;
        mNormalHelper.onContextChanged(context);
        mIncognitoHelper.onContextChanged(context);
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        getInactiveStripLayoutHelper().finishAnimationsAndPushTabUpdates();
        boolean animationFinished = getActiveStripLayoutHelper().updateLayout(time);
        if (animationFinished) {
            // Update the touchable area when tab strip has an update on its layout. This is
            // probably an overkill, since the touch size does not change when the tab is full.
            // TODO(crbug/332957442): Reduce the call freq for this method.
            updateTouchableAreas();
        }
        return animationFinished;
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

        // If we are in DW mode, notify DW state provider since the model changed.
        if (AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateProvider)) {
            mDesktopWindowStateProvider.updateForegroundColor(getBackgroundColor());
        }

        mUpdateHost.requestUpdate();
    }

    private void updateModelSwitcherButton() {
        if (mModelSelectorButton == null) return;
        mModelSelectorButton.setIncognito(mIsIncognito);
        if (mTabModelSelector != null) {
            boolean isVisible = mTabModelSelector.getModel(true).getCount() != 0;

            if (isVisible == mModelSelectorButton.isVisible()) return;

            mModelSelectorButton.setVisible(isVisible);

            // msbTouchTargetSize = msbEndPadding(8dp) + msbWidth(32dp) + msbStartPadding(8dp to
            // create more gap between MSB and NTB so there is enough space for touch target).
            float msbTouchTargetSize =
                    isVisible
                            ? getModelSelectorButtonWidthWithEndPadding()
                                    + getButtonStartPaddingForTouchTarget()
                            : 0.0f;
            mNormalHelper.updateEndMarginForStripButtons(msbTouchTargetSize);
            mIncognitoHelper.updateEndMarginForStripButtons(msbTouchTargetSize);
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

    @VisibleForTesting
    @StripVisibilityState
    int getStripVisibilityState() {
        return mStripVisibilityStateSupplier.get();
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

    void simulateOnDownForTesting(float x, float y, boolean fromMouse, int buttons) {
        mTabStripEventHandler.onDown(x, y, fromMouse, buttons);
    }

    void setTabStripTreeProviderForTesting(TabStripSceneLayer tabStripTreeProvider) {
        mTabStripTreeProvider = tabStripTreeProvider;
    }

    ViewStub getTabHoverCardViewStubForTesting() {
        return mTabHoverCardViewStub;
    }

    void disableAnimationsForTesting() {
        mAnimationsDisabledForTesting = true;
    }

    public TabDragSource getTabDragSourceForTesting() {
        return mTabDragSource;
    }

    public void setIsIncognitoForTesting(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    public boolean isStripScrimVisibleForTesting() {
        return mStripTransitionScrimOpacity == 1f;
    }
}
