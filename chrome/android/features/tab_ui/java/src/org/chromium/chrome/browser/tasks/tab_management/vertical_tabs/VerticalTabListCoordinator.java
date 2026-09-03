// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.view.GestureDetector;
import android.view.HapticFeedbackConstants;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.AnchorInfo;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.TabStripLayoutType;
import org.chromium.chrome.browser.compositor.overlays.strip.TabGroupContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabUnderlineManager;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.StripDragShadowView;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksUtils;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.dragdrop.ChromeDragAndDropBrowserDelegate;
import org.chromium.chrome.browser.dragdrop.ChromeDragDropUtils;
import org.chromium.chrome.browser.dragdrop.ChromeMultiTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabGroupDropDataAndroid;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabListMode;
import org.chromium.chrome.browser.tabmodel.NextTabSelectionUtil;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabCreatorUtil;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabGroupCreationCallback;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.MultiThumbnailCardProvider;
import org.chromium.chrome.browser.tasks.tab_management.NestedTabReorderUtils;
import org.chromium.chrome.browser.tasks.tab_management.StaticPinnedTabsMediator;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabComponentId;
import org.chromium.chrome.browser.tasks.tab_management.TabDragHandlerBase;
import org.chromium.chrome.browser.tasks.tab_management.TabListConfig;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListItemOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabMultiSelectHelper;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherBackPressHandlerManager;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherDragHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherDragHandler.DragHandlerDelegate;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalExternalViewDragDropReorderStrategy.DropTargetResult;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardController.TabHoverCardListener;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.accessibility.KeyboardFocusUtil;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragAndDropDelegateImpl;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** Coordinator to manage and display the Vertical Tab List. */
@NullMarked
public class VerticalTabListCoordinator {
    static final int DEFAULT_GRID_SPAN_COUNT = 4;
    static final int MAX_SINGLE_ROW_SPAN_COUNT = 5;
    static final int COLLAPSED_GRID_SPAN_COUNT = 1;
    // Epsilon (5% of a column span) absorbs sub-pixel rounding errors at boundary thresholds.
    private static final float SPAN_CALCULATION_EPSILON = 0.05f;
    private static @Nullable Supplier<TabSwitcherDragHandler>
            sTabSwitcherDragHandlerSupplierForTesting;
    private final VerticalTabRailLayout mContainerView;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabListModel mModelList;
    private final TabListMediator mMediator;
    private final VerticalTabListRecyclerView mRecyclerView;
    private final TabListModel mPinnedTabsModelList;
    private final StaticPinnedTabsMediator mPinnedTabsMediator;
    private final TabListRecyclerView mPinnedTabsRecyclerView;
    private final SimpleRecyclerViewAdapter mPinnedTabsAdapter;
    private final GridLayoutManager mPinnedLayoutManager;
    private final ListObservable.ListObserver<Void> mPinnedTabsListObserver;
    private final TabModelSelector mTabModelSelector;
    private final Profile mProfile;
    private final WindowAndroid mWindowAndroid;
    private final ActivityResultTracker mActivityResultTracker;
    private final MultiInstanceManager mMultiInstanceManager;
    private final SnackbarManager mSnackbarManager;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final Callback<TabModel> mCurrentTabModelObserver;
    // Create a mutable coordinate holder.
    private final Point mLastTouchPoint = new Point();
    private final MonotonicObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private final DataSharingTabManager mDataSharingTabManager;
    private final VerticalTabGroupSpineDecoration mSpineDecoration;
    private final VerticalTabDropIndicatorDecoration mDropIndicatorDecoration;
    private final VerticalTabPinnedDropIndicatorDecoration mPinnedDropIndicatorDecoration;
    private final TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private final NonNullObservableSupplier<Boolean> mVerticalTabsActiveSupplier;
    private final VerticalTabRailCollapseController mCollapseController;
    private final Callback<Boolean> mActiveObserver = this::setActive;
    private final PropertyModel mContainerModel;
    private final VerticalExternalViewDragDropReorderStrategy mReorderStrategy;
    private final List<TabSwitcherDragHandler> mTabSwitcherDragHandlers = new ArrayList<>();
    private final View.OnLayoutChangeListener mContainerLayoutChangeListener;
    private final View.OnLayoutChangeListener mPinnedTabsLayoutChangeListener;
    private final VerticalTabHoverCardController mTabHoverCardController;
    private final RecyclerView.OnScrollListener mOnScrollListener;
    private final VerticalTabKeyboardHandler mKeyboardHandler;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final @Nullable AppHeaderObserver mAppHeaderObserver;
    private final @Nullable BooleanSupplier mCanActivateTabLayoutToggleMenuSupplier;
    private final @Nullable UndoBarThrottle mUndoBarThrottle;
    private final @Nullable TabUnderlineManager mTabUnderlineManager;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final Supplier<TabContentManager> mTabContentManagerSupplier;
    private final List<VerticalTabListItemTouchHelperCallback> mTouchHelperCallbacks =
            new ArrayList<>();
    private @Nullable TabStripContextMenuCoordinator mTabStripContextMenuCoordinator;
    private @Nullable TabContextMenuCoordinator mTabContextMenuCoordinator;
    private @Nullable TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;
    private @Nullable VerticalTabListItemTouchHelperCallback mMainTouchHelperCallback;
    private @Nullable StripDragShadowView mDragShadowView;
    private @Nullable MultiThumbnailCardProvider mMultiThumbnailCardProvider;

    private boolean mIsActive;
    private boolean mIsInTransition;

    private class VerticalTabListClickHandler implements TabListItemOnClickListenerProvider {
        private final TabActionListener mTabGroupClickedListener =
                new TabActionListener() {
                    @Override
                    public void run(
                            View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                        toggleTabGroupExpansion(tabId);

                        int headerIndex = mMediator.getGroupHeaderIndexForTabId(tabId);
                        if (headerIndex == TabModel.INVALID_TAB_INDEX) {
                            return;
                        }
                        PropertyModel headerModel = mModelList.get(headerIndex).model;
                        if (headerModel.get(TabProperties.IS_COLLAPSED)) {
                            return;
                        }
                        // Scroll the header to the top only if the last child is off-screen.
                        // This brings child tabs into view while keeping the header anchored at the
                        // top for context.
                        Token groupId = headerModel.get(TabProperties.TAB_GROUP_HEADER_ID);
                        int childCount =
                                mTabModelSelector.getCurrentModel().getTabCountForGroup(groupId);
                        if (childCount > 0) {
                            int lastChildIndex = headerIndex + childCount;
                            mRecyclerView.post(
                                    () -> {
                                        RecyclerView.LayoutManager layoutManager =
                                                mRecyclerView.getLayoutManager();
                                        if (layoutManager instanceof LinearLayoutManager lm) {
                                            int lastVisible =
                                                    lm.findLastCompletelyVisibleItemPosition();
                                            if (lastChildIndex > lastVisible) {
                                                lm.scrollToPositionWithOffset(
                                                        headerIndex, /* offset= */ 0);
                                            }
                                        }
                                    });
                        }
                    }

                    @Override
                    public void run(
                            View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                        // Intentional no-op: Sync groups are not supported in Vertical Tabs.
                    }
                };

        @Override
        public @Nullable TabActionListener onTabGroupClicked(Tab tab) {
            return mTabGroupClickedListener;
        }

        @Override
        public @Nullable TabActionListener onTabGroupClicked(String syncId) {
            return null;
        }

        @Override
        public void onTabSelecting(int tabId) {
            TabModelUtils.selectTabById(mTabModelSelector, tabId, TabSelectionType.FROM_USER);
        }

        @Override
        public @Nullable Boolean isTabGroupSelected(Tab tab, PropertyModel model) {
            // In Vertical Tabs, the Group Header card acts strictly as an expandable accordion
            // header, and is never selectable (individual child webpage rows show active
            // highlights).
            return false;
        }

