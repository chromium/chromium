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
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.TabStripLayoutType;
import org.chromium.chrome.browser.compositor.overlays.strip.TabGroupContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripContextMenuCoordinator;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.dragdrop.ChromeDragAndDropBrowserDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
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
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabComponentId;
import org.chromium.chrome.browser.tasks.tab_management.TabGridViewBinder;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListConfigDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListItemOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherBackPressHandlerManager;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherDragHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardHelper.TabHoverCardListener;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragAndDropDelegateImpl;
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
    private static @Nullable Supplier<TabSwitcherDragHandler>
            sTabSwitcherDragHandlerSupplierForTesting;
    private final VerticalTabRailLayout mContainerView;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabListModel mModelList;
    private final TabListMediator mMediator;
    private final TabListRecyclerView mRecyclerView;
    private final TabListModel mPinnedTabsModelList;
    private final StaticPinnedTabsMediator mPinnedTabsMediator;
    private final TabListRecyclerView mPinnedTabsRecyclerView;
    private final SimpleRecyclerViewAdapter mPinnedTabsAdapter;
    private final GridLayoutManager mPinnedLayoutManager;
    private final TabModelSelector mTabModelSelector;
    private final WindowAndroid mWindowAndroid;
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
    private final SettableNonNullObservableSupplier<@RailCollapseState Integer>
            mRailCollapseStateSupplier;
    private final Callback<Boolean> mActiveObserver = this::setActive;
    private final PropertyModel mContainerModel;
    private final List<TabSwitcherDragHandler> mTabSwitcherDragHandlers = new ArrayList<>();
    private final View.OnLayoutChangeListener mContainerLayoutChangeListener;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final @Nullable AppHeaderObserver mAppHeaderObserver;
    private final @Nullable BooleanSupplier mCanActivateTabLayoutToggleMenuSupplier;
    private final @Nullable TabHoverCardListener mTabHoverCardListener;
    private final @Nullable ViewStub mTabHoverCardViewStub;
    private @Nullable TabStripContextMenuCoordinator mTabStripContextMenuCoordinator;
    private @Nullable TabContextMenuCoordinator mTabContextMenuCoordinator;
    private @Nullable TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;
    private @Nullable RailCollapseListener mRailCollapseListener;
    private @Nullable TabHoverCardView mTabHoverCardView;
    private @Nullable Token mLastDraggedGroupId;
    private @Nullable VerticalTabListItemTouchHelperCallback mMainTouchHelperCallback;

    private boolean mIsActive;

    /** Listener for collapse state changes. */
    interface RailCollapseListener {
        void onRailCollapseStateChangeRequested(@RailCollapseState int targetState);
    }

    private class VerticalTabListClickHandler implements TabListItemOnClickListenerProvider {
        private final TabActionListener mTabGroupClickedListener =
                new TabActionListener() {
                    @Override
                    public void run(
                            View view, int tabId, @Nullable MotionEventInfo triggeringMotion) {
                        toggleTabGroupExpansion(tabId);
                    }

                    @Override
                    public void run(
                            View view, String syncId, @Nullable MotionEventInfo triggeringMotion) {
                        // Intentional no-op.
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
        public void onTabSelecting(int tabId, boolean fromActionButton) {
            // TODO(crbug.com/509226293): Coordinate tab selection with smooth side panel
            // dismissal or collapse animations when running on narrow screens.
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
            // Vertical Tabs group header cards act strictly as accordion expansion toggles
            // and do not display any action button (neither close nor overflow menu).
            return null;
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    public VerticalTabListCoordinator(
            Activity activity,
            TabModelSelector tabModelSelector,
            Profile profile,
            VerticalTabsActionDelegate verticalTabsActionDelegate,
            WindowAndroid windowAndroid,
            MultiInstanceManager multiInstanceManager,
            SnackbarManager snackbarManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            DataSharingTabManager dataSharingTabManager,
            NonNullObservableSupplier<Boolean> verticalTabsActiveSupplier,
            SettableNonNullObservableSupplier<Integer> verticalTabsWidthSupplier,
            @Nullable BooleanSupplier canActivateTabLayoutToggleMenuSupplier,
            @Nullable ViewStub tabHoverCardViewStub,
            Supplier<TabContentManager> tabContentManagerSupplier) {
        mCanActivateTabLayoutToggleMenuSupplier = canActivateTabLayoutToggleMenuSupplier;
        mVerticalTabsActiveSupplier = verticalTabsActiveSupplier;
        mTabModelSelector = tabModelSelector;
        mWindowAndroid = windowAndroid;
        mMultiInstanceManager = multiInstanceManager;
        mSnackbarManager = snackbarManager;
        mShareDelegateSupplier = shareDelegateSupplier;
        mDataSharingTabManager = dataSharingTabManager;
        mTabHoverCardViewStub = tabHoverCardViewStub;
        mTabHoverCardListener = this::showOrHideTabHoverCard;
        mRailCollapseStateSupplier = ObservableSuppliers.createNonNull(RailCollapseState.EXPANDED);
        mModelList = new TabListModel();

        if (tabHoverCardViewStub != null) {
            tabHoverCardViewStub.setOnInflateListener(
                    (viewStub, view) -> {
                        mTabHoverCardView = (TabHoverCardView) view;
                        @SuppressWarnings("NullAway")
                        Supplier<@Nullable TabContentManager> nullableSupplier =
                                tabContentManagerSupplier;

                        mTabHoverCardView.initialize(mTabModelSelector, nullableSupplier);
                    });
        }
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

        TabListRecyclerView recyclerView = mContainerView.getRecyclerView();
        mRecyclerView = recyclerView;
        mContainerView.initRecyclerView(adapter);

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

        View gridButton = mContainerView.findViewById(R.id.grid_button);
        if (gridButton != null) {
            gridButton.setOnTouchListener(createLocalCoordinateTrackingTouchListener());
            gridButton.setOnContextClickListener(
                    createEmptySpaceContextClickListener(activity, gridButton));
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

        TabListConfigDelegate tabListConfigDelegate =
                new TabListConfigDelegate() {
                    @Override
                    public @TabListLayoutType int getLayoutType() {
                        return TabListLayoutType.NESTED;
                    }

                    @Override
                    public boolean supportsMessageCards() {
                        return false;
                    }

                    @Override
                    public @Nullable NonNullObservableSupplier<@RailCollapseState Integer>
                            getRailCollapseStateSupplier() {
                        return mRailCollapseStateSupplier;
                    }

                    @Override
                    public @Nullable TabHoverCardListener getTabHoverCardListener() {
                        return mTabHoverCardListener;
                    }
                };

        // TODO(crbug.com/527641177): Persist rail collapse state in SharedPreferences.
        mContainerModel =
                new PropertyModel.Builder(VerticalTabListProperties.ALL_KEYS)
                        .with(
                                VerticalTabListProperties.ON_GRID_CLICK_LISTENER,
                                v -> verticalTabsActionDelegate.openHubPane(PaneId.TAB_GROUPS))
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
                                VerticalTabListProperties.ON_COLLAPSE_CLICK_LISTENER,
                                v -> toggleCollapseState())
                        .with(
                                VerticalTabListProperties.EXPAND_OR_COLLAPSE_ON_HOVER_LISTENER,
                                this::expandOrCollapseOnHover)
                        .with(VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED, true)
                        .with(VerticalTabListProperties.COLLAPSE_STATE, RailCollapseState.EXPANDED)
                        .build();
        PropertyModelChangeProcessor.create(
                mContainerModel, mContainerView, VerticalTabListViewBinder::bind);

        mMediator =
                new TabListMediator(
                        activity,
                        mModelList,
                        TabListMode.VERTICAL,
                        /* modalDialogManager */ null,
                        tabModelSelector.getCurrentTabModelSupplier(),
                        /* thumbnailProvider */ null,
                        mTabListFaviconProvider,
                        /* selectionDelegateProvider */ null,
                        new VerticalTabListClickHandler(),
                        tabListConfigDelegate,
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
                        /* onDragStateChangedListener */ () -> {});

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

    public void destroy() {
        mPinnedTabsMediator.destroy();
        mPinnedTabsRecyclerView.setAdapter(null);
        mMediator.destroy();
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

        if (mTabHoverCardView != null) {
            mTabHoverCardView.destroy();
            mTabHoverCardView = null;
        }

        if (mContainerView != null) {
            mContainerView.removeOnLayoutChangeListener(mContainerLayoutChangeListener);
        }

        mRailCollapseListener = null;
        mLastDraggedGroupId = null;
    }

    /**
     * Sets the listener to be notified when the rail's collapsed state changes.
     *
     * @param listener The listener to receive collapse state change events.
     */
    public void setCollapseListener(@Nullable RailCollapseListener listener) {
        mRailCollapseListener = listener;
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
        mPinnedLayoutManager.setSpanCount(
                railCollapseState == RailCollapseState.COLLAPSED ? 1 : getSpanCount());
        mRailCollapseStateSupplier.set(railCollapseState);
    }

    /**
     * Sets whether the rail collapse button is enabled.
     *
     * @param enabled True if the collapse button should be enabled, false otherwise.
     */
    void setCollapseButtonEnabled(boolean enabled) {
        mContainerModel.set(VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED, enabled);
    }

    private void setActive(boolean isActive) {
        mIsActive = isActive;
        if (mIsActive) {
            scrollActiveTabIntoView();
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
            mContainerView.scrollToPositionWithOffset(uiIndex);
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

    private void resetWithListOfTabs(@Nullable TabModel tabModel) {
        if (tabModel == null) return;

        mMediator.resetWithListOfTabs(
                tabModel.getRepresentativeTabList(),
                /* tabGroupSyncIds */ null,
                /* quickMode */ false);
        mPinnedTabsMediator.updateTabModel(tabModel);
    }

    private void handleNewTabButtonClick() {
        TabModel model = mTabModelSelector.getCurrentModel();

        if (!model.isIncognitoBranded()) model.commitAllTabClosures();
        TabCreatorUtil.launchNtp(model.getTabCreator());
        RecordUserAction.record("MobileNewTabOpened.VerticalTabs");
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
    }

    private void requestRailCollapseStateChange(@RailCollapseState int targetState) {
        if (mContainerModel.get(VerticalTabListProperties.COLLAPSE_STATE) == targetState) return;

        if (mRailCollapseListener != null) {
            mRailCollapseListener.onRailCollapseStateChangeRequested(targetState);
        } else {
            setRailCollapseState(targetState);
        }
    }

    private void toggleCollapseState() {
        if (!mContainerModel.get(VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED)) {
            return;
        }

        @RailCollapseState
        int currentState = mContainerModel.get(VerticalTabListProperties.COLLAPSE_STATE);
        @RailCollapseState
        int targetState =
                currentState == RailCollapseState.EXPANDED
                        ? RailCollapseState.COLLAPSED
                        : RailCollapseState.EXPANDED;
        RecordHistogram.recordBooleanHistogram(
                "Android.VerticalTabs.RailCollapsed", targetState == RailCollapseState.COLLAPSED);
        requestRailCollapseStateChange(targetState);
    }

    private void expandOrCollapseOnHover(@RailCollapseState int targetState) {
        @RailCollapseState
        int currentState = mContainerModel.get(VerticalTabListProperties.COLLAPSE_STATE);
        if (currentState != RailCollapseState.EXPANDED_FOR_HOVERING
                && currentState != RailCollapseState.COLLAPSED) {
            return;
        }

        requestRailCollapseStateChange(targetState);
    }

    private void showOrHideTabHoverCard(int tabId, View view, boolean isHovered) {
        if (isHovered) {
            // Skip showing for the selected tab.
            if (mTabModelSelector.getCurrentTabId() == tabId) return;

            if (mTabHoverCardViewStub != null && mTabHoverCardViewStub.getParent() != null) {
                mTabHoverCardViewStub.inflate();
            }
            if (mTabHoverCardView == null) return;

            Tab tab = mTabModelSelector.getTabById(tabId);
            if (tab == null) return;

            float[] position =
                    VerticalTabHoverCardHelper.getHoverCardPosition(
                            view, mContainerView, mTabHoverCardView);
            mTabHoverCardView.show(tab, position[0], position[1]);
        } else {
            if (mTabHoverCardView != null) {
                mTabHoverCardView.hide();
            }
        }
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
                        tabModelSelector.getCurrentTabModelSupplier().asNonNull());
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
                    showMenuForAdapterPosition(activity, recyclerView, trueChildView, position);

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

        touchHelperCallback.setOnDragOutListener(
                (viewHolder, dX, dY) -> {
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
                        if (!VerticalTabUtils.isGroupHeaderDragEnabled()) {
                            return;
                        }

                        Token tabGroupId =
                                assumeNonNull(model.get(TabProperties.TAB_GROUP_HEADER_ID));

                        // Do not allow dragging out a tab group if it contains all tabs in the
                        // window.
                        if (tabModel.getCount() == tabModel.getTabsInGroup(tabGroupId).size()) {
                            return;
                        }

                        itemTouchHelper.setExternalDragItem(viewHolder);
                        dragHandler.setDragHandlerDelegate(
                                createDragHandlerDelegate(itemTouchHelper));

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
                    dragHandler.setDragHandlerDelegate(createDragHandlerDelegate(itemTouchHelper));

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

    private TabSwitcherDragHandler.DragHandlerDelegate createDragHandlerDelegate(
            ItemTouchHelper2 itemTouchHelper) {
        return new TabSwitcherDragHandler.DragHandlerDelegate() {
            @Override
            public boolean handleDragStart(float xPx, float yPx) {
                itemTouchHelper.onExternalDragStart(xPx, yPx, /* hideItemWhileDragging= */ true);

                // Since the OS-level drag-and-drop only initiates after the cursor has moved
                // outside the bounds of the RecyclerView, we will never receive an
                // ACTION_DRAG_EXITED event. Therefore, we must explicitly trigger the collapse of
                // the drag gap right away.
                itemTouchHelper.clearExternalDragItemVisibility();
                return true;
            }

            @Override
            public boolean handleDragLocation(float xPx, float yPx) {
                itemTouchHelper.onExternalDragLocation(xPx, yPx);
                return true;
            }

            @Override
            public boolean handleDragEnter() {
                itemTouchHelper.restoreExternalDragItemVisibility(/* isOSNewWindowDrop= */ false);
                return true;
            }

            @Override
            public boolean handleDragExit() {
                itemTouchHelper.clearExternalDragItemVisibility();
                return true;
            }

            @Override
            public boolean handleExternalDragEnd(float xPx, float yPx, boolean isOSNewWindowDrop) {
                itemTouchHelper.restoreExternalDragItemVisibility(isOSNewWindowDrop);
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
            public int handleInternalDragEnd() {
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

    /** Returns the default grid column span count for the Left Rail. */
    private int getSpanCount() {
        // TODO(crbug.com/509226293): When the Left Rail becomes collapsible or resizable, the span
        // count must be calculated dynamically based on the measured width of the container.
        return DEFAULT_GRID_SPAN_COUNT;
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

        // If childView is null, the coordinates landed on an empty space. Launch empty space menu.
        if (childView == null) {
            showEmptySpaceContextMenu(activity, recyclerView, localX, localY);
            return true;
        }
        int position = recyclerView.getChildAdapterPosition(childView);
        return showMenuForAdapterPosition(activity, recyclerView, childView, position);
    }

    private boolean showMenuForAdapterPosition(
            Activity activity, RecyclerView recyclerView, View childView, int position) {
        if (position == RecyclerView.NO_POSITION) return false;

        TabListModel modelList =
                (recyclerView == mPinnedTabsRecyclerView) ? mPinnedTabsModelList : mModelList;

        ListItem item = modelList.get(position);
        int resolvedItemViewType =
                assumeNonNull(recyclerView.getAdapter()).getItemViewType(position);
        if (resolvedItemViewType == UiType.TAB || resolvedItemViewType == UiType.PINNED_TAB) {
            // The user clicked directly on a tab item (regular tab, pinned tab, or child tab).
            int tabId = TabProperties.getTabId(item.model);
            showTabItemContextMenu(activity, recyclerView, childView, tabId);
            return true;
        } else if (resolvedItemViewType == UiType.TAB_GROUP) {
            Token tabGroupId = item.model.get(TabProperties.TAB_GROUP_HEADER_ID);
            if (tabGroupId != null) {
                showTabGroupHeaderContextMenu(childView, tabGroupId);
                return true;
            }
        }
        return false;
    }

    private void showTabGroupHeaderContextMenu(View itemView, Token tabGroupId) {
        RectProvider rectProvider = getAnchorRectProvider(mRecyclerView, itemView);
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
        mTabGroupContextMenuCoordinator.showMenu(rectProvider, tabGroupId);
    }

    private void showTabItemContextMenu(
            Activity activity, RecyclerView recyclerView, View itemView, int tabId) {
        RectProvider rectProvider = getAnchorRectProvider(recyclerView, itemView);
        List<Integer> allTabIds = List.of(tabId);
        var anchorInfo = new TabContextMenuCoordinator.AnchorInfo(tabId, allTabIds);

        if (mTabContextMenuCoordinator == null) {
            TabGroupCreationCallback tabGroupCreationCallback =
                    (newTabGroupId) -> {
                        if (newTabGroupId != null) {
                            showTabGroupHeaderContextMenu(itemView, newTabGroupId);
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
                            /* activityResultTracker= */ null,
                            /* modalDialogManager= */ mWindowAndroid.getModalDialogManager(),
                            TabClosingSource.VERTICAL_TAB_STRIP,
                            mCanActivateTabLayoutToggleMenuSupplier,
                            TabStripLayoutType.VERTICAL);
        }
        mTabContextMenuCoordinator.showMenu(rectProvider, anchorInfo);
    }

    private void showEmptySpaceContextMenu(
            Activity activity, View targetView, float localX, float localY) {
        RectProvider rectProvider = calculateTouchAnchor(targetView, localX, localY);
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
        mTabStripContextMenuCoordinator.showMenu(rectProvider, isIncognito, activity);
    }

    private RectProvider getAnchorRectProvider(RecyclerView recyclerView, View itemView) {
        if (mLastTouchPoint.x != 0 || mLastTouchPoint.y != 0) {
            return calculateTouchAnchor(recyclerView, mLastTouchPoint.x, mLastTouchPoint.y);
        }

        // Fallback: Create a precise bounding box wrapped around the tab item.
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
                    showEmptySpaceContextMenu(activity, targetView, e.getX(), e.getY());
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
                        activity, targetView, mLastTouchPoint.x, mLastTouchPoint.y);
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
                res.getDimensionPixelSize(R.dimen.vertical_tab_action_button_touch_target_height);

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

    NonNullObservableSupplier<@RailCollapseState Integer> getRailCollapseStateSupplierForTesting() {
        return mRailCollapseStateSupplier;
    }

    @Nullable TabHoverCardListener getTabHoverCardListenerForTesting() {
        return mTabHoverCardListener;
    }
}
