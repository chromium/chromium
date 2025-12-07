// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.RecyclerViewScroller.smoothScrollToPosition;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ID;

import android.app.Activity;
import android.graphics.PointF;
import android.graphics.Rect;
import android.util.Size;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ItemAnimator.ItemAnimatorFinishedListener;
import androidx.recyclerview.widget.RecyclerView.OnItemTouchListener;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceWelcomeMessageProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemTouchHelperCallback.OnDropOnArchivalMessageCardEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarExplicitTrigger;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.recyclerview.widget.ItemTouchHelper2;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/** Coordinator for showing UI for a list of tabs. Can be used in GRID or STRIP modes. */
@NullMarked
public class TabListCoordinator implements PriceWelcomeMessageProvider, DestroyObserver {
    private static final String TAG = "TabListCoordinator";

    /** Observer interface for the size of tab list items. */
    public interface TabListItemSizeChangedObserver {
        /**
         * Called when the size of the tab list items changes.
         *
         * @param spanCount The number of items which span one row.
         * @param cardSize The size of the tab list item.
         */
        void onSizeChanged(int spanCount, Size cardSize);
    }

    /** Observer interface for the tab drag actions. */
    interface DragObserver {
        void onDragStart();

        void onDragEnd();
    }

    /**
     * Modes of showing the list of tabs.
     *
     * <p>NOTE: STRIP and GRID modes will have height equal to that of the container view.
     */
    @IntDef({TabListMode.GRID, TabListMode.STRIP, TabListMode.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabListMode {
        int GRID = 0;
        int STRIP = 1;
        // int CAROUSEL_DEPRECATED = 2;
        // int LIST_DEPRECATED = 3;
        int NUM_ENTRIES = 4;
    }

    static final int GRID_LAYOUT_SPAN_COUNT_COMPACT = 2;
    static final int GRID_LAYOUT_SPAN_COUNT_MEDIUM = 3;
    static final int GRID_LAYOUT_SPAN_COUNT_LARGE = 4;
    static final int MAX_SCREEN_WIDTH_COMPACT_DP = 600;
    static final int MAX_SCREEN_WIDTH_MEDIUM_DP = 800;
    static final float PERCENTAGE_AREA_OVERLAP_MERGE_THRESHOLD = 0.5f;

    private final ObserverList<TabListItemSizeChangedObserver> mTabListItemSizeChangedObserverList =
            new ObserverList<>();
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabListMediator mMediator;
    private final TabListRecyclerView mRecyclerView;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final @TabListMode int mMode;
    private final Activity mActivity;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final TabListModel mModelList;
    private final ViewGroup mParentView;
    private final @DrawableRes int mEmptyStateImageResId;
    private final @StringRes int mEmptyStateHeadingResId;
    private final @StringRes int mEmptyStateSubheadingResId;
    private final boolean mAllowDragAndDrop;
    private final boolean mAllowDetachingTabsToCreateNewWindows;
    private final @Nullable TabSwitcherDragHandler mTabSwitcherDragHandler;
    private final ObservableSupplier<@Nullable TabGroupModelFilter> mTabGroupModelFilterSupplier;
    private final ObserverList<DragObserver> mDragObserverList = new ObserverList<>();
    private final TabListHighlighter mTabListHighlighter;
    private final TabListMergeAnimationManager mTabListMergeAnimationManager;

    private boolean mIsInitialized;
    private @Nullable OnLayoutChangeListener mListLayoutListener;
    private boolean mLayoutListenerRegistered;
    private @Nullable TabStripSnapshotter mTabStripSnapshotter;
    private @Nullable ItemTouchHelper2 mItemTouchHelper;
    private @Nullable OnItemTouchListener mOnBeforeItemTouchHelperItemTouchListener;
    private @Nullable OnItemTouchListener mOnAfterItemTouchHelperItemTouchListener;
    private @Nullable TabListEmptyCoordinator mTabListEmptyCoordinator;
    private boolean mIsEmptyViewInitialized;
    private @Nullable Runnable mAwaitingLayoutRunnable;
    private int mAwaitingTabId = Tab.INVALID_TAB_ID;
    private @TabActionState int mTabActionState;

    /**
     * Construct a coordinator for UI that shows a list of tabs.
     *
     * @param mode Modes of showing the list of tabs. Can be used in GRID or STRIP.
     * @param activity The activity to use for accessing {@link android.content.res.Resources}.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} for top
     *     controls.
     * @param modalDialogManager Used for managing the modal dialogs.
     * @param tabGroupModelFilterSupplier The supplier for the current tab model filter.
     * @param thumbnailProvider Provider to provide screenshot related details.
     * @param actionOnRelatedTabs Whether tab-related actions should be operated on all related
     *     tabs.
     * @param dataSharingTabManager The service used to initiate data sharing.
     * @param gridCardOnClickListenerProvider Provides the onClickListener for opening dialog when
     *     click on a grid card.
     * @param dialogHandler A handler to handle requests about updating TabGridDialog.
     * @param initialTabActionState The initial {@link TabActionState} to use for the shown tabs.
     *     Must always be CLOSABLE for TabListMode.STRIP.
     * @param selectionDelegateProvider Provider to provide selected Tabs for a selectable tab list.
     *     It's NULL when selection is not possible.
     * @param priceWelcomeMessageControllerSupplier A supplier for a controller to show
     *     PriceWelcomeMessage.
     * @param parentView {@link ViewGroup} The root view of the UI.
     * @param attachToParent Whether the UI should attach to root view.
     * @param componentName A unique string uses to identify different components for UMA recording.
     *     Recommended to use the class name or make sure the string is unique through actions.xml
     *     file.
     * @param onModelTokenChange Callback to invoke whenever a model changes. Only currently
     *     respected in TabListMode.STRIP mode.
     * @param emptyViewParent {@link ViewGroup} The root view of the empty state view.
     * @param emptyImageResId Drawable resource for empty state.
     * @param emptyHeadingStringResId String resource for empty heading.
     * @param emptySubheadingStringResId String resource for empty subheading.
     * @param onTabGroupCreation Runnable invoked on tab group creation
     * @param allowDragAndDrop Whether to allow drag and drop for this tab list coordinator.
     * @param tabSwitcherDragHandler An instance of the {@link TabSwitcherDragHandler}.
     * @param undoBarExplicitTrigger An interface to explicitly trigger the undo closure snackbar.
     */
    TabListCoordinator(
            @TabListMode int mode,
            Activity activity,
            BrowserControlsStateProvider browserControlsStateProvider,
            ModalDialogManager modalDialogManager,
            ObservableSupplier<@Nullable TabGroupModelFilter> tabGroupModelFilterSupplier,
            @Nullable ThumbnailProvider thumbnailProvider,
            boolean actionOnRelatedTabs,
            @Nullable DataSharingTabManager dataSharingTabManager,
            TabListMediator.@Nullable GridCardOnClickListenerProvider
                    gridCardOnClickListenerProvider,
            TabListMediator.@Nullable TabGridDialogHandler dialogHandler,
            @TabActionState int initialTabActionState,
            TabListMediator.@Nullable SelectionDelegateProvider selectionDelegateProvider,
            @Nullable Supplier<@Nullable PriceWelcomeMessageController>
                    priceWelcomeMessageControllerSupplier,
            ViewGroup parentView,
            boolean attachToParent,
            String componentName,
            @Nullable Callback<Object> onModelTokenChange,
            @Nullable ViewGroup emptyViewParent,
            @DrawableRes int emptyImageResId,
            @StringRes int emptyHeadingStringResId,
            @StringRes int emptySubheadingStringResId,
            @Nullable Runnable onTabGroupCreation,
            boolean allowDragAndDrop,
            @Nullable TabSwitcherDragHandler tabSwitcherDragHandler,
            @Nullable UndoBarExplicitTrigger undoBarExplicitTrigger,
            @Nullable SnackbarManager snackbarManager,
            int allowedSelectionCount) {
        mMode = mode;
        mTabActionState = initialTabActionState;
        mActivity = activity;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mModelList = new TabListModel();
        mParentView = parentView;
        mAdapter =
                new SimpleRecyclerViewAdapter(mModelList) {
                    @Override
                    public void onViewRecycled(SimpleRecyclerViewAdapter.ViewHolder viewHolder) {
                        PropertyModel model = viewHolder.model;
                        assumeNonNull(model);
                        if (mMode == TabListMode.GRID) {
                            TabGridViewBinder.onViewRecycled(model, viewHolder.itemView);
                        } else if (mMode == TabListMode.STRIP) {
                            TabStripViewBinder.onViewRecycled(model, viewHolder.itemView);
                        }
                        super.onViewRecycled(viewHolder);
                    }
                };
        mAllowDragAndDrop = allowDragAndDrop;
        mTabSwitcherDragHandler = tabSwitcherDragHandler;
        mTabGroupModelFilterSupplier = tabGroupModelFilterSupplier;
        mAllowDetachingTabsToCreateNewWindows =
                MultiWindowUtils.isMultiInstanceApi31Enabled()
                        && ChromeFeatureList.isEnabled(
                                ChromeFeatureList.TAB_SWITCHER_DRAG_DROP_ANDROID);

        if (mAllowDetachingTabsToCreateNewWindows && mTabSwitcherDragHandler != null) {
            TabSwitcherDragHandler.DragHandlerDelegate dragHandlerDelegate =
                    new TabSwitcherDragHandler.DragHandlerDelegate() {
                        @Override
                        public boolean handleDragStart(float xPx, float yPx) {
                            for (DragObserver observer : mDragObserverList) {
                                observer.onDragStart();
                            }
                            assumeNonNull(mItemTouchHelper);
                            mItemTouchHelper.onExternalDragStart(
                                    xPx, yPx, /* hideItemWhileDragging= */ true);
                            return true;
                        }

                        @Override
                        public boolean handleDragLocation(float xPx, float yPx) {
                            assumeNonNull(mItemTouchHelper);
                            mItemTouchHelper.onExternalDragLocation(xPx, yPx);
                            return true;
                        }

                        @Override
                        public boolean handleDragEnd(float xPx, float yPx) {
                            for (DragObserver observer : mDragObserverList) {
                                observer.onDragEnd();
                            }
                            assumeNonNull(mItemTouchHelper);
                            mItemTouchHelper.onExternalDragStop(/* recoverItem= */ false);
                            return true;
                        }
                    };
            mTabSwitcherDragHandler.setDragHandlerDelegate(dragHandlerDelegate);
        }

        RecyclerView.RecyclerListener recyclerListener = null;
        if (mMode == TabListMode.GRID) {
            mAdapter.registerType(
                    UiType.TAB,
                    parent -> {
                        ViewGroup group =
                                (ViewGroup)
                                        LayoutInflater.from(activity)
                                                .inflate(
                                                        R.layout.tab_grid_card_item,
                                                        parentView,
                                                        false);
                        group.setClickable(true);
                        return group;
                    },
                    TabGridViewBinder::bindTab);
            if (ChromeFeatureList.sAndroidTabDeclutterArchiveTabGroups.isEnabled()) {
                // If the need arises based on diverging functionality between tabs and tab
                // groups, an alternative view binder and model can be implemented.
                mAdapter.registerType(
                        UiType.TAB_GROUP,
                        parent -> {
                            ViewGroup group =
                                    (ViewGroup)
                                            LayoutInflater.from(activity)
                                                    .inflate(
                                                            R.layout.tab_grid_card_item,
                                                            parentView,
                                                            false);
                            group.setClickable(true);
                            return group;
                        },
                        TabGridViewBinder::bindTab);
            }

            recyclerListener =
                    (holder) -> {
                        int holderItemViewType = holder.getItemViewType();

                        if (holderItemViewType != UiType.TAB
                                || holderItemViewType != UiType.TAB_GROUP) {
                            return;
                        }

                        ViewLookupCachingFrameLayout root =
                                (ViewLookupCachingFrameLayout) holder.itemView;
                        ImageView thumbnail = root.fastFindViewById(R.id.tab_thumbnail);
                        if (thumbnail == null) return;

                        thumbnail.setImageDrawable(null);
                    };
        } else if (mMode == TabListMode.STRIP) {
            mAdapter.registerType(
                    UiType.STRIP,
                    parent -> {
                        return (ViewGroup)
                                LayoutInflater.from(activity)
                                        .inflate(R.layout.tab_strip_item, parentView, false);
                    },
                    TabStripViewBinder::bind);
        } else {
            throw new IllegalArgumentException(
                    "Attempting to create a tab list UI with invalid mode");
        }

        // TODO (https://crbug.com/1048632): Use the current profile (i.e., regular profile or
        // incognito profile) instead of always using regular profile. It works correctly now, but
        // it is not safe.
        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        mActivity,
                        mMode == TabListMode.STRIP,
                        R.dimen.default_favicon_corner_radius,
                        TabFavicon::getBitmap);

        mMediator =
                new TabListMediator(
                        activity,
                        mModelList,
                        mMode,
                        modalDialogManager,
                        tabGroupModelFilterSupplier,
                        thumbnailProvider,
                        mTabListFaviconProvider,
                        actionOnRelatedTabs,
                        selectionDelegateProvider,
                        gridCardOnClickListenerProvider,
                        dialogHandler,
                        priceWelcomeMessageControllerSupplier,
                        componentName,
                        initialTabActionState,
                        dataSharingTabManager,
                        onTabGroupCreation,
                        undoBarExplicitTrigger,
                        snackbarManager,
                        allowedSelectionCount);

        try (TraceEvent e = TraceEvent.scoped("TabListCoordinator.setupRecyclerView")) {
            // Ignore attachToParent initially. In some activitys multiple TabListCoordinators are
            // created with the same parentView. Using attachToParent and subsequently trying to
            // locate the View with findViewById could then resolve to the wrong view. Instead use
            // LayoutInflater to return the inflated view and addView to circumvent the issue.
            mRecyclerView =
                    (TabListRecyclerView)
                            LayoutInflater.from(activity)
                                    .inflate(
                                            R.layout.tab_list_recycler_view_layout,
                                            parentView,
                                            /* attachToParent= */ false);
            if (attachToParent) {
                parentView.addView(mRecyclerView);
            }

            // GRID has a fixed size. STRIP has a fixed size only if DATA_SHARING is off.
            boolean hasFixedSize =
                    mMode != TabListMode.STRIP || !TabUiUtils.isDataSharingFunctionalityEnabled();
            mRecyclerView.setAdapter(mAdapter);
            mRecyclerView.setHasFixedSize(hasFixedSize);
            mRecyclerView.setOnDragListener(mTabSwitcherDragHandler);
            if (recyclerListener != null) mRecyclerView.setRecyclerListener(recyclerListener);

            if (mMode == TabListMode.GRID) {
                GridLayoutManager gridLayoutManager =
                        new GridLayoutManager(activity, GRID_LAYOUT_SPAN_COUNT_COMPACT) {
                            @Override
                            public void onLayoutCompleted(RecyclerView.State state) {
                                super.onLayoutCompleted(state);
                                checkAwaitingLayout();
                            }
                        };
                mRecyclerView.setLayoutManager(gridLayoutManager);
                mMediator.registerOrientationListener(gridLayoutManager);
                mMediator.updateSpanCount(
                        gridLayoutManager,
                        activity.getResources().getConfiguration().screenWidthDp);
                mMediator.setupAccessibilityDelegate(mRecyclerView);
                Rect frame = new Rect();
                mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(frame);
                updateGridCardLayout(frame.width());
            } else if (mMode == TabListMode.STRIP) {
                LinearLayoutManager layoutManager =
                        new LinearLayoutManager(activity, LinearLayoutManager.HORIZONTAL, false) {
                            @Override
                            public void onLayoutCompleted(RecyclerView.State state) {
                                super.onLayoutCompleted(state);
                                checkAwaitingLayout();
                            }
                        };
                mRecyclerView.setLayoutManager(layoutManager);
            }
            mMediator.setRecyclerViewItemAnimationToggle(mRecyclerView::setDisableItemAnimations);
        }

        if (mMode == TabListMode.GRID) {
            mListLayoutListener =
                    (view, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) ->
                            updateGridCardLayout(right - left);
        } else if (mMode == TabListMode.STRIP) {
            assert onModelTokenChange != null;
            mTabStripSnapshotter =
                    new TabStripSnapshotter(onModelTokenChange, mModelList, mRecyclerView);
        }
        mEmptyStateHeadingResId = emptyHeadingStringResId;
        mEmptyStateSubheadingResId = emptySubheadingStringResId;
        mEmptyStateImageResId = emptyImageResId;
        if (emptyViewParent != null) {
            mTabListEmptyCoordinator =
                    new TabListEmptyCoordinator(
                            emptyViewParent, mModelList, this::runOnItemAnimatorFinished);
        }
        mTabListHighlighter = new TabListHighlighter(mModelList);
        mTabListMergeAnimationManager = new TabListMergeAnimationManager(mRecyclerView);
        configureRecyclerViewTouchHelpers();
    }

    /**
     * @param onLongPressTabItemEventListener to handle long press events on tabs.
     */
    public void setOnLongPressTabItemEventListener(
            TabGridItemLongPressOrchestrator.@Nullable OnLongPressTabItemEventListener
                    onLongPressTabItemEventListener) {
        assert mMediator != null;
        mMediator.setOnLongPressTabItemEventListener(onLongPressTabItemEventListener);
    }

    /**
     * @param listener the handler for dropping tabs on top of an archival message card.
     */
    public void setOnDropOnArchivalMessageCardEventListener(
            @Nullable OnDropOnArchivalMessageCardEventListener listener) {
        assert mMediator != null;
        mMediator.setOnDropOnArchivalMessageCardEventListener(listener);
    }

    public void addDragObserver(DragObserver observer) {
        mDragObserverList.addObserver(observer);
    }

    public void removeDragObserver(DragObserver observer) {
        mDragObserverList.removeObserver(observer);
    }

    /** Sets the current {@link TabActionState} for the TabList. */
    public void setTabActionState(@TabActionState int tabActionState) {
        assert mMediator != null;
        mTabActionState = tabActionState;
        configureRecyclerViewTouchHelpers();
        mMediator.setTabActionState(tabActionState);
    }

    /** Adds an observer of the tab list item size. Also triggers an observer method. */
    public void addTabListItemSizeChangedObserver(TabListItemSizeChangedObserver observer) {
        mTabListItemSizeChangedObserverList.addObserver(observer);
        Size size = mMediator.getDefaultGridCardSize();
        assert size != null;
        observer.onSizeChanged(mMediator.getCurrentSpanCount(), size);
    }

    /** Remove an observer of the tab list item size. */
    public void removeTabListItemSizeChangedObserver(TabListItemSizeChangedObserver observer) {
        mTabListItemSizeChangedObserverList.removeObserver(observer);
    }

    Rect getThumbnailLocationOfCurrentTab() {
        // TODO(crbug.com/40627995): calculate the location before the real one is ready.
        Rect rect =
                mRecyclerView.getRectOfCurrentThumbnail(
                        mModelList.indexFromTabId(mMediator.selectedTabId()),
                        mMediator.selectedTabId());
        if (rect == null) return new Rect();
        rect.offset(0, 0);
        return rect;
    }

    /**
     * @param tabId The tab ID to get a rect for.
     * @return a {@link Rect} for the tab's thumbnail (may be an empty rect if the tab is not
     *     found).
     */
    Rect getTabThumbnailRect(int tabId) {
        int index = getIndexForTabIdWithRelatedTabs(tabId);
        if (index == TabModel.INVALID_TAB_INDEX) return new Rect();

        return mRecyclerView.getRectOfTabThumbnail(
                index, mModelList.get(index).model.get(TabProperties.TAB_ID));
    }

    Size getThumbnailSize() {
        Size size = mMediator.getDefaultGridCardSize();
        assert size != null;
        return TabUtils.deriveThumbnailSize(size, mActivity);
    }

    void waitForLayoutWithTab(int tabId, Runnable r) {
        // Very fast navigations to/from the tab list may not have time for a layout to reach a
        // completed state. Since this is primarily used for cancellable or skippable animations
        // where the runnable will not be serviced downstream, dropping the runnable altogether is
        // safe.
        if (mAwaitingLayoutRunnable != null) {
            Log.d(TAG, "Dropping AwaitingLayoutRunnable for " + mAwaitingTabId);
            mAwaitingLayoutRunnable = null;
            mAwaitingTabId = Tab.INVALID_TAB_ID;
        }
        int index = getIndexForTabIdWithRelatedTabs(tabId);
        if (index == TabModel.INVALID_TAB_INDEX) {
            r.run();
            return;
        }
        mAwaitingLayoutRunnable = r;
        mAwaitingTabId = mModelList.get(index).model.get(TabProperties.TAB_ID);
        mRecyclerView.runOnNextLayout(this::checkAwaitingLayout);
    }

    Rect getRecyclerViewLocation() {
        Rect recyclerViewRect = new Rect();
        mRecyclerView.getGlobalVisibleRect(recyclerViewRect);
        return recyclerViewRect;
    }

    /** Returns the position and offset of the first visible element in the list. */
    RecyclerViewPosition getRecyclerViewPosition() {
        return mRecyclerView.getRecyclerViewPosition();
    }

    /**
     * @param recyclerViewPosition the position and offset to scroll the recycler view to.
     */
    void setRecyclerViewPosition(RecyclerViewPosition recyclerViewPosition) {
        mRecyclerView.setRecyclerViewPosition(recyclerViewPosition);
    }

    void initWithNative(Profile originalProfile) {
        if (mIsInitialized) return;

        try (TraceEvent e = TraceEvent.scoped("TabListCoordinator.initWithNative")) {
            mIsInitialized = true;

            assert !originalProfile.isOffTheRecord() : "Expecting a non-incognito profile.";
            mMediator.initWithNative(originalProfile);
        }
    }

    private void configureRecyclerViewTouchHelpers() {
        boolean modeAllowsDragAndDrop = mMode == TabListMode.GRID;
        boolean actionStateAllowsDragAndDrop = mTabActionState != TabActionState.SELECTABLE;
        if (mAllowDragAndDrop && modeAllowsDragAndDrop && actionStateAllowsDragAndDrop) {
            if (mOnBeforeItemTouchHelperItemTouchListener == null
                    || mItemTouchHelper == null
                    || mOnAfterItemTouchHelperItemTouchListener == null) {
                TabGridItemTouchHelperCallback callback =
                        (TabGridItemTouchHelperCallback)
                                mMediator.getItemTouchHelperCallback(
                                        mActivity
                                                .getResources()
                                                .getDimension(R.dimen.swipe_to_dismiss_threshold),
                                        PERCENTAGE_AREA_OVERLAP_MERGE_THRESHOLD,
                                        mActivity
                                                .getResources()
                                                .getDimension(R.dimen.bottom_sheet_peek_height));

                // Override default ItemTouchHelper's long press listener to handle drag start.
                ItemTouchHelper2.LongPressHandler longPressHandler = null;
                if (mAllowDetachingTabsToCreateNewWindows && mTabSwitcherDragHandler != null) {
                    longPressHandler = new LongPressHandler();
                }

                // Detects if inputs are coming from a mouse or not. This is used to modify
                // behaviors of the TabGridItemTouchHelperCallback.
                mOnBeforeItemTouchHelperItemTouchListener =
                        new OnItemTouchListener() {
                            @Override
                            public boolean onInterceptTouchEvent(
                                    RecyclerView recyclerView, MotionEvent event) {
                                callback.setIsMouseInputSource(
                                        event.getSource() == InputDevice.SOURCE_MOUSE);
                                return false;
                            }

                            @Override
                            public void onTouchEvent(
                                    RecyclerView recyclerView, MotionEvent event) {}

                            @Override
                            public void onRequestDisallowInterceptTouchEvent(
                                    boolean disallowIntercept) {}
                        };

                // Creates an instance of the ItemTouchHelper using TabGridItemTouchHelperCallback
                // and attach a downstream mOnAfterItemTouchHelperItemTouchListener that watches for
                // TabGridItemTouchHelperCallback#shouldBlockAction() to occur. This determines if
                // on a longpress the final MOTION_UP event should be intercepted if it should have
                // been filtered in the ItemTouchHelper, but was not handled. This then allows
                // the mOnItemTouchHelper to intercept the event and prevent subsequent downstream
                // click handlers from receiving an input possibly causing unexpected behaviors.
                //
                // See similar comments in TabGridItemTouchHelperCallback for more details.
                mItemTouchHelper = new ItemTouchHelper2(callback, longPressHandler);
                mOnAfterItemTouchHelperItemTouchListener =
                        new OnItemTouchListener() {
                            @Override
                            public boolean onInterceptTouchEvent(
                                    RecyclerView recyclerView, MotionEvent event) {
                                // There can be an edge case when adding the block action logic
                                // where minimal movement not picked up by the mItemTouchHelper
                                // can result in attempting to block an action that did have a
                                // DRAG event.
                                // Actually, blocking the next event in this can result in an
                                // unexpected event being consumed leading to an unexpected
                                // sequence of MotionEvents.
                                // This bad sequence can then result in invalid UI & click state for
                                // downstream touch handlers. This additional check ensures that for
                                // a given action, if a block is requested it must be the UP
                                // motion that ends the input.
                                if (callback.shouldBlockAction()
                                        && (event.getActionMasked() == MotionEvent.ACTION_UP
                                                || event.getActionMasked()
                                                        == MotionEvent.ACTION_POINTER_UP)) {
                                    return true;
                                }
                                return false;
                            }

                            @Override
                            public void onTouchEvent(
                                    RecyclerView recyclerView, MotionEvent event) {}

                            @Override
                            public void onRequestDisallowInterceptTouchEvent(
                                    boolean disallowIntercept) {
                                // If a child component does not allow this recyclerView and any
                                // parent components to intercept touch events, shouldBlockAction
                                // should be called anyways to reset the tracking boolean.
                                // Otherwise, the original intercept method will do the check.
                                if (!disallowIntercept) return;
                                callback.shouldBlockAction();
                            }
                        };
            }
            mRecyclerView.addOnItemTouchListener(mOnBeforeItemTouchHelperItemTouchListener);
            mItemTouchHelper.attachToRecyclerView(mRecyclerView);
            mRecyclerView.addOnItemTouchListener(mOnAfterItemTouchHelperItemTouchListener);
        } else {
            if (mOnBeforeItemTouchHelperItemTouchListener != null
                    && mItemTouchHelper != null
                    && mOnAfterItemTouchHelperItemTouchListener != null) {
                mRecyclerView.addOnItemTouchListener(mOnBeforeItemTouchHelperItemTouchListener);
                mItemTouchHelper.attachToRecyclerView(null);
                mRecyclerView.removeOnItemTouchListener(mOnAfterItemTouchHelperItemTouchListener);
            }
        }
    }

    private void updateGridCardLayout(int viewWidth) {
        // Determine and set span count
        final GridLayoutManager layoutManager =
                (GridLayoutManager) mRecyclerView.getLayoutManager();
        assumeNonNull(layoutManager);
        boolean updatedSpan =
                mMediator.updateSpanCount(
                        layoutManager, mActivity.getResources().getConfiguration().screenWidthDp);
        if (updatedSpan) {
            // Update the cards for the span change.
            ViewUtils.requestLayout(mRecyclerView, "TabListCoordinator#updateGridCardLayout");
        }
        // Determine grid card width and account for margins on left and right.
        final int cardWidthPx =
                ((viewWidth - mRecyclerView.getPaddingStart() - mRecyclerView.getPaddingEnd())
                        / layoutManager.getSpanCount());
        final int cardHeightPx =
                TabUtils.deriveGridCardHeight(
                        cardWidthPx, mActivity, mBrowserControlsStateProvider);

        final Size oldDefaultSize = mMediator.getDefaultGridCardSize();
        final Size newDefaultSize = new Size(cardWidthPx, cardHeightPx);
        if (oldDefaultSize != null && newDefaultSize.equals(oldDefaultSize)) return;

        mMediator.setDefaultGridCardSize(newDefaultSize);
        for (int i = 0; i < mModelList.size(); i++) {
            PropertyModel tabPropertyModel = mModelList.get(i).model;
            // Other GTS items might intentionally have different dimensions. For example, the
            // pre-selected tab group divider and the large price tracking message span the width of
            // the recycler view.
            if (tabPropertyModel.get(CARD_TYPE) == ModelType.TAB
                    || tabPropertyModel.get(CARD_TYPE) == ModelType.TAB_GROUP) {
                tabPropertyModel.set(
                        TabProperties.GRID_CARD_SIZE, new Size(cardWidthPx, cardHeightPx));
            }
        }

        for (TabListItemSizeChangedObserver observer : mTabListItemSizeChangedObserverList) {
            observer.onSizeChanged(mMediator.getCurrentSpanCount(), newDefaultSize);
        }
    }

    /**
     * @see TabListMediator#getPriceWelcomeMessageInsertionIndex().
     */
    int getPriceWelcomeMessageInsertionIndex() {
        return mMediator.getPriceWelcomeMessageInsertionIndex();
    }

    /**
     * @return The container {@link androidx.recyclerview.widget.RecyclerView} that is showing the
     *         tab list UI.
     */
    public TabListRecyclerView getContainerView() {
        return mRecyclerView;
    }

    /**
     * @see TabListMediator#resetWithListOfTabs(List, boolean)
     */
    boolean resetWithListOfTabs(
            @Nullable List<Tab> tabs, @Nullable List<String> tabGroupSyncIds, boolean quickMode) {
        return mMediator.resetWithListOfTabs(tabs, tabGroupSyncIds, quickMode);
    }

    void softCleanup() {
        mMediator.softCleanup();
    }

    private void registerLayoutChangeListener() {
        if (mListLayoutListener != null) {
            // TODO(crbug.com/40288028): There might be a timing or race condition that
            // LayoutListener
            // has been registered while it shouldn't be with Start surface refactor is enabled.
            if (mLayoutListenerRegistered) return;

            mLayoutListenerRegistered = true;
            mRecyclerView.addOnLayoutChangeListener(mListLayoutListener);
        }
    }

    private void unregisterLayoutChangeListener() {
        if (mListLayoutListener != null) {
            if (!mLayoutListenerRegistered) return;

            mRecyclerView.removeOnLayoutChangeListener(mListLayoutListener);
            mLayoutListenerRegistered = false;
        }
    }

    void prepareTabSwitcherPaneView() {
        registerLayoutChangeListener();
        mRecyclerView.setupCustomItemAnimator();
    }

    private void initializeEmptyStateView() {
        if (mIsEmptyViewInitialized) {
            return;
        }
        if (mTabListEmptyCoordinator != null) {
            mTabListEmptyCoordinator.initializeEmptyStateView(
                    mEmptyStateImageResId, mEmptyStateHeadingResId, mEmptyStateSubheadingResId);
            mTabListEmptyCoordinator.attachEmptyView();
            mIsEmptyViewInitialized = true;
            TabUiUtils.applyXrEmptyStateBackplate(mParentView);
        }
    }

    public void prepareTabGridView() {
        registerLayoutChangeListener();
        mRecyclerView.setupCustomItemAnimator();
    }

    public void cleanupTabGridView() {
        unregisterLayoutChangeListener();
    }

    public void destroyEmptyView() {
        if (mTabListEmptyCoordinator != null) {
            mTabListEmptyCoordinator.destroyEmptyView();
            mIsEmptyViewInitialized = false;
        }
    }

    public void attachEmptyView() {
        if (!mIsEmptyViewInitialized) {
            initializeEmptyStateView();
        }
        if (mTabListEmptyCoordinator != null) {
            mTabListEmptyCoordinator.setIsTabSwitcherShowing(true);
        }
    }

    /** Returns the handler for showing notifications. */
    public TabListNotificationHandler getTabListNotificationHandler() {
        return mMediator;
    }

    void postHiding() {
        unregisterLayoutChangeListener();
        mMediator.postHiding();
        if (mTabListEmptyCoordinator != null) {
            mTabListEmptyCoordinator.setIsTabSwitcherShowing(false);
        }
    }

    /** Destroy any members that needs clean up. */
    @Override
    public void onDestroy() {
        mMediator.destroy();
        destroyEmptyView();
        if (mTabListEmptyCoordinator != null) {
            mTabListEmptyCoordinator.removeListObserver();
        }
        if (mListLayoutListener != null) {
            mRecyclerView.removeOnLayoutChangeListener(mListLayoutListener);
            mLayoutListenerRegistered = false;
        }
        mRecyclerView.setRecyclerListener(null);
        if (mTabStripSnapshotter != null) {
            mTabStripSnapshotter.destroy();
        }
        if (mOnBeforeItemTouchHelperItemTouchListener != null) {
            mRecyclerView.removeOnItemTouchListener(mOnBeforeItemTouchHelperItemTouchListener);
        }
        if (mItemTouchHelper != null) {
            mItemTouchHelper.attachToRecyclerView(null);
        }
        if (mOnAfterItemTouchHelperItemTouchListener != null) {
            mRecyclerView.removeOnItemTouchListener(mOnAfterItemTouchHelperItemTouchListener);
        }
        if (mTabSwitcherDragHandler != null) {
            mTabSwitcherDragHandler.destroy();
        }
        mTabListFaviconProvider.destroy();
    }

    /**
     * Register a new view type for the component.
     *
     * @see MVCListAdapter#registerType(int, MVCListAdapter.ViewBuilder,
     *     PropertyModelChangeProcessor.ViewBinder).
     */
    <T extends View> void registerItemType(
            @UiType int typeId,
            MVCListAdapter.ViewBuilder<T> builder,
            PropertyModelChangeProcessor.ViewBinder<PropertyModel, T, PropertyKey> binder) {
        mAdapter.registerType(typeId, builder, binder);
    }

    /**
     * Inserts a special {@link ListItem} at given index of the model list.
     *
     * @see TabListMediator#addSpecialItemToModel(int, int, PropertyModel).
     */
    void addSpecialListItem(int index, @UiType int uiType, PropertyModel model) {
        mMediator.addSpecialItemToModel(index, uiType, model);
    }

    /**
     * Removes a special {@link ListItem} that has the given {@code uiType} and/or its {@link
     * PropertyModel} has the given {@code itemIdentifier}.
     *
     * @param uiType The uiType to match.
     * @param itemIdentifier The itemIdentifier to match. This can be obsoleted if the {@link
     *     ListItem} does not need additional identifier.
     */
    void removeSpecialListItem(@UiType int uiType, int itemIdentifier) {
        mMediator.removeSpecialItemFromModelList(uiType, itemIdentifier);
    }

    /**
     * Removes a {@link ListItem} that has the given {@code uiType} and the {@link PropertyModel}
     * has the given {@link TabListEditorItemSelectionId}.
     *
     * @param uiType The uiType to match.
     * @param itemId The itemId to match.
     */
    void removeListItem(@UiType int uiType, TabListEditorItemSelectionId itemId) {
        mMediator.removeListItemFromModelList(uiType, itemId);
    }

    /**
     * Retrieves the span count in the GridLayoutManager for the item at a given index.
     *
     * @param manager The GridLayoutManager the span count is retrieved from.
     * @param index The index of the item in the model list.
     */
    int getSpanCountForItem(GridLayoutManager manager, int index) {
        return mMediator.getSpanCountForItem(manager, index);
    }

    // PriceWelcomeMessageService.PriceWelcomeMessageProvider implementation.
    @Override
    public int getTabIndexFromTabId(@TabId int tabId) {
        return mModelList.indexFromTabId(tabId);
    }

    @Override
    public void showPriceDropTooltip(int index) {
        mModelList.get(index).model.set(TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP, true);
    }

    /**
     * Converts a list of tab IDs into a list of their corresponding indexes within the model list.
     * Note that this method does not account for tabs in tab groups.
     *
     * @param tabIds A list of tab IDs to convert.
     */
    public List<Integer> getCardIndexesFromTabIds(List<@TabId Integer> tabIds) {
        Set<@TabId Integer> tabIdSet = new HashSet<>(tabIds);
        List<Integer> indexes = new ArrayList<>();
        for (int i = 0; i < mModelList.size(); i++) {
            PropertyModel model = mModelList.get(i).model;
            if (model.get(CARD_TYPE) == TAB && tabIdSet.contains(model.get(TAB_ID))) {
                indexes.add(i);
            }
        }
        assert tabIds.size() == indexes.size();
        return indexes;
    }

    int getIndexOfNthTabCard(int index) {
        return mModelList.indexOfNthTabCardOrInvalid(index);
    }

    /** Returns the filter index of a tab from its view index or TabList.INVALID_TAB_INDEX. */
    int indexOfTabCardsOrInvalid(int index) {
        return mModelList.indexOfTabCardsOrInvalid(index);
    }

    int getTabListModelSize() {
        return mModelList.size();
    }

    /**
     * @see TabListMediator#specialItemExistsInModel(int)
     */
    boolean specialItemExists(@TabSwitcherMessageManager.MessageType int itemIdentifier) {
        return mMediator.specialItemExistsInModel(itemIdentifier);
    }

    /** Provides the tab ID for the most recently swiped tab. */
    ObservableSupplier<Integer> getRecentlySwipedTabSupplier() {
        return mMediator.getRecentlySwipedTabSupplier();
    }

    private void checkAwaitingLayout() {
        if (mAwaitingLayoutRunnable != null) {
            SimpleRecyclerViewAdapter.ViewHolder holder =
                    (SimpleRecyclerViewAdapter.ViewHolder)
                            mRecyclerView.findViewHolderForAdapterPosition(
                                    mModelList.indexFromTabId(mAwaitingTabId));
            if (holder == null) return;
            assert assumeNonNull(holder.model).get(TabProperties.TAB_ID) == mAwaitingTabId;
            Runnable r = mAwaitingLayoutRunnable;
            mAwaitingTabId = Tab.INVALID_TAB_ID;
            mAwaitingLayoutRunnable = null;
            r.run();
        }
    }

    /** Returns the index for the tab with related tabs. */
    public int getIndexForTabIdWithRelatedTabs(int tabId) {
        return mMediator.getIndexForTabIdWithRelatedTabs(tabId);
    }

    void showQuickDeleteAnimation(Runnable onAnimationEnd, List<Tab> tabs) {
        assert mMode == TabListMode.GRID : "Can only run animation in GRID mode.";
        mMediator.showQuickDeleteAnimation(onAnimationEnd, tabs, mRecyclerView);
    }

    /** Runs a runnable after the item animator has finished its animations. */
    void runOnItemAnimatorFinished(Runnable r) {
        Runnable attachListener =
                () -> {
                    // The item animator sometimes gets removed. If this happens run immediately.
                    var itemAnimator = mRecyclerView.getItemAnimator();
                    if (itemAnimator == null) {
                        r.run();
                        return;
                    }
                    // Create a listener that is executed once the item animator is done all its
                    // animations.
                    var listener =
                            new ItemAnimatorFinishedListener() {
                                @Override
                                public void onAnimationsFinished() {
                                    r.run();
                                }
                            };
                    itemAnimator.isRunning(listener);
                };
        // Delay attaching the listener in two ways:
        // 1) Post so that the current model updates in the current task complete before we attempt
        //    anything.
        // 2) Attach the listener only after the adapter has flushed any pending updates so
        //    animations have actually started.
        mRecyclerView.post(() -> runAfterAdapterUpdates(attachListener));
    }

    /**
     * Runs a runnable after the recycler view adapter has flushed any pending updates and started
     * animations for them.
     */
    private void runAfterAdapterUpdates(Runnable r) {
        if (!mRecyclerView.hasPendingAdapterUpdates()) {
            r.run();
            return;
        }

        // It is unfortunate that a global layout listener is required, but we need to wait for
        // views to be added/removed/rearranged as there is no other signal that pending updates
        // were applied.
        mRecyclerView
                .getViewTreeObserver()
                .addOnGlobalLayoutListener(
                        new OnGlobalLayoutListener() {
                            @Override
                            public void onGlobalLayout() {
                                // Keep waiting until all updates are applied.
                                if (mRecyclerView.hasPendingAdapterUpdates()) {
                                    return;
                                }
                                mRecyclerView
                                        .getViewTreeObserver()
                                        .removeOnGlobalLayoutListener(this);
                                r.run();
                            }
                        });
    }

    /**
     * Triggers an animation where a set of tabs merge into a single target tab.
     *
     * @param targetIndex The model index of the tab that other tabs will merge into.
     * @param visibleTabIndexes The model indexes for all tabs that will be merged into the target
     *     tab.
     * @param onAnimationEnd Executed after the merge animation has finished.
     */
    public void triggerMergeAnimation(
            int targetIndex, List<Integer> visibleTabIndexes, Runnable onAnimationEnd) {
        Runnable wrappedOnAnimationEnd =
                () -> {
                    onAnimationEnd.run();
                    configureRecyclerViewTouchHelpers();
                };
        mTabListMergeAnimationManager.playAnimation(
                targetIndex, visibleTabIndexes, wrappedOnAnimationEnd);
    }

    /** Returns the coordinator that manages the overflow menu for tab group cards in the GTS. */
    public @Nullable TabListGroupMenuCoordinator getTabListGroupMenuCoordinator() {
        return mMediator.getTabListGroupMenuCoordinator();
    }

    /**
     * Scrolls to the specified card index specified.
     *
     * @param cardIndex The card index to scroll to.
     */
    public void scrollToPosition(int cardIndex) {
        mRecyclerView.setSmoothScrolling(true);
        smoothScrollToPosition(
                mRecyclerView, cardIndex, () -> mRecyclerView.setSmoothScrolling(false));
    }

    /**
     * Maps a tab ID to an index. For use with {@link #addSpecialListItem(int, int, PropertyModel)}.
     */
    /* package */ int getIndexFromTabId(@TabId int tabId) {
        return mModelList.indexFromTabId(tabId);
    }

    /**
     * This is used to handle a long press event coming from the ItemTouchHelper, and allows us to
     * trigger an external drag start by using the initial touch point position.
     */
    private class LongPressHandler implements ItemTouchHelper2.LongPressHandler {
        @Override
        public boolean handleLongPress(MotionEvent event) {
            boolean res = false;
            View view = mRecyclerView.findChildViewUnder(event.getX(), event.getY());
            if (view instanceof TabGridView) {
                int selectedIndex = mRecyclerView.getChildAdapterPosition(view);
                if (selectedIndex != -1) {
                    PropertyModel model = mModelList.get(selectedIndex).model;
                    int tabId = model.get(TabProperties.TAB_ID);
                    TabGroupModelFilter filter = mTabGroupModelFilterSupplier.get();
                    assumeNonNull(filter);
                    Tab tab = filter.getTabModel().getTabById(tabId);
                    assumeNonNull(tab);
                    Token groupToken = tab.getTabGroupId();

                    FrameLayout colorViewContainer =
                            view.findViewById(R.id.tab_group_color_view_container);
                    PointF touchPoint = new PointF(event.getX(), event.getY());
                    boolean isGroupTile = colorViewContainer.getChildCount() > 0;

                    assumeNonNull(mTabSwitcherDragHandler);
                    if (isGroupTile && groupToken != null) {
                        res =
                                mTabSwitcherDragHandler.startGroupDragAction(
                                        view, groupToken, touchPoint);
                    } else if (!isGroupTile) {
                        res = mTabSwitcherDragHandler.startTabDragAction(view, tab, touchPoint);
                    }
                }
            }
            return res;
        }
    }

    public TabListHighlighter getTabListHighlighter() {
        return mTabListHighlighter;
    }

    public TabListModel getTabListModel() {
        return mModelList;
    }
}
