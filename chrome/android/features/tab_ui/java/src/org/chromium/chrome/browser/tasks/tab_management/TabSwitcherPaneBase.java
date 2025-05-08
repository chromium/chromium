// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_FADE_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.HUB_LAYOUT_SHRINK_EXPAND_DURATION_MS;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherConstants.DESTROY_COORDINATOR_DELAY_MS;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherConstants.HARD_CLEANUP_DELAY_MS;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherConstants.SOFT_CLEANUP_DELAY_MS;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Handler;
import android.os.SystemClock;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.SyncOneshotSupplier;
import org.chromium.base.supplier.SyncOneshotSupplierImpl;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.hub.DisplayButtonData;
import org.chromium.chrome.browser.hub.FadeHubLayoutAnimationFactory;
import org.chromium.chrome.browser.hub.FullButtonData;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.hub.HubLayoutAnimationListener;
import org.chromium.chrome.browser.hub.HubLayoutAnimatorProvider;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.ShrinkExpandAnimationData;
import org.chromium.chrome.browser.hub.ShrinkExpandHubLayoutAnimationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController.MenuOrKeyboardActionHandler;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.function.DoubleConsumer;

/**
 * An abstract {@link Pane} representing a tab switcher for shared logic between the normal and
 * incognito modes.
 */
public abstract class TabSwitcherPaneBase implements Pane, TabSwitcher, TabSwitcherResetHandler {
    private static final String TAG = "TabSwitcherPaneBase";
    private static final int ON_SHOWN_IPH_DELAY = 700;

    private static boolean sShowIphForTesting;

    protected final ObservableSupplierImpl<DisplayButtonData> mReferenceButtonDataSupplier =
            new ObservableSupplierImpl<>();
    protected final ObservableSupplierImpl<FullButtonData> mNewTabButtonDataSupplier =
            new ObservableSupplierImpl<>();
    protected final ObservableSupplierImpl<Boolean> mHairlineVisibilitySupplier =
            new ObservableSupplierImpl<>();
    protected final UserEducationHelper mUserEducationHelper;
    protected final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    protected final ObservableSupplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final ObservableSupplierImpl<Boolean> mIsVisibleSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mIsAnimatingSupplier =
            new ObservableSupplierImpl<>();
    private final Callback<Boolean> mVisibilityObserver = this::onVisibilityChanged;
    private final Handler mHandler = new Handler();
    private final Runnable mSoftCleanupRunnable = this::softCleanupInternal;
    private final Runnable mHardCleanupRunnable = this::hardCleanupInternal;
    private final Runnable mDestroyCoordinatorRunnable = this::destroyTabSwitcherPaneCoordinator;
    private final TabSwitcherCustomViewManager mTabSwitcherCustomViewManager =
            new TabSwitcherCustomViewManager();

