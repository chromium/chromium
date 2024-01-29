// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Rect;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnItemTouchListener;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Coordinator for showing UI for a list of tabs. Can be used in GRID or STRIP modes. */
public class TabListCoordinator
        implements PriceMessageService.PriceWelcomeMessageProvider, DestroyObserver {
    private static final String TAG = "TabListCoordinator";

    /**
     * Modes of showing the list of tabs.
     *
     * <p>NOTE: STRIP, LIST, and GRID modes will have height equal to that of the container view.
     */
    @IntDef({TabListMode.GRID, TabListMode.STRIP, TabListMode.LIST, TabListMode.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabListMode {
        int GRID = 0;
        int STRIP = 1;
        // int CAROUSEL_DEPRECATED = 2;
        int LIST = 3;
        int NUM_ENTRIES = 4;
    }

    static final int GRID_LAYOUT_SPAN_COUNT_COMPACT = 2;
    static final int GRID_LAYOUT_SPAN_COUNT_MEDIUM = 3;
    static final int GRID_LAYOUT_SPAN_COUNT_LARGE = 4;
    static final int MAX_SCREEN_WIDTH_COMPACT_DP = 600;
    static final int MAX_SCREEN_WIDTH_MEDIUM_DP = 800;

    private final TabListMediator mMediator;
    private final TabListRecyclerView mRecyclerView;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final @TabListMode int mMode;
    private final Context mContext;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final ObservableSupplier<TabModelFilter> mCurrentTabModelFilterSupplier;
    private final TabListModel mModel;
    private final @UiType int mItemType;
    private final ViewGroup mRootView;

    private boolean mIsInitialized;
    private OnLayoutChangeListener mListLayoutListener;
    private boolean mLayoutListenerRegistered;
    private @Nullable TabStripSnapshotter mTabStripSnapshotter;
    private ItemTouchHelper mItemTouchHelper;
    private OnItemTouchListener mOnItemTouchListener;
    private TabListEmptyCoordinator mTabListEmptyCoordinator;
    private boolean mHasEmptyView;
    private int mEmptyStateImageResId;
    private int mEmptyStateHeadingResId;
    private int mEmptyStateSubheadingResId;
    private boolean mIsEmptyViewInitialized;

    private @Nullable Runnable mAwaitingLayoutRunnable;
    private int mAwaitingTabId = Tab.INVALID_TAB_ID;

    /**
     * Construct a coordinator for UI that shows a list of tabs.
     *
     * @param mode Modes of showing the list of tabs. Can be used in GRID or STRIP.
     * @param context The context to use for accessing {@link android.content.res.Resources}.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} for top
     *     controls.
     * @param tabModelFilterSupplier The supplier for the current tab model filter.
     * @param regularTabModelSupplier The supplier for the regular tab model.
     * @param thumbnailProvider Provider to provide screenshot related details.
     * @param titleProvider Provider for a given tab's title.
     * @param actionOnRelatedTabs Whether tab-related actions should be operated on all related
     *     tabs.
     * @param gridCardOnClickListenerProvider Provides the onClickListener for opening dialog when
     *     click on a grid card.
     * @param dialogHandler A handler to handle requests about updating TabGridDialog.
     * @param itemType The item type to put in the list of tabs.
     * @param selectionDelegateProvider Provider to provide selected Tabs for a selectable tab list.
     *     It's NULL when selection is not possible.
     * @param priceWelcomeMessageControllerSupplier A supplier for a controller to show
     *     PriceWelcomeMessage.
     * @param parentView {@link ViewGroup} The root view of the UI.
     * @param attachToParent Whether the UI should attach to root view.
     * @param componentName A unique string uses to identify different components for UMA recording.
     *     Recommended to use the class name or make sure the string is unique through actions.xml
     *     file.
     * @param rootView The root view of the app.
     * @param onModelTokenChange Callback to invoke whenever a model changes. Only currently
     *     respected in TabListMode.STRIP mode.
     * @param hasEmptyView A boolean to determine if we should show empty view in tab switcher.
     *     Currently only valid for TabListMode.GRID and TabListMode.LIST.
     */
    TabListCoordinator(
            @TabListMode int mode,
            Context context,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull ObservableSupplier<TabModelFilter> tabModelFilterSupplier,
            @NonNull Supplier<TabModel> regularTabModelSupplier,
            @Nullable ThumbnailProvider thumbnailProvider,
            @Nullable PseudoTab.TitleProvider titleProvider,
            boolean actionOnRelatedTabs,
            @Nullable
                    TabListMediator.GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            @Nullable TabListMediator.TabGridDialogHandler dialogHandler,
            @UiType int itemType,
            @Nullable TabListMediator.SelectionDelegateProvider selectionDelegateProvider,
            @NonNull Supplier<PriceWelcomeMessageController> priceWelcomeMessageControllerSupplier,
            @NonNull ViewGroup parentView,
            boolean attachToParent,
            String componentName,
            @NonNull ViewGroup rootView,
            @Nullable Callback<Object> onModelTokenChange) {
        this(
                mode,
                context,
                browserControlsStateProvider,
                tabModelFilterSupplier,
                regularTabModelSupplier,
                thumbnailProvider,
                titleProvider,
                actionOnRelatedTabs,
                gridCardOnClickListenerProvider,
                dialogHandler,
                itemType,
                selectionDelegateProvider,
                priceWelcomeMessageControllerSupplier,
                parentView,
                attachToParent,
                componentName,
                rootView,
                onModelTokenChange,
                false,
                0,
                0,
                0);
    }

    TabListCoordinator(
            @TabListMode int mode,
            Context context,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull ObservableSupplier<TabModelFilter> tabModelFilterSupplier,
            @NonNull Supplier<TabModel> regularTabModelSupplier,
            @Nullable ThumbnailProvider thumbnailProvider,
            @Nullable PseudoTab.TitleProvider titleProvider,
            boolean actionOnRelatedTabs,
            @Nullable
                    TabListMediator.GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            @Nullable TabListMediator.TabGridDialogHandler dialogHandler,
            @UiType int itemType,
            @Nullable TabListMediator.SelectionDelegateProvider selectionDelegateProvider,
            @NonNull Supplier<PriceWelcomeMessageController> priceWelcomeMessageControllerSupplier,
            @NonNull ViewGroup parentView,
            boolean attachToParent,
            String componentName,
            @NonNull ViewGroup rootView,
            @Nullable Callback<Object> onModelTokenChange,
            boolean hasEmptyView,
            int emptyImageResId,
            int emptyHeadingStringResId,
            int emptySubheadingStringResId) {
        mMode = mode;
        mItemType = itemType;
        mContext = context;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mCurrentTabModelFilterSupplier = tabModelFilterSupplier;
        mModel = new TabListModel();
        mAdapter = new SimpleRecyclerViewAdapter(mModel);
        mRootView = rootView;

        RecyclerView.RecyclerListener recyclerListener = null;
        if (mMode == TabListMode.GRID) {
            mAdapter.registerType(
                    UiType.SELECTABLE,
                    parent -> {
                        ViewGroup group =
                                (ViewGroup)
                                        LayoutInflater.from(context)
                                                .inflate(
                                                        R.layout.selectable_tab_grid_card_item,
                                                        parentView,
                                                        false);
                        group.setClickable(true);

                        return group;
                    },
                    TabGridViewBinder::bindSelectableTab);

            mAdapter.registerType(
                    UiType.CLOSABLE,
                    parent -> {
                        ViewGroup group =
                                (ViewGroup)
                                        LayoutInflater.from(context)
                                                .inflate(
                                                        R.layout.closable_tab_grid_card_item,
                                                        parentView,
                                                        false);
                        group.setClickable(true);
                        return group;
                    },
                    TabGridViewBinder::bindClosableTab);

            recyclerListener =
                    (holder) -> {
                        int holderItemViewType = holder.getItemViewType();

                        // TODO(crbug.com/1508423): Convert this logic block to a callback.
                        // If a custom message card item type is present, ensure that all attached
                        // child views are removed when the card is recycled.
                        if (holderItemViewType == UiType.CUSTOM_MESSAGE) {
                            CustomMessageCardView view = (CustomMessageCardView) holder.itemView;
                            view.removeAllViews();
                        }

                        if (holderItemViewType != UiType.CLOSABLE
                                && holderItemViewType != UiType.SELECTABLE) {
                            return;
                        }

                        ViewLookupCachingFrameLayout root =
                                (ViewLookupCachingFrameLayout) holder.itemView;
                        ImageView thumbnail = (ImageView) root.fastFindViewById(R.id.tab_thumbnail);
                        if (thumbnail == null) return;

                        thumbnail.setImageDrawable(null);
                    };
        } else if (mMode == TabListMode.STRIP) {
            mAdapter.registerType(
                    UiType.STRIP,
                    parent -> {
                        return (ViewGroup)
                                LayoutInflater.from(context)
                                        .inflate(R.layout.tab_strip_item, parentView, false);
                    },
                    TabStripViewBinder::bind);
        } else if (mMode == TabListMode.LIST) {
            mAdapter.registerType(
                    UiType.CLOSABLE,
                    parent -> {
                        ViewLookupCachingFrameLayout group =
                                (ViewLookupCachingFrameLayout)
                                        LayoutInflater.from(context)
                                                .inflate(
                                                        R.layout.closable_tab_list_card_item,
                                                        parentView,
                                                        false);
                        group.setClickable(true);

                        ImageView actionButton =
                                (ImageView) group.fastFindViewById(R.id.end_button);
                        actionButton.setVisibility(View.VISIBLE);
                        Resources resources = group.getResources();
                        int closeButtonSize =
                                (int) resources.getDimension(R.dimen.tab_grid_close_button_size);
                        Bitmap bitmap =
                                BitmapFactory.decodeResource(resources, R.drawable.btn_close);
                        Bitmap.createScaledBitmap(bitmap, closeButtonSize, closeButtonSize, true);
                        actionButton.setImageBitmap(bitmap);

                        return group;
                    },
                    TabListViewBinder::bindClosableListTab);

            mAdapter.registerType(
                    UiType.SELECTABLE,
                    parent -> {
                        ViewGroup group =
                                (ViewGroup)
                                        LayoutInflater.from(context)
                                                .inflate(
                                                        R.layout.selectable_tab_list_card_item,
                                                        parentView,
                                                        false);
                        group.setClickable(true);

                        return group;
                    },
                    TabListViewBinder::bindSelectableListTab);
        } else {
            throw new IllegalArgumentException(
                    "Attempting to create a tab list UI with invalid mode");
        }

        // TODO (https://crbug.com/1048632): Use the current profile (i.e., regular profile or
        // incognito profile) instead of always using regular profile. It works correctly now, but
        // it is not safe.
        TabListFaviconProvider tabListFaviconProvider =
                new TabListFaviconProvider(
                        mContext,
                        mMode == TabListMode.STRIP,
                        R.dimen.default_favicon_corner_radius);

        mMediator =
                new TabListMediator(
                        context,
                        mModel,
                        mMode,
                        tabModelFilterSupplier,
                        regularTabModelSupplier,
                        thumbnailProvider,
                        titleProvider,
                        tabListFaviconProvider,
                        actionOnRelatedTabs,
                        selectionDelegateProvider,
                        gridCardOnClickListenerProvider,
                        dialogHandler,
                        priceWelcomeMessageControllerSupplier,
                        componentName,
                        itemType);

        try (TraceEvent e = TraceEvent.scoped("TabListCoordinator.setupRecyclerView")) {
            // Ignore attachToParent initially. In some contexts multiple TabListCoordinators are
            // created with the same parentView. Using attachToParent and subsequently trying to
            // locate the View with findViewById could then resolve to the wrong view. Instead use
            // LayoutInflater to return the inflated view and addView to circumvent the issue.
            mRecyclerView =
                    (TabListRecyclerView)
                            LayoutInflater.from(context)
                                    .inflate(
                                            R.layout.tab_list_recycler_view_layout,
                                            parentView,
                                            /* attachToParent= */ false);
            if (attachToParent) {
                parentView.addView(mRecyclerView);
            }

            mRecyclerView.setAdapter(mAdapter);
            mRecyclerView.setHasFixedSize(true);
            if (recyclerListener != null) mRecyclerView.setRecyclerListener(recyclerListener);

            if (mMode == TabListMode.GRID) {
                GridLayoutManager gridLayoutManager =
                        new GridLayoutManager(context, GRID_LAYOUT_SPAN_COUNT_COMPACT) {
                            @Override
                            public void onLayoutCompleted(RecyclerView.State state) {
                                super.onLayoutCompleted(state);
                                checkAwaitingLayout();
                            }
                        };
                mRecyclerView.setLayoutManager(gridLayoutManager);
                mMediator.registerOrientationListener(gridLayoutManager);
                mMediator.updateSpanCount(
                        gridLayoutManager, context.getResources().getConfiguration().screenWidthDp);
                mMediator.setupAccessibilityDelegate(mRecyclerView);
                Rect frame = new Rect();
                ((Activity) mRecyclerView.getContext())
                        .getWindow()
                        .getDecorView()
                        .getWindowVisibleDisplayFrame(frame);
                updateGridCardLayout(frame.width());
            } else if (mMode == TabListMode.STRIP
                    || mMode == TabListMode.LIST) {
                LinearLayoutManager layoutManager =
                        new LinearLayoutManager(
                                context,
                                mMode == TabListMode.LIST
                                        ? LinearLayoutManager.VERTICAL
                                        : LinearLayoutManager.HORIZONTAL,
                                false) {
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
            mTabStripSnapshotter =
                    new TabStripSnapshotter(onModelTokenChange, mModel, mRecyclerView);
        }

        mHasEmptyView = hasEmptyView;
        if (mHasEmptyView) {
            mTabListEmptyCoordinator =
                    new TabListEmptyCoordinator(parentView, mModel, browserControlsStateProvider);
            mEmptyStateHeadingResId = emptyHeadingStringResId;
            mEmptyStateSubheadingResId = emptySubheadingStringResId;
            mEmptyStateImageResId = emptyImageResId;
        }
    }

    /**
     * @param onLongPressTabItemEventListener to handle long press events on tabs.
     */
    public void setOnLongPressTabItemEventListener(
            @Nullable
                    TabGridItemTouchHelperCallback.OnLongPressTabItemEventListener
                            onLongPressTabItemEventListener) {
        assert mMediator != null;
        mMediator.setOnLongPressTabItemEventListener(onLongPressTabItemEventListener);
    }

    @NonNull
    Rect getThumbnailLocationOfCurrentTab() {
        // TODO(crbug.com/964406): calculate the location before the real one is ready.
        Rect rect =
                mRecyclerView.getRectOfCurrentThumbnail(
                        mModel.indexFromId(mMediator.selectedTabId()), mMediator.selectedTabId());
        if (rect == null) return new Rect();
        rect.offset(0, getTabListTopOffset());
        return rect;
    }

    /**
     * @param tabId The tab ID to get a rect for.
     * @return a {@link Rect} for the tab's thumbnail (may be an empty rect if the tab is not
     *     found).
     */
    @NonNull
    Rect getTabThumbnailRect(int tabId) {
        int index = getIndexForTabId(tabId);
        if (index == TabModel.INVALID_TAB_INDEX) return new Rect();

        return mRecyclerView.getRectOfTabThumbnail(
                index, mModel.get(index).model.get(TabProperties.TAB_ID));
    }

    @NonNull
    Size getThumbnailSize() {
        Size size = mMediator.getDefaultGridCardSize();
        return TabUtils.deriveThumbnailSize(size, mContext);
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
        int index = getIndexForTabId(tabId);
        if (index == TabModel.INVALID_TAB_INDEX) {
            r.run();
            return;
        }
        mAwaitingLayoutRunnable = r;
        mAwaitingTabId = mModel.get(index).model.get(TabProperties.TAB_ID);
        runAnimationOnNextLayout(this::checkAwaitingLayout);
    }

    @NonNull
    Rect getRecyclerViewLocation() {
        Rect recyclerViewRect = new Rect();
        mRecyclerView.getGlobalVisibleRect(recyclerViewRect);
        return recyclerViewRect;
    }

    /**
     * @return the position and offset of the first visible element in the list.
     */
    @NonNull
    RecyclerViewPosition getRecyclerViewPosition() {
        return mRecyclerView.getRecyclerViewPosition();
    }

    /**
     * @param recyclerViewPosition the position and offset to scroll the recycler view to.
     */
    void setRecyclerViewPosition(@NonNull RecyclerViewPosition recyclerViewPosition) {
        mRecyclerView.setRecyclerViewPosition(recyclerViewPosition);
    }

    void initWithNative(
            @NonNull Profile profile, @Nullable DynamicResourceLoader dynamicResourceLoader) {
        if (mIsInitialized) return;

        try (TraceEvent e = TraceEvent.scoped("TabListCoordinator.initWithNative")) {
            mIsInitialized = true;

            assert !profile.isOffTheRecord() : "Expecting a non-incognito profile.";
            mMediator.initWithNative(profile);
            if (dynamicResourceLoader != null) {
                mRecyclerView.createDynamicView(dynamicResourceLoader);
            }

            if ((mMode == TabListMode.GRID || mMode == TabListMode.LIST)
                    && mItemType != UiType.SELECTABLE) {
                TabGridItemTouchHelperCallback callback =
                        (TabGridItemTouchHelperCallback)
                                mMediator.getItemTouchHelperCallback(
                                        mContext.getResources()
                                                .getDimension(R.dimen.swipe_to_dismiss_threshold),
                                        mContext.getResources()
                                                .getDimension(R.dimen.tab_grid_merge_threshold),
                                        mContext.getResources()
                                                .getDimension(R.dimen.bottom_sheet_peek_height));

                // Creates an instance of the ItemTouchHelper using TabGridItemTouchHelperCallback
                // and attach a downsteam mOnItemTouchListener that watches for
                // TabGridItemTouchHelperCallback#shouldBlockAction() to occur. This determines if
                // on a longpress the final MOTION_UP event should be intercepted if it should have
                // been filtered in the ItemTouchHelper, but was not handled. This then allows
                // the mOnItemTouchHelper to intercept the event and prevent subsequent downstream
                // click handlers from receiving an input possibly causing unexpected behaviors.
                //
                // See similar comments in TabGridItemTouchHelperCallback for more details.
                mItemTouchHelper = new ItemTouchHelper(callback);
                mItemTouchHelper.attachToRecyclerView(mRecyclerView);
                mOnItemTouchListener =
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
                mRecyclerView.addOnItemTouchListener(mOnItemTouchListener);
            }
        }
    }

    private void updateGridCardLayout(int viewWidth) {
        // Determine and set span count
        final GridLayoutManager layoutManager =
                (GridLayoutManager) mRecyclerView.getLayoutManager();
        boolean updatedSpan =
                mMediator.updateSpanCount(
                        layoutManager, mContext.getResources().getConfiguration().screenWidthDp);
        if (updatedSpan) {
            // Update the cards for the span change.
            ViewUtils.requestLayout(mRecyclerView, "TabListCoordinator#updateGridCardLayout");
        }
        // Determine grid card width and account for margins on left and right.
        final int cardWidthPx =
                ((viewWidth - mRecyclerView.getPaddingStart() - mRecyclerView.getPaddingEnd())
                        / layoutManager.getSpanCount());
        final int cardHeightPx =
                TabUtils.deriveGridCardHeight(cardWidthPx, mContext, mBrowserControlsStateProvider);

        final Size oldDefaultSize = mMediator.getDefaultGridCardSize();
        final Size newDefaultSize = new Size(cardWidthPx, cardHeightPx);
        if (oldDefaultSize != null && newDefaultSize.equals(oldDefaultSize)) return;

        mMediator.setDefaultGridCardSize(newDefaultSize);
        for (int i = 0; i < mModel.size(); i++) {
            PropertyModel tabPropertyModel = mModel.get(i).model;
            // Other GTS items might intentionally have different dimensions. For example, the
            // pre-selected tab group divider and the large price tracking message span the width of
            // the recycler view.
            if (tabPropertyModel.get(CARD_TYPE) == ModelType.TAB) {
                tabPropertyModel.set(
                        TabProperties.GRID_CARD_SIZE, new Size(cardWidthPx, cardHeightPx));
            }
        }
    }

    /**
     * @return The top offset from top toolbar to the tab list recycler view. Used to adjust the
     *         animations for tab switcher.
     */
    int getTabListTopOffset() {
        if (!ReturnToChromeUtil.isStartSurfaceEnabled(mContext)) return 0;
        Rect tabListRect = getRecyclerViewLocation();
        Rect parentRect = new Rect();
        mRootView.getGlobalVisibleRect(parentRect);
        // Offset by CompositorViewHolder top offset and top toolbar height.
        tabListRect.offset(
                0,
                -parentRect.top
                        - (int)
                                mContext.getResources()
                                        .getDimension(R.dimen.toolbar_height_no_shadow));
        return tabListRect.top;
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
     * @return The editor {@link TabGroupTitleEditor} that is used to update tab group title.
     */
    TabGroupTitleEditor getTabGroupTitleEditor() {
        return mMediator.getTabGroupTitleEditor();
    }

    /**
     * @see TabListMediator#resetWithListOfTabs(List, boolean)
     */
    boolean resetWithListOfTabs(@Nullable List<PseudoTab> tabs, boolean quickMode) {
        return mMediator.resetWithListOfTabs(tabs, quickMode);
    }

    boolean resetWithListOfTabs(@Nullable List<Tab> tabs) {
        return resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false);
    }

    void softCleanup() {
        mMediator.softCleanup();
    }

    void hardCleanup() {
        mMediator.hardCleanup();
    }

    private void registerLayoutChangeListener() {
        if (mListLayoutListener != null) {
            // TODO(crbug/1500016): There might be a timing or race condition that LayoutListener
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

    void prepareTabSwitcherView() {
        registerLayoutChangeListener();
        mRecyclerView.prepareTabSwitcherView();
        mMediator.registerOnScrolledListener(mRecyclerView);
    }

    void prepareTabSwitcherPaneView() {
        registerLayoutChangeListener();
        mRecyclerView.prepareTabSwitcherPaneView();
        mMediator.registerOnScrolledListener(mRecyclerView);
    }

    private void initializeEmptyStateView() {
        if (mIsEmptyViewInitialized) {
            return;
        }
        if (mHasEmptyView && mTabListEmptyCoordinator != null) {
            mTabListEmptyCoordinator.initializeEmptyStateView(
                    mEmptyStateImageResId, mEmptyStateHeadingResId, mEmptyStateSubheadingResId);
            mTabListEmptyCoordinator.attachEmptyView();
            mIsEmptyViewInitialized = true;
        }
    }

    public void prepareTabGridView() {
        registerLayoutChangeListener();
    }

    public void cleanupTabGridView() {
        unregisterLayoutChangeListener();
    }

    public void destroyEmptyView() {
        if (mHasEmptyView && mTabListEmptyCoordinator != null) {
            mTabListEmptyCoordinator.destroyEmptyView();
            mIsEmptyViewInitialized = false;
        }
    }

    public void attachEmptyView() {
        if (!mIsEmptyViewInitialized) {
            initializeEmptyStateView();
        }
        if (mHasEmptyView && mTabListEmptyCoordinator != null) {
            mTabListEmptyCoordinator.setIsTabSwitcherShowing(true);
        }
    }

    void postHiding() {
        unregisterLayoutChangeListener();
        mRecyclerView.postHiding();
        mMediator.postHiding();
    }

    /** Destroy any members that needs clean up. */
    @Override
    public void onDestroy() {
        mMediator.destroy();
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
        if (mItemTouchHelper != null) {
            mItemTouchHelper.attachToRecyclerView(null);
        }
        if (mOnItemTouchListener != null) {
            mRecyclerView.removeOnItemTouchListener(mOnItemTouchListener);
        }
    }

    int getResourceId() {
        return mRecyclerView.getResourceId();
    }

    /**
     * Register a new view type for the component.
     * @see MVCListAdapter#registerType(int, MVCListAdapter.ViewBuilder,
     *         PropertyModelChangeProcessor.ViewBinder).
     */
    <T extends View> void registerItemType(
            @UiType int typeId,
            MVCListAdapter.ViewBuilder<T> builder,
            PropertyModelChangeProcessor.ViewBinder<PropertyModel, T, PropertyKey> binder) {
        mAdapter.registerType(typeId, builder, binder);
    }

    /**
     * Inserts a special {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} at given index of
     * the model list.
     * @see TabListMediator#addSpecialItemToModel(int, int, PropertyModel).
     */
    void addSpecialListItem(int index, @UiType int uiType, PropertyModel model) {
        mMediator.addSpecialItemToModel(index, uiType, model);
    }

    /**
     * Inserts a special {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} to the end of
     * model list.
     */
    void addSpecialListItemToEnd(@UiType int uiType, PropertyModel model) {
        mMediator.addSpecialItemToModel(mModel.size(), uiType, model);
    }

    /**
     * Removes a special {@link org.chromium.ui.modelutil.MVCListAdapter.ListItem} that
     * has the given {@code uiType} and/or its {@link PropertyModel} has the given
     * {@code itemIdentifier}.
     *
     * @param uiType The uiType to match.
     * @param itemIdentifier The itemIdentifier to match. This can be obsoleted if the {@link
     *         org.chromium.ui.modelutil.MVCListAdapter.ListItem} does not need additional
     *         identifier.
     */
    void removeSpecialListItem(@UiType int uiType, int itemIdentifier) {
        mMediator.removeSpecialItemFromModel(uiType, itemIdentifier);
    }

    // PriceWelcomeMessageService.PriceWelcomeMessageProvider implementation.
    @Override
    public int getTabIndexFromTabId(int tabId) {
        return mModel.indexFromId(tabId);
    }

    @Override
    public void showPriceDropTooltip(int index) {
        mModel.get(index).model.set(TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP, true);
    }

    void runAnimationOnNextLayout(Runnable runnable) {
        mRecyclerView.runAnimationOnNextLayout(runnable);
    }

    int getTabListModelSize() {
        return mModel.size();
    }

    /**
     * @see TabListMediator#specialItemExistsInModel(int)
     */
    boolean specialItemExists(@MessageService.MessageType int itemIdentifier) {
        return mMediator.specialItemExistsInModel(itemIdentifier);
    }

    private void checkAwaitingLayout() {
        if (mAwaitingLayoutRunnable != null) {
            SimpleRecyclerViewAdapter.ViewHolder holder =
                    (SimpleRecyclerViewAdapter.ViewHolder)
                            mRecyclerView.findViewHolderForAdapterPosition(
                                    mModel.indexFromId(mAwaitingTabId));
            if (holder == null) return;
            assert holder.model.get(TabProperties.TAB_ID) == mAwaitingTabId;
            Runnable r = mAwaitingLayoutRunnable;
            mAwaitingTabId = Tab.INVALID_TAB_ID;
            mAwaitingLayoutRunnable = null;
            r.run();
        }
    }

    private int getIndexForTabId(int tabId) {
        int index = mModel.indexFromId(tabId);
        if (index != TabModel.INVALID_TAB_INDEX) return index;

        TabModel tabModel = mCurrentTabModelFilterSupplier.get().getTabModel();
        Tab tab = TabModelUtils.getTabById(tabModel, tabId);
        if (tab == null) return TabModel.INVALID_TAB_INDEX;

        return mMediator.getIndexForTabWithRelatedTabs(tab);
    }

    void showQuickDeleteAnimation(Runnable onAnimationEnd, List<Tab> tabs) {
        mMediator.showQuickDeleteAnimation(onAnimationEnd, tabs, mRecyclerView);
    }
}
