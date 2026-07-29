// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.BUTTON_TOUCH_TARGET_SIZE_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.MIN_TAB_WIDTH_DP;
import static org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils.TAB_OVERLAP_WIDTH_DP;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Handler;
import android.os.SystemClock;
import android.util.FloatProperty;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnDragListener;
import android.view.ViewStub;
import android.view.animation.Interpolator;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorTextButton;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.AreaMotionEventFilter;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.AreaMotionEventHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper.LeadingButtonDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper.TrailingButtonDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnKeyboardFocusHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.TabStripDragHandler;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayer;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicButtonDelegate;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
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
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.MediaState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabClosingSource;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabstrip.StripVisibilityState;
import org.chromium.chrome.browser.tabstrip.TabStripSceneLayerHolder;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTrackerFactory;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.util.StyleUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/**
 * This class handles managing which StripLayoutHelper is currently active and dispatches all input
 * and model events to the proper destination.
 */
@NullMarked
public class StripLayoutHelperManager
        implements SceneOverlay,
                PauseResumeWithNativeObserver,
                TabStripSceneLayerHolder,
                TopResumedActivityChangedObserver,
                AppHeaderObserver,
                TabObscuringHandler.Observer {
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

    // Shared button constants (Model selector and Glic).
    static final float BUTTON_DESIRED_TOUCH_TARGET_SIZE =
            StyleUtils.shouldApplyDesktopDensity() ? 32.f : 48.f;

    // Tab strip transition constants.
    @VisibleForTesting
    static final Interpolator TAB_STRIP_TRANSITION_INTERPOLATOR =
            Interpolators.STANDARD_DEFAULT_EFFECTS;

    // Caching Variables
    private final RectF mStripFilterArea = new RectF();
    private final boolean mIsHeaderCustomizationSupported;

    // External influences
    private @MonotonicNonNull TabModelSelector mTabModelSelector; // Set on native initialization.
    private final LayoutManagerHost mManagerHost;
    private final LayoutUpdateHost mUpdateHost;
    private final LayoutRenderHost mRenderHost;
    private final TabObscuringHandler mTabObscuringHandler;
    private @Nullable ResourceManager mResourceManager;

    // Event Filters
    private @Nullable AreaMotionEventFilter mEventFilter;

    // Internal state
    private boolean mTabStripObscured;
    private boolean mIsIncognito;
    private final StripLayoutHelper mNormalHelper;
    private final StripLayoutHelper mIncognitoHelper;

    // UI State
    private float mWidth; // in dp units
    private float mHeight; // Height of the entire tab strip compositor layer in DP.
    private final float mScrollableStripHeight; // Height of the scrollable tab strip layer in DP.

    // Padding regions that tabs should remain untouchable.
    private float mLeftPadding; // in dp units
    private float mRightPadding; // in dp units
    private float mTopPadding; // in dp units
    private final float mDensity;
    private int mOrientation;
    private final StripLayoutTrailingButtonsCoordinator mTrailingButtonsCoordinator;
    private final Context mContext;
    private float mStripTransitionScrimOpacity;
    private @Nullable Animator mFadeTransitionAnimator;
    // This will be set only when a strip height transition runs to update the strip visibility and
    // not when this transition runs to solely update the strip top padding.
    private boolean mIsHeightTransitioning;
    private final ToolbarManager mToolbarManager;
    private final StatusBarColorController mStatusBarColorController;
    private TabStripSceneLayer mTabStripTreeProvider;
    private final ActivityWindowAndroid mWindowAndroid;
    private TabStripEventHandler mTabStripEventHandler;
    private final TabSwitcherLayoutObserver mTabSwitcherLayoutObserver;
    private @Nullable Runnable mFadeTransitionThresholdChangedCallback;
    private final View mControlContainer;
    private final ViewStub mTabHoverCardViewStub;
    private float mLastVisibleViewportOffsetY;
    private @Nullable OmniboxStub mOmniboxStub;
    private final Callback<String> mUrlTextChangeListener =
            (ignored) -> {
                getActiveStripLayoutHelper().clearTabHoverState();
            };
    private float mSceneLayerYOffset;
    private float mSceneLayerVisibleHeight; // Used during height transition.

    /**
     * Whether the current activity is the top resumed activity. This is only relevant for use in
     * the desktop windowing mode, to determine the tab strip background color and the Glic button
     * opacity.
     */
    private boolean mIsTopResumedActivity;

    private final SettableNonNullObservableSupplier<Boolean> mStaticLayoutNeedsOffsetTagSupplier =
            ObservableSuppliers.createNonNull(false);

    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;

    private @MonotonicNonNull TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private @MonotonicNonNull TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private final Callback<TabModel> mCurrentTabModelObserver =
            (tabModel) -> {
                tabModelSwitched(tabModel.isIncognito());
            };
    private final ActorUiTabController.Observer mActorObserver;

    private @MonotonicNonNull TabModelObserver mTabModelObserver; // Set on native initialization.
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final String mDefaultTitle;
    private final MonotonicObservableSupplier<LayerTitleCache> mLayerTitleCacheSupplier;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final Callback<Integer> mStripVisibilityStateObserver;
    private final SettableNonNullObservableSupplier<@StripVisibilityState Integer>
            mStripVisibilityStateSupplier =
                    ObservableSuppliers.createNonNull(StripVisibilityState.VISIBLE);
    private final SettableNonNullObservableSupplier<Integer> mStripBottomPxSupplier =
            ObservableSuppliers.createNonNull(0);
    private final @Nullable NonNullObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;

    // Drag-Drop
    private @Nullable TabStripDragHandler mTabStripDragHandler;

    private class TabStripEventHandler implements AreaMotionEventHandler {
        @Override
        public void onDown(float x, float y, int buttons) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
            if (mTrailingButtonsCoordinator.onDown(x, y, buttons)) {
                return;
            }
            getActiveStripLayoutHelper().onDown(x, y, buttons);
        }

        @Override
        public void onUpOrCancel() {
            if (mTrailingButtonsCoordinator.onUpOrCancel()) {
                return;
            }
            getActiveStripLayoutHelper().onUpOrCancel();
        }

        @Override
        public void drag(float x, float y, float dx, float dy, float tx, float ty) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
            mTrailingButtonsCoordinator.drag(x, y);
            getActiveStripLayoutHelper().drag(x, y, dx);
        }

        @Override
        public void click(float x, float y, int buttons, int modifiers) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
            long time = time();
            if (mTrailingButtonsCoordinator.click(time, x, y, buttons, modifiers)) {
                return;
            }
            getActiveStripLayoutHelper().click(time(), x, y, buttons, modifiers);
        }

        @Override
        public void fling(float x, float y, float velocityX, float velocityY) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
            getActiveStripLayoutHelper().fling(time(), velocityX);
        }

        @Override
        public void onLongPress(float x, float y) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
            if (mTrailingButtonsCoordinator.onLongPress(x, y)) {
                return;
            }
            getActiveStripLayoutHelper().onLongPress(x, y);
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

            // Run whichever coordinator is losing hover before the one gaining hover so that
            // clearing an old button's hover state ("") doesn't clobber the new button's tooltip.
            boolean isTrailingHovered = mTrailingButtonsCoordinator.checkClickedOrHovered(x, y);
            if (isTrailingHovered) {
                getActiveStripLayoutHelper().onHoverEnter(x, y, true);
                mTrailingButtonsCoordinator.onHoverEvent(x, y);
            } else {
                mTrailingButtonsCoordinator.onHoverEvent(x, y);
                getActiveStripLayoutHelper().onHoverEnter(x, y, false);
            }
        }

        @Override
        public void onHoverMove(float x, float y) {
            if (DragDropGlobalState.hasValue()) {
                return;
            }
            // Order unhover before hover to prevent tooltip clobbering (see onHoverEnter).
            boolean isTrailingHovered = mTrailingButtonsCoordinator.checkClickedOrHovered(x, y);
            if (isTrailingHovered) {
                getActiveStripLayoutHelper().onHoverMove(x, y, true);
                mTrailingButtonsCoordinator.onHoverEvent(x, y);
            } else {
                mTrailingButtonsCoordinator.onHoverEvent(x, y);
                getActiveStripLayoutHelper().onHoverMove(x, y, false);
            }
        }

        @Override
        public void onHoverExit(boolean inArea) {
            getActiveStripLayoutHelper().onHoverExit(inArea);
            mTrailingButtonsCoordinator.onHoverExit();
        }

        @Override
        public void onScroll(float horizontalAxisScroll, float verticalAxisScroll) {
            getActiveStripLayoutHelper().onScroll(horizontalAxisScroll, verticalAxisScroll);
        }

        private long time() {
            return LayoutManagerImpl.time();
        }
    }

    /** Observer for Tab Switcher layout events. */
    class TabSwitcherLayoutObserver implements LayoutStateObserver {
        @Override
        public void onStartedShowing(int layoutType) {
            if (layoutType == LayoutType.HUB && isActivityInXrFullSpaceModeNow()) {
                setStripVisibilityState(StripVisibilityState.OBSCURED, /* clear= */ false);
            }
        }

        @Override
        public void onFinishedShowing(@LayoutType int layoutType) {
            if (layoutType != LayoutType.HUB) return;
            setStripVisibilityState(StripVisibilityState.OBSCURED, /* clear= */ false);
        }

        @Override
        public void onStartedHiding(@LayoutType int layoutType) {
            if (layoutType != LayoutType.HUB) return;
            if (!isActivityInXrFullSpaceModeNow()) {
                setStripVisibilityState(StripVisibilityState.OBSCURED, /* clear= */ true);
            }

            // Expand tab group on GTS exit.
            mNormalHelper.expandGroupOnGtsExit();
            mIncognitoHelper.expandGroupOnGtsExit();
        }

        @Override
        public void onFinishedHiding(int layoutType) {
            if (layoutType != LayoutType.HUB) return;
            if (isActivityInXrFullSpaceModeNow()) {
                setStripVisibilityState(StripVisibilityState.OBSCURED, /* clear= */ true);
            }
        }
    }

    /**
     * @return Returns layout observer for tab switcher.
     */
    public LayoutStateObserver getTabSwitcherObserver() {
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
     * @param multiInstanceManager The {@link MultiInstanceManager} used to move tabs to other
     *     windows.
     * @param dragDropDelegate DragAndDropDelegate passed to {@link TabStripDragHandler} to initiate
     *     tab drag and drop.
     * @param controlContainerView View passed to {@link TabStripDragHandler} for drag and drop.
     * @param tabHoverCardViewStub The ViewStub representing the strip tab hover card.
     * @param tabContentManagerSupplier Supplier of the TabContentManager instance.
     * @param browserControlsStateProvider BrowserControlsStateProvider for drag drop.
     * @param windowAndroid The {@link WindowAndroid} instance to access Activity.
     * @param toolbarManager The ToolbarManager instance.
     * @param desktopWindowStateManager The DesktopWindowStateManager for the app header.
     * @param actionConfirmationManager The {@link ActionConfirmationManager} for group actions.
     * @param dataSharingTabManager The {@link DataSharingTabManager} for shared groups.
     * @param bottomSheetController The {@link BottomSheetController} used to show bottom sheets.
     * @param shareDelegateSupplier Supplies {@link ShareDelegate} to share tab URLs.
     * @param tabBookmarkerSupplier Supplies {@link TabBookmarker} to add/edit bookmarks.
     * @param xrSpaceModeObservableSupplier Supplies current XR space mode status. True for XR full
     *     space mode, false otherwise.
     * @param backPressManager The {@link BackPressManager} for handling back press.
     * @param snackbarManager The {@link SnackbarManager} used to show snackbar UI.
     * @param activityResultTracker The {@link ActivityResultTracker}.
     * @param glicClickHandler The {@link GlicButtonDelegate} for the Glic button.
     * @param leadingButtonDelegate The {@link LeadingButtonDelegate} for the leading button.
     * @param sideUiStateProviderSupplier Supplier of the {@link SideUiStateProvider}.
     * @param tabObscuringHandler The {@link TabObscuringHandler} to manage tab obscuring.
     */
    // TODO(crbug.com/484116872): Suppressing to observe SharedPreferences, which is discouraged;
    // should use another messaging channel instead.
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    public StripLayoutHelperManager(
            Context context,
            LayoutManagerHost managerHost,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            MonotonicObservableSupplier<LayerTitleCache> layerTitleCacheSupplier,
            MonotonicObservableSupplier<TabModelStartupInfo> tabModelStartupInfoSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            MultiInstanceManager multiInstanceManager,
            DragAndDropDelegate dragDropDelegate,
            View controlContainerView,
            ViewStub tabHoverCardViewStub,
            MonotonicObservableSupplier<TabContentManager> tabContentManagerSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            ActivityWindowAndroid windowAndroid,
            // TODO(crbug.com/40939440): Avoid passing the ToolbarManager instance. Potentially
            // implement an interface to manage strip transition states.
            ToolbarManager toolbarManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            ActionConfirmationManager actionConfirmationManager,
            DataSharingTabManager dataSharingTabManager,
            BottomSheetController bottomSheetController,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            Supplier<TabBookmarker> tabBookmarkerSupplier,
            @Nullable NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier,
            BackPressManager backPressManager,
            SnackbarManager snackbarManager,
            @Nullable ActivityResultTracker activityResultTracker,
            GlicButtonDelegate glicClickHandler,
            LeadingButtonDelegate leadingButtonDelegate,
            OneshotSupplier<SideUiStateProvider> sideUiStateProviderSupplier,
            TabObscuringHandler tabObscuringHandler,
            @Nullable BooleanSupplier canActivateTabLayoutToggleMenuSupplier) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        Resources res = context.getResources();
        mManagerHost = managerHost;
        mUpdateHost = updateHost;
        mRenderHost = renderHost;

        mActorObserver =
                state -> {
                    getStripLayoutHelper(false)
                            .onActuationStateChanged(state.tabId, state.tabIndicator);
                    mRenderHost.requestRender();
                };
        mLayerTitleCacheSupplier = layerTitleCacheSupplier;
        mDensity = res.getDisplayMetrics().density;
        mTabStripTreeProvider = new TabStripSceneLayer(mDensity);
        mTabStripEventHandler = new TabStripEventHandler();
        mTabSwitcherLayoutObserver = new TabSwitcherLayoutObserver();
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mDefaultTitle = context.getString(R.string.tab_loading_default_title);
        mControlContainer = controlContainerView;
        mEventFilter =
                new AreaMotionEventFilter(context, mTabStripEventHandler, null, false, false) {
                    @Override
                    protected boolean isMotionEventInArea(MotionEvent e) {
                        if (super.isMotionEventInArea(e)) return true;

                        // Allow right-clicks in empty spaces of the tab strip (e.g., top/side
                        // paddings) to be intercepted by the tab strip to show the context menu.
                        // Regular touch events in these regions should still fall through to the
                        // OS for window dragging.
                        if (e.getButtonState() == MotionEvent.BUTTON_SECONDARY) {
                            float x = e.getX() / mDensity;
                            float y = e.getY() / mDensity;
                            if (x >= 0 && x <= mWidth && y >= 0 && y <= mStripFilterArea.bottom) {
                                return true;
                            }
                        }
                        return false;
                    }
                };

        mIsHeaderCustomizationSupported =
                ToolbarFeatures.isAppHeaderCustomizationSupported(
                        /* isTablet= */ true, DisplayUtil.isContextInDefaultDisplay(mContext));
        mScrollableStripHeight = res.getDimension(R.dimen.tab_strip_height) / mDensity;
        mHeight =
                mIsHeaderCustomizationSupported
                        ? toolbarManager.getTabStripHeightSupplier().get() / mDensity
                        : mScrollableStripHeight;
        mTopPadding = mHeight - mScrollableStripHeight;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mStripVisibilityStateObserver =
                state -> {
                    if (mEventFilter == null) return;
                    // Consume motion events only on a visible strip.
                    mEventFilter.setEventArea(
                            state == StripVisibilityState.VISIBLE ? mStripFilterArea : null);
                };
        mStripVisibilityStateSupplier.addSyncObserverAndPostIfNonNull(
                mStripVisibilityStateObserver);

        Runnable selectorClickHandler = () -> handleModelSelectorButtonClick();
        StripLayoutViewOnKeyboardFocusHandler selectorKeyboardFocusHandler =
                (isFocused, view) -> getActiveStripLayoutHelper().onKeyboardFocus(isFocused, view);
        StripLayoutViewOnKeyboardFocusHandler glicKeyboardFocusHandler =
                (isFocused, view) -> mRenderHost.requestRender();

        mTrailingButtonsCoordinator =
                new StripLayoutTrailingButtonsCoordinator(
                        context,
                        mUpdateHost,
                        mRenderHost,
                        mWindowAndroid,
                        mDensity,
                        controlContainerView,
                        isAppInDesktopWindow(),
                        mIsTopResumedActivity,
                        ChromeAndroidTaskTrackerFactory.getInstance(),
                        mIsIncognito,
                        () -> mTabModelSelector,
                        sideUiStateProviderSupplier,
                        () -> getActiveStripLayoutHelper().getUnpinnedTabWidth(),
                        selectorClickHandler,
                        selectorKeyboardFocusHandler,
                        glicClickHandler,
                        glicKeyboardFocusHandler,
                        this::isNormalHelperGlicIphShowing,
                        this::updateHelperEndMargins);

        mTabHoverCardViewStub = tabHoverCardViewStub;

        if (MultiWindowUtils.isMultiInstanceApi31Enabled()) {
            mTabStripDragHandler =
                    new TabStripDragHandler(
                            context,
                            this::getActiveStripLayoutHelper,
                            () ->
                                    getStripVisibilityStateSupplier().get()
                                            == StripVisibilityState.VISIBLE,
                            tabContentManagerSupplier,
                            mLayerTitleCacheSupplier,
                            multiInstanceManager,
                            dragDropDelegate,
                            browserControlsStateProvider,
                            () -> windowAndroid.getActivity().get(),
                            toolbarManager.getTabStripHeightSupplier());

            if (ChromeFeatureList.sEscCancelDrag.isEnabled()) {
                backPressManager.addHandler(
                        mTabStripDragHandler, BackPressHandler.Type.CANCEL_TAB_STRIP_DRAG);
            }
        }

        mToolbarManager = toolbarManager;
        mStatusBarColorController = mToolbarManager.getStatusBarColorController();

        TrailingButtonDelegate trailingButtonDelegate =
                new TrailingButtonDelegate() {
                    @Override
                    public boolean isMenuShowing() {
                        return mTrailingButtonsCoordinator.isMenuShowing();
                    }

                    @Override
                    public void dismissContextMenu() {
                        mTrailingButtonsCoordinator.dismissTrailingButtonsMenu();
                    }

                    @Override
                    public void fadeCompositorButtons(boolean fade) {
                        mTrailingButtonsCoordinator.fadeCompositorButtons(fade);
                    }

                    @Override
                    public boolean isGlicButtonVisible() {
                        return mTrailingButtonsCoordinator.isGlicButtonVisible();
                    }

                    @Override
                    public boolean isGlicUiVisible() {
                        return mTrailingButtonsCoordinator.isGlicUiVisible();
                    }

                    @Override
                    public @Nullable TintedCompositorButton getGlicButton() {
                        return mTrailingButtonsCoordinator.getGlicButton();
                    }
                };

        mNormalHelper =
                new StripLayoutHelper(
                        context,
                        this,
                        trailingButtonDelegate,
                        leadingButtonDelegate,
                        managerHost,
                        updateHost,
                        renderHost,
                        /* incognito= */ false,
                        mTabStripDragHandler,
                        controlContainerView,
                        windowAndroid,
                        actionConfirmationManager,
                        dataSharingTabManager,
                        () ->
                                getStripVisibilityStateSupplier().get()
                                        == StripVisibilityState.VISIBLE,
                        bottomSheetController,
                        multiInstanceManager,
                        shareDelegateSupplier,
                        tabBookmarkerSupplier,
                        TabGroupListBottomSheetCoordinator::new,
                        snackbarManager,
                        activityResultTracker,
                        canActivateTabLayoutToggleMenuSupplier);
        mIncognitoHelper =
                new StripLayoutHelper(
                        context,
                        this,
                        trailingButtonDelegate,
                        leadingButtonDelegate,
                        managerHost,
                        updateHost,
                        renderHost,
                        /* incognito= */ true,
                        mTabStripDragHandler,
                        controlContainerView,
                        windowAndroid,
                        actionConfirmationManager,
                        dataSharingTabManager,
                        () ->
                                getStripVisibilityStateSupplier().get()
                                        == StripVisibilityState.VISIBLE,
                        bottomSheetController,
                        multiInstanceManager,
                        shareDelegateSupplier,
                        tabBookmarkerSupplier,
                        TabGroupListBottomSheetCoordinator::new,
                        snackbarManager,
                        activityResultTracker,
                        canActivateTabLayoutToggleMenuSupplier);

        tabHoverCardViewStub.setOnInflateListener(
                (viewStub, view) -> {
                    var hoverCardView = (TabHoverCardView) view;
                    hoverCardView.initialize(
                            assumeNonNull(mTabModelSelector), tabContentManagerSupplier);
                    mNormalHelper.setTabHoverCardView(hoverCardView);
                    mIncognitoHelper.setTabHoverCardView(hoverCardView);
                });

        if (tabModelStartupInfoSupplier != null) {
            var tabModelStartupInfo = tabModelStartupInfoSupplier.get();
            if (tabModelStartupInfo != null) {
                setTabModelStartupInfo(tabModelStartupInfo);
            } else {
                tabModelStartupInfoSupplier.addSyncObserverAndPostIfNonNull(
                        this::setTabModelStartupInfo);
            }
        }

        mLayerTitleCacheSupplier.addSyncObserverAndPostIfNonNull(
                (LayerTitleCache layerTitleCache) -> {
                    mNormalHelper.setLayerTitleCache(layerTitleCache);
                    mIncognitoHelper.setLayerTitleCache(layerTitleCache);
                    mTrailingButtonsCoordinator.setLayerTitleCache(layerTitleCache);
                });

        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.addObserver(this);
            mIsTopResumedActivity = !mDesktopWindowStateManager.isInUnfocusedDesktopWindow();
        } else {
            mIsTopResumedActivity = AppHeaderUtils.isActivityFocusedAtStartup(lifecycleDispatcher);
        }
        if (isAppInDesktopWindow()) {
            @Nullable AppHeaderState appHeaderState =
                    mDesktopWindowStateManager.getAppHeaderState();
            if (appHeaderState != null) {
                onAppHeaderStateChanged(appHeaderState);
            }
        }

        mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;
        mTabObscuringHandler = tabObscuringHandler;
        mTabObscuringHandler.addObserver(this);
        if (mToolbarManager != null && mToolbarManager.getOmniboxStubSupplier() != null) {
            mToolbarManager.getOmniboxStubSupplier().onAvailable(this::onOmniboxStubAvailable);
        }
    }

    private void onOmniboxStubAvailable(OmniboxStub omniboxStub) {
        mOmniboxStub = omniboxStub;
        mOmniboxStub.addUrlTextChangeListener(mUrlTextChangeListener);
    }

    @EnsuresNonNullIf("mDesktopWindowStateManager")
    private boolean isAppInDesktopWindow() {
        return AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager)
                && mDesktopWindowStateManager != null;
    }

    private boolean isNormalHelperGlicIphShowing() {
        return mNormalHelper != null && mNormalHelper.isGlicIphShowing();
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

    /** Cleans up internal state. An instance should not be used after this method is called. */
    @SuppressWarnings({"NullAway", "UseSharedPreferencesManagerFromChromeCheck"})
    public void destroy() {
        if (mOmniboxStub != null) {
            mOmniboxStub.removeUrlTextChangeListener(mUrlTextChangeListener);
            mOmniboxStub = null;
        }
        mTabObscuringHandler.removeObserver(this);
        mTabStripTreeProvider.destroy();
        mTabStripTreeProvider = null;
        mTrailingButtonsCoordinator.destroy();
        mLifecycleDispatcher.unregister(this);
        // Remove the observer to prevent any updates on a destroyed EventFilter.
        mStripVisibilityStateSupplier.removeObserver(mStripVisibilityStateObserver);
        // Delete the EventFilter to avoid any updates on destroyed StripLayoutHelpers.
        mEventFilter = null;
        mTabStripEventHandler = null;
        mIncognitoHelper.destroy();
        mNormalHelper.destroy();
        if (mTabModelSelector != null) {
            mTabModelSelector.removeObserverFromAllModels(mTabModelObserver);

            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);

            // Remove observers for Glic actuation icons.
            TabModel standardModel = mTabModelSelector.getModel(false);
            for (int i = 0; i < standardModel.getCount(); i++) {
                unregisterActorObserver(standardModel.getTabAt(i));
            }

            mTabModelSelectorTabModelObserver.destroy();
            mTabModelSelectorTabObserver.destroy();
        }
        if (mTabStripDragHandler != null) {
            mTabStripDragHandler.destroy();
            mTabStripDragHandler = null;
        }
        if (mDesktopWindowStateManager != null) {
            mDesktopWindowStateManager.removeObserver(this);
        }
    }

    /** Mark whether tab strip is hidden by a height transition. */
    public void setIsTabStripHiddenByHeightTransition(boolean isHidden) {
        setStripVisibilityState(StripVisibilityState.HIDDEN_BY_HEIGHT_TRANSITION, !isHidden);
        mStatusBarColorController.setTabStripHiddenOnTablet(isHidden);
    }

    @Override
    public void onResumeWithNative() {
        if (mTabModelSelector == null) return;
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab == null) return;
        getStripLayoutHelper(currentTab.isIncognito())
                .scrollTabToView(LayoutManagerImpl.time(), true);
    }

    @Override
    public void onPauseWithNative() {
        // Clear any persisting tab strip hover state when the activity is paused.
        getActiveStripLayoutHelper().onHoverExit(/* inTabStrip= */ false);
    }

    private void handleModelSelectorButtonClick() {
        if (mTabModelSelector == null) return;
        getActiveStripLayoutHelper().finishAnimationsAndPushTabUpdates();
        TintedCompositorButton modelSelectorButton =
                mTrailingButtonsCoordinator.getModelSelectorButton();
        if (modelSelectorButton == null || !modelSelectorButton.isVisible()) return;
        mTabModelSelector.selectModel(!mTabModelSelector.isIncognitoSelected());
        RecordUserAction.record("MobileToolbarModelSelected");
    }

    public void simulateUrlTextChangeForTesting(String text) {
        mUrlTextChangeListener.onResult(text);
    }

    @VisibleForTesting
    public void simulateClick(float x, float y, int buttons, int modifiers) {
        mTabStripEventHandler.click(x, y, buttons, modifiers);
    }

    @VisibleForTesting
    public void simulateLongPress(float x, float y) {
        mTabStripEventHandler.onLongPress(x, y);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager) {
        assert mTabStripTreeProvider != null;
        mResourceManager = resourceManager;

        pushAndUpdateStrip(mSceneLayerYOffset, mSceneLayerVisibleHeight);
        return mTabStripTreeProvider;
    }

    private void pushAndUpdateStrip(float yOffsetDp, float visibleHeightDp) {
        if (mResourceManager == null) return;

        setStripVisibilityState(
                StripVisibilityState.HIDDEN_BY_SCROLL,
                /* clear= */ mBrowserControlsStateProvider.getTopControlOffset() >= 0);
        Tab selectedTab =
                mTabModelSelector == null
                        ? null
                        : mTabModelSelector
                                .getCurrentModel()
                                .getTabAt(mTabModelSelector.getCurrentModel().index());
        int selectedTabId = selectedTab == null ? TabModel.INVALID_TAB_INDEX : selectedTab.getId();
        int hoveredTabId =
                getActiveStripLayoutHelper().getLastHoveredTab() == null
                        ? TabModel.INVALID_TAB_INDEX
                        : getActiveStripLayoutHelper().getLastHoveredTab().getTabId();

        // When tab strip is hiding, animation will trigger the toolbar moving up and tab strip
        // fade-out in place. We use the visible height to decide the transition progress then
        // update the scrim opacity.
        if (duringTabStripHeightTransition()) {
            // The fade-out is implemented by adding a scrim layer on top of the tab strip, with the
            // same bg as the toolbar background color.
            calculateScrimOpacityDuringHeightTransition(visibleHeightDp);
            mStatusBarColorController.setTabStripColorOverlay(
                    getStripTransitionScrimColor(), mStripTransitionScrimOpacity);
        }

        mTabStripTreeProvider.pushAndUpdateStrip(
                this,
                mTrailingButtonsCoordinator,
                assertNonNull(mLayerTitleCacheSupplier.get()),
                mResourceManager,
                getActiveStripLayoutHelper().getStripLayoutTabsToRender(),
                getActiveStripLayoutHelper().getStripLayoutGroupTitlesToRender(),
                yOffsetDp,
                selectedTabId,
                hoveredTabId,
                getStripTransitionScrimColor(),
                mStripTransitionScrimOpacity,
                getActiveStripLayoutHelper().getLeftPaddingToDraw(),
                getActiveStripLayoutHelper().getRightPaddingToDraw(),
                mTopPadding);
    }

    @Override
    public void removeFromParent() {
        mTabStripTreeProvider.removeFromParent();
    }

    private int getStripTransitionScrimColor() {
        return mToolbarManager.getPrimaryColor();
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        // TODO(mdjones): This matches existing behavior but can be improved to return false if
        // the browser controls offset is equal to the browser controls height.
        return (getStripVisibilityStateSupplier().get() & StripVisibilityState.OBSCURED) == 0;
    }

    @Override
    public @Nullable EventFilter getEventFilter() {
        return mEventFilter;
    }

    public Context getContext() {
        return mContext;
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
        mTrailingButtonsCoordinator.onSizeChanged(mWidth, mRightPadding, mLeftPadding, mTopPadding);

        mNormalHelper.onSizeChanged(
                mWidth,
                mScrollableStripHeight,
                orientationChanged,
                LayoutManagerImpl.time(),
                mLeftPadding,
                mRightPadding,
                mTopPadding);
        mIncognitoHelper.onSizeChanged(
                mWidth,
                mScrollableStripHeight,
                orientationChanged,
                LayoutManagerImpl.time(),
                mLeftPadding,
                mRightPadding,
                mTopPadding);

        mStripFilterArea.set(
                mLeftPadding,
                mTopPadding,
                mWidth - mRightPadding,
                Math.min(getHeight(), visibleViewportOffsetY));
        // Avoid handling motion events when invisible strip state persists after a size change.
        if (mEventFilter != null
                && getStripVisibilityStateSupplier().get() == StripVisibilityState.VISIBLE) {
            mEventFilter.setEventArea(mStripFilterArea);
        }
    }

    // Implements TabStripTransitionDelegate.

    @Override
    public void onHeightChanged(int newHeightPx, int topPaddingPx, boolean applyScrimOverlay) {
        if (applyScrimOverlay && !isFadeTransitionRunning()) {
            mIsHeightTransitioning = true;
            boolean hideStrip = newHeightPx == 0;
            mStripTransitionScrimOpacity = hideStrip ? 0f : 1f;
            // Update the strip visibility state in StatusBarController just after the margins are
            // updated during a hide->show transition so that the status bar assumes the base tab
            // strip color for the remaining duration of the transition while a scrim is applied.
            if (!hideStrip) {
                mStatusBarColorController.setTabStripHiddenOnTablet(false);
            }
            // Set the status bar color and scrim overlay at the start of the transition.
            mStatusBarColorController.setTabStripColorOverlay(
                    getStripTransitionScrimColor(), mStripTransitionScrimOpacity);
            // The height transition is running to update strip visibility. Ensure that any stale
            // state set by a previous fade transition is cleared at this time.
            setStripVisibilityState(StripVisibilityState.HIDDEN_BY_HEIGHT_TRANSITION, !hideStrip);
            setStripVisibilityState(StripVisibilityState.HIDDEN_BY_FADE, /* clear= */ true);
        }

        if (mIsHeaderCustomizationSupported) {
            mHeight = newHeightPx / mDensity;
            mTopPadding = topPaddingPx / mDensity;
            onSizeChanged(mWidth, mHeight, mLastVisibleViewportOffsetY, mOrientation);
        }
    }

    @Override
    public void onLayerYOffsetChanged(int yOffsetPx, int visibleHeightPx) {
        float yOffsetDp = yOffsetPx / mDensity;
        float visibleHeightDp = visibleHeightPx / mDensity;

        // If yOffset does not change (e.g. other layers are moving), no need to push for update.
        if (mSceneLayerYOffset != yOffsetDp || mSceneLayerVisibleHeight != visibleHeightDp) {
            mSceneLayerYOffset = yOffsetDp;
            mSceneLayerVisibleHeight = visibleHeightDp;
            pushAndUpdateStrip(mSceneLayerYOffset, mSceneLayerVisibleHeight);
            @Px
            int tabStripBottomPx =
                    Math.round(mDensity * (mSceneLayerYOffset + mSceneLayerVisibleHeight));
            mStripBottomPxSupplier.set(tabStripBottomPx);
        }
    }

    @Override
    public void onFadeTransitionRequested(float newOpacity, int durationMs) {
        // Opacity is already the desired value, return early.
        if (newOpacity == mStripTransitionScrimOpacity) return;

        if (mIsHeightTransitioning) {
            // If a height transition is currently running to update the scrim when a fade
            // transition is also requested, the fade transition should be prioritized to update the
            // strip visibility so immediately set this boolean to false to avoid a race to update
            // the strip scrim opacity.
            mIsHeightTransitioning = false;
        }
        boolean showStrip = newOpacity == 0f;

        // Update the status bar color to ensure that it reflects the current strip visibility state
        // and to override any stale value impacted by visibility set during a previous height
        // transition.
        mStatusBarColorController.setTabStripHiddenOnTablet(!showStrip);
        mStatusBarColorController.setTabStripColorOverlay(
                getStripTransitionScrimColor(), newOpacity);

        if (isFadeTransitionRunning()) {
            mFadeTransitionAnimator.cancel();
        }
        mFadeTransitionAnimator =
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        this,
                        StripLayoutHelperManager.SCRIM_OPACITY,
                        mStripTransitionScrimOpacity,
                        newOpacity,
                        durationMs);
        mFadeTransitionAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        onFadeTransitionEnd(showStrip);
                    }
                });
        mFadeTransitionAnimator.start();
    }

    private void onFadeTransitionEnd(boolean showStrip) {
        assert !mIsHeightTransitioning
                : "Height transition to update the scrim should not be running when a fade"
                        + " transition is finishing.";
        mFadeTransitionAnimator = null;
        // The fade transition is running to update strip visibility. Ensure that any stale
        // state set by a previous height transition is cleared at this time.
        setStripVisibilityState(StripVisibilityState.HIDDEN_BY_FADE, showStrip);
        setStripVisibilityState(
                StripVisibilityState.HIDDEN_BY_HEIGHT_TRANSITION, /* clear= */ true);
    }

    @EnsuresNonNullIf("mFadeTransitionAnimator")
    private boolean isFadeTransitionRunning() {
        return mFadeTransitionAnimator != null && mFadeTransitionAnimator.isRunning();
    }

    @Override
    public void onHeightTransitionFinished(boolean success) {
        if (!mIsHeightTransitioning) return;

        assert !isFadeTransitionRunning()
                : "Fade transition should not be running when a height transition to update the"
                        + " scrim is finishing.";
        mIsHeightTransitioning = false;
        mStripTransitionScrimOpacity = 0f;
        // Update the strip visibility state in StatusBarColorController only after a show->hide
        // transition, so that the status bar assumes the toolbar color when the strip is hidden.
        if ((getStripVisibilityStateSupplier().get()
                        & StripVisibilityState.HIDDEN_BY_HEIGHT_TRANSITION)
                != 0) {
            mStatusBarColorController.setTabStripHiddenOnTablet(true);
        }
        mStatusBarColorController.setTabStripColorOverlay(
                Color.TRANSPARENT, mStripTransitionScrimOpacity);
    }

    @Override
    public boolean isHiddenByFadeTransition() {
        return (getStripVisibilityStateSupplier().get() & StripVisibilityState.HIDDEN_BY_FADE) != 0;
    }

    @Override
    public int getFadeTransitionThresholdDp() {
        if (mTabModelSelector == null) return 0;
        TabModel incognitoTabModel = mTabModelSelector.getModel(/* incognito= */ true);
        boolean hasIncognitoTabs = incognitoTabModel != null && incognitoTabModel.getCount() > 0;
        boolean shouldShowMsb = !IncognitoUtils.shouldOpenIncognitoAsWindow() && hasIncognitoTabs;

        // The threshold is the minimum width required to start showing fade.
        // Base = 2 * minTabWidth - tabOverlap + newTabButton:
        //   Tablet Base: 2 * minTabWidth(108) - tabOverlap(28) + newTabButton (48) = 236dp
        //   Desktop Base: 2 * minTabWidth(76) - tabOverlap(28) + newTabButton (32) = 156dp
        // Optional Additions:
        //   + Tab Search Button: 48dp (Tablet) / 32dp (Desktop)
        //   + Trailing Buttons (Glic, Glic actor): Dynamic (e.g. ~109dp in default state with only
        //     Glic showing, ~96dp in collapsed state with both Glic and Glic actor showing)
        //   + Model Selector Button (MSB): 48dp (Tablet) / 32dp (Desktop)

        float thresholdDp =
                (2 * MIN_TAB_WIDTH_DP)
                        - TAB_OVERLAP_WIDTH_DP
                        + BUTTON_TOUCH_TARGET_SIZE_DP
                        + getActiveStripLayoutHelper().getTabSearchButton().getWidth()
                        + mTrailingButtonsCoordinator.getTrailingButtonsWidthWithPadding()
                        + (shouldShowMsb ? BUTTON_TOUCH_TARGET_SIZE_DP : 0f);
        return Math.round(thresholdDp);
    }

    @Override
    public void setFadeTransitionThresholdChangedCallback(@Nullable Runnable callback) {
        mFadeTransitionThresholdChangedCallback = callback;
    }

    private boolean duringTabStripHeightTransition() {
        return mIsHeightTransitioning;
    }

    @VisibleForTesting
    float calculateScrimOpacityDuringHeightTransition(float visibleHeight) {
        if (!duringTabStripHeightTransition()) {
            return 0.0f;
        }

        // Otherwise, the alpha fraction is based on the percent of the tab strip visibility.
        // Use mScrollableStripHeight as the baseline height because mHeight may have already
        // changed during a height transition to hide the strip.
        float divisor = mHeight > 0 ? mHeight : mScrollableStripHeight;
        float ratio = 1 - visibleHeight / divisor;
        float newOpacity = TAB_STRIP_TRANSITION_INTERPOLATOR.getInterpolation(ratio);
        boolean isHidden =
                (getStripVisibilityStateSupplier().get()
                                & StripVisibilityState.HIDDEN_BY_HEIGHT_TRANSITION)
                        != 0;

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
    public void updateOffsetTagsInfo(@Nullable BrowserControlsOffsetTagsInfo offsetTagsInfo) {
        // LINT.IfChange(updateOffsetTagsInfo)
        if (offsetTagsInfo != null) {
            // Use the content OffsetTag here, because the tab strip and content are part of
            // the same subtree and move together with the same offset. We need to release the
            // content offset tag from the static layout first before adding it to the tab strip.
            mStaticLayoutNeedsOffsetTagSupplier.set(false);
            mTabStripTreeProvider.updateOffsetTag(offsetTagsInfo.getContentOffsetTag());
        } else {
            mTabStripTreeProvider.updateOffsetTag(null);
            mStaticLayoutNeedsOffsetTagSupplier.set(true);
        }
        // LINT.ThenChange(chrome/android/java/src/org/chromium/chrome/browser/compositor/layouts/StaticLayout.java:updateOffsetTag)
    }

    public NonNullObservableSupplier<Boolean> getLayoutNeedOffsetTagSupplier() {
        return mStaticLayoutNeedsOffsetTagSupplier;
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        // TODO (crbug/328055199): Check if losing focus to a non-Chrome task.
        if (!mIsHeaderCustomizationSupported) return;
        mIsTopResumedActivity = isTopResumedActivity;

        mTrailingButtonsCoordinator.updateGlicButtonOpacity(
                isAppInDesktopWindow(), mIsTopResumedActivity);

        mUpdateHost.requestUpdate();
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

    public @Nullable TintedCompositorButton getModelSelectorButton() {
        return mTrailingButtonsCoordinator.getModelSelectorButton();
    }

    @Override
    public void updateObscured(boolean obscureTabContent, boolean obscureToolbar) {
        if (mTabStripObscured == obscureToolbar) return;
        mTabStripObscured = obscureToolbar;
    }

    @Override
    public void getVirtualViews(List<VirtualView> views) {
        if (mTabStripObscured
                || duringTabStripHeightTransition()
                || getStripVisibilityStateSupplier().get() != StripVisibilityState.VISIBLE) {
            return;
        }
        // Remove the a11y views when top controls is partially invisible.
        if (mBrowserControlsStateProvider.getTopControlOffset() < 0) return;

        getActiveStripLayoutHelper().getVirtualViews(views);
        mTrailingButtonsCoordinator.getVirtualViews(views);
    }

    /** Allow / disallow system gestures on touchable areas on the strip. */
    private void updateTouchableAreas() {
        if (!mIsHeaderCustomizationSupported) return;

        if ((getStripVisibilityStateSupplier().get() & StripVisibilityState.HIDDEN_BY_FADE) != 0) {
            // Reset the system gesture exclusion rects to allow system gestures on the tab strip
            // area.
            mControlContainer.setSystemGestureExclusionRects(List.of(new Rect(0, 0, 0, 0)));
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

        TintedCompositorButton ntb = getNewTabButton();
        if (ntb != null && ntb.isVisible()) {
            var ntbTouchRect = new RectF();
            ntb.getTouchTarget(ntbTouchRect);
            // The click slop in `CompositorButton` can extend the touchable region of the new
            // tab button into the `mTopPadding` region, so the "top" coordinate  of `ntbRect`
            // intentionally isn't bound by `mTopPadding`. Doing so causes an inaccurate region
            // to ultimately be reported in `setSystemGestureExclusionRects()`.
            Rect ntbRect =
                    new Rect(
                            (int) Math.floor(ntbTouchRect.left * mDensity),
                            (int) Math.floor(ntbTouchRect.top * mDensity),
                            (int) Math.ceil(ntbTouchRect.right * mDensity),
                            (int) Math.ceil(ntbTouchRect.bottom * mDensity));
            rects.add(ntbRect);
        }

        TintedCompositorButton modelSelectorButton =
                mTrailingButtonsCoordinator.getModelSelectorButton();
        if (modelSelectorButton != null && modelSelectorButton.isVisible()) {
            var msbTouchRect = new RectF();
            modelSelectorButton.getTouchTarget(msbTouchRect);
            Rect msbRect =
                    new Rect(
                            (int) Math.floor(msbTouchRect.left * mDensity),
                            (int) Math.floor(Math.max(msbTouchRect.top, mTopPadding) * mDensity),
                            (int) Math.ceil(msbTouchRect.right * mDensity),
                            (int) Math.ceil(Math.min(msbTouchRect.bottom, mHeight) * mDensity));
            rects.add(msbRect);
        }

        TintedCompositorTextButton glicButton = mTrailingButtonsCoordinator.getGlicButton();
        if (glicButton != null && glicButton.isVisible()) {
            var glicTouchRect = new RectF();
            glicButton.getTouchTarget(glicTouchRect);
            Rect glicRect =
                    new Rect(
                            (int) Math.floor(glicTouchRect.left * mDensity),
                            (int) Math.floor(Math.max(glicTouchRect.top, mTopPadding) * mDensity),
                            (int) Math.ceil(glicTouchRect.right * mDensity),
                            (int) Math.ceil(Math.min(glicTouchRect.bottom, mHeight) * mDensity));
            rects.add(glicRect);
        }

        TintedCompositorTextButton glicActorButton =
                mTrailingButtonsCoordinator.getGlicActorButton();
        if (glicActorButton != null && glicActorButton.isVisible()) {
            var actorTouchRect = new RectF();
            glicActorButton.getTouchTarget(actorTouchRect);
            Rect actorRect =
                    new Rect(
                            (int) Math.floor(actorTouchRect.left * mDensity),
                            (int) Math.floor(Math.max(actorTouchRect.top, mTopPadding) * mDensity),
                            (int) Math.ceil(actorTouchRect.right * mDensity),
                            (int) Math.ceil(Math.min(actorTouchRect.bottom, mHeight) * mDensity));
            rects.add(actorRect);
        }

        mControlContainer.setSystemGestureExclusionRects(rects);
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

    public float getLeftFadeGradientWidth() {
        return getActiveStripLayoutHelper().getLeftFadeGradientWidth();
    }

    public float getRightFadeGradientWidth() {
        return getActiveStripLayoutHelper().getRightFadeGradientWidth();
    }

    public float getLeftFadeOpaqueWidth() {
        return getActiveStripLayoutHelper().getLeftFadeOpaqueWidth();
    }

    public float getRightFadeOpaqueWidth() {
        return getActiveStripLayoutHelper().getRightFadeOpaqueWidth();
    }

    /** Returns drag listener for tab strip. */
    public @Nullable OnDragListener getDragListener() {
        return mTabStripDragHandler;
    }

    /** Update the title cache for the available tabs in the model. */
    private void updateTitleCacheForInit() {
        LayerTitleCache titleCache = mLayerTitleCacheSupplier.get();
        if (mTabModelSelector == null || titleCache == null) return;

        // Make sure any tabs already restored get loaded into the title cache.
        List<TabModel> models = mTabModelSelector.getModels();
        for (int i = 0; i < models.size(); i++) {
            TabModel model = models.get(i);
            for (Tab tab : model) {
                if (tab != null) {
                    titleCache.getUpdatedTitle(
                            tab, tab.getContext().getString(R.string.tab_loading_default_title));
                }
            }
        }
    }

    private boolean isSpinnerFixEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STRIP_STOP_SPINNER_ON_LOAD_STOP)
                || DeviceInfo.isDesktop();
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
        modelSelector.addObserverToAllModels(mTabModelObserver);

        mTabModelSelector = modelSelector;

        updateTitleCacheForInit();

        if (mTabModelSelector.isTabStateInitialized()) {
            mTrailingButtonsCoordinator.updateTrailingButtons();
        } else {
            mTabModelSelector.addObserver(
                    new TabModelSelectorObserver() {
                        @Override
                        public void onTabStateInitialized() {
                            mTrailingButtonsCoordinator.updateTrailingButtons();
                            // mTabModelSelector should be non-null because it is set to non-null
                            // `modelSelector` parameter in enclosing function `setTabModelSelector`
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
        tabModelSwitched(mTabModelSelector.isIncognitoSelected());
        // Manually called on initialization, since the logic in #tabModelSwitched only runs if the
        // Incognito state actually changes. Since mIncognito defaults to false, it may not actually
        // change on initialization.
        getActiveStripLayoutHelper().setSelected(/* selected= */ true);

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
                        getStripLayoutHelper(tab.isIncognitoBranded()).willCloseTab(tab);
                        unregisterActorObserver(tab);
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        getStripLayoutHelper(tab.isIncognitoBranded()).tabClosed(tab);
                        unregisterActorObserver(tab);
                        mTrailingButtonsCoordinator.updateTrailingButtons();
                    }

                    @Override
                    public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                        // For right-direction move, layout helper re-ordering logic
                        // expects destination index = position + 1
                        getStripLayoutHelper(tab.isIncognitoBranded())
                                .tabMoved(
                                        tab.getId(),
                                        curIndex,
                                        newIndex > curIndex ? newIndex + 1 : newIndex);
                    }

                    @Override
                    public void tabClosureUndone(Tab tab) {
                        getStripLayoutHelper(tab.isIncognitoBranded())
                                .tabClosureCancelled(time(), tab.getId());
                        registerActorObserver(tab);
                        mTrailingButtonsCoordinator.updateTrailingButtons();
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        LayerTitleCache titleCache = mLayerTitleCacheSupplier.get();
                        if (titleCache != null) {
                            titleCache.removeTabTitle(tab.getId());
                        }
                    }

                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs,
                            boolean isAllTabs,
                            @TabClosingSource int closingSource) {
                        if (tabs.isEmpty()) return;
                        getStripLayoutHelper(tabs.get(0).isIncognitoBranded())
                                .multipleTabsClosed(tabs);
                        mTrailingButtonsCoordinator.updateTrailingButtons();
                    }

                    @Override
                    public void onFinishingTabClosure(
                            Tab tab, @TabClosingSource int closingSource) {
                        getStripLayoutHelper(tab.isIncognitoBranded()).tabClosed(tab);
                        mTrailingButtonsCoordinator.updateTrailingButtons();
                    }

                    @Override
                    public void willCloseAllTabs(boolean incognito) {
                        getStripLayoutHelper(incognito).willCloseAllTabs();
                        mTrailingButtonsCoordinator.updateTrailingButtons();
                    }

                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if (tab.getId() == lastId) return;
                        getStripLayoutHelper(tab.isIncognitoBranded())
                                .tabSelected(time(), tab.getId(), lastId, type);
                    }

                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        boolean onStartup = type == TabLaunchType.FROM_RESTORE;
                        getStripLayoutHelper(tab.isIncognitoBranded())
                                .tabCreated(
                                        time(), tab.getId(), markedForSelection, false, onStartup);
                        registerActorObserver(tab);
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
                        if (!isSpinnerFixEnabled() && !toDifferentDocument) return;
                        StripLayoutHelper helper = getStripLayoutHelper(tab.isIncognitoBranded());
                        helper.tabLoadStarted(tab.getId());
                    }

                    @Override
                    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                        if (!isSpinnerFixEnabled() && !toDifferentDocument) {
                            return;
                        }
                        StripLayoutHelper helper = getStripLayoutHelper(tab.isIncognitoBranded());
                        helper.tabLoadFinished(tab.getId());
                    }

                    @Override
                    public void onLoadProgressChanged(Tab tab, float progress) {
                        if (isSpinnerFixEnabled() && MathUtils.areFloatsEqual(progress, 1.0f)) {
                            StripLayoutHelper helper =
                                    getStripLayoutHelper(tab.isIncognitoBranded());
                            helper.tabLoadFinished(tab.getId());
                        }
                    }

                    @Override
                    public void onPageLoadFailed(Tab tab, int errorCode) {
                        if (isSpinnerFixEnabled()) {
                            StripLayoutHelper helper =
                                    getStripLayoutHelper(tab.isIncognitoBranded());
                            helper.tabLoadFinished(tab.getId());
                        }
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        StripLayoutHelper helper = getStripLayoutHelper(tab.isIncognitoBranded());
                        helper.tabLoadFinished(tab.getId());
                    }

                    @Override
                    public void onDocumentLoadedInPrimaryMainFrame(Tab tab) {
                        if (isSpinnerFixEnabled()) {
                            StripLayoutHelper helper =
                                    getStripLayoutHelper(tab.isIncognitoBranded());
                            helper.tabLoadFinished(tab.getId());
                        }
                    }

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        StripLayoutHelper helper = getStripLayoutHelper(tab.isIncognitoBranded());
                        if (isSpinnerFixEnabled()) {
                            helper.tabLoadFinished(tab.getId());
                        }
                        if (tab == mTabModelSelector.getCurrentTab()) {
                            helper.attemptToQueueGlicIph(tab);
                        }
                    }

                    @Override
                    public void onTitleUpdated(Tab tab) {
                        updateTitleForTab(tab);
                    }

                    @Override
                    public void onFaviconUpdated(
                            Tab tab, @Nullable Bitmap icon, @Nullable GURL iconUrl) {
                        updateTitleForTab(tab);
                    }

                    @Override
                    public void onMediaStateChanged(Tab tab, @MediaState int mediaState) {
                        getStripLayoutHelper(tab.isIncognito())
                                .onMediaStateChanged(tab, mediaState);
                        mRenderHost.requestRender();
                    }
                };

        mTabModelSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndPostIfNonNull(mCurrentTabModelObserver);
        if (mTabStripDragHandler != null) {
            mTabStripDragHandler.setTabModelSelector(mTabModelSelector);
        }

        // Register Glic actor observer for existing standard tabs.
        TabModel standardModel = mTabModelSelector.getModel(false);
        for (int i = 0; i < standardModel.getCount(); i++) {
            Tab tab = standardModel.getTabAt(i);
            if (tab != null) {
                registerActorObserver(tab);
            }
        }

        // Register Glic pref change observer for Glic button pin state.
        Profile profile = standardModel.getProfile();
        if (profile != null) {
            mTrailingButtonsCoordinator.onProfileAvailable(profile);
        }
    }

    @Override
    public void onAppHeaderStateChanged(AppHeaderState newState) {
        assert mDesktopWindowStateManager != null;
        // We do not update the layer's height in this method. The height adjustment will be
        // triggered by #onHeightChanged.

        mDesktopWindowStateManager.updateForegroundColor(getBackgroundColor());
        updateHorizontalPaddings(newState.getLeftPadding(), newState.getRightPadding());

        mTrailingButtonsCoordinator.updateGlicButtonOpacity(
                isAppInDesktopWindow(), mIsTopResumedActivity);
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
        LayerTitleCache layerCache = mLayerTitleCacheSupplier.get();
        if (layerCache == null) return;

        String title = layerCache.getUpdatedTitle(tab, mDefaultTitle);
        getStripLayoutHelper(tab.isIncognito()).tabTitleChanged(tab.getId(), title);
        mUpdateHost.requestUpdate();
    }

    private void registerActorObserver(Tab tab) {
        if (tab.isIncognitoBranded()) return;
        ActorUiTabController controller = ActorUiTabController.from(tab);
        if (controller == null) return;

        controller.addObserver(mActorObserver);

        ActorUiTabController.UiTabState state = controller.getUiTabState();
        if (state != null) {
            getStripLayoutHelper(/* incognito= */ false)
                    .onActuationStateChanged(tab.getId(), state.tabIndicator);
        }
    }

    private void unregisterActorObserver(Tab tab) {
        if (tab == null || tab.isIncognitoBranded()) return;
        ActorUiTabController controller = ActorUiTabController.from(tab);
        if (controller != null) {
            controller.removeObserver(mActorObserver);
        }
    }

    public float getHeight() {
        return mHeight;
    }

    public float getWidth() {
        return mWidth;
    }

    public @ColorInt int getBackgroundColor() {
        return TabUiThemeUtil.getTabStripBackgroundColor(
                mContext, mIsIncognito, isAppInDesktopWindow(), mIsTopResumedActivity);
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

    @VisibleForTesting
    /*package*/ void tabModelSwitched(boolean incognito) {
        if (incognito == mIsIncognito) return;
        mIsIncognito = incognito;

        mIncognitoHelper.tabModelSelected(mIsIncognito);
        mNormalHelper.tabModelSelected(!mIsIncognito);
        mTrailingButtonsCoordinator.onTabModelSwitched(mIsIncognito);

        mTrailingButtonsCoordinator.updateTrailingButtons();

        // If we are in DW mode, notify DW state provider since the model changed.
        if (isAppInDesktopWindow()) {
            mDesktopWindowStateManager.updateForegroundColor(getBackgroundColor());
        }

        mManagerHost.resetKeyboardFocus(); // Reset virtual views index & keyboard focus state.
        mUpdateHost.requestUpdate();
    }

    private void updateHelperEndMargins() {
        float trailingButtonsTouchTargetSize =
                mTrailingButtonsCoordinator.getTrailingButtonsWidthWithPadding();
        mNormalHelper.updateEndMarginForStripButtons(trailingButtonsTouchTargetSize);
        mIncognitoHelper.updateEndMarginForStripButtons(trailingButtonsTouchTargetSize);

        if (mFadeTransitionThresholdChangedCallback != null) {
            mFadeTransitionThresholdChangedCallback.run();
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

    public NonNullObservableSupplier<@StripVisibilityState Integer>
            getStripVisibilityStateSupplier() {
        // TODO(crbug.com/417238089): get() returns a stale value during height transitions.
        return mStripVisibilityStateSupplier;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public void setStripVisibilityState(@StripVisibilityState int visibilityState, boolean clear) {
        @StripVisibilityState int curVisibility = mStripVisibilityStateSupplier.get();
        mStripVisibilityStateSupplier.set(
                clear ? (curVisibility & ~visibilityState) : (curVisibility | visibilityState));
    }

    /** Returns a {@link NonNullObservableSupplier} for the bottom of the tab strip in px. */
    public NonNullObservableSupplier<Integer> getStripBottomPxSupplier() {
        return mStripBottomPxSupplier;
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

    void simulateOnDownForTesting(float x, float y, int buttons) {
        mTabStripEventHandler.onDown(x, y, buttons);
    }

    void setTabStripTreeProviderForTesting(TabStripSceneLayer tabStripTreeProvider) {
        mTabStripTreeProvider = tabStripTreeProvider;
    }

    ViewStub getTabHoverCardViewStubForTesting() {
        return mTabHoverCardViewStub;
    }

    public @Nullable TabStripDragHandler getTabStripDragHandlerForTesting() {
        return mTabStripDragHandler;
    }

    /** Returns true if the current tab model is incognito. */
    public boolean isIncognito() {
        return mIsIncognito;
    }

    public void setIsIncognitoForTesting(boolean isIncognito) {
        mIsIncognito = isIncognito;
    }

    public boolean isStripScrimVisibleForTesting() {
        return mStripTransitionScrimOpacity == 1f;
    }

    /** Request keyboard focus on the tab strip. */
    public void requestKeyboardFocus() {
        mManagerHost.requestKeyboardFocus(this);
    }

    /**
     * @return Whether the tab strip contains keyboard focus.
     */
    public boolean containsKeyboardFocus() {
        return mManagerHost.containsKeyboardFocus(this);
    }

    /**
     * Opens the context menu for the currently keyboard-focused item, if applicable.
     *
     * @return Whether the context menu was successfully opened.
     */
    public boolean openKeyboardFocusedContextMenu() {
        if (getActiveStripLayoutHelper().openKeyboardFocusedContextMenu()) {
            return true;
        }
        var activity = assertNonNull(mWindowAndroid.getActivity().get());
        return mTrailingButtonsCoordinator.openKeyboardFocusedContextMenu(activity);
    }

    /**
     * Reorders the currently keyboard-focused item, if applicable.
     *
     * @param toLeft Whether the focused item should be reordered to the left (note: this is still
     *     left in RTL).
     * @return Whether the item was successfully reordered.
     */
    public boolean reorderKeyboardFocusedItem(boolean toLeft) {
        return getActiveStripLayoutHelper().moveSelectedStripView(toLeft);
    }

    /**
     * Toggles multiselection on the keyboard focused tab.
     *
     * @return Whether the multiselect action was successfully performed.
     */
    public boolean multiselectKeyboardFocusedItem() {
        return getActiveStripLayoutHelper().multiselectKeyboardFocusedItem();
    }

    private boolean isActivityInXrFullSpaceModeNow() {
        return mXrSpaceModeObservableSupplier != null && mXrSpaceModeObservableSupplier.get();
    }
}