    private final MenuOrKeyboardActionHandler mMenuOrKeyboardActionHandler =
            new MenuOrKeyboardActionHandler() {
                @Override
                public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
                    if (id == R.id.menu_select_tabs) {
                        @Nullable
                        TabSwitcherPaneCoordinator coordinator =
                                mTabSwitcherPaneCoordinatorSupplier.get();
                        if (coordinator == null) return false;

                        coordinator.showTabListEditor();
                        RecordUserAction.record("MobileMenuSelectTabs");
                        return true;
                    } else if (id == R.id.new_tab_group_menu_id) {
                        mUiFlow.newTabGroupFlow();
                        RecordUserAction.record("MobileMenuNewTabGroup");
                        if (mTracker != null) {
                            mTracker.notifyEvent("tab_switcher_add_to_group_clicked");
                        }
                        return true;
                    }
                    return false;
                }
            };
    private final ObservableSupplierImpl<TabSwitcherPaneCoordinator>
            mTabSwitcherPaneCoordinatorSupplier = new ObservableSupplierImpl<>();
    private final TransitiveObservableSupplier<TabSwitcherPaneCoordinator, Boolean>
            mHandleBackPressChangedSupplier =
                    new TransitiveObservableSupplier<>(
                            mTabSwitcherPaneCoordinatorSupplier,
                            pc -> pc.getHandleBackPressChangedSupplier());
    private final FrameLayout mRootView;
    private final TabSwitcherPaneCoordinatorFactory mFactory;
    private final boolean mIsIncognito;
    private final DoubleConsumer mOnToolbarAlphaChange;
    private final TabGroupCreationUiDelegate mUiFlow;
    private final HubLayoutAnimationListener mAnimationListener =
            new HubLayoutAnimationListener() {
                @Override
                public void beforeStart() {
                    mIsAnimatingSupplier.set(true);
                    if (OmniboxFeatures.sAndroidHubSearch.isEnabled()
                            && mPaneHubController != null) {
                        mPaneHubController.setSearchBoxBackgroundProperties(/* shouldShow= */ true);
                    }
                }

                @Override
                public void afterEnd() {
                    if (OmniboxFeatures.sAndroidHubSearch.isEnabled()
                            && mPaneHubController != null) {
                        mPaneHubController.setSearchBoxBackgroundProperties(
                                /* shouldShow= */ false);
                    }
                    mIsAnimatingSupplier.set(false);
                }
            };

    protected @Nullable Tracker mTracker;
    private boolean mNativeInitialized;
    private @Nullable PaneHubController mPaneHubController;
    private @Nullable Long mWaitForTabStateInitializedStartTimeMs;

    /**
     * @param context The activity context.
     * @param factory The factory used to construct {@link TabSwitcherPaneCoordinator}s.
     * @param isIncognito Whether the pane is incognito.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param userEducationHelper Used for showing IPHs.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param compositorViewHolderSupplier Supplier to the {@link CompositorViewHolder} instance.
     * @param tabGroupCreationUiDelegate Orchestrates the tab group creation UI flow.
     */
    TabSwitcherPaneBase(
            @NonNull Context context,
            @NonNull TabSwitcherPaneCoordinatorFactory factory,
            boolean isIncognito,
            @NonNull DoubleConsumer onToolbarAlphaChange,
            @NonNull UserEducationHelper userEducationHelper,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @NonNull ObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull TabGroupCreationUiDelegate tabGroupCreationUiDelegate) {
        mFactory = factory;
        mIsIncognito = isIncognito;

        mRootView = new FrameLayout(context);
        mIsVisibleSupplier.set(false);
        mIsVisibleSupplier.addObserver(mVisibilityObserver);
        mIsAnimatingSupplier.set(false);
        mOnToolbarAlphaChange = onToolbarAlphaChange;
        mUserEducationHelper = userEducationHelper;
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mUiFlow = tabGroupCreationUiDelegate;
    }

    @Override
    public void destroy() {
        removeDelayedCallbacks();
        mIsVisibleSupplier.removeObserver(mVisibilityObserver);
        destroyTabSwitcherPaneCoordinator();
    }

    @Override
    public @NonNull ViewGroup getRootView() {
        return mRootView;
    }

    @Override
    public @Nullable MenuOrKeyboardActionHandler getMenuOrKeyboardActionHandler() {
        return mMenuOrKeyboardActionHandler;
    }

    @Override
    public boolean getMenuButtonVisible() {
        return true;
    }

    @Override
    public void setPaneHubController(@Nullable PaneHubController paneHubController) {
        mPaneHubController = paneHubController;
    }

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        boolean isVisible = loadHint == LoadHint.HOT;
        mIsVisibleSupplier.set(isVisible);
        removeDelayedCallbacks();

        if (isVisible) {
            createTabSwitcherPaneCoordinator();
            showAllTabs();
            setInitialScrollIndexOffset();
            // TODO(crbug.com/40942549): This should only happen when the Pane becomes user visible
            // which might only happen after the Hub animation finishes. Figure out how to handle
            // that since the load hint for hot will come before the animation is started. Panes
            // likely need to know an animation is going to play and when it is finished (possibly
            // using the isAnimatingSupplier?).
            requestAccessibilityFocusOnCurrentTab();
        } else {
            cancelWaitForTabStateInitializedTimer();
        }

        if (loadHint == LoadHint.WARM) {
            if (mTabSwitcherPaneCoordinatorSupplier.hasValue()) {
                mHandler.postDelayed(mSoftCleanupRunnable, SOFT_CLEANUP_DELAY_MS);
            } else if (shouldEagerlyCreateCoordinator()) {
                createTabSwitcherPaneCoordinator();
            }
        }

        if (loadHint == LoadHint.COLD) {
            if (mTabSwitcherPaneCoordinatorSupplier.hasValue()) {
                mHandler.postDelayed(mSoftCleanupRunnable, SOFT_CLEANUP_DELAY_MS);
                mHandler.postDelayed(mHardCleanupRunnable, HARD_CLEANUP_DELAY_MS);
                mHandler.postDelayed(mDestroyCoordinatorRunnable, DESTROY_COORDINATOR_DELAY_MS);
            }
        }
    }

    @Override
    public @NonNull ObservableSupplier<FullButtonData> getActionButtonDataSupplier() {
        return mNewTabButtonDataSupplier;
    }

    @Override
    public @NonNull ObservableSupplier<DisplayButtonData> getReferenceButtonDataSupplier() {
        return mReferenceButtonDataSupplier;
    }

    @Override
    public @NonNull ObservableSupplier<Boolean> getHairlineVisibilitySupplier() {
        return mHairlineVisibilitySupplier;
    }

    @Override
    public @Nullable HubLayoutAnimationListener getHubLayoutAnimationListener() {
        return mAnimationListener;
    }

    @Override
    public @NonNull HubLayoutAnimatorProvider createShowHubLayoutAnimatorProvider(
            @NonNull HubContainerView hubContainerView) {
        assert !DeviceFormFactor.isNonMultiDisplayContextOnTablet(hubContainerView.getContext());
        @Nullable Tab tab = getCurrentTab();
        if (getTabListMode() == TabListMode.LIST || tab == null || SysUtils.isLowEndDevice()) {
            return FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                    hubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
        }

        @ColorInt int backgroundColor = getAnimationBackgroundColor();
        SyncOneshotSupplier<ShrinkExpandAnimationData> animationDataSupplier =
                requestAnimationData(hubContainerView, /* isShrink= */ true, tab);
        return ShrinkExpandHubLayoutAnimationFactory.createShrinkTabAnimatorProvider(
                hubContainerView,
                animationDataSupplier,
                backgroundColor,
                HUB_LAYOUT_SHRINK_EXPAND_DURATION_MS,
                mOnToolbarAlphaChange);
    }

    @Override
    public @NonNull HubLayoutAnimatorProvider createHideHubLayoutAnimatorProvider(
            @NonNull HubContainerView hubContainerView) {
        assert !DeviceFormFactor.isNonMultiDisplayContextOnTablet(hubContainerView.getContext());
        Tab tab = getCurrentTab();
        if (getTabListMode() == TabListMode.LIST || tab == null || SysUtils.isLowEndDevice()) {
            return FadeHubLayoutAnimationFactory.createFadeOutAnimatorProvider(
                    hubContainerView, HUB_LAYOUT_FADE_DURATION_MS, mOnToolbarAlphaChange);
        }

        @ColorInt int backgroundColor = getAnimationBackgroundColor();
        SyncOneshotSupplier<ShrinkExpandAnimationData> animationDataSupplier =
                requestAnimationData(hubContainerView, /* isShrink= */ false, tab);
        return ShrinkExpandHubLayoutAnimationFactory.createExpandTabAnimatorProvider(
                hubContainerView,
                animationDataSupplier,
                backgroundColor,
                HUB_LAYOUT_SHRINK_EXPAND_DURATION_MS,
                mOnToolbarAlphaChange);
    }

    private @ColorInt int getAnimationBackgroundColor() {
        if (mIsIncognito) {
            return ChromeColors.getPrimaryBackgroundColor(mRootView.getContext(), mIsIncognito);
        } else {
            // TODO(crbug.com/40948541): Consider not getting the color from home surface.
            return ContextCompat.getColor(
                    mRootView.getContext(), R.color.home_surface_background_color);
        }
    }

    private SyncOneshotSupplier<ShrinkExpandAnimationData> requestAnimationData(
            @NonNull HubContainerView hubContainerView, boolean isShrink, @NonNull Tab tab) {
        SyncOneshotSupplierImpl<ShrinkExpandAnimationData> animationDataSupplier =
                new SyncOneshotSupplierImpl<>();
        @Nullable TabSwitcherPaneCoordinator coordinator = getTabSwitcherPaneCoordinator();
        assert coordinator != null;

        Resources res = hubContainerView.getContext().getResources();
        int thumbnailRadiusTop =
                res.getDimensionPixelSize(R.dimen.tab_grid_card_thumbnail_corner_radius_top);
        int thumbnailRadiusBottom =
                res.getDimensionPixelSize(R.dimen.tab_grid_card_thumbnail_corner_radius_bottom);

        int initialTopCornerRadius;
        int finalTopCornerRadius;
        int initialBottomCornerRadius;
        int finalBottomCornerRadius;
        if (isShrink) {
            initialTopCornerRadius = 0;
            finalTopCornerRadius = thumbnailRadiusTop;
            initialBottomCornerRadius = 0;
            finalBottomCornerRadius = thumbnailRadiusBottom;
        } else {
            initialTopCornerRadius = thumbnailRadiusTop;
            finalTopCornerRadius = 0;
            initialBottomCornerRadius = thumbnailRadiusBottom;
            finalBottomCornerRadius = 0;
        }

        int tabId = tab.getId();
        boolean isTopToolbar = ToolbarPositionController.shouldShowToolbarOnTop(tab);

        Runnable provideAnimationData =
                () -> {
                    Rect hubRect = new Rect();
                    hubContainerView.getGlobalVisibleRect(hubRect);
                    Rect initialRect;
                    Rect finalRect;
                    Rect viewportRect = new Rect();

                    CompositorViewHolder viewHolder = mCompositorViewHolderSupplier.get();
                    RectF viewportRectf = new RectF();
                    viewHolder.getVisibleViewport(viewportRectf);
                    viewportRectf.round(viewportRect);
                    if (!isTopToolbar) {
                        // TODO(crubug.com/390714662): This is a temporary fix and should be removed
                        // once the fade-in and fade-out behavior of the bottom toolbar is
                        // implemented.
                        // When the bottom toolbar is visible, the tab's view must extend all the
                        // way to the bottom to prevent the tab switcher from being visible behind
                        // it during animations.
                        RectF windowViewportRectf = new RectF();
                        Rect windowViewportRect = new Rect();
                        viewHolder.getWindowViewport(windowViewportRectf);
                        windowViewportRectf.round(windowViewportRect);
                        viewportRect.bottom = windowViewportRect.bottom;
                    }

                    int leftOffset = 0;
                    int initialTopOffset = 0;
                    int finalTopOffset = 0;
                    if (isShrink) {
                        initialRect = viewportRect;
                        finalRect = coordinator.getTabThumbnailRect(tabId);
                        leftOffset = initialRect.left;
                        finalTopOffset = hubRect.top;
                    } else {
                        initialRect = coordinator.getTabThumbnailRect(tabId);
                        finalRect = viewportRect;
                        leftOffset = finalRect.left;
                        initialTopOffset = hubRect.top;
                    }

                    boolean useFallbackAnimation = false;
                    if (initialRect.isEmpty() || finalRect.isEmpty()) {
                        Log.d(TAG, "Geometry not ready using fallback animation.");
                        useFallbackAnimation = true;
                    }
                    // Ignore left offset and just ensure the width is correct. See crbug/1502437.
                    initialRect.offset(-leftOffset, -initialTopOffset);
                    finalRect.offset(-leftOffset, -finalTopOffset);
                    animationDataSupplier.set(
                            ShrinkExpandAnimationData.createHubShrinkExpandAnimationData(
                                    initialRect,
                                    finalRect,
                                    initialTopCornerRadius,
                                    initialBottomCornerRadius,
                                    finalTopCornerRadius,
                                    finalBottomCornerRadius,
                                    coordinator.getThumbnailSize(),
                                    isTopToolbar,
                                    useFallbackAnimation));
                };
        coordinator.waitForLayoutWithTab(tabId, provideAnimationData);
        return animationDataSupplier;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return BackPressResult.FAILURE;
        return coordinator.handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHandleBackPressChangedSupplier;
    }

    @Override
    public void initWithNative() {
        if (mNativeInitialized) return;

        mNativeInitialized = true;
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator != null) {
            coordinator.initWithNative();
        }
    }

    /** Returns a {@link Supplier} that provides dialog visibility information. */
    @Override
    public @Nullable Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return null;
        return coordinator.getTabGridDialogVisibilitySupplier();
    }

    /** Returns a {@link TabSwitcherCustomViewManager} for supplying custom views. */
    @Override
    public @Nullable TabSwitcherCustomViewManager getTabSwitcherCustomViewManager() {
        return mTabSwitcherCustomViewManager;
    }

    /** Returns the number of elements in the tab switcher's tab list model. */
    @Override
    public int getTabSwitcherTabListModelSize() {
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return 0;
        return coordinator.getTabSwitcherTabListModelSize();
    }

    /** Set the tab switcher's RecyclerViewPosition. */
    @Override
    public void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition position) {
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return;
        coordinator.setTabSwitcherRecyclerViewPosition(position);
    }

    /** Show the Quick Delete animation on the tab list . */
    @Override
    public void showQuickDeleteAnimation(Runnable onAnimationEnd, List<Tab> tabs) {
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null || getTabListMode() != TabListMode.GRID) {
            onAnimationEnd.run();
            return;
        }
        coordinator.showQuickDeleteAnimation(onAnimationEnd, tabs);
    }

    @Override
    public boolean requestOpenTabGroupDialog(int tabId) {
        @Nullable
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator != null) {
            coordinator.requestOpenTabGroupDialog(tabId);
            return true;
        } else {
            return false;
        }
    }

    /**
     * Request to show all the tabs in the pane. Subclasses should override this method to invoke
     * {@link TabSwitcherResetHandler#resetWithListOfTabs} with their available tabs.
     */
    protected abstract void showAllTabs();

    /** Returns the current selected tab. */
    protected abstract @Nullable Tab getCurrentTab();

    /** Returns whether to eagerly create the coordinator in the {@link LoadHint.WARM} state. */
    protected abstract boolean shouldEagerlyCreateCoordinator();

    /** A runnable that will be invoked when delegate UI creates a tab group. */
    protected abstract Runnable getOnTabGroupCreationRunnable();

    /** Called when the pane is shown to indicate IPH should maybe be shown. */
    protected abstract void tryToTriggerOnShownIphs();

    /** Requests accessibility focus on the currently selected tab in the tab switcher. */
    protected void requestAccessibilityFocusOnCurrentTab() {
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return;
        coordinator.requestAccessibilityFocusOnCurrentTab();
    }

    /** Returns the {@link TabListMode} of the {@link TabListCoordinator} that the pane hosts. */
    protected @TabListMode int getTabListMode() {
        return mFactory.getTabListMode();
    }

    /**
     * Returns a supplier for whether the pane is visible onscreen. Note this is not the same as
     * being focused.
     */
    protected ObservableSupplier<Boolean> getIsVisibleSupplier() {
        return mIsVisibleSupplier;
    }

    /**
     * Holds whether there's an ongoing animation with this Pane and outside the hub. Care must be
     * taken when reading this supplier as animations do not start synchronously with focus changes,
     * and a Pane may be shown before the enter animation actually starts.
     */
    protected @NonNull ObservableSupplier<Boolean> getIsAnimatingSupplier() {
        return mIsAnimatingSupplier;
    }

    /** Returns whether the pane is focused. */
    protected boolean isFocused() {
        return mPaneHubController != null;
    }

    /** Returns the PaneHubController if one exists or null otherwise. */
    protected @Nullable PaneHubController getPaneHubController() {
        return mPaneHubController;
    }

    /** Returns the current {@link TabSwitcherPaneCoordinator} or null if one doesn't exist. */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    @Nullable
    TabSwitcherPaneCoordinator getTabSwitcherPaneCoordinator() {
        return mTabSwitcherPaneCoordinatorSupplier.get();
    }

    /** Returns an observable supplier that hold the current coordinator. */
    protected @NonNull ObservableSupplier<TabSwitcherPaneCoordinator>
            getTabSwitcherPaneCoordinatorSupplier() {
        return mTabSwitcherPaneCoordinatorSupplier;
    }

    /** Creates a {@link TabSwitcherCoordinator}. */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    void createTabSwitcherPaneCoordinator() {
        if (mTabSwitcherPaneCoordinatorSupplier.hasValue()) return;

        @NonNull
        TabSwitcherPaneCoordinator coordinator =
                mFactory.create(
                        mRootView,
                        /* resetHandler= */ this,
                        mIsVisibleSupplier,
                        mIsAnimatingSupplier,
                        this::onTabClick,
                        mHairlineVisibilitySupplier::set,
                        mIsIncognito,
                        getOnTabGroupCreationRunnable(),
                        mEdgeToEdgeSupplier);
        mTabSwitcherPaneCoordinatorSupplier.set(coordinator);
        mTabSwitcherCustomViewManager.setDelegate(
                coordinator.getTabSwitcherCustomViewManagerDelegate());
        if (mNativeInitialized) {
            coordinator.initWithNative();
        }
    }

    /**
     * Destroys the current {@link TabSwitcherCoordinator}. It is safe to call this even if a
     * coordinator does not exist.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    void destroyTabSwitcherPaneCoordinator() {
        if (!mTabSwitcherPaneCoordinatorSupplier.hasValue()) return;

        @NonNull TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        mTabSwitcherPaneCoordinatorSupplier.set(null);
        mRootView.removeAllViews();
        mTabSwitcherCustomViewManager.setDelegate(null);
        coordinator.destroy();
    }

    protected void startWaitForTabStateInitializedTimer() {
        if (mWaitForTabStateInitializedStartTimeMs != null) return;

        mWaitForTabStateInitializedStartTimeMs = SystemClock.elapsedRealtime();
    }

    protected void finishWaitForTabStateInitializedTimer() {
        if (mWaitForTabStateInitializedStartTimeMs != null) {
            RecordHistogram.recordTimesHistogram(
                    "Android.GridTabSwitcher.TimeToTabStateInitializedFromShown",
                    SystemClock.elapsedRealtime()
                            - mWaitForTabStateInitializedStartTimeMs.longValue());
            mWaitForTabStateInitializedStartTimeMs = null;
        }
    }

    protected void cancelWaitForTabStateInitializedTimer() {
        mWaitForTabStateInitializedStartTimeMs = null;
    }

    private void onTabClick(int tabId) {
        if (mPaneHubController == null) return;

        // TODO(crbug.com/41489932): Consider using INVALID_TAB_ID if already selected to prevent a
        // repeat selection. For now this is required to ensure the tab gets marked as shown when
        // exiting the Hub. See if this can be updated/changed.
        mPaneHubController.selectTabAndHideHub(tabId);
    }

    private void setInitialScrollIndexOffset() {
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return;
        coordinator.setInitialScrollIndexOffset();
    }

    private void softCleanupInternal() {
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return;
        coordinator.softCleanup();
    }

    private void hardCleanupInternal() {
        TabSwitcherPaneCoordinator coordinator = mTabSwitcherPaneCoordinatorSupplier.get();
        if (coordinator == null) return;
        coordinator.hardCleanup();
    }

    private void removeDelayedCallbacks() {
        mHandler.removeCallbacks(mSoftCleanupRunnable);
        mHandler.removeCallbacks(mHardCleanupRunnable);
        mHandler.removeCallbacks(mDestroyCoordinatorRunnable);
    }

    private void onVisibilityChanged(boolean visible) {
        if (visible) {
            postOnShownIphRunner();
        }
    }

    private void postOnShownIphRunner() {
        // TODO(crbug.com/346356139): Figure out a more elegant way of observing entering the hub as
        // well as switching between panes. Knowing when these animations complete turns out to be
        // fairly difficult, especially knowing when we're about to enter a transition.
        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, this::onShownIphRunner, ON_SHOWN_IPH_DELAY);
    }

    private void onShownIphRunner() {
        if (BuildConfig.IS_FOR_TEST && !sShowIphForTesting) return;

        // The IPH system will ensure we don't show everything at once.
        tryToTriggerOnShownIphs();
    }

    void softCleanupForTesting() {
        mSoftCleanupRunnable.run();
    }

    void hardCleanupForTesting() {
        mHardCleanupRunnable.run();
    }

    void destroyCoordinatorForTesting() {
        mDestroyCoordinatorRunnable.run();
    }

    static void setShowIphForTesting(boolean showIphForTesting) {
        boolean oldValue = sShowIphForTesting;
        sShowIphForTesting = showIphForTesting;
        ResettersForTesting.register(() -> sShowIphForTesting = oldValue);
    }
}
