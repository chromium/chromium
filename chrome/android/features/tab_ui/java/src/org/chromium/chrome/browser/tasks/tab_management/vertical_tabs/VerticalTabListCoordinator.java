// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabGroupContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripContextMenuCoordinator;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabSelectionType;
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
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListConfigDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListItemOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager.AppHeaderObserver;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;
import org.chromium.ui.widget.RectProvider;

import java.util.List;
import java.util.function.Supplier;

/** Coordinator to manage and display the Vertical Tab List. */
@NullMarked
public class VerticalTabListCoordinator {
    static final int DEFAULT_GRID_SPAN_COUNT = 4;
    private static final float SCROLL_OFFSET_DIVISOR = 4f;
    private final ViewGroup mContainerView;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabListModel mModelList;
    private final TabListMediator mMediator;
    private final TabListRecyclerView mRecyclerView;
    private final TabListModel mPinnedTabsModelList;
    private final StaticPinnedTabsMediator mPinnedTabsMediator;
    private final TabListRecyclerView mPinnedTabsRecyclerView;
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
    private final View mSpacerView;
    private final VerticalTabGroupSpineDecoration mSpineDecoration;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final @Nullable AppHeaderObserver mAppHeaderObserver;
    private final TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private final NonNullObservableSupplier<Boolean> mVerticalTabsActiveSupplier;
    private final Callback<Boolean> mActiveObserver = this::setActive;
    private @Nullable TabStripContextMenuCoordinator mTabStripContextMenuCoordinator;
    private @Nullable TabContextMenuCoordinator mTabContextMenuCoordinator;
    private @Nullable TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;
    private boolean mIsActive;

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
            NonNullObservableSupplier<Boolean> verticalTabsActiveSupplier) {
        mVerticalTabsActiveSupplier = verticalTabsActiveSupplier;
        mModelList = new TabListModel();
        SimpleRecyclerViewAdapter adapter =
                new SimpleRecyclerViewAdapter(mModelList) {
                    @Override
                    public int getItemViewType(int position) {
                        ListItem item = mModelList.get(position);
                        if (item.type == UiType.TAB) {
                            if (item.model.get(TabProperties.IS_PINNED)) {
                                return UiType.PINNED_TAB;
                            } else if (item.model.get(TabProperties.TAB_GROUP_HEADER_ID) != null) {
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
                                        .inflate(R.layout.vertical_tab_item, parent, false),
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
                                        .inflate(R.layout.vertical_tab_group_header, parent, false),
                TabVerticalViewBinder::bindTabGroupHeader);

        mContainerView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_layout, null, false);
        mContainerView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        mSpacerView = mContainerView.findViewById(R.id.desktop_window_spacer);

        TabListRecyclerView recyclerView = mContainerView.findViewById(R.id.tab_list_recycler_view);
        mRecyclerView = recyclerView;

        LinearLayoutManager layoutManager =
                new LinearLayoutManager(activity, LinearLayoutManager.VERTICAL, false);

        recyclerView.setLayoutManager(layoutManager);
        recyclerView.setAdapter(adapter);
        recyclerView.setupCustomItemAnimator(/* useClipAnimations= */ true);
        recyclerView.setVisibility(View.VISIBLE);

        // Create the gesture detector to catch long-presses on VT empty space.
        GestureDetector gestureDetector =
                new GestureDetector(
                        activity,
                        new GestureDetector.SimpleOnGestureListener() {
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

                                View childView =
                                        recyclerView.findChildViewUnder(e.getX(), e.getY());
                                if (childView != null) {
                                    // Allow ItemTouchHelper2 + Orchestrator to manage it instead.
                                    return;
                                }

                                handleContextMenuInteraction(
                                        activity, recyclerView, e.getX(), e.getY());
                            }
                        });

        // Item Touch Listeners intercept all incoming window events before they are sent down to
        // the child views.
        recyclerView.addOnItemTouchListener(
                new RecyclerView.SimpleOnItemTouchListener() {
                    @Override
                    public boolean onInterceptTouchEvent(RecyclerView recyclerView, MotionEvent e) {
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
                            View childView = recyclerView.findChildViewUnder(e.getX(), e.getY());

                            if (childView != null) {
                                handleContextMenuInteraction(
                                        activity, recyclerView, e.getX(), e.getY());
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
                });

        // Handles right-click for empty space context menu.
        recyclerView.setOnContextClickListener(
                v ->
                        handleContextMenuInteraction(
                                activity, recyclerView, mLastTouchPoint.x, mLastTouchPoint.y));

        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        activity,
                        TabListMode.VERTICAL,
                        R.dimen.default_favicon_corner_radius,
                        TabFavicon::getBitmap);

        setupItemTouchHelper(
                activity,
                recyclerView,
                mModelList,
                tabModelSelector.getCurrentTabModelSupplier().asNonNull());

        mSpineDecoration =
                new VerticalTabGroupSpineDecoration(
                        activity, recyclerView::postInvalidate, mModelList, tabModelSelector);
        recyclerView.addItemDecoration(mSpineDecoration);

        mTabModelSelector = tabModelSelector;
        mWindowAndroid = windowAndroid;
        mMultiInstanceManager = multiInstanceManager;
        mSnackbarManager = snackbarManager;
        mShareDelegateSupplier = shareDelegateSupplier;
        mDataSharingTabManager = dataSharingTabManager;

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
                };

        PropertyModel model =
                new PropertyModel.Builder(VerticalTabListProperties.ALL_KEYS)
                        .with(
                                VerticalTabListProperties.ON_GRID_CLICK_LISTENER,
                                v -> verticalTabsActionDelegate.openHubPane(PaneId.TAB_GROUPS))
                        .with(
                                VerticalTabListProperties.ON_SEARCH_CLICK_LISTENER,
                                v -> verticalTabsActionDelegate.openTabSearch())
                        .with(
                                VerticalTabListProperties.ON_NEW_TAB_CLICK_LISTENER,
                                v -> handleNewTabButtonClick())
                        .build();
        PropertyModelChangeProcessor.create(model, mContainerView, VerticalTabListViewBinder::bind);

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
        TabListRecyclerView pinnedTabsRecyclerView =
                mContainerView.findViewById(R.id.pinned_tabs_recycler_view);
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
                                        .inflate(R.layout.vertical_tab_pinned_item, parent, false),
                TabVerticalViewBinder::bindPinnedTab);

        pinnedTabsRecyclerView.setAdapter(pinnedTabsAdapter);
        pinnedTabsRecyclerView.setLayoutManager(new GridLayoutManager(activity, getSpanCount()));

        pinnedTabsRecyclerView.addOnItemTouchListener(
                new RecyclerView.SimpleOnItemTouchListener() {
                    @Override
                    public boolean onInterceptTouchEvent(RecyclerView rv, MotionEvent e) {
                        if (e.getActionMasked() == MotionEvent.ACTION_DOWN) {
                            mLastTouchPoint.set((int) e.getX(), (int) e.getY());
                        }

                        if ((e.getButtonState() & MotionEvent.BUTTON_SECONDARY) != 0) {
                            handleContextMenuInteraction(activity, rv, e.getX(), e.getY());
                            return true;
                        }
                        return false;
                    }
                });

        // TODO(crbug.com/509226293): Create a lightweight touch helper for pinned tabs if needed.
        // Setup drag-and-drop reordering. Reuses the vertical tab touch helper since pinned tabs
        // can be reordered in 2D, and movements translate directly back to TabModel moves.
        setupItemTouchHelper(
                activity,
                pinnedTabsRecyclerView,
                pinnedTabsModelList,
                tabModelSelector.getCurrentTabModelSupplier().asNonNull());

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
            RecyclerView.LayoutManager layoutManager = mRecyclerView.getLayoutManager();
            if (layoutManager instanceof LinearLayoutManager linearLayoutManager) {
                // Wait for the RecyclerView to finish drawing before scrolling, to prevent
                // incorrect visible item positions.
                mRecyclerView.post(
                        () -> {
                            int first =
                                    linearLayoutManager.findFirstCompletelyVisibleItemPosition();
                            int last = linearLayoutManager.findLastCompletelyVisibleItemPosition();

                            // Only scroll if the active tab is not fully visible on screen.
                            if (uiIndex < first || uiIndex > last) {
                                // Add an offset so the tab doesn't appear flush against the top or
                                // bottom edge.
                                int offset =
                                        Math.round(
                                                mRecyclerView.getHeight() / SCROLL_OFFSET_DIVISOR);
                                linearLayoutManager.scrollToPositionWithOffset(
                                        uiIndex, Math.max(0, offset));
                            }
                        });
            }
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
    }

    private void updatePinnedTabsVisibility() {
        mPinnedTabsRecyclerView.setVisibility(
                mPinnedTabsModelList.isEmpty() ? View.GONE : View.VISIBLE);
    }

    private void setupItemTouchHelper(
            Activity activity,
            RecyclerView recyclerView,
            TabListModel modelList,
            Supplier<TabModel> tabModelSupplier) {
        VerticalTabListItemTouchHelperCallback touchHelperCallback =
                new VerticalTabListItemTouchHelperCallback(activity, modelList, tabModelSupplier);

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

        itemTouchHelper.attachToRecyclerView(recyclerView);
        touchHelperCallback.setRecyclerView(recyclerView);

        recyclerView.addOnItemTouchListener(
                VerticalTabListItemTouchHelperCallback.createAfterOnItemTouchListener(
                        touchHelperCallback));
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
            int tabId = item.model.get(TabProperties.TAB_ID);
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
                            TabClosingSource.VERTICAL_TAB_STRIP);
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
                            TabClosingSource.VERTICAL_TAB_STRIP);
        }
        mTabContextMenuCoordinator.showMenu(rectProvider, anchorInfo);
    }

    private void showEmptySpaceContextMenu(
            Activity activity, RecyclerView recyclerView, float localX, float localY) {
        RectProvider rectProvider = calculateTouchAnchor(recyclerView, localX, localY);

        if (mTabStripContextMenuCoordinator == null) {
            mTabStripContextMenuCoordinator =
                    TabStripContextMenuCoordinator.createContextMenuCoordinator(
                            mTabModelSelector.getCurrentModel(),
                            mMultiInstanceManager,
                            mWindowAndroid,
                            mSnackbarManager,
                            this::handleNewTabButtonClick);
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

    private RectProvider calculateTouchAnchor(
            RecyclerView recyclerView, float localX, float localY) {
        // Get the top-left edge pos of the scrollable recycler view relative to the Android
        // application window screen.
        int[] recyclerViewPos = new int[2];
        recyclerView.getLocationInWindow(recyclerViewPos);

        // Calculate window-relative anchor coordinates, where localX and localY are relative to the
        // recycler view.
        int windowX = recyclerViewPos[0] + (int) localX;
        int windowY = recyclerViewPos[1] + (int) localY;

        // Build a precise 1x1 bounding box right under the cursor/pointer.
        Rect anchorRect = new Rect(windowX, windowY, windowX + 1, windowY + 1);
        return new RectProvider(anchorRect);
    }

    void dismissActiveContextMenus() {
        if (mTabStripContextMenuCoordinator != null) mTabStripContextMenuCoordinator.dismiss();
        if (mTabContextMenuCoordinator != null) mTabContextMenuCoordinator.dismiss();
        if (mTabGroupContextMenuCoordinator != null) mTabGroupContextMenuCoordinator.dismiss();
    }

    @Nullable TabStripContextMenuCoordinator getTabStripContextMenuCoordinatorForTesting() {
        return mTabStripContextMenuCoordinator;
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

    private void updateSpacerVisibility(@Nullable AppHeaderState appHeaderState) {
        boolean isInDesktopWindow = appHeaderState != null && appHeaderState.isInDesktopWindow();
        mSpacerView.setVisibility(isInDesktopWindow ? View.VISIBLE : View.GONE);
    }
}