        @Override
        public @Nullable TabActionButtonData getTabGroupActionButtonData(
                Tab tab,
                PropertyModel model,
                Supplier<TabActionListener> defaultOverflowListenerSupplier) {
            Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId == null) {
                return null;
            }
            return new TabActionButtonData(
                    TabActionButtonType.OVERFLOW,
                    new TabActionListener() {
                        @Override
                        public void run(
                                View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                            RecordUserAction.record(
                                    "Android.VerticalTabs.GroupHeaderMenuButtonClicked");
                            showTabGroupHeaderContextMenu(
                                    getItemViewAnchorRectProvider(view), tabGroupId);
                        }

                        @Override
                        public void run(
                                View view,
                                String syncId,
                                @Nullable MotionEventInfo triggeringMotion) {
                            // Intentional no-op: Sync groups are not supported in Vertical Tabs.
                        }
                    });
        }
    }

    /**
     * Constructs a {@link VerticalTabListCoordinator}.
     *
     * @param activity The host activity.
     * @param tabModelSelector The selector for accessing tab models.
     * @param profile The current user profile.
     * @param verticalTabsActionDelegate Delegate for performing actions on tabs.
     * @param windowAndroid The window hosting the UI.
     * @param activityResultTracker Tracker for activity results.
     * @param multiInstanceManager Manager for multi-instance Chrome.
     * @param snackbarManager Manager for displaying snackbars.
     * @param desktopWindowStateManager Manager for desktop window state.
     * @param shareDelegateSupplier Supplier for the share delegate.
     * @param dataSharingTabManager Manager for collaborative tab group sharing.
     * @param verticalTabsActiveSupplier Supplier indicating if vertical tabs UI is active.
     * @param verticalTabsWidthSupplier Supplier for the width of the vertical tabs rail.
     * @param canActivateTabLayoutToggleMenuSupplier Supplier for whether layout toggle menu is
     *     enabled.
     * @param tabHoverCardViewStub ViewStub for inflating the tab hover card.
     * @param tabGroupHoverCardViewStub ViewStub for inflating the tab group hover card.
     * @param tabContentManagerSupplier Supplier for the tab content manager.
     * @param undoBarThrottle Throttler for undo bar messages.
     * @param browserControlsStateProvider Provider for browser controls sizing and state.
     */
    @SuppressLint("ClickableViewAccessibility")
    public VerticalTabListCoordinator(
            Activity activity,
            TabModelSelector tabModelSelector,
            Profile profile,
            VerticalTabsActionDelegate verticalTabsActionDelegate,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            MultiInstanceManager multiInstanceManager,
            SnackbarManager snackbarManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            DataSharingTabManager dataSharingTabManager,
            NonNullObservableSupplier<Boolean> verticalTabsActiveSupplier,
            SettableNonNullObservableSupplier<Integer> verticalTabsWidthSupplier,
            @Nullable BooleanSupplier canActivateTabLayoutToggleMenuSupplier,
            @Nullable ViewStub tabHoverCardViewStub,
            @Nullable ViewStub tabGroupHoverCardViewStub,
            Supplier<TabContentManager> tabContentManagerSupplier,
            @Nullable UndoBarThrottle undoBarThrottle,
            BrowserControlsStateProvider browserControlsStateProvider) {
        mCanActivateTabLayoutToggleMenuSupplier = canActivateTabLayoutToggleMenuSupplier;
        mVerticalTabsActiveSupplier = verticalTabsActiveSupplier;
        mTabModelSelector = tabModelSelector;
        mProfile = profile;
        mWindowAndroid = windowAndroid;
        mActivityResultTracker = activityResultTracker;
        mMultiInstanceManager = multiInstanceManager;
        mSnackbarManager = snackbarManager;
        mShareDelegateSupplier = shareDelegateSupplier;
        mDataSharingTabManager = dataSharingTabManager;
        mUndoBarThrottle = undoBarThrottle;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTabContentManagerSupplier = tabContentManagerSupplier;
        if (GlicEnabling.isEnabledByFlags() || ContextualTasksUtils.isContextualTasksUiEnabled()) {
            mTabUnderlineManager = new TabUnderlineManager(windowAndroid);
        } else {
            mTabUnderlineManager = null;
        }
        mCollapseController = new VerticalTabRailCollapseController(this::setRailCollapseState);
        mModelList = new TabListModel();
        SimpleRecyclerViewAdapter adapter =
                new SimpleRecyclerViewAdapter(mModelList) {
                    @Override
                    public int getItemViewType(int position) {
                        ListItem item = mModelList.get(position);
                        if (item.type == UiType.TAB) {
                            if (TabProperties.isPinnedTab(item.model)) {
                                return UiType.PINNED_TAB;
                            } else if (TabProperties.isTabGroupHeader(item.model)) {
                                return UiType.TAB_GROUP;
                            }
                        }
                        return super.getItemViewType(position);
                    }
                };

        adapter.registerType(
                UiType.TAB,
                parent ->
                        (ViewGroup)
                                LayoutInflater.from(activity)
                                        .inflate(
                                                R.layout.vertical_tab_item,
                                                parent,
                                                /* attachToRoot= */ false),
                TabVerticalViewBinder::bindTab);

        // Pinned tabs are rendered in a separate sticky layout. This zero-height hidden layout in
        // the main list preserves the 1:1 index alignment with the TabModel without taking space.
        adapter.registerType(
                UiType.PINNED_TAB,
                parent ->
                        (ViewGroup)
                                LayoutInflater.from(activity)
                                        .inflate(
                                                R.layout.vertical_tab_pinned_item_hidden,
                                                parent,
                                                /* attachToRoot= */ false),
                TabVerticalViewBinder::bindPinnedTab);

        adapter.registerType(
                UiType.TAB_GROUP,
                parent ->
                        (ViewGroup)
                                LayoutInflater.from(activity)
                                        .inflate(
                                                R.layout.vertical_tab_group_header,
                                                parent,
                                                /* attachToRoot= */ false),
                TabVerticalViewBinder::bindTabGroupHeader);

        mContainerView =
                (VerticalTabRailLayout)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.vertical_tab_layout,
                                        null,
                                        /* attachToRoot= */ false);
        mContainerView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        @SuppressWarnings("NullAway")
        Supplier<@Nullable TabContentManager> nullableSupplier = tabContentManagerSupplier;
        mTabHoverCardController =
                new VerticalTabHoverCardController(
                        mContainerView,
                        tabHoverCardViewStub,
                        tabGroupHoverCardViewStub,
                        tabModelSelector,
                        nullableSupplier,
                        this::isAnyContextMenuShowing);

        VerticalTabListRecyclerView recyclerView = mContainerView.getRecyclerView();
        mRecyclerView = recyclerView;
        mRecyclerView.initialize(adapter);
        mOnScrollListener =
                new RecyclerView.OnScrollListener() {
                    @Override
                    public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                        if (newState != RecyclerView.SCROLL_STATE_IDLE) {
                            mTabHoverCardController.hideHoverCard();
                        }
                        if (newState == RecyclerView.SCROLL_STATE_DRAGGING) {
                            dismissActiveContextMenus();
                        }
                    }
                };
        recyclerView.addOnScrollListener(mOnScrollListener);

        // Create the gesture detector to catch long-presses on VT empty space for the tab list
        // recycler view.
        GestureDetector gestureDetector =
                new GestureDetector(
                        activity,
                        createEmptySpaceGestureListener(activity, recyclerView, recyclerView));

        // Item Touch Listeners intercept all incoming window events before they are sent down to
        // the child views.
        recyclerView.addOnItemTouchListener(
                createRecyclerViewItemTouchListener(activity, gestureDetector));

        // Handles right-click for empty space context menu on tab_list_recycler_view.
        recyclerView.setOnContextClickListener(
                createEmptySpaceContextClickListener(activity, recyclerView));

        // Set up empty space context menu handlers for the header container.
        View headerContainer = mContainerView.getHeaderContainer();
        if (headerContainer != null) {
            GestureDetector headerGestureDetector =
                    new GestureDetector(
                            activity,
                            createEmptySpaceGestureListener(
                                    activity, headerContainer, mRecyclerView));

            headerContainer.setOnTouchListener(
                    (v, event) -> {
                        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                            mLastTouchPoint.set((int) event.getX(), (int) event.getY());
                        }
                        headerGestureDetector.onTouchEvent(event);
                        return false;
                    });

            headerContainer.setOnContextClickListener(
                    v -> {
                        // Handles right-click for empty space context menu on
                        // vertical_tab_header_container.
                        return createEmptySpaceContextClickListener(activity, headerContainer)
                                .onContextClick(v);
                    });
        }

        View tabSearchButton = mContainerView.findViewById(R.id.tab_search_button);
        if (tabSearchButton != null) {
            tabSearchButton.setOnTouchListener(createLocalCoordinateTrackingTouchListener());
            tabSearchButton.setOnContextClickListener(
                    createEmptySpaceContextClickListener(activity, tabSearchButton));
        }

        // Create a gesture detector to catch long-presses on any raw root container background
        // space.
        GestureDetector rootSpaceGestureDetector =
                new GestureDetector(
                        activity,
                        createEmptySpaceGestureListener(activity, mContainerView, mRecyclerView));

        mContainerView.setOnTouchListener(
                (v, event) -> {
                    if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                        mLastTouchPoint.set((int) event.getX(), (int) event.getY());
                    }
                    rootSpaceGestureDetector.onTouchEvent(event);
                    // Return false so interactive child views still get touched.
                    return false;
                });

        // Handle right-clicks on the root background space and fully consume the window event.
        mContainerView.setOnContextClickListener(
                createEmptySpaceContextClickListener(activity, mContainerView));

        mContainerLayoutChangeListener =
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    int width = right - left;
                    boolean isVisible = mContainerView.getVisibility() == View.VISIBLE;
                    verticalTabsWidthSupplier.set(isVisible ? width : 0);
                    if (isVisible && width > 0 && width != (oldRight - oldLeft)) {
                        updatePinnedLayoutSpanCount();
                    }
                };
        mContainerView.addOnLayoutChangeListener(mContainerLayoutChangeListener);

        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        activity,
                        TabListMode.VERTICAL,
                        R.dimen.default_favicon_corner_radius,
                        TabFavicon::getBitmap);

        setupItemTouchHelper(activity, recyclerView, mModelList, tabModelSelector);

        mSpineDecoration =
                new VerticalTabGroupSpineDecoration(
                        activity, recyclerView::postInvalidate, mModelList, tabModelSelector);
        recyclerView.addItemDecoration(mSpineDecoration);

        mDropIndicatorDecoration = new VerticalTabDropIndicatorDecoration(activity);
        recyclerView.addItemDecoration(mDropIndicatorDecoration);

        TabListConfig tabListConfig =
                new TabListConfig.Builder(TabListLayoutType.NESTED)
                        .setSupportsModifierMultiSelect(/* supportsModifierMultiSelect= */ true)
                        .setSupportsTabLoadingState(/* supportsTabLoadingState= */ true)
                        .setTabClosingSource(TabClosingSource.VERTICAL_TAB_STRIP)
                        .setRailCollapseStateSupplier(
                                mCollapseController.getRailCollapseStateSupplier())
                        .setTabHoverCardListener(mTabHoverCardController.getTabHoverCardListener())
                        .setTabUnderlineManager(mTabUnderlineManager)
                        .build();

        mContainerModel =
                new PropertyModel.Builder(VerticalTabListProperties.ALL_KEYS)
                        .with(
                                VerticalTabListProperties.ON_SEARCH_CLICK_LISTENER,
                                v -> {
                                    RecordUserAction.record(
                                            "Android.VerticalTabs.SearchButtonClicked");
                                    if (ChromeFeatureList.sTabSearchForDesktop.isEnabled()) {
                                        verticalTabsActionDelegate.openTabSearch();
                                    } else {
                                        verticalTabsActionDelegate.openHubSearch();
                                    }
                                })
                        .with(
                                VerticalTabListProperties.ON_NEW_TAB_CLICK_LISTENER,
                                v -> handleNewTabButtonClick())
                        .with(
                                VerticalTabListProperties.ON_INCOGNITO_CLICK_LISTENER,
                                v -> handleIncognitoButtonClick())
                        .with(
                                VerticalTabListProperties.IS_INCOGNITO_BUTTON_VISIBLE,
                                isIncognitoButtonVisible())
                        .with(
                                VerticalTabListProperties.ON_COLLAPSE_CLICK_LISTENER,
                                v -> mCollapseController.toggleCollapseState())
                        .with(
                                VerticalTabListProperties.EXPAND_OR_COLLAPSE_ON_HOVER_LISTENER,
                                mCollapseController::expandOrCollapseOnHover)
                        .with(
                                VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED,
                                mCollapseController.isCollapseButtonEnabled())
                        .with(
                                VerticalTabListProperties.COLLAPSE_STATE,
                                mCollapseController.getRailCollapseStateByUser())
                        .build();
        PropertyModelChangeProcessor.create(
                mContainerModel, mContainerView, VerticalTabListViewBinder::bind);

        mMediator =
                new TabListMediator(
                        activity,
                        mModelList,
                        /* modalDialogManager */ null,
                        tabModelSelector.getCurrentTabModelSupplier(),
                        /* thumbnailProvider */ null,
                        mTabListFaviconProvider,
                        /* selectionDelegateProvider */ null,
                        new VerticalTabListClickHandler(),
                        tabListConfig,
                        /* ungroupBarStatusHandler= */ null,
                        /* priceWelcomeMessageControllerSupplier */ null,
                        TabComponentId.VERTICAL_TABS,
                        TabProperties.TabActionState.CLOSABLE,
                        /* dataSharingTabManager */ null,
                        /* onTabGroupCreation */ null,
                        /* undoBarExplicitTrigger */ null,
                        /* snackbarManager */ null,
                        TabListEditorCoordinator.UNLIMITED_SELECTION,
                        /* isSingleContextMode */ false,
                        /* onDragStateChangedListener */ CallbackUtils.emptyRunnable());

        mMediator.initWithNative(profile.getOriginalProfile());
        mMediator.setupAccessibilityDelegate(mRecyclerView);
        mMediator.setOnLongPressTabItemEventListener(
                (tabId, cardView) -> {
                    if (cardView != null) {
                        showMenuForItemView(getItemViewAnchorRectProvider(cardView), cardView);
                    }
                    return this::dismissActiveContextMenus;
                });

        // Setup Pinned Tabs UI & Mediator.
        TabListRecyclerView pinnedTabsRecyclerView = mContainerView.getPinnedTabsRecyclerView();
        mPinnedTabsRecyclerView = pinnedTabsRecyclerView;
        TabListModel pinnedTabsModelList = new TabListModel();
        mPinnedTabsModelList = pinnedTabsModelList;
        SimpleRecyclerViewAdapter pinnedTabsAdapter =
                new SimpleRecyclerViewAdapter(pinnedTabsModelList) {
                    @Override
                    public int getItemViewType(int position) {
                        ListItem item = pinnedTabsModelList.get(position);
                        if (item.type == UiType.TAB) {
                            return UiType.PINNED_TAB;
                        }
                        return super.getItemViewType(position);
                    }
                };

        pinnedTabsAdapter.registerType(
                UiType.PINNED_TAB,
                parent ->
                        (ViewGroup)
                                LayoutInflater.from(activity)
                                        .inflate(
                                                R.layout.vertical_tab_pinned_item,
                                                parent,
                                                /* attachToRoot= */ false),
                TabVerticalViewBinder::bindPinnedTab);

        pinnedTabsRecyclerView.setAdapter(pinnedTabsAdapter);
        mPinnedTabsAdapter = pinnedTabsAdapter;
        pinnedTabsRecyclerView.setupCustomItemAnimator(/* useClipAnimations= */ true);
        // TODO(crbug.com/509226293): Move pinned tab RecyclerView and LayoutManager into a
        // dedicated class (mirroring VerticalTabListRecyclerView) to encapsulate layout and extra
        // space logic.
        mPinnedLayoutManager =
                new GridLayoutManager(activity, getSpanCount()) {
                    @Override
                    protected void calculateExtraLayoutSpace(
                            RecyclerView.State state, int[] extraLayoutSpace) {
                        super.calculateExtraLayoutSpace(state, extraLayoutSpace);
                        calculatePinnedExtraLayoutSpace(activity, state, extraLayoutSpace);
                    }
                };
        pinnedTabsRecyclerView.setLayoutManager(mPinnedLayoutManager);
        pinnedTabsRecyclerView.addItemDecoration(createPinnedTabItemDecoration());

        mPinnedDropIndicatorDecoration = new VerticalTabPinnedDropIndicatorDecoration(activity);
        pinnedTabsRecyclerView.addItemDecoration(mPinnedDropIndicatorDecoration);

        mPinnedTabsLayoutChangeListener =
                (_, left, _, right, _, oldLeft, _, oldRight, _) -> {
                    int width = right - left;
                    if (width > 0 && width != (oldRight - oldLeft)) {
                        updatePinnedLayoutSpanCount();
                    }
                };
        pinnedTabsRecyclerView.addOnLayoutChangeListener(mPinnedTabsLayoutChangeListener);

        // Create a gesture detector to catch long-presses on the pinned tabs rv empty background
        // area.
        GestureDetector pinnedSpaceGestureDetector =
                new GestureDetector(
                        activity,
                        createEmptySpaceGestureListener(
                                activity, pinnedTabsRecyclerView, pinnedTabsRecyclerView));

        pinnedTabsRecyclerView.addOnItemTouchListener(
                createRecyclerViewItemTouchListener(activity, pinnedSpaceGestureDetector));

        // While it is possible to handle recycler view right-clicks using
        // ItemTouchListener#onInterceptTouch, onContextClickListeners are needed to avoid the
        // "page" context menu from showing when a web page is open behind the vt.
        pinnedTabsRecyclerView.setOnContextClickListener(
                createEmptySpaceContextClickListener(activity, pinnedTabsRecyclerView));

        // TODO(crbug.com/509226293): Create a lightweight touch helper for pinned tabs if needed.
        // Setup drag-and-drop reordering. Reuses the vertical tab touch helper since pinned tabs
        // can be reordered in 2D, and movements translate directly back to TabModel moves.
        setupItemTouchHelper(
                activity, pinnedTabsRecyclerView, pinnedTabsModelList, tabModelSelector);

        mPinnedTabsListObserver =
                new ListObservable.ListObserver<>() {
                    @Override
                    public void onItemRangeInserted(ListObservable source, int index, int count) {
                        updatePinnedLayoutSpanCount();
                    }

                    @Override
                    public void onItemRangeRemoved(ListObservable source, int index, int count) {
                        updatePinnedLayoutSpanCount();
                    }

                    @Override
                    public void onItemRangeChanged(
                            ListObservable source, int index, int count, @Nullable Void payload) {}

                    @Override
                    public void onItemMoved(ListObservable source, int curIndex, int newIndex) {
                        mPinnedTabsRecyclerView.invalidateItemDecorations();
                    }
                };
        pinnedTabsModelList.addObserver(mPinnedTabsListObserver);
        mReorderStrategy =
                new VerticalExternalViewDragDropReorderStrategy(
                        mTabModelSelector::getCurrentModel,
                        mModelList,
                        mRecyclerView,
                        pinnedTabsRecyclerView);

        mPinnedTabsMediator =
                new StaticPinnedTabsMediator(
                        tabModelSelector.getCurrentModel(),
                        mModelList,
                        pinnedTabsModelList,
                        this::updatePinnedTabsVisibility);
        updatePinnedTabsVisibility();

        mKeyboardHandler =
                new VerticalTabKeyboardHandler(
                        tabModelSelector,
                        mModelList,
                        pinnedTabsModelList,
                        mRecyclerView,
                        pinnedTabsRecyclerView,
                        mTabHoverCardController);
        mContainerView.setKeyEventListener(mKeyboardHandler);

        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabStateInitialized() {
                        resetWithListOfTabs(mTabModelSelector.getCurrentModel());
                        updateIncognitoButtonVisibility();
                    }
                };
        tabModelSelector.addObserver(mTabModelSelectorObserver);

        mCurrentTabModelObserver = this::onCurrentTabModelChanged;
        tabModelSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndCallIfNonNull(mCurrentTabModelObserver);

        mDesktopWindowStateManager = desktopWindowStateManager;
        if (mDesktopWindowStateManager != null) {
            mAppHeaderObserver =
                    new AppHeaderObserver() {
                        @Override
                        public void onAppHeaderStateChanged(AppHeaderState newState) {
                            updateSpacerVisibility(newState);
                        }

                        @Override
                        public void onDesktopWindowingModeChanged(boolean isInDesktopWindow) {
                            updateSpacerVisibility(mDesktopWindowStateManager.getAppHeaderState());
                        }
                    };
            mDesktopWindowStateManager.addObserver(mAppHeaderObserver);
            updateSpacerVisibility(mDesktopWindowStateManager.getAppHeaderState());
        } else {
            mAppHeaderObserver = null;
        }

        // Auto-scroll to keep the newly selected tab visible when tab selection changes externally
        // while the vertical rail is open.
        mTabModelSelectorTabModelObserver =
                new TabModelSelectorTabModelObserver(tabModelSelector) {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if (mIsActive && type != TabSelectionType.FROM_DRAG) {
                            scrollActiveTabIntoView();
                        }
                        mTabHoverCardController.hideHoverCard();
                    }

                    @Override
                    public void didChangePinState(Tab tab) {
                        // Scroll the newly unpinned active tab into view.
                        if (!tab.getIsPinned()
                                && tab.getId() == mTabModelSelector.getCurrentTabId()) {
                            mRecyclerView.post(() -> scrollActiveTabIntoView());
                        }
                    }

                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        updateIncognitoButtonVisibility();
                    }

                    @Override
                    public void tabRemoved(Tab tab) {
                        updateIncognitoButtonVisibility();
                    }

                    @Override
                    public void didRemoveTabForClosure(Tab tab) {
                        updateIncognitoButtonVisibility();
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        mTabHoverCardController.hideHoverCard();
                    }

                    @Override
                    public void willCloseTabs(
                            List<Tab> tabs, boolean isAllTabs, boolean allowUndo) {
                        mTabHoverCardController.hideHoverCard();
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        mTabHoverCardController.hideHoverCard();
                    }

                    @Override
                    public void willAddTab(Tab tab, @TabLaunchType int type) {
                        mTabHoverCardController.hideHoverCard();
                    }
                };

        mVerticalTabsActiveSupplier.addSyncObserverAndCallIfNonNull(mActiveObserver);

        // Context menus should not appear upon right-clicking the new tab button or the collapse
        // button.
        View newTabButton = mContainerView.findViewById(R.id.new_tab_button);
        if (newTabButton != null) {
            newTabButton.setOnContextClickListener(v -> true);
        }

        View collapseButton = mContainerView.findViewById(R.id.collapse_button);
        if (collapseButton != null) {
            collapseButton.setOnContextClickListener(v -> true);
        }
    }

    /** Returns the root ViewGroup container representing the Left Rail sidebar. */
    public View getView() {
        return mContainerView;
    }

    /** Requests keyboard focus on the first tab in the rail. */
    public void requestKeyboardFocus() {
        if (!mPinnedTabsModelList.isEmpty()) {
            if (KeyboardFocusUtil.setFocusOnFirstFocusableDescendant(mPinnedTabsRecyclerView)) {
                return;
            }
            requestFocusAtFirstItem(mPinnedTabsRecyclerView);
            return;
        }

        if (!mModelList.isEmpty()) {
            RecyclerView.ViewHolder holder = mRecyclerView.findViewHolderForAdapterPosition(0);
            if (holder != null && KeyboardFocusUtil.setFocus(holder.itemView)) {
                return;
            }
            mRecyclerView.scrollToPositionWithOffset(0);
            requestFocusAtFirstItem(mRecyclerView);
            return;
        }

        View collapseButton = mContainerView.findViewById(R.id.collapse_button);
        if (collapseButton != null && KeyboardFocusUtil.setFocus(collapseButton)) {
            return;
        }
        KeyboardFocusUtil.setFocusOnFirstFocusableDescendant(mContainerView);
    }

    public void destroy() {
        mContainerView.setKeyEventListener(null);
        mPinnedTabsModelList.removeObserver(mPinnedTabsListObserver);
        mPinnedTabsMediator.destroy();
        mPinnedTabsRecyclerView.setAdapter(null);
        mPinnedTabsModelList.clear();
        mMediator.destroy();
        mRecyclerView.setAdapter(null);
        mModelList.clear();
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        mTabListFaviconProvider.destroy();

        if (mTabStripContextMenuCoordinator != null) {
            mTabStripContextMenuCoordinator.destroy();
            mTabStripContextMenuCoordinator = null;
        }

        if (mDesktopWindowStateManager != null && mAppHeaderObserver != null) {
            mDesktopWindowStateManager.removeObserver(mAppHeaderObserver);
        }

        if (mTabContextMenuCoordinator != null) {
            mTabContextMenuCoordinator.dismiss();
            mTabContextMenuCoordinator = null;
        }

        if (mTabGroupContextMenuCoordinator != null) {
            mTabGroupContextMenuCoordinator.destroy();
            mTabGroupContextMenuCoordinator = null;
        }

        mSpineDecoration.destroy();
        mTabModelSelectorTabModelObserver.destroy();
        mVerticalTabsActiveSupplier.removeObserver(mActiveObserver);
        for (TabSwitcherDragHandler dragHandler : mTabSwitcherDragHandlers) {
            dragHandler.destroy();
        }
        mTabSwitcherDragHandlers.clear();

        mTabHoverCardController.destroy();

        mContainerView.removeOnLayoutChangeListener(mContainerLayoutChangeListener);
        mPinnedTabsRecyclerView.removeOnLayoutChangeListener(mPinnedTabsLayoutChangeListener);
        mRecyclerView.removeOnScrollListener(mOnScrollListener);

        mCollapseController.destroy();
        if (mTabUnderlineManager != null) {
            mTabUnderlineManager.destroy();
        }
        for (VerticalTabListItemTouchHelperCallback callback : mTouchHelperCallbacks) {
            callback.cancelDelayedExternalItemRestoration();
        }
        mTouchHelperCallbacks.clear();
        mReorderStrategy.clear();
        mDropIndicatorDecoration.clear();
        mPinnedDropIndicatorDecoration.clear();
        if (mDragShadowView != null) {
            mDragShadowView.clear();
            ViewGroup parent = (ViewGroup) mDragShadowView.getParent();
            if (parent != null) {
                parent.removeView(mDragShadowView);
            }
            mDragShadowView = null;
        }
        if (mMultiThumbnailCardProvider != null) {
            mMultiThumbnailCardProvider.destroy();
            mMultiThumbnailCardProvider = null;
        }
    }

    public VerticalExternalViewDragDropReorderStrategy getReorderStrategyForTesting() {
        return mReorderStrategy;
    }

    public VerticalTabDropIndicatorDecoration getDropIndicatorDecorationForTesting() {
        return mDropIndicatorDecoration;
    }

    public VerticalTabPinnedDropIndicatorDecoration getPinnedDropIndicatorDecorationForTesting() {
        return mPinnedDropIndicatorDecoration;
    }

    /** Returns the {@link VerticalTabRailCollapseController} for managing rail collapse state. */
    VerticalTabRailCollapseController getCollapseController() {
        return mCollapseController;
    }

    /**
     * Sets the collapsed state of the vertical tab rail.
     *
     * <p>This updates the model properties and layouts for the rail container and all tab items to
     * transition between the expanded (icons + text) and collapsed (icons only) states.
     *
     * @param railCollapseState The {@link RailCollapseState} to apply to the rail.
     */
    void setRailCollapseState(@RailCollapseState int railCollapseState) {
        if (mTabHoverCardController != null) {
            mTabHoverCardController.hideHoverCard();
        }
        mContainerModel.set(VerticalTabListProperties.COLLAPSE_STATE, railCollapseState);
        updatePinnedLayoutSpanCount();
        mCollapseController.setRailCollapseStateSupplierValue(railCollapseState);
    }

    /**
     * Sets whether the rail collapse button is enabled.
     *
     * @param enabled True if the collapse button should be enabled, false otherwise.
     */
    void setCollapseButtonEnabled(boolean enabled) {
        mContainerModel.set(VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED, enabled);
        mCollapseController.setCollapseButtonEnabled(enabled);
    }

    /**
     * Sets whether an animated rail collapse/expand transition is in progress.
     *
     * @param inTransition True if the rail is actively transitioning.
     */
    void setInTransition(boolean inTransition) {
        if (mIsInTransition == inTransition) return;
        mIsInTransition = inTransition;
        // Request layout after transition ends to immediately recycle extra items back
        // to viewport bounds (transition start is already laid out by the container width change).
        if (!mIsInTransition) {
            ViewUtils.requestLayout(
                    mPinnedTabsRecyclerView, "VerticalTabListCoordinator.setInTransition");
        }
    }

    /**
     * Opens the context menu for the currently keyboard-focused tab item or group header, if any.
     *
     * @return Whether the context menu was successfully opened.
     */
    boolean openKeyboardFocusedContextMenu() {
        if (mRecyclerView.hasFocus()) {
            return openContextMenuForFocusedItem(mRecyclerView);
        }
        if (mPinnedTabsRecyclerView.hasFocus()) {
            return openContextMenuForFocusedItem(mPinnedTabsRecyclerView);
        }
        return false;
    }

    /**
     * Toggles the expanded/collapsed visual and layout state of a tab group.
     *
     * @param tabId the ID of the representative tab representing the tab group.
     */
    @VisibleForTesting
    void toggleTabGroupExpansion(int tabId) {
        mMediator.toggleTabGroupExpansion(tabId);
    }

    /**
     * Calculates and applies extra layout space for pinned tabs during transitions so that
     * boundary/trailing pinned tabs remain attached for ChangeBounds transitions.
     */
    @VisibleForTesting
    void calculatePinnedExtraLayoutSpace(
            Activity activity, RecyclerView.State state, int[] extraLayoutSpace) {
        if (!mIsInTransition) return;
        int height =
                Math.max(
                        mContainerView.getHeight(),
                        mContainerView.getResources().getDisplayMetrics().heightPixels);
        int itemCount = state.getItemCount();
        if (itemCount > 0) {
            int itemHeight =
                    TabVerticalViewBinder.getPinnedItemHeight(activity)
                            + activity.getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.vertical_tab_pinned_item_margin_bottom);
            int padding =
                    mPinnedTabsRecyclerView.getPaddingTop()
                            + mPinnedTabsRecyclerView.getPaddingBottom();
            int totalContentHeight = itemCount * itemHeight + padding;
            // Cap to the maximum items that can physically fit in the expanded
            // viewport across all columns (at most MAX_SINGLE_ROW_SPAN_COUNT = 5),
            // avoiding layout overhead for items that remain offscreen.
            int maxExpandedHeight = height * MAX_SINGLE_ROW_SPAN_COUNT + padding;
            height = Math.clamp(totalContentHeight, height, maxExpandedHeight);
        }
        extraLayoutSpace[0] = Math.max(extraLayoutSpace[0], height);
        extraLayoutSpace[1] = Math.max(extraLayoutSpace[1], height);
    }

    private void setActive(boolean isActive) {
        mIsActive = isActive;
        if (mIsActive) {
            scrollActiveTabIntoView();
        } else {
            mTabHoverCardController.hideHoverCard();
        }
    }

    private void scrollActiveTabIntoView() {
        int activeTabId = mTabModelSelector.getCurrentTabId();
        if (activeTabId == Tab.INVALID_TAB_ID) return;

        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (tabModel == null) return;
        Tab activeTab = tabModel.getTabById(activeTabId);
        if (activeTab == null || activeTab.getIsPinned()) return;

        int uiIndex = getIndexForTabScroll(activeTabId);

        if (uiIndex != TabModel.INVALID_TAB_INDEX) {
            RecyclerView.LayoutManager layoutManager = mRecyclerView.getLayoutManager();
            if (layoutManager instanceof LinearLayoutManager lm) {
                int firstVisible = lm.findFirstCompletelyVisibleItemPosition();
                int lastVisible = lm.findLastCompletelyVisibleItemPosition();
                if (firstVisible != RecyclerView.NO_POSITION
                        && lastVisible != RecyclerView.NO_POSITION
                        && uiIndex >= firstVisible
                        && uiIndex <= lastVisible) {
                    return;
                }
            }
            mRecyclerView.scrollToPositionWithOffset(uiIndex);
        }
    }

    /**
     * Resolves the UI index of a given tab for auto-scrolling. If the tab is hidden inside a
     * collapsed group, it returns the index of the group header instead.
     *
     * @param tabId The ID of the tab to find.
     * @return The UI index in mModelList, or {@link TabModel#INVALID_TAB_INDEX} if not found.
     */
    private int getIndexForTabScroll(int tabId) {
        int uiIndex = mModelList.indexFromTabId(tabId);

        // If the tab is hidden inside a collapsed group, find the group header's index instead.
        if (uiIndex == TabModel.INVALID_TAB_INDEX) {
            uiIndex = mMediator.getGroupHeaderIndexForTabId(tabId);
        }
        return uiIndex;
    }

    /**
     * Attempts to request keyboard focus on the first item view in {@code recyclerView}. If the
     * view holder is not yet attached, posts a fallback request to the message queue to wait for
     * layout completion.
     */
    private void requestFocusAtFirstItem(RecyclerView recyclerView) {
        RecyclerView.ViewHolder holder = recyclerView.findViewHolderForAdapterPosition(0);
        if (holder != null && KeyboardFocusUtil.setFocus(holder.itemView)) {
            return;
        }
        recyclerView.post(
                () -> {
                    RecyclerView.ViewHolder postHolder =
                            recyclerView.findViewHolderForAdapterPosition(0);
                    if (postHolder != null && KeyboardFocusUtil.setFocus(postHolder.itemView)) {
                        return;
                    }
                    if (!KeyboardFocusUtil.setFocusOnFirstFocusableDescendant(recyclerView)) {
                        KeyboardFocusUtil.setFocusOnFirstFocusableDescendant(mContainerView);
                    }
                });
    }

    private void onCurrentTabModelChanged(TabModel tabModel) {
        if (mTabModelSelector.isTabStateInitialized()) {
            resetWithListOfTabs(tabModel);
        }
    }

    /**
     * Resets the vertical tab list and container models with tabs from the given tab model. Updates
     * incognito container styling when running in a shared activity window.
     */
    private void resetWithListOfTabs(@Nullable TabModel tabModel) {
        if (tabModel == null) return;

        mMediator.resetWithListOfTabs(
                tabModel.getRepresentativeTabList(),
                /* tabGroupSyncIds */ null,
                /* quickMode */ false);
        mPinnedTabsMediator.updateTabModel(tabModel);
        boolean isIncognito =
                !IncognitoUtils.shouldOpenIncognitoAsWindow() && tabModel.isIncognitoBranded();
        mContainerModel.set(VerticalTabListProperties.IS_INCOGNITO, isIncognito);
        updateIncognitoButtonVisibility();
    }

    private void handleNewTabButtonClick() {
        TabModel model = mTabModelSelector.getCurrentModel();

        if (!model.isIncognitoBranded()) model.commitAllTabClosures();
        TabCreatorUtil.launchNtp(model.getTabCreator());
        RecordUserAction.record("MobileNewTabOpened.VerticalTabs");
    }

    /** Switches between standard and incognito tab models when the incognito button is clicked. */
    private void handleIncognitoButtonClick() {
        mTabModelSelector.selectModel(!mTabModelSelector.isIncognitoSelected());
        RecordUserAction.record("MobileToolbarModelSelected");
    }

    private boolean isIncognitoButtonVisible() {
        if (!VerticalTabUtils.isIncognitoButtonEnabled()
                || IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            return false;
        }
        TabModel incognitoModel = mTabModelSelector.getModel(/* incognito= */ true);
        boolean hasIncognitoTabs = incognitoModel != null && incognitoModel.getCount() > 0;
        return IncognitoUtils.isIncognitoModeEnabled(mProfile) && hasIncognitoTabs;
    }

    private void updateIncognitoButtonVisibility() {
        if (mContainerModel != null) {
            mContainerModel.set(
                    VerticalTabListProperties.IS_INCOGNITO_BUTTON_VISIBLE,
                    isIncognitoButtonVisible());
        }
    }

    private void updatePinnedTabsVisibility() {
        boolean isEmpty = mPinnedTabsModelList.isEmpty();
        if (isEmpty) {
            if (mPinnedTabsRecyclerView.getVisibility() != View.GONE) {
                mPinnedTabsRecyclerView.setVisibility(View.GONE);
                mPinnedTabsRecyclerView.swapAdapter(
                        mPinnedTabsAdapter, /* removeAndRecycleExistingViews= */ true);
            }
        } else {
            mPinnedTabsRecyclerView.setVisibility(View.VISIBLE);
        }
        updatePinnedLayoutSpanCount();
    }

    private void setupItemTouchHelper(
            Activity activity,
            RecyclerView recyclerView,
            TabListModel modelList,
            TabModelSelector tabModelSelector) {
        VerticalTabListItemTouchHelperCallback touchHelperCallback =
                new VerticalTabListItemTouchHelperCallback(
                        activity,
                        modelList,
                        tabModelSelector.getCurrentTabModelSupplier().asNonNull(),
                        mUndoBarThrottle);
        if (mMainTouchHelperCallback == null) {
            mMainTouchHelperCallback = touchHelperCallback;
        }
        mTouchHelperCallbacks.add(touchHelperCallback);

        // Handles long-presses for tab item/group context menus. Long-presses for empty space
        // context menus are handled by the gesture detector.
        touchHelperCallback.setOnLongPressTabItemEventListener(
                (tabId, cardView) -> {
                    // Drop the incorrect cardView sent by the orchestrator timer thread.
                    // Find the true, exact child view underneath the physical touch coordinates.
                    View trueChildView =
                            recyclerView.findChildViewUnder(mLastTouchPoint.x, mLastTouchPoint.y);
                    if (trueChildView == null) return null;

                    int position = recyclerView.getChildAdapterPosition(trueChildView);
                    showMenuForAdapterPosition(
                            calculateTouchAnchor(
                                    recyclerView, mLastTouchPoint.x, mLastTouchPoint.y),
                            activity,
                            recyclerView,
                            position);

                    return this::dismissActiveContextMenus;
                });
        touchHelperCallback.setOnDragStartCallback(this::dismissActiveContextMenus);

        recyclerView.addOnItemTouchListener(
                VerticalTabListItemTouchHelperCallback.createBeforeOnItemTouchListener(
                        touchHelperCallback));

        ItemTouchHelper2 itemTouchHelper =
                new ItemTouchHelper2(touchHelperCallback, /* externalLongPressHandler= */ null);

        recyclerView.addOnItemTouchListener(
                touchHelperCallback.createMouseDragDetector(itemTouchHelper));

        TabSwitcherDragHandler dragHandler =
                createTabSwitcherDragHandler(activity, tabModelSelector);
        DragHandlerDelegate nonOriginatingDelegate =
                createNonOriginatingDragHandlerDelegate(recyclerView, dragHandler);
        dragHandler.setDragHandlerDelegate(nonOriginatingDelegate);
        recyclerView.setOnDragListener(dragHandler);
        if (recyclerView == mRecyclerView) {
            mContainerView.setOnDragListener(dragHandler);
            View newTabButton = mContainerView.findViewById(R.id.new_tab_button);
            if (newTabButton != null) {
                newTabButton.setOnDragListener(dragHandler);
            }
        }

        touchHelperCallback.setOnDragOutListener(
                (viewHolder, dX, dY) -> {
                    if (dragHandler.isViewDraggingInProgress()) {
                        return;
                    }

                    if (!(viewHolder
                            instanceof SimpleRecyclerViewAdapter.ViewHolder simpleViewHolder)) {
                        return;
                    }

                    PropertyModel model = simpleViewHolder.model;
                    if (model == null) return;

                    TabModel tabModel = tabModelSelector.getCurrentModel();
                    if (tabModel == null) return;

                    boolean isGroupHeader = TabProperties.isTabGroupHeader(model);
                    @Nullable Tab tab = null;
                    @Nullable Token tabGroupId = null;
                    @Nullable Tab firstGroupTab = null;
                    if (isGroupHeader) {
                        tabGroupId = assumeNonNull(model.get(TabProperties.TAB_GROUP_HEADER_ID));
                        List<Tab> groupTabs = tabModel.getTabsInGroup(tabGroupId);
                        if (groupTabs.isEmpty()) return;
                        firstGroupTab = groupTabs.get(0);
                    } else {
                        int tabId = model.get(TabProperties.TAB_ID);
                        if (tabId == Tab.INVALID_TAB_ID) return;

                        tab = tabModel.getTabById(tabId);
                        if (tab == null) return;

                        // Do not allow dragging out the last tab in a group.
                        // (To be handled when dragging out tab groups is enabled).
                        if (tabModel.isTabInTabGroup(tab)
                                && tabModel.getRelatedTabList(tabId).size() == 1) {
                            return;
                        }
                    }

                    PointF startPoint = new PointF(mLastTouchPoint.x + dX, mLastTouchPoint.y + dY);

                    itemTouchHelper.setExternalDragItem(viewHolder);
                    dragHandler.setDragHandlerDelegate(
                            createDragHandlerDelegate(
                                    recyclerView,
                                    itemTouchHelper,
                                    touchHelperCallback,
                                    dragHandler,
                                    nonOriginatingDelegate,
                                    viewHolder,
                                    model));

                    initDragShadowView(viewHolder.itemView.getContext());

                    boolean dragStarted;
                    if (isGroupHeader) {
                        if (mDragShadowView != null) {
                            mDragShadowView.prepareForGroupDrag(
                                    assumeNonNull(firstGroupTab), viewHolder.itemView.getWidth());
                        }

                        dragStarted =
                                dragHandler.startGroupDragAction(
                                        viewHolder.itemView,
                                        assumeNonNull(tabGroupId),
                                        startPoint,
                                        mDragShadowView);
                    } else {
                        if (mDragShadowView != null) {
                            mDragShadowView.prepareForTabDrag(
                                    assumeNonNull(tab), viewHolder.itemView.getWidth());
                        }
                        dragStarted =
                                dragHandler.startTabDragAction(
                                        viewHolder.itemView,
                                        assumeNonNull(tab),
                                        startPoint,
                                        mDragShadowView);
                    }

                    if (!dragStarted) {
                        if (mDragShadowView != null) {
                            mDragShadowView.clear();
                        }
                        itemTouchHelper.onExternalDragStop(/* recoverItem= */ true);
                        dragHandler.setDragHandlerDelegate(nonOriginatingDelegate);
                    }
                });

        itemTouchHelper.attachToRecyclerView(recyclerView);
        touchHelperCallback.setRecyclerView(recyclerView);

        recyclerView.addOnItemTouchListener(
                VerticalTabListItemTouchHelperCallback.createAfterOnItemTouchListener(
                        touchHelperCallback));
    }

    /**
     * Initializes the custom drag shadow view and multi-thumbnail card provider for external drag.
     */
    private void initDragShadowView(Context context) {
        if (mDragShadowView != null) return;

        TabContentManager tabContentManager = mTabContentManagerSupplier.get();
        if (tabContentManager == null) return;

        mMultiThumbnailCardProvider =
                new MultiThumbnailCardProvider(
                        context,
                        mBrowserControlsStateProvider,
                        tabContentManager,
                        mTabModelSelector.getCurrentTabModelSupplier());
        mMultiThumbnailCardProvider.initWithNative(mProfile.getOriginalProfile());

        mDragShadowView =
                (StripDragShadowView)
                        LayoutInflater.from(context).inflate(R.layout.strip_drag_shadow_view, null);
        mDragShadowView.initialize(
                mBrowserControlsStateProvider,
                mMultiThumbnailCardProvider,
                tabContentManager,
                /* layerTitleCacheSupplier= */ null,
                mTabModelSelector,
                () -> {
                    for (TabSwitcherDragHandler dragHandler : mTabSwitcherDragHandlers) {
                        if (dragHandler.hasActiveDragShadow()) {
                            dragHandler.refreshDragShadow(mDragShadowView);
                            break;
                        }
                    }
                });
    }

    private TabSwitcherDragHandler createTabSwitcherDragHandler(
            Activity activity, TabModelSelector tabModelSelector) {
        if (sTabSwitcherDragHandlerSupplierForTesting != null) {
            TabSwitcherDragHandler dragHandler = sTabSwitcherDragHandlerSupplierForTesting.get();
            mTabSwitcherDragHandlers.add(dragHandler);
            return dragHandler;
        }

        Supplier<@Nullable Activity> activitySupplier = () -> activity;
        DragAndDropDelegate dragDropDelegate = new DragAndDropDelegateImpl();
        dragDropDelegate.setDragAndDropBrowserDelegate(
                new ChromeDragAndDropBrowserDelegate(activitySupplier));

        TabSwitcherDragHandler dragHandler =
                new TabSwitcherDragHandler(
                        activitySupplier,
                        mMultiInstanceManager,
                        dragDropDelegate,
                        // TODO(crbug.com/518307037): Provide back press handler manager?
                        new TabSwitcherBackPressHandlerManager(),
                        /* fadeDragShadow= */ false);
        dragHandler.setTabModelSelector(tabModelSelector);
        mTabSwitcherDragHandlers.add(dragHandler);
        return dragHandler;
    }

    private void clearDropIndicators() {
        mReorderStrategy.clear();
        mDropIndicatorDecoration.clear();
        mPinnedDropIndicatorDecoration.clear();
        mRecyclerView.invalidate();
        if (mPinnedTabsRecyclerView != null) {
            mPinnedTabsRecyclerView.invalidate();
        }
    }

    private DragHandlerDelegate createNonOriginatingDragHandlerDelegate(
            RecyclerView recyclerView, TabSwitcherDragHandler dragHandler) {
        return new DragHandlerDelegate() {
            @Override
            public boolean handleDragStart(float xPx, float yPx) {
                mTabHoverCardController.hideHoverCard();
                return true;
            }

            @Override
            public boolean handleDragEnter() {
                if (!dragHandler.isDragSourceInstance()) {
                    dragHandler.showDragShadow(recyclerView, false);
                }
                return true;
            }

            @Override
            public boolean handleDragLocation(View view, float xPx, float yPx) {
                if (!dragHandler.isDragSourceInstance()) {
                    DropTargetResult result = mReorderStrategy.calculateDropTarget(view, xPx, yPx);
                    mDropIndicatorDecoration.setDropTargetResult(result);
                    mPinnedDropIndicatorDecoration.setDropTargetResult(result);
                    mRecyclerView.invalidate();
                    if (mPinnedTabsRecyclerView != null) {
                        mPinnedTabsRecyclerView.invalidate();
                    }
                }
                return true;
            }

            @Override
            public boolean handleDragExit() {
                clearDropIndicators();
                if (!dragHandler.isDragSourceInstance()) {
                    dragHandler.showDragShadow(recyclerView, true);
                }
                return true;
            }

            @Override
            public boolean handleExternalDragEnd(
                    View view, float xPx, float yPx, boolean isOSNewWindowDrop) {
                clearDropIndicators();
                return true;
            }

            @Override
            public boolean handleDrop(View view, float xPx, float yPx) {
                boolean result = handleDropInternal(view, xPx, yPx);
                clearDropIndicators();
                return result;
            }

            private boolean handleDropInternal(View view, float xPx, float yPx) {
                if (dragHandler.isDragSourceInstance()) {
                    return true;
                }

                DragDropGlobalState globalState = TabDragHandlerBase.getDragDropGlobalState(null);
                if (globalState == null) {
                    return false;
                }

                TabModel tabModel = mTabModelSelector.getCurrentModel();
                if (tabModel == null) {
                    return false;
                }

                DropTargetResult dropTarget = mReorderStrategy.getLastDropTargetResult();
                if (dropTarget == null) {
                    dropTarget = mReorderStrategy.calculateDropTarget(view, xPx, yPx);
                }
                if (dropTarget == null) {
                    return false;
                }

                int destWindowId = mMultiInstanceManager.getCurrentInstanceId();

                if (globalState.getData() instanceof ChromeTabGroupDropDataAndroid) {
                    TabGroupMetadata tabGroupMetadata =
                            ChromeDragDropUtils.getTabGroupMetadataFromGlobalState(globalState);
                    if (tabGroupMetadata == null) {
                        return false;
                    }
                    if (tabGroupMetadata.isIncognito != tabModel.isIncognitoBranded()) {
                        return false;
                    }

                    MultiInstanceOrchestratorFactory.getInstance()
                            .moveTabGroupToWindowByIdChecked(
                                    destWindowId,
                                    tabGroupMetadata,
                                    dropTarget.destTabIndex,
                                    /* bringToFront= */ true);
                    DragDropMetricUtils.recordDragDropType(
                            DragDropType.TAB_STRIP_TO_TAB_STRIP,
                            /* isTabGroup= */ true,
                            /* isMultiTab= */ false);
                    return true;
                } else if (globalState.getData() instanceof ChromeMultiTabDropDataAndroid) {
                    // TODO(crbug.com/550564967): Support multi-tab drop reparenting in Vertical
                    // Tabs.
                    return false;
                } else if (globalState.getData() instanceof ChromeTabDropDataAndroid) {
                    Tab tab = ChromeDragDropUtils.getTabFromGlobalState(globalState);
                    if (tab == null) {
                        return false;
                    }
                    if (tab.isIncognitoBranded() != tabModel.isIncognitoBranded()) {
                        return false;
                    }

                    maybeUngroupTab(tab, (ChromeTabDropDataAndroid) globalState.getData());

                    int destGroupTabId = dropTarget.destGroupTabId;
                    int destTabIndex = dropTarget.destTabIndex;
                    int indexInGroup = TabList.INVALID_TAB_INDEX;

                    if (destGroupTabId != TabList.INVALID_TAB_INDEX) {
                        Tab destGroupTab = tabModel.getTabById(destGroupTabId);
                        if (destGroupTab != null) {
                            Token groupId = destGroupTab.getTabGroupId();
                            List<Tab> groupTabs =
                                    groupId != null
                                            ? tabModel.getTabsInGroup(groupId)
                                            : Collections.emptyList();
                            int firstGroupModelIndex =
                                    !groupTabs.isEmpty()
                                            ? tabModel.indexOf(groupTabs.get(0))
                                            : destTabIndex;
                            indexInGroup =
                                    MathUtils.clamp(
                                            destTabIndex - firstGroupModelIndex,
                                            0,
                                            groupTabs.size());
                        }
                    }

                    MultiInstanceOrchestratorFactory.getInstance()
                            .moveTabsToWindowByIdChecked(
                                    destWindowId,
                                    Collections.singletonList(tab),
                                    destTabIndex,
                                    /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                                    /* bringToFront= */ true);

                    if (destGroupTabId != TabList.INVALID_TAB_INDEX) {
                        Tab destGroupTab = tabModel.getTabById(destGroupTabId);
                        if (destGroupTab != null && indexInGroup != TabList.INVALID_TAB_INDEX) {
                            tabModel.mergeListOfTabsToGroup(
                                    Collections.singletonList(tab),
                                    destGroupTab,
                                    indexInGroup,
                                    TabGroupMergeNotificationType.DONT_NOTIFY);
                        }
                    }

                    DragDropMetricUtils.recordDragDropType(
                            DragDropType.TAB_STRIP_TO_TAB_STRIP,
                            /* isTabGroup= */ false,
                            /* isMultiTab= */ false);
                    return true;
                }

                return false;
            }

            private void maybeUngroupTab(Tab tab, ChromeTabDropDataAndroid dropData) {
                if (tab.getTabGroupId() == null && !dropData.isTabInGroup) {
                    return;
                }
                TabModel sourceTabModel = null;
                if (dropData.windowId != TabWindowManager.INVALID_WINDOW_ID) {
                    TabModelSelector sourceSelector =
                            TabWindowManagerSingleton.getInstance()
                                    .getTabModelSelectorById(dropData.windowId);
                    if (sourceSelector != null) {
                        sourceTabModel = sourceSelector.getModel(tab.isIncognitoBranded());
                    }
                }
                if (sourceTabModel == null) {
                    sourceTabModel = TabWindowManagerSingleton.getInstance().getTabModelForTab(tab);
                }
                if (sourceTabModel != null && sourceTabModel.isTabInTabGroup(tab)) {
                    sourceTabModel
                            .getTabUngrouper()
                            .ungroupTabs(
                                    Collections.singletonList(tab),
                                    /* trailing= */ true,
                                    /* allowDialog= */ false);
                }
            }
        };
    }

    private DragHandlerDelegate createDragHandlerDelegate(
            RecyclerView recyclerView,
            ItemTouchHelper2 itemTouchHelper,
            VerticalTabListItemTouchHelperCallback touchHelperCallback,
            TabSwitcherDragHandler dragHandler,
            DragHandlerDelegate nonOriginatingDelegate,
            RecyclerView.ViewHolder viewHolder,
            @Nullable PropertyModel model) {
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        List<Tab> draggedTabs = new ArrayList<>();
        int originallySelectedTabId = Tab.INVALID_TAB_ID;
        if (tabModel != null && model != null) {
            if (TabProperties.isTabGroupHeader(model)) {
                Token groupId = model.get(TabProperties.TAB_GROUP_HEADER_ID);
                if (groupId != null) {
                    draggedTabs.addAll(tabModel.getTabsInGroup(groupId));
                }
            } else {
                int tabId = model.get(TabProperties.TAB_ID);
                Tab tab = tabModel.getTabById(tabId);
                if (tab != null) {
                    draggedTabs.add(tab);
                }
            }
            Tab currentSelectedTab = TabModelUtils.getCurrentTab(tabModel);
            if (currentSelectedTab != null && draggedTabs.contains(currentSelectedTab)) {
                originallySelectedTabId = currentSelectedTab.getId();
            }
        }
        final int selectedDraggedTabId = originallySelectedTabId;

        return new DragHandlerDelegate() {
            private final int[] mTempViewLoc = new int[2];
            private final int[] mTempRvLoc = new int[2];
            private final float[] mTempCoords = new float[2];

            private void deselectDraggedTabIfNeeded() {
                if (tabModel == null || selectedDraggedTabId == Tab.INVALID_TAB_ID) return;
                if (tabModel.getCurrentTabSupplier() == null
                        || tabModel.getNextTabPolicySupplier() == null) {
                    return;
                }
                Tab nextTab =
                        NextTabSelectionUtil.getNextTabIfClosed(
                                tabModel,
                                /* modelDelegate= */ null,
                                draggedTabs,
                                /* uponExit= */ false);
                if (nextTab != null) {
                    int nextIndex = tabModel.indexOf(nextTab);
                    if (nextIndex != TabModel.INVALID_TAB_INDEX && nextIndex != tabModel.index()) {
                        tabModel.setIndex(nextIndex, TabSelectionType.FROM_DRAG);
                    }
                }
            }

            private void reselectDraggedTabIfNeeded() {
                if (tabModel == null || selectedDraggedTabId == Tab.INVALID_TAB_ID) return;
                Tab tab = tabModel.getTabById(selectedDraggedTabId);
                if (tab != null) {
                    int index = tabModel.indexOf(tab);
                    if (index != TabModel.INVALID_TAB_INDEX && index != tabModel.index()) {
                        tabModel.setIndex(index, TabSelectionType.FROM_DRAG);
                    }
                }
            }

            private float[] toRvCoordinates(View view, float x, float y) {
                if (view == recyclerView) {
                    mTempCoords[0] = x;
                    mTempCoords[1] = y;
                } else {
                    view.getLocationOnScreen(mTempViewLoc);
                    recyclerView.getLocationOnScreen(mTempRvLoc);
                    mTempCoords[0] = x + mTempViewLoc[0] - mTempRvLoc[0];
                    mTempCoords[1] = y + mTempViewLoc[1] - mTempRvLoc[1];
                }
                return mTempCoords;
            }

            @Override
            public boolean handleDragStart(View view, float xPx, float yPx) {
                float[] coords = toRvCoordinates(view, xPx, yPx);
                return handleDragStart(coords[0], coords[1]);
            }

            @Override
            public boolean handleDragStart(float xPx, float yPx) {
                mTabHoverCardController.hideHoverCard();
                itemTouchHelper.onExternalDragStart(xPx, yPx, /* hideItemWhileDragging= */ true);
                deselectDraggedTabIfNeeded();

                moveDraggedPinnedTabToEndIfNeeded(model);
                // Keep a minimum height during external drag so a single-item list does not
                // collapse to 0px.
                updateSingleTabListMinHeight(model, /* useMinHeight= */ true);

                // Since the OS-level drag-and-drop only initiates after the cursor has moved
                // outside the bounds of the RecyclerView, we will never receive an
                // ACTION_DRAG_EXITED event. Therefore, we must explicitly trigger the collapse of
                // the drag gap right away.
                touchHelperCallback.collapseDraggedItem(viewHolder);
                return true;
            }

            @Override
            public boolean handleDragLocation(View view, float xPx, float yPx) {
                float[] coords = toRvCoordinates(view, xPx, yPx);
                return handleDragLocation(coords[0], coords[1]);
            }

            @Override
            public boolean handleDragLocation(float xPx, float yPx) {
                itemTouchHelper.onExternalDragLocation(xPx, yPx);
                return true;
            }

            @Override
            public boolean handleDragEnter(View view) {
                if (view != recyclerView) {
                    return true;
                }
                return handleDragEnter();
            }

            @Override
            public boolean handleDragEnter() {
                dragHandler.showDragShadow(recyclerView, false);
                reselectDraggedTabIfNeeded();
                updateSingleTabListMinHeight(model, /* useMinHeight= */ false);
                touchHelperCallback.restoreDraggedItem(/* isOSNewWindowDrop= */ false);
                return true;
            }

            @Override
            public boolean handleDragExit(View view) {
                if (view != recyclerView) {
                    return true;
                }
                return handleDragExit();
            }

            @Override
            public boolean handleDragExit() {
                dragHandler.showDragShadow(recyclerView, true);
                deselectDraggedTabIfNeeded();
                moveDraggedPinnedTabToEndIfNeeded(model);
                // Keep a minimum height during external drag so a single-item list does not
                // collapse to 0px.
                updateSingleTabListMinHeight(model, /* useMinHeight= */ true);
                touchHelperCallback.collapseDraggedItem(null);
                return true;
            }

            @Override
            public boolean handleExternalDragEnd(
                    View view, float xPx, float yPx, boolean isOSNewWindowDrop) {
                float[] coords = toRvCoordinates(view, xPx, yPx);
                return handleExternalDragEnd(coords[0], coords[1], isOSNewWindowDrop);
            }

            @Override
            public boolean handleExternalDragEnd(float xPx, float yPx, boolean isOSNewWindowDrop) {
                if (!isOSNewWindowDrop) {
                    reselectDraggedTabIfNeeded();
                }
                updateSingleTabListMinHeight(model, /* useMinHeight= */ false);
                touchHelperCallback.restoreDraggedItem(isOSNewWindowDrop);
                itemTouchHelper.onExternalDragStop(/* recoverItem= */ false);
                if (mDragShadowView != null) {
                    mDragShadowView.clear();
                }

                dragHandler.setDragHandlerDelegate(nonOriginatingDelegate);
                return true;
            }

            @Override
            public boolean handleDrop(View view, float xPx, float yPx) {
                float[] coords = toRvCoordinates(view, xPx, yPx);
                return handleDrop(coords[0], coords[1]);
            }

            @Override
            public int handleInternalDragEnd() {
                reselectDraggedTabIfNeeded();
                updateSingleTabListMinHeight(model, /* useMinHeight= */ false);
                itemTouchHelper.stopInternalDrag();
                dragHandler.setDragHandlerDelegate(nonOriginatingDelegate);
                return BackPressHandler.BackPressResult.SUCCESS;
            }

            @Override
            public boolean isDragInProcess() {
                return itemTouchHelper.isDragInProcess();
            }
        };
    }

    private void updateSingleTabListMinHeight(@Nullable PropertyModel model, boolean useMinHeight) {
        if (model == null) return;

        boolean isSinglePinned =
                TabProperties.isPinnedTab(model) && mPinnedTabsModelList.size() <= 1;
        boolean isSingleRegular = !TabProperties.isPinnedTab(model) && mModelList.size() <= 1;
        if (!isSinglePinned && !isSingleRegular) return;

        TabListRecyclerView recyclerView = isSinglePinned ? mPinnedTabsRecyclerView : mRecyclerView;
        int minHeight =
                useMinHeight
                        ? recyclerView
                                .getResources()
                                .getDimensionPixelSize(R.dimen.pinned_tab_strip_item_favicon_height)
                        : 0;
        recyclerView.setMinimumHeight(minHeight);
    }

    private void moveDraggedPinnedTabToEndIfNeeded(@Nullable PropertyModel model) {
        if (model == null || !TabProperties.isPinnedTab(model)) return;

        int draggedTabId = model.get(TabProperties.TAB_ID);
        if (draggedTabId == Tab.INVALID_TAB_ID) return;
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (tabModel == null) return;

        Tab tab = tabModel.getTabById(draggedTabId);
        if (tab == null) return;

        int numPinned = tabModel.getPinnedTabsCount();
        if (numPinned <= 1) return;

        int currentIndex = tabModel.indexOf(tab);
        int lastPinnedIndex = numPinned - 1;
        if (currentIndex != TabModel.INVALID_TAB_INDEX && currentIndex < lastPinnedIndex) {
            tabModel.moveTab(draggedTabId, lastPinnedIndex);
        }
    }

    /**
     * Returns the grid column span count for the Left Rail based on measured width and pinned tab
     * count.
     */
    private int getSpanCount() {
        if (mContainerModel != null) {
            @RailCollapseState
            int collapseState = mContainerModel.get(VerticalTabListProperties.COLLAPSE_STATE);
            if (collapseState == RailCollapseState.COLLAPSED) {
                return COLLAPSED_GRID_SPAN_COUNT;
            }
        }

        int containerWidth = mContainerView.getWidth();
        if (containerWidth <= 0) return DEFAULT_GRID_SPAN_COUNT;

        int paddingStart = mContainerView.getPaddingStart();
        int paddingEnd = mContainerView.getPaddingEnd();
        int availableWidth = containerWidth - paddingStart - paddingEnd;

        Resources res = mContainerView.getContext().getResources();
        boolean isTablet = VerticalTabUtils.isTablet(mContainerView.getContext());
        int pinnedTabCount = mPinnedTabsModelList != null ? mPinnedTabsModelList.size() : 0;
        return calculateBalancedSpanCount(availableWidth, pinnedTabCount, res, isTablet);
    }

    private void updatePinnedLayoutSpanCount() {
        if (mPinnedLayoutManager == null) return;
        mPinnedLayoutManager.setSpanCount(getSpanCount());
        mPinnedTabsRecyclerView.invalidateItemDecorations();
    }

    private boolean openContextMenuForFocusedItem(RecyclerView recyclerView) {
        View focusedChild = recyclerView.findFocus();
        if (focusedChild == null) return false;

        View itemView = recyclerView.findContainingItemView(focusedChild);
        if (itemView == null) return false;

        boolean menuShown = showMenuForItemView(getItemViewAnchorRectProvider(itemView), itemView);
        if (menuShown) {
            itemView.performHapticFeedback(HapticFeedbackConstants.CONTEXT_CLICK);
        }
        return menuShown;
    }

    private boolean showMenuForItemView(RectProvider rectProvider, View itemView) {
        if (!(itemView.getParent() instanceof RecyclerView recyclerView)) return false;
        int position = recyclerView.getChildAdapterPosition(itemView);
        if (position == RecyclerView.NO_POSITION) return false;
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) return false;
        return showMenuForAdapterPosition(rectProvider, activity, recyclerView, position);
    }

    /**
     * Handles any context-trigger gesture (such as right-click or long-press) inside the VT rail.
     * Evaluates if the interaction targeted a specific tab item row or fell on the vertical
     * layout's empty background space, launching the appropriate context menu.
     *
     * @param activity The activity context where this context menu will show.
     * @param recyclerView The vertical tabs scrollable container.
     * @param localX The touch-point offset on the X-axis relative to the layout.
     * @param localY The touch-point offset on the Y-axis relative to the layout.
     * @return true if a context menu was successfully displayed; false otherwise.
     */
    private boolean handleContextMenuInteraction(
            Activity activity, RecyclerView recyclerView, float localX, float localY) {
        View childView = recyclerView.findChildViewUnder(localX, localY);
        RectProvider rectProvider = calculateTouchAnchor(recyclerView, localX, localY);

        // If childView is null, the coordinates landed on an empty space. Launch empty space menu.
        if (childView == null) {
            showEmptySpaceContextMenu(rectProvider, activity);
            return true;
        }
        int position = recyclerView.getChildAdapterPosition(childView);
        return showMenuForAdapterPosition(rectProvider, activity, recyclerView, position);
    }

    private boolean showMenuForAdapterPosition(
            RectProvider rectProvider, Activity activity, RecyclerView recyclerView, int position) {
        TabListModel modelList =
                (recyclerView == mPinnedTabsRecyclerView) ? mPinnedTabsModelList : mModelList;
        if (!modelList.isValidIndex(position)) return false;

        ListItem item = modelList.get(position);
        int resolvedItemViewType =
                assumeNonNull(recyclerView.getAdapter()).getItemViewType(position);
        if (resolvedItemViewType == UiType.TAB || resolvedItemViewType == UiType.PINNED_TAB) {
            // The user clicked directly on a tab item (regular tab, pinned tab, or child tab).
            int tabId = TabProperties.getTabId(item.model);
            return showTabItemContextMenu(rectProvider, activity, tabId);
        } else if (resolvedItemViewType == UiType.TAB_GROUP) {
            Token tabGroupId = item.model.get(TabProperties.TAB_GROUP_HEADER_ID);
            if (tabGroupId != null) {
                return showTabGroupHeaderContextMenu(rectProvider, tabGroupId);
            }
        }
        return false;
    }

    /**
     * Shows the context menu for a newly created tab group header by resolving its current view
     * from the recycler view. This avoids capturing a stale tab item view across the asynchronous
     * group creation callback.
     */
    private void showTabGroupHeaderContextMenuForGroupId(Token tabGroupId) {
        mRecyclerView.post(
                () -> {
                    TabModel currentModel = mTabModelSelector.getCurrentModel();
                    if (currentModel == null || !currentModel.tabGroupExists(tabGroupId)) {
                        return;
                    }

                    int index = mModelList.indexFromTabGroupId(tabGroupId);
                    if (index == TabModel.INVALID_TAB_INDEX) return;

                    RecyclerView.ViewHolder holder =
                            mRecyclerView.findViewHolderForAdapterPosition(index);
                    if (holder == null) {
                        mRecyclerView.scrollToPosition(index);
                        mRecyclerView.post(
                                () -> {
                                    RecyclerView.ViewHolder retryHolder =
                                            mRecyclerView.findViewHolderForAdapterPosition(index);
                                    if (retryHolder != null) {
                                        showTabGroupHeaderContextMenu(
                                                getItemViewAnchorRectProvider(retryHolder.itemView),
                                                tabGroupId);
                                    }
                                });
                        return;
                    }

                    showTabGroupHeaderContextMenu(
                            getItemViewAnchorRectProvider(holder.itemView), tabGroupId);
                });
    }

    private boolean showTabGroupHeaderContextMenu(RectProvider rectProvider, Token tabGroupId) {
        if (tabGroupId == null) return false;

        TabModel currentModel = mTabModelSelector.getCurrentModel();
        if (currentModel == null || !currentModel.tabGroupExists(tabGroupId)) {
            return false;
        }
        List<Tab> tabsInGroup = currentModel.getTabsInGroup(tabGroupId);
        if (tabsInGroup.isEmpty()) {
            return false;
        }
        boolean hasNonClosingTab = false;
        for (Tab tab : tabsInGroup) {
            if (!tab.isClosing()) {
                hasNonClosingTab = true;
                break;
            }
        }
        if (!hasNonClosingTab) {
            return false;
        }

        if (mTabGroupContextMenuCoordinator == null) {
            mTabGroupContextMenuCoordinator =
                    TabGroupContextMenuCoordinator.createContextMenuCoordinator(
                            mTabModelSelector.getCurrentModel(),
                            mMultiInstanceManager,
                            mWindowAndroid,
                            mDataSharingTabManager,
                            /* reorderFunction= */ (groupId, toPrevious) ->
                                    NestedTabReorderUtils.reorderTabGroup(
                                            mTabModelSelector.getCurrentModel(),
                                            groupId,
                                            toPrevious),
                            TabClosingSource.VERTICAL_TAB_STRIP,
                            TabStripLayoutType.VERTICAL);
        }
        mTabHoverCardController.hideHoverCard();
        mTabGroupContextMenuCoordinator.showMenu(rectProvider, tabGroupId);
        return true;
    }

    private boolean showTabItemContextMenu(
            RectProvider rectProvider, Activity activity, int tabId) {
        if (tabId == Tab.INVALID_TAB_ID) return false;

        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (tabModel == null) return false;

        Tab tab = tabModel.getTabById(tabId);
        if (tab == null || tab.isClosing()) return false;

        List<Integer> allTabIds;
        if (TabMultiSelectHelper.hasMultipleTabsSelected(tabModel)
                && tabModel.isTabMultiSelected(tabId)) {
            allTabIds = tabModel.getOrderedMultiSelectedTabIds();
        } else {
            allTabIds = List.of(tabId);
        }

        var anchorInfo = new AnchorInfo(tabId, allTabIds);

        if (mTabContextMenuCoordinator == null) {
            TabGroupCreationCallback tabGroupCreationCallback =
                    (newTabGroupId) -> {
                        if (newTabGroupId != null) {
                            showTabGroupHeaderContextMenuForGroupId(newTabGroupId);
                        }
                    };

            mTabContextMenuCoordinator =
                    TabContextMenuCoordinator.createContextMenuCoordinator(
                            mTabModelSelector::getCurrentModel,
                            /* tabGroupListBottomSheetCoordinator= */ null,
                            tabGroupCreationCallback,
                            mMultiInstanceManager,
                            mShareDelegateSupplier,
                            mWindowAndroid,
                            activity,
                            /* tabBookmarkerSupplier= */ null,
                            /* reorderFunction= */ (info, toPrevious) ->
                                    NestedTabReorderUtils.reorderTabById(
                                            mTabModelSelector.getCurrentModel(),
                                            mPinnedTabsModelList,
                                            mModelList,
                                            info.getAnchorTabId(),
                                            toPrevious),
                            mSnackbarManager,
                            mActivityResultTracker,
                            /* modalDialogManager= */ mWindowAndroid.getModalDialogManager(),
                            TabClosingSource.VERTICAL_TAB_STRIP,
                            mCanActivateTabLayoutToggleMenuSupplier,
                            TabStripLayoutType.VERTICAL);
        }
        mTabHoverCardController.hideHoverCard();
        mTabContextMenuCoordinator.showMenu(rectProvider, anchorInfo);
        return true;
    }

    private void showEmptySpaceContextMenu(RectProvider rectProvider, Activity activity) {
        if (mTabStripContextMenuCoordinator == null) {
            mTabStripContextMenuCoordinator =
                    TabStripContextMenuCoordinator.createContextMenuCoordinator(
                            mTabModelSelector.getCurrentModel(),
                            mMultiInstanceManager,
                            mWindowAndroid,
                            mSnackbarManager,
                            this::handleNewTabButtonClick,
                            mCanActivateTabLayoutToggleMenuSupplier,
                            TabStripLayoutType.VERTICAL);
        }

        boolean isIncognito = mTabModelSelector.getCurrentModel().isIncognitoBranded();
        mTabHoverCardController.hideHoverCard();
        mTabStripContextMenuCoordinator.showMenu(rectProvider, isIncognito, activity);
    }

    private RectProvider getItemViewAnchorRectProvider(View itemView) {
        int[] viewPos = new int[2];
        itemView.getLocationInWindow(viewPos);
        Rect anchorRect =
                new Rect(
                        viewPos[0],
                        viewPos[1],
                        viewPos[0] + itemView.getWidth(),
                        viewPos[1] + itemView.getHeight());
        return new RectProvider(anchorRect);
    }

    private RectProvider calculateTouchAnchor(View targetView, float localX, float localY) {
        // Get the top-left edge pos of the touched view relative to the Android
        // application window screen.
        int[] viewPos = new int[2];
        targetView.getLocationInWindow(viewPos);

        // Calculate window-relative anchor coordinates, where localX and localY are relative to the
        // target view.
        int windowX = viewPos[0] + (int) localX;
        int windowY = viewPos[1] + (int) localY;

        // Build a precise 1x1 bounding box right under the cursor/pointer.
        Rect anchorRect = new Rect(windowX, windowY, windowX + 1, windowY + 1);
        return new RectProvider(anchorRect);
    }

    /**
     * Creates a unified SimpleOnItemTouchListener for RecyclerView surfaces to passively track
     * touch coordinates, capture tab item right-clicks, and delegate empty space long-presses.
     *
     * @param activity The active context.
     * @param gestureDetector The companion GestureDetector built for this specific list surface.
     */
    private RecyclerView.OnItemTouchListener createRecyclerViewItemTouchListener(
            Activity activity, GestureDetector gestureDetector) {
        return new RecyclerView.SimpleOnItemTouchListener() {
            @Override
            public boolean onInterceptTouchEvent(RecyclerView rv, MotionEvent e) {
                // Save the coordinates in mLastTouchPoint the moment a finger or mouse
                // pointer hits the view surface.
                if (e.getActionMasked() == MotionEvent.ACTION_DOWN) {
                    mLastTouchPoint.set((int) e.getX(), (int) e.getY());
                }

                // Intercept mouse right-clicks directly on child tab items before interactive
                // sub-views (like buttons or click listeners) consume the touch sequence.
                if ((e.getButtonState() & MotionEvent.BUTTON_SECONDARY) != 0) {
                    View childView = rv.findChildViewUnder(e.getX(), e.getY());
                    if (childView != null) {
                        return handleContextMenuInteraction(activity, rv, e.getX(), e.getY());
                    }
                    // For empty space context menus, we let
                    // recyclerView.setOnContextClickListener call
                    // #handleContextMenuInteraction instead of calling it here.
                    return false;
                }

                // Feed all touch events to the detector. ACTION_DOWN schedules a long-press
                // timeout (~500ms). Trailing events (ACTION_MOVE, ACTION_UP) are processed
                // to either cancel the timeout if the finger drags too far, or reset the
                // tracking state engine when lifted.
                gestureDetector.onTouchEvent(e);

                // Return false to keep our tracking passive. If we return true, subsequent
                // events (ACTION_UP, ACTION_MOVE, etc.) bypass this intercept method.
                return false;
            }
        };
    }

    /**
     * Creates a reusable SimpleOnGestureListener for empty space long-presses.
     *
     * @param activity The active context.
     * @param targetView The view receiving the touch events.
     * @param targetRecyclerView The RecyclerView containing list items to evaluate (if targetView
     *     is a RecyclerView).
     */
    private GestureDetector.SimpleOnGestureListener createEmptySpaceGestureListener(
            Activity activity, View targetView, RecyclerView targetRecyclerView) {
        return new GestureDetector.SimpleOnGestureListener() {
            @Override
            public boolean onDown(MotionEvent e) {
                // Turns on the gesture engine's internal stopwatch to calculate the
                // long-press.
                return true;
            }

            @Override
            public void onLongPress(MotionEvent e) {
                // Ignore long-press actions if a secondary button modifier
                // (right-click) is active. Right-clicks are already handled by
                // setOnContextClickListener; allowing this to proceed causes
                // double-rendering on desktop workspaces where trackpad taps are
                // emulated via TOOL_TYPE_FINGER (instead of TOOL_TYPE_MOUSE).
                if ((e.getButtonState() & MotionEvent.BUTTON_SECONDARY) != 0) {
                    return;
                }

                // If this is a RecyclerView, ensure we didn't long-press an actual child item.
                if (targetView instanceof RecyclerView rv) {
                    View childView = rv.findChildViewUnder(e.getX(), e.getY());
                    if (childView != null) {
                        // Allow ItemTouchHelper2 + Orchestrator to manage it instead.
                        return;
                    }
                    handleContextMenuInteraction(activity, targetRecyclerView, e.getX(), e.getY());
                } else {
                    // For the header view (or non-lists), directly show the empty space menu.
                    showEmptySpaceContextMenu(
                            calculateTouchAnchor(targetView, e.getX(), e.getY()), activity);
                }
            }
        };
    }

    /**
     * Creates a reusable OnContextClickListener for empty space right-clicks.
     *
     * @param activity The active context.
     * @param targetView The view receiving the click.
     */
    private View.OnContextClickListener createEmptySpaceContextClickListener(
            Activity activity, View targetView) {
        return v -> {
            // If the click landed on an actual child item, onInterceptTouchEvent already
            // handled it. Only show the empty space menu if there is no child view under the
            // cursor.
            if (targetView instanceof RecyclerView rv) {
                View childView = rv.findChildViewUnder(mLastTouchPoint.x, mLastTouchPoint.y);
                if (childView != null) {
                    return true;
                }
            }
            showEmptySpaceContextMenu(
                    calculateTouchAnchor(targetView, mLastTouchPoint.x, mLastTouchPoint.y),
                    activity);
            return true;
        };
    }

    /**
     * Creates a passive View.OnTouchListener that caches the immediate touch coordinates local to
     * the clicked view surface upon an ACTION_DOWN event without consuming the touch sequence. This
     * is needed to update the coordinate tracking frame relative to the specific view surface
     * instead of its parent layout wrapper.
     */
    @SuppressLint("ClickableViewAccessibility")
    private View.OnTouchListener createLocalCoordinateTrackingTouchListener() {
        return (v, event) -> {
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                mLastTouchPoint.set((int) event.getX(), (int) event.getY());
            }
            return false;
        };
    }

    private void dismissActiveContextMenus() {
        if (mTabStripContextMenuCoordinator != null) mTabStripContextMenuCoordinator.dismiss();
        if (mTabContextMenuCoordinator != null) mTabContextMenuCoordinator.dismiss();
        if (mTabGroupContextMenuCoordinator != null) mTabGroupContextMenuCoordinator.dismiss();
    }

    private boolean isAnyContextMenuShowing() {
        return (mTabStripContextMenuCoordinator != null
                        && mTabStripContextMenuCoordinator.isMenuShowing())
                || (mTabContextMenuCoordinator != null
                        && mTabContextMenuCoordinator.isMenuShowing())
                || (mTabGroupContextMenuCoordinator != null
                        && mTabGroupContextMenuCoordinator.isMenuShowing());
    }

    private void updateSpacerVisibility(@Nullable AppHeaderState appHeaderState) {
        boolean isInDesktopWindow = appHeaderState != null && appHeaderState.isInDesktopWindow();
        mContainerView.setDesktopWindowSpacerVisible(isInDesktopWindow);
    }

    /**
     * Calculates the grid column span count for pinned tabs based on available width and tab count.
     */
    @VisibleForTesting
    static int calculateBalancedSpanCount(
            int availableWidth, int pinnedTabCount, Resources res, boolean isTablet) {
        int minItemWidth =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tab_pinned_item_min_width_tablet
                                : R.dimen.vertical_tab_pinned_item_min_width);
        int minHorizontalGap = res.getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_gap);
        if (minItemWidth <= 0) return DEFAULT_GRID_SPAN_COUNT;

        float spansFittingWidth =
                (float) (availableWidth + minHorizontalGap) / (minItemWidth + minHorizontalGap)
                        + SPAN_CALCULATION_EPSILON;
        int maxFitSpans =
                Math.clamp((int) Math.floor(spansFittingWidth), 1, MAX_SINGLE_ROW_SPAN_COUNT);

        if (pinnedTabCount <= 0) {
            return maxFitSpans;
        }

        // Uses integer ceiling division (A + B - 1) / B instead of A / B (which truncates and
        // would yield 0 rows when pinnedTabCount < maxFitSpans) to calculate the full number of
        // rows needed, balancing tabs evenly across rows.
        int rows = (pinnedTabCount + maxFitSpans - 1) / maxFitSpans;
        int columns = (pinnedTabCount + rows - 1) / rows;
        return Math.clamp(columns, 1, maxFitSpans);
    }

    @VisibleForTesting
    static RecyclerView.ItemDecoration createPinnedTabItemDecoration() {
        return new RecyclerView.ItemDecoration() {
            @Override
            public void getItemOffsets(
                    Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
                calculatePinnedTabItemOffsets(outRect, view, parent);
            }
        };
    }

    /**
     * Distributes inter-item horizontal gaps evenly across grid columns without outer margins,
     * ensuring identical visual item widths since RecyclerView does not support layout_weight.
     */
    private static void calculatePinnedTabItemOffsets(
            Rect outRect, View view, RecyclerView parent) {
        int position = parent.getChildAdapterPosition(view);
        if (position == RecyclerView.NO_POSITION) {
            position = parent.indexOfChild(view);
        }
        if (position == RecyclerView.NO_POSITION) return;
        if (!(parent.getLayoutManager() instanceof GridLayoutManager gridLayoutManager)) return;
        int spanCount = gridLayoutManager.getSpanCount();
        if (spanCount <= 1) {
            outRect.left = 0;
            outRect.right = 0;
            return;
        }
        int minHorizontalGap =
                parent.getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_gap);
        int column = position % spanCount;
        int left = column * minHorizontalGap / spanCount;
        int right = minHorizontalGap - (column + 1) * minHorizontalGap / spanCount;
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        outRect.left = isRtl ? right : left;
        outRect.right = isRtl ? left : right;
    }

    @Nullable TabStripContextMenuCoordinator getTabStripContextMenuCoordinatorForTesting() {
        return mTabStripContextMenuCoordinator;
    }

    PropertyModel getContainerModelForTesting() {
        return mContainerModel;
    }

    @Nullable TabContextMenuCoordinator getTabContextMenuCoordinatorForTesting() {
        return mTabContextMenuCoordinator;
    }

    @Nullable TabGroupContextMenuCoordinator getTabGroupContextMenuCoordinatorForTesting() {
        return mTabGroupContextMenuCoordinator;
    }

    void setTabGroupContextMenuCoordinatorForTesting(TabGroupContextMenuCoordinator coordinator) {
        mTabGroupContextMenuCoordinator = coordinator;
    }

    void setTabStripContextMenuCoordinatorForTesting(
            TabStripContextMenuCoordinator contextMenuCoordinator) {
        mTabStripContextMenuCoordinator = contextMenuCoordinator;
    }

    void setTabContextMenuCoordinatorForTesting(TabContextMenuCoordinator contextMenuCoordinator) {
        mTabContextMenuCoordinator = contextMenuCoordinator;
    }

    Point getLastTouchPointForTesting() {
        return mLastTouchPoint;
    }

    boolean handleContextMenuInteractionForTesting(
            Activity activity, RecyclerView recyclerView, float localX, float localY) {
        return handleContextMenuInteraction(activity, recyclerView, localX, localY);
    }

    void showTabGroupHeaderContextMenuForGroupIdForTesting(Token tabGroupId) {
        showTabGroupHeaderContextMenuForGroupId(tabGroupId);
    }

    View.OnContextClickListener createEmptySpaceContextClickListenerForTesting(
            Activity activity, View targetView) {
        return createEmptySpaceContextClickListener(activity, targetView);
    }

    GridLayoutManager getPinnedLayoutManagerForTesting() {
        return mPinnedLayoutManager;
    }

    TabListModel getPinnedTabsModelListForTesting() {
        return mPinnedTabsModelList;
    }

    /** Sets the tab switcher drag handler supplier override for testing. */
    static void setTabSwitcherDragHandlerSupplierForTesting(
            @Nullable Supplier<TabSwitcherDragHandler> supplier) {
        sTabSwitcherDragHandlerSupplierForTesting = supplier;
        ResettersForTesting.register(() -> sTabSwitcherDragHandlerSupplierForTesting = null);
    }

    /** Returns the main touch helper callback for testing. */
    @Nullable VerticalTabListItemTouchHelperCallback getMainTouchHelperCallbackForTesting() {
        return mMainTouchHelperCallback;
    }

    /** Returns the active tab switcher drag handlers for testing. */
    List<TabSwitcherDragHandler> getTabSwitcherDragHandlersForTesting() {
        return mTabSwitcherDragHandlers;
    }

    TabHoverCardListener getTabHoverCardListenerForTesting() {
        return mTabHoverCardController.getTabHoverCardListener();
    }

    RecyclerView.OnScrollListener getOnScrollListenerForTesting() {
        return mOnScrollListener;
    }

    /** Returns the {@link VerticalTabKeyboardHandler} instance for testing. */
    VerticalTabKeyboardHandler getKeyboardHandlerForTesting() {
        return mKeyboardHandler;
    }

    /** Returns the {@link TabListMediator} instance for testing. */
    TabListMediator getMediatorForTesting() {
        return mMediator;
    }

    /** Returns the {@link VerticalTabListRecyclerView} instance for testing. */
    VerticalTabListRecyclerView getRecyclerViewForTesting() {
        return mRecyclerView;
    }
}
