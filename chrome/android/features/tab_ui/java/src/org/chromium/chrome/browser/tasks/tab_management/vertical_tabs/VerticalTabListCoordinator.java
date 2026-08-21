// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
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
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.AnchorInfo;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.TabStripLayoutType;
import org.chromium.chrome.browser.compositor.overlays.strip.TabGroupContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabUnderlineManager;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksUtils;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.dragdrop.ChromeDragAndDropBrowserDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
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
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabCreatorUtil;
import org.chromium.chrome.browser.tabmodel.TabGroupUtils.TabGroupCreationCallback;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.StaticPinnedTabsMediator;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabComponentId;
import org.chromium.chrome.browser.tasks.tab_management.TabGridViewBinder;
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
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
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
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragAndDropDelegateImpl;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;
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
    private final TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private final NonNullObservableSupplier<Boolean> mVerticalTabsActiveSupplier;
    private final VerticalTabRailCollapseController mCollapseController;
    private final Callback<Boolean> mActiveObserver = this::setActive;
    private final PropertyModel mContainerModel;
    private final List<TabSwitcherDragHandler> mTabSwitcherDragHandlers = new ArrayList<>();
    private final View.OnLayoutChangeListener mContainerLayoutChangeListener;
    private final View.OnLayoutChangeListener mPinnedTabsLayoutChangeListener;
    private final VerticalTabHoverCardController mTabHoverCardController;
    private final RecyclerView.OnScrollListener mOnScrollListener;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final @Nullable AppHeaderObserver mAppHeaderObserver;
    private final @Nullable BooleanSupplier mCanActivateTabLayoutToggleMenuSupplier;
    private final @Nullable UndoBarThrottle mUndoBarThrottle;
    private final @Nullable TabUnderlineManager mTabUnderlineManager;
    private @Nullable TabStripContextMenuCoordinator mTabStripContextMenuCoordinator;
    private @Nullable TabContextMenuCoordinator mTabContextMenuCoordinator;
    private @Nullable TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;
    private @Nullable Token mLastDraggedGroupId;
    private @Nullable VerticalTabListItemTouchHelperCallback mMainTouchHelperCallback;

    private boolean mIsActive;

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
            Supplier<TabContentManager> tabContentManagerSupplier,
            @Nullable UndoBarThrottle undoBarThrottle) {
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

        TabListConfig tabListConfig =
                new TabListConfig.Builder(TabListLayoutType.NESTED)
                        .setSupportsModifierMultiSelect(VerticalTabUtils.isMultiSelectEnabled())
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
                        /* dialogHandler */ null,
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
        mPinnedLayoutManager = new GridLayoutManager(activity, getSpanCount());
        pinnedTabsRecyclerView.setLayoutManager(mPinnedLayoutManager);
        pinnedTabsRecyclerView.addItemDecoration(
                new RecyclerView.ItemDecoration() {
                    @Override
                    public void getItemOffsets(
                            Rect outRect,
                            View view,
                            RecyclerView parent,
                            RecyclerView.State state) {
                        calculatePinnedTabItemOffsets(outRect, view, parent);
                    }
                });

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
                    public void onItemMoved(ListObservable source, int curIndex, int newIndex) {}
                };
        pinnedTabsModelList.addObserver(mPinnedTabsListObserver);

        mPinnedTabsMediator =
                new StaticPinnedTabsMediator(
                        tabModelSelector.getCurrentModel(),
                        mModelList,
                        pinnedTabsModelList,
                        this::updatePinnedTabsVisibility);
        updatePinnedTabsVisibility();

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
                        if (mIsActive) {
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
        // TODO(crbug.com/509226293): Check with UX if we want to match desktop behavior by
        // focusing the currently active tab instead of the first tab.
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
        mLastDraggedGroupId = null;
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

    /**
     * Toggles the expanded/collapsed visual and layout state of a tab group.
     *
     * @param tabId the ID of the representative tab representing the tab group.
     */
    @VisibleForTesting
    void toggleTabGroupExpansion(int tabId) {
        mMediator.toggleTabGroupExpansion(tabId);
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

        recyclerView.addOnItemTouchListener(
                VerticalTabListItemTouchHelperCallback.createBeforeOnItemTouchListener(
                        touchHelperCallback));

        ItemTouchHelper2 itemTouchHelper =
                new ItemTouchHelper2(touchHelperCallback, /* externalLongPressHandler= */ null);

        recyclerView.addOnItemTouchListener(
                touchHelperCallback.createMouseDragDetector(itemTouchHelper));

        TabSwitcherDragHandler dragHandler =
                createTabSwitcherDragHandler(activity, tabModelSelector);
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
                    if (!VerticalTabUtils.isExternalDragEnabled()) {
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
                    PointF startPoint = new PointF(mLastTouchPoint.x + dX, mLastTouchPoint.y + dY);

                    if (isGroupHeader) {
                        Token tabGroupId =
                                assumeNonNull(model.get(TabProperties.TAB_GROUP_HEADER_ID));

                        itemTouchHelper.setExternalDragItem(viewHolder);
                        dragHandler.setDragHandlerDelegate(
                                createDragHandlerDelegate(
                                        recyclerView,
                                        itemTouchHelper,
                                        touchHelperCallback,
                                        dragHandler,
                                        viewHolder,
                                        model));

                        mLastDraggedGroupId = tabGroupId;

                        View groupDragShadowView = null;
                        List<Tab> groupTabs = tabModel.getTabsInGroup(tabGroupId);
                        if (!groupTabs.isEmpty()) {
                            int repTabId = groupTabs.get(0).getId();
                            // TODO(crbug.com/509226293): Construct or fetch 2D GTS grid card drag
                            // shadow for collapsed tab groups.
                            PropertyModel repModel = mModelList.getModelFromTabId(repTabId);
                            if (repModel != null) {
                                groupDragShadowView = buildGridCardDragShadow(activity, repModel);
                            }
                        }
                        if (groupDragShadowView == null) {
                            groupDragShadowView = buildGroupHeaderDragShadow(activity, model);
                        }

                        dragHandler.startGroupDragAction(
                                viewHolder.itemView, tabGroupId, startPoint, groupDragShadowView);

                        if (!TabProperties.isTabGroupCollapsed(model) && !groupTabs.isEmpty()) {
                            int repTabId = groupTabs.get(0).getId();
                            recyclerView.post(() -> toggleTabGroupExpansion(repTabId));
                        }
                        return;
                    }

                    int tabId = model.get(TabProperties.TAB_ID);
                    if (tabId == Tab.INVALID_TAB_ID) return;

                    Tab tab = tabModel.getTabById(tabId);
                    if (tab == null) return;

                    // Do not allow dragging out the last tab in a group.
                    // (To be handled when dragging out tab groups is enabled).
                    if (tabModel.isTabInTabGroup(tab)
                            && tabModel.getRelatedTabList(tabId).size() == 1) {
                        return;
                    }

                    itemTouchHelper.setExternalDragItem(viewHolder);
                    dragHandler.setDragHandlerDelegate(
                            createDragHandlerDelegate(
                                    recyclerView,
                                    itemTouchHelper,
                                    touchHelperCallback,
                                    dragHandler,
                                    viewHolder,
                                    model));

                    mLastDraggedGroupId = null;
                    View gridCardView = buildGridCardDragShadow(activity, model);
                    dragHandler.startTabDragAction(
                            viewHolder.itemView, tab, startPoint, gridCardView);
                });

        itemTouchHelper.attachToRecyclerView(recyclerView);
        touchHelperCallback.setRecyclerView(recyclerView);

        recyclerView.addOnItemTouchListener(
                VerticalTabListItemTouchHelperCallback.createAfterOnItemTouchListener(
                        touchHelperCallback));
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
                        new TabSwitcherBackPressHandlerManager());
        dragHandler.setTabModelSelector(tabModelSelector);
        mTabSwitcherDragHandlers.add(dragHandler);
        return dragHandler;
    }

    private DragHandlerDelegate createDragHandlerDelegate(
            RecyclerView recyclerView,
            ItemTouchHelper2 itemTouchHelper,
            VerticalTabListItemTouchHelperCallback touchHelperCallback,
            TabSwitcherDragHandler dragHandler,
            RecyclerView.ViewHolder viewHolder,
            @Nullable PropertyModel model) {
        return new DragHandlerDelegate() {
            private final int[] mTempViewLoc = new int[2];
            private final int[] mTempRvLoc = new int[2];
            private final float[] mTempCoords = new float[2];

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
            public boolean handleDragEnter() {
                dragHandler.showDragShadow(recyclerView, false);
                updateSingleTabListMinHeight(model, /* useMinHeight= */ false);
                touchHelperCallback.restoreDraggedItem(/* isOSNewWindowDrop= */ false);
                return true;
            }

            @Override
            public boolean handleDragExit() {
                dragHandler.showDragShadow(recyclerView, true);
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
                updateSingleTabListMinHeight(model, /* useMinHeight= */ false);
                touchHelperCallback.restoreDraggedItem(isOSNewWindowDrop);
                itemTouchHelper.onExternalDragStop(/* recoverItem= */ false);

                if (mLastDraggedGroupId != null) {
                    Token groupId = mLastDraggedGroupId;
                    mLastDraggedGroupId = null;
                    TabModel tabModel = mTabModelSelector.getCurrentModel();
                    if (tabModel != null) {
                        // Always expand tab group on drop.
                        tabModel.setTabGroupCollapsed(
                                groupId, /* isCollapsed= */ false, /* animate= */ false);
                    }
                }
                return true;
            }

            @Override
            public boolean handleDrop(View view, float xPx, float yPx) {
                float[] coords = toRvCoordinates(view, xPx, yPx);
                return handleDrop(coords[0], coords[1]);
            }

            @Override
            public int handleInternalDragEnd() {
                updateSingleTabListMinHeight(model, /* useMinHeight= */ false);
                itemTouchHelper.stopInternalDrag();
                mLastDraggedGroupId = null;
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

        if (!VerticalTabUtils.isAutoResizeEnabled()) {
            return DEFAULT_GRID_SPAN_COUNT;
        }

        int containerWidth = mContainerView.getWidth();
        if (containerWidth <= 0) return DEFAULT_GRID_SPAN_COUNT;

        int paddingStart = mContainerView.getPaddingStart();
        int paddingEnd = mContainerView.getPaddingEnd();
        int availableWidth = containerWidth - paddingStart - paddingEnd;

        Resources res = mContainerView.getContext().getResources();
        int minItemWidth = res.getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_min_width);
        int minHorizontalGap = res.getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_gap);
        if (minItemWidth <= 0) return DEFAULT_GRID_SPAN_COUNT;

        float spansFittingWidth =
                (float) (availableWidth + minHorizontalGap) / (minItemWidth + minHorizontalGap)
                        + SPAN_CALCULATION_EPSILON;
        int maxFitSpans =
                Math.clamp((int) Math.floor(spansFittingWidth), 1, MAX_SINGLE_ROW_SPAN_COUNT);

        int pinnedTabCount = mPinnedTabsModelList != null ? mPinnedTabsModelList.size() : 0;
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

    private void updatePinnedLayoutSpanCount() {
        if (mPinnedLayoutManager == null) return;
        mPinnedLayoutManager.setSpanCount(getSpanCount());
        mPinnedTabsRecyclerView.invalidateItemDecorations();
    }

    /**
     * Distributes inter-item horizontal gaps evenly across grid columns without outer margins,
     * ensuring identical visual item widths since RecyclerView does not support layout_weight.
     */
    private void calculatePinnedTabItemOffsets(Rect outRect, View view, RecyclerView parent) {
        if (!VerticalTabUtils.isAutoResizeEnabled()) {
            outRect.set(0, 0, 0, 0);
            return;
        }
        int position = parent.getChildAdapterPosition(view);
        if (position == RecyclerView.NO_POSITION) {
            position = parent.indexOfChild(view);
        }
        if (position == RecyclerView.NO_POSITION) return;
        int spanCount = mPinnedLayoutManager.getSpanCount();
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
        if (view.getLayoutParams() instanceof GridLayoutManager.LayoutParams gridLp
                && gridLp.getSpanIndex() >= 0) {
            column = gridLp.getSpanIndex();
        }
        int left = column * minHorizontalGap / spanCount;
        int right = minHorizontalGap - (column + 1) * minHorizontalGap / spanCount;
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        outRect.left = isRtl ? right : left;
        outRect.right = isRtl ? left : right;
    }

    private boolean openContextMenuForFocusedItem(RecyclerView recyclerView) {
        View focusedChild = recyclerView.findFocus();
        if (focusedChild == null) return false;

        View itemView = recyclerView.findContainingItemView(focusedChild);
        if (itemView == null) return false;

        int position = recyclerView.getChildAdapterPosition(itemView);
        if (position == RecyclerView.NO_POSITION) return false;

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) return false;

        boolean menuShown =
                showMenuForAdapterPosition(
                        getItemViewAnchorRectProvider(itemView), activity, recyclerView, position);
        if (menuShown) {
            itemView.performHapticFeedback(HapticFeedbackConstants.CONTEXT_CLICK);
        }
        return menuShown;
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
            showTabItemContextMenu(rectProvider, activity, tabId);
            return true;
        } else if (resolvedItemViewType == UiType.TAB_GROUP) {
            Token tabGroupId = item.model.get(TabProperties.TAB_GROUP_HEADER_ID);
            if (tabGroupId != null) {
                showTabGroupHeaderContextMenu(rectProvider, tabGroupId);
                return true;
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
        int index = mModelList.indexFromTabGroupId(tabGroupId);
        if (index == TabModel.INVALID_TAB_INDEX) return;

        RecyclerView.ViewHolder holder = mRecyclerView.findViewHolderForAdapterPosition(index);
        if (holder == null) return;

        showTabGroupHeaderContextMenu(getItemViewAnchorRectProvider(holder.itemView), tabGroupId);
    }

    private void showTabGroupHeaderContextMenu(RectProvider rectProvider, Token tabGroupId) {
        if (mTabGroupContextMenuCoordinator == null) {
            mTabGroupContextMenuCoordinator =
                    TabGroupContextMenuCoordinator.createContextMenuCoordinator(
                            mTabModelSelector.getCurrentModel(),
                            mMultiInstanceManager,
                            mWindowAndroid,
                            mDataSharingTabManager,
                            /* reorderFunction= */ (info, toLeft) -> {
                                // TODO(crbug.com/521982129): Implement tab reordering for a11y.
                            },
                            TabClosingSource.VERTICAL_TAB_STRIP,
                            TabStripLayoutType.VERTICAL);
        }
        mTabHoverCardController.hideHoverCard();
        mTabGroupContextMenuCoordinator.showMenu(rectProvider, tabGroupId);
    }

    private void showTabItemContextMenu(RectProvider rectProvider, Activity activity, int tabId) {
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        List<Integer> allTabIds;
        if (VerticalTabUtils.isMultiSelectEnabled()
                && TabMultiSelectHelper.hasMultipleTabsSelected(tabModel)
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
                            /* reorderFunction= */ (info, toLeft) -> {
                                // TODO(crbug.com/521982129): Implement tab reordering for a11y.
                            },
                            mSnackbarManager,
                            mActivityResultTracker,
                            /* modalDialogManager= */ mWindowAndroid.getModalDialogManager(),
                            TabClosingSource.VERTICAL_TAB_STRIP,
                            mCanActivateTabLayoutToggleMenuSupplier,
                            TabStripLayoutType.VERTICAL);
        }
        mTabHoverCardController.hideHoverCard();
        mTabContextMenuCoordinator.showMenu(rectProvider, anchorInfo);
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

                // Intercept mouse right-clicks. While setOnContextClickListener works for
                // empty background space (where no child views capture the event), actual
                // tab row child views swallow right-clicks internally without bubbling them
                // up to the parent (recyclerView), causing
                // recyclerView.setOnContextClickListener to be skipped.
                if ((e.getButtonState() & MotionEvent.BUTTON_SECONDARY) != 0) {
                    View childView = rv.findChildViewUnder(e.getX(), e.getY());
                    if (childView != null) {
                        handleContextMenuInteraction(activity, rv, e.getX(), e.getY());
                        return true;
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
            if (targetView instanceof RecyclerView rv) {
                return handleContextMenuInteraction(
                        activity, rv, mLastTouchPoint.x, mLastTouchPoint.y);
            } else {
                showEmptySpaceContextMenu(
                        calculateTouchAnchor(targetView, mLastTouchPoint.x, mLastTouchPoint.y),
                        activity);
                return true;
            }
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

    private View buildGridCardDragShadow(Activity activity, PropertyModel model) {
        ViewGroup gridCardView =
                (ViewGroup)
                        LayoutInflater.from(activity).inflate(R.layout.tab_grid_card_item, null);

        for (PropertyKey key : model.getAllSetProperties()) {
            TabGridViewBinder.bindTab(model, gridCardView, key);
        }

        // Force layout the grid card at standard phone width/height ratios
        Resources res = activity.getResources();
        float maxTabWidthDp = TabUiThemeUtil.getMaxTabStripTabWidthDp();
        int width = (int) (maxTabWidthDp * res.getDisplayMetrics().density);
        int height = (int) (maxTabWidthDp * res.getDisplayMetrics().density);
        // TODO(crbug.com/518307037): @jthiesen to update comment explaining why this manual layout
        // is necessary.
        gridCardView.setLayoutParams(new ViewGroup.MarginLayoutParams(width, height));
        gridCardView.measure(
                View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
        gridCardView.layout(0, 0, width, height);

        return gridCardView;
    }

    private View buildGroupHeaderDragShadow(Activity activity, PropertyModel model) {
        ViewGroup groupHeaderView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_group_header, null);

        for (PropertyKey key : model.getAllSetProperties()) {
            TabVerticalViewBinder.bindTabGroupHeader(model, groupHeaderView, key);
        }

        Resources res = activity.getResources();
        float maxTabWidthDp = TabUiThemeUtil.getMaxTabStripTabWidthDp();
        int width = (int) (maxTabWidthDp * res.getDisplayMetrics().density);
        int height =
                res.getDimensionPixelSize(
                        VerticalTabUtils.isTablet(activity)
                                ? R.dimen.vertical_tab_item_height_tablet
                                : R.dimen.vertical_tab_item_height);

        groupHeaderView.setLayoutParams(new ViewGroup.MarginLayoutParams(width, height));
        groupHeaderView.measure(
                View.MeasureSpec.makeMeasureSpec(width, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(height, View.MeasureSpec.EXACTLY));
        groupHeaderView.layout(0, 0, width, height);

        return groupHeaderView;
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
}
