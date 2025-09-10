// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;
import android.util.Pair;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkViewUtils;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils.BookmarkBarClickType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.base.KeyNavigationUtil;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.function.BiConsumer;
import java.util.function.Supplier;

/** Mediator for the bookmark bar which provides users with bookmark access from top chrome. */
@NullMarked
class BookmarkBarMediator implements BookmarkBarItemsProvider.Observer {

    private static final int INVALID_INDEX = -1;
    @VisibleForTesting static @Nullable Bitmap sFolderIconBitmap;
    private final Activity mActivity;
    private final PropertyModel mAllBookmarksButtonModel;
    private final Supplier<Pair<Integer, Integer>> mControlsHeightSupplier;
    private final ModelList mItemsModel;
    private final ObservableSupplier<Boolean> mItemsOverflowSupplier;
    private final BookmarkBarItemsLayoutManager mBookmarkBarItemsLayoutManager;
    private final Callback<Boolean> mItemsOverflowSupplierObserver;
    private final PropertyModel mModel;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver;
    private @Nullable final Tab mCurrentTab;
    private final BookmarkOpener mBookmarkOpener;
    private final ObservableSupplier<BookmarkManagerOpener> mBookmarkManagerOpenerSupplier;
    private final RecyclerView mItemsRecyclerView;
    private final BookmarkBar mBookmarkBarView;

    // The popup window that displays the contents of a bookmark folder. Instantiated in {@code
    // showPopupMenu} when a folder is tapped.
    private @Nullable AnchoredPopupWindow mAnchoredPopupWindow;
    private @Nullable BookmarkImageFetcher mImageFetcher;
    private @Nullable BookmarkBarItemsProvider mItemsProvider;
    private @Nullable BrowserControlsRectProvider mBrowserControlsRectProvider;

    /**
     * Constructs the bookmark bar mediator.
     *
     * @param activity The activity which is hosting the bookmark bar.
     * @param allBookmarksButtonModel The model for the 'All Bookmarks' button.
     * @param controlsHeightSupplier The supplier for the height of the top and bottom controls.
     *     Used to get the initial heights of the controls.
     * @param itemsModel The model for the items which are rendered within the bookmark bar.
     * @param bookmarkBarItemsLayoutManager The layout manager used to render the horizontal list of
     *     items within the bookmark bar.
     * @param model The model used to read/write bookmark bar properties.
     * @param profileSupplier The supplier for the currently active profile.
     * @param currentTab The current tab if it exists.
     * @param bookmarkOpener Used to open bookmarks.
     * @param bookmarkManagerOpenerSupplier Used to open the bookmark manager.
     * @param itemsRecyclerView The bookmark_bar_items_container recycler view that is inside the
     *     bookmark_bar view.
     * @param bookmarkBarView The bookmark_bar view that contains the entire bookmarks bar.
     */
    public BookmarkBarMediator(
            Activity activity,
            PropertyModel allBookmarksButtonModel,
            Supplier<Pair<Integer, Integer>> controlsHeightSupplier,
            ModelList itemsModel,
            BookmarkBarItemsLayoutManager bookmarkBarItemsLayoutManager,
            PropertyModel model,
            ObservableSupplier<Profile> profileSupplier,
            @Nullable Tab currentTab,
            BookmarkOpener bookmarkOpener,
            ObservableSupplier<BookmarkManagerOpener> bookmarkManagerOpenerSupplier,
            RecyclerView itemsRecyclerView,
            BookmarkBar bookmarkBarView) {
        mActivity = activity;

        mAllBookmarksButtonModel = allBookmarksButtonModel;
        mControlsHeightSupplier = controlsHeightSupplier;
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.CLICK_CALLBACK, this::onAllBookmarksButtonClick);
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.ICON_SUPPLIER,
                LazyOneshotSupplier.fromValue(
                        AppCompatResources.getDrawable(mActivity, R.drawable.star_outline_24dp)));
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.ICON_TINT_LIST_ID,
                R.color.default_icon_color_tint_list);
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.TITLE,
                mActivity.getString(R.string.bookmark_bar_all_bookmarks_button_title));

        mItemsModel = itemsModel;

        mBookmarkBarItemsLayoutManager = bookmarkBarItemsLayoutManager;
        mItemsOverflowSupplier = mBookmarkBarItemsLayoutManager.getItemsOverflowSupplier();
        mItemsOverflowSupplierObserver = this::onItemsOverflowChange;
        mItemsOverflowSupplier.addObserver(mItemsOverflowSupplierObserver);

        mModel = model;
        mModel.set(
                BookmarkBarProperties.OVERFLOW_BUTTON_CLICK_CALLBACK, this::onOverflowButtonClick);
        mModel.set(BookmarkBarProperties.OVERFLOW_BUTTON_VISIBILITY, View.INVISIBLE);

        mProfileSupplier = profileSupplier;
        mProfileSupplierObserver = this::onProfileChange;
        mProfileSupplier.addObserver(mProfileSupplierObserver);

        mCurrentTab = currentTab;
        mBookmarkOpener = bookmarkOpener;
        mBookmarkManagerOpenerSupplier = bookmarkManagerOpenerSupplier;
        mItemsRecyclerView = itemsRecyclerView;
        mBookmarkBarView = bookmarkBarView;
    }

    /** Destroys the bookmark bar mediator. */
    public void destroy() {
        mAllBookmarksButtonModel.set(BookmarkBarButtonProperties.CLICK_CALLBACK, null);
        mItemsOverflowSupplier.removeObserver(mItemsOverflowSupplierObserver);

        // TODO(crbug.com/430044890): Change it to a member variable.
        sFolderIconBitmap = null;

        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mItemsProvider != null) {
            mItemsProvider.destroy();
            mItemsProvider = null;
        }

        mProfileSupplier.removeObserver(mProfileSupplierObserver);
    }

    protected void setTopMargin(int newTopMargin) {
        mModel.set(BookmarkBarProperties.TOP_MARGIN, newTopMargin);
    }

    // BookmarkBarItemsProvider.Observer implementation.

    @Override
    public void onBookmarkItemAdded(
            @BookmarkBarItemsProvider.ObservationId int observationId,
            BookmarkItem item,
            int index) {
        mItemsModel.add(
                index,
                BookmarkBarUtils.createListItemFor(
                        this::onBookmarkItemClick, mActivity, mImageFetcher, item));
    }

    @Override
    public void onBookmarkItemMoved(
            @BookmarkBarItemsProvider.ObservationId int observationId, int index, int oldIndex) {
        mItemsModel.move(oldIndex, index);
    }

    @Override
    public void onBookmarkItemRemoved(
            @BookmarkBarItemsProvider.ObservationId int observationId, int index) {
        mItemsModel.removeAt(index);
    }

    @Override
    public void onBookmarkItemUpdated(
            @BookmarkBarItemsProvider.ObservationId int observationId,
            BookmarkItem item,
            int index) {
        mItemsModel.update(
                index,
                BookmarkBarUtils.createListItemFor(
                        this::onBookmarkItemClick, mActivity, mImageFetcher, item));
    }

    @Override
    public void onBookmarkItemsAdded(
            @BookmarkBarItemsProvider.ObservationId int observationId,
            List<BookmarkItem> items,
            int index) {
        final List<ListItem> batch = new ArrayList<>();
        for (int i = 0; i < items.size(); i++) {
            batch.add(
                    BookmarkBarUtils.createListItemFor(
                            this::onBookmarkItemClick, mActivity, mImageFetcher, items.get(i)));
        }
        mItemsModel.addAll(batch, index);
    }

    @Override
    public void onBookmarkItemsRemoved(
            @BookmarkBarItemsProvider.ObservationId int observationId, int index, int count) {
        mItemsModel.removeRange(index, count);
    }

    // Private methods.

    // TODO(crbug.com/394614779): Open in popup window instead of bookmark manager.
    private void onAllBookmarksButtonClick(int metaState) {
        // Open the manager iff the active profile and model are unchanged to prevent accidentally
        // opening the manager for the wrong profile/model. We will only record the click event if
        // this guard passes, so the data shows only actions that resulted in a change.
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profileAfterLoading, modelAfterLoading) -> {
                    BookmarkBarUtils.recordClick(BookmarkBarClickType.ALL_BOOKMARKS);
                    mBookmarkManagerOpenerSupplier
                            .get()
                            .showBookmarkManager(
                                    mActivity,
                                    mCurrentTab,
                                    profileAfterLoading,
                                    modelAfterLoading.getRootFolderId());
                });
    }

    // TODO(crbug.com/394614166): Handle shift-click to open in new window.
    private void onBookmarkItemClick(BookmarkItem item, int metaState) {
        final Profile profile = mProfileSupplier.get();

        if (item.isFolder()) {
            // Get the view of the folder that was clicked.
            View anchorView = getAnchorViewForBookmark(item);
            if (anchorView == null) return;
            runIfStillRelevantAfterFinishLoadingBookmarkModel(
                    (profileAfterLoading, modelAfterLoading) -> {
                        // Build the entire model list for this folder. The grandchildren are stored
                        // in SUBMENU_ITEMS.
                        ModelList menuModel =
                                buildMenuModelListForFolder(modelAfterLoading, item.getId());
                        BookmarkBarUtils.recordClick(BookmarkBarClickType.BOOKMARK_BAR_FOLDER);
                        showPopupMenu(menuModel, anchorView);
                    });
            return;
        }

        BookmarkBarUtils.recordClick(BookmarkBarClickType.BOOKMARK_BAR_URL);
        final boolean isCtrlPressed = (metaState & KeyEvent.META_CTRL_ON) != 0;
        if (isCtrlPressed) {
            mBookmarkOpener.openBookmarksInNewTabs(
                    List.of(item.getId()),
                    profile.isOffTheRecord(),
                    Optional.of(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));
            return;
        }

        mBookmarkOpener.openBookmarkInCurrentTab(item.getId(), profile.isOffTheRecord());
    }

    private void onItemsOverflowChange(boolean itemsOverflow) {
        mModel.set(
                BookmarkBarProperties.OVERFLOW_BUTTON_VISIBILITY,
                itemsOverflow ? View.VISIBLE : View.INVISIBLE);
    }

    private void onOverflowButtonClick() {
        // Open the manager iff the active profile and model are unchanged to prevent accidentally
        // opening the manager for the wrong profile/model. We will only record the click event if
        // this guard passes, so the data shows only actions that resulted in a change.
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profileAfterLoading, modelAfterLoading) -> {
                    // Get the id of the entire bookmarks bar.
                    BookmarkId bookmarkBarDesktopFolderId =
                            Optional.ofNullable(modelAfterLoading.getAccountDesktopFolderId())
                                    .orElseGet(modelAfterLoading::getDesktopFolderId);

                    if (bookmarkBarDesktopFolderId == null) return;

                    // Get an ordered list of all the children (both folders and web pages) of the
                    // bookmarks bar.
                    List<BookmarkId> allBookmarkItems =
                            modelAfterLoading.getChildIds(bookmarkBarDesktopFolderId);

                    // Get the index of the first hidden item from the LayoutManager.
                    int firstHiddenIndex =
                            mBookmarkBarItemsLayoutManager.getFirstHiddenItemPosition();

                    // Create a new list containing only the hidden items.
                    List<BookmarkId> hiddenItems = new ArrayList<>();
                    if (firstHiddenIndex < allBookmarkItems.size()) {
                        hiddenItems =
                                allBookmarkItems.subList(firstHiddenIndex, allBookmarkItems.size());
                    }

                    // Build the menu model using only the hidden items.
                    ModelList hiddenItemsModelList =
                            buildMenuModelListFromIds(modelAfterLoading, hiddenItems);

                    // Get the anchor view, which is bookmark_bar_overflow_button.
                    View anchorView = mBookmarkBarView.getOverflowButton();
                    if (anchorView == null) return;

                    // Show the popup with the filtered model. Notice that when we call
                    // #showPopupMenu inside #onBookmarkItemClick, we are calling it for one
                    // specific folder in the bookmarks bar, whereas here it is for all the hidden
                    // items in the entire bookmarks bar ("desktopFolder").
                    BookmarkBarUtils.recordClick(BookmarkBarClickType.OVERFLOW_MENU);
                    showPopupMenu(hiddenItemsModelList, anchorView);
                });
    }

    private void onProfileChange(@Nullable Profile profile) {
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mItemsProvider != null) {
            mItemsProvider.destroy();
            mItemsProvider = null;
        }

        mItemsModel.clear();

        if (profile == null) {
            return;
        }

        // Instantiate dependencies iff the active profile and model are unchanged to prevent
        // accidentally instantiating dependencies for the wrong profile/model.
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profileAfterLoading, modelAfterLoading) -> {
                    mImageFetcher =
                            new BookmarkImageFetcher(
                                    profileAfterLoading,
                                    mActivity,
                                    modelAfterLoading,
                                    ImageFetcherFactory.createImageFetcher(
                                            ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                                            profileAfterLoading.getProfileKey(),
                                            GlobalDiscardableReferencePool.getReferencePool()),
                                    FaviconUtils.createCircularIconGenerator(mActivity));

                    mItemsProvider = new BookmarkBarItemsProvider(modelAfterLoading, this);
                });
    }

    private void runIfStillRelevantAfterFinishLoadingBookmarkModel(
            BiConsumer<Profile, BookmarkModel> callback) {
        final var profile = mProfileSupplier.get();
        if (profile == null) return;

        final var model = BookmarkModel.getForProfile(profile);
        model.finishLoadingBookmarkModel(
                () -> {
                    // Ensure the active profile hasn't changed while loading the model.
                    final var profileAfterLoading = mProfileSupplier.get();
                    if (!Objects.equals(profile, profileAfterLoading)) return;

                    // Ensure the active model hasn't changed while loading the model.
                    final var modelAfterLoading = BookmarkModel.getForProfile(profileAfterLoading);
                    if (!Objects.equals(model, modelAfterLoading)) return;

                    // Run the callback iff the active profile and model are unchanged to avoid
                    // running the callback for the wrong profile/model.
                    callback.accept(profileAfterLoading, modelAfterLoading);
                });
    }

    public void setVisibility(boolean isVisible) {
        mModel.set(BookmarkBarProperties.VISIBILITY, isVisible ? View.VISIBLE : View.GONE);
    }

    private void showPopupMenu(ModelList bookmarkItems, View anchorView) {
        // Dismiss any existing popup windows.
        if (mAnchoredPopupWindow != null) mAnchoredPopupWindow.dismiss();

        BasicListMenu popupListMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        mActivity,
                        bookmarkItems,
                        (model) -> model.get(ListMenuItemProperties.CLICK_LISTENER).onClick(null));

        // Go through the entire model list and add the click listeners.
        popupListMenu.setupCallbacksRecursively(
                () -> {
                    if (mAnchoredPopupWindow != null) {
                        mAnchoredPopupWindow.dismiss();
                    }
                },
                /* drillDownOverrideValue= */ true);

        View popupContentView = popupListMenu.getContentView();
        // This is needed because list_menu_layout.xml already sets a background, and we want to
        // avoid double backgrounds. If we were to create a new BasicListMenu and pass 0 as the
        // background drawable, the BasicListMenu would just use the pre-defined background.
        popupContentView.setBackground(null);

        setupEmptyView(popupContentView);

        mBrowserControlsRectProvider = new BrowserControlsRectProvider(mActivity);

        Pair<Integer, Integer> initialHeights = mControlsHeightSupplier.get();
        mBrowserControlsRectProvider.updateRectAndNotify(
                initialHeights.first, initialHeights.second);

        mAnchoredPopupWindow =
                new AnchoredPopupWindow(
                        mActivity,
                        mBookmarkBarView,
                        AppCompatResources.getDrawable(mActivity, R.drawable.default_popup_menu_bg),
                        popupListMenu::getContentView,
                        new ViewRectProvider(anchorView),
                        mBrowserControlsRectProvider);

        mAnchoredPopupWindow.setFocusable(true);
        mAnchoredPopupWindow.setPreferredVerticalOrientation(
                AnchoredPopupWindow.VerticalOrientation.BELOW);
        mAnchoredPopupWindow.setHorizontalOverlapAnchor(true);
        mAnchoredPopupWindow.setElevation(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bookmarks_bar_popup_elevation));

        // Set the margin because anchoredPopupWindow is responsible for calculating the dynamic
        // height of the popup.
        int marginPx =
                mActivity.getResources().getDimensionPixelSize(R.dimen.bookmarks_bar_popup_margin);
        mAnchoredPopupWindow.setMargin(marginPx);

        // Create an observer that will re-run the sizing logic whenever the list content changes.
        // This is triggered when navigating into or out of a submenu. This observer is needed
        // because the anchored popup and basic list menu are created only once when the root folder
        // is tapped. When the user navigates into and out of submenus, the model list is updated
        // but the container (popup window & list menu) itself is not. This means that without
        // this observer, the dynamic sizing logic would only work on the root popups.
        final ListObservable.ListObserver<Void> sizeUpdaterObserver =
                new ListObservable.ListObserver<>() {
                    private void updatePopupSize(int count) {
                        popupContentView.post(
                                () -> {
                                    if (mAnchoredPopupWindow != null
                                            && mAnchoredPopupWindow.isShowing()) {
                                        configurePopupWindowSize(popupListMenu, count);
                                    }
                                });
                    }

                    @Override
                    public void onItemRangeChanged(
                            ListObservable<Void> source,
                            int index,
                            int count,
                            @Nullable Void payload) {
                        updatePopupSize(count);
                    }

                    @Override
                    public void onItemRangeInserted(ListObservable source, int index, int count) {
                        updatePopupSize(count);
                    }

                    @Override
                    public void onItemRangeRemoved(ListObservable source, int index, int count) {
                        updatePopupSize(count);
                    }
                };

        // Add the observer to the model list.
        bookmarkItems.addObserver(sizeUpdaterObserver);

        mAnchoredPopupWindow.addOnDismissListener(
                () -> {
                    mBrowserControlsRectProvider = null;
                    bookmarkItems.removeObserver(sizeUpdaterObserver);
                });

        configurePopupWindowSize(popupListMenu, bookmarkItems.size());
        mAnchoredPopupWindow.show();
    }

    // popupContentView is list_menu_layout, which is the root view of the menu.
    @VisibleForTesting
    void setupEmptyView(View popupContentView) {
        ListView menuList = popupContentView.findViewById(R.id.menu_list);

        ViewGroup contentParent = (ViewGroup) popupContentView;

        TextView emptyView = contentParent.findViewById(R.id.bookmarks_bar_empty_view);
        // The empty view will be added as a sibling to menu_list, menu_header, etc.
        if (emptyView == null) {
            emptyView = new TextView(mActivity);
            emptyView.setId(R.id.bookmarks_bar_empty_view);
            emptyView.setText(R.string.bookmarks_bar_empty_message);
            emptyView.setGravity(Gravity.CENTER);

            // Fill the entire space.
            emptyView.setLayoutParams(
                    new LinearLayout.LayoutParams(
                            LinearLayout.LayoutParams.MATCH_PARENT,
                            LinearLayout.LayoutParams.MATCH_PARENT));

            contentParent.addView(emptyView);
        }

        // #setupEmptyView is called once when a folder in the bookmarks bar is tapped, but
        // setEmptyView will be applied to the submenus as well because menu_list (ListView) has a
        // built-in observer.
        menuList.setEmptyView(emptyView);
    }

    @VisibleForTesting
    void configurePopupWindowSize(BasicListMenu popupListMenu, int count) {
        if (mAnchoredPopupWindow == null) {
            return;
        }

        Resources resources = mActivity.getResources();
        DisplayMetrics displayMetrics = resources.getDisplayMetrics();

        // Configure the width.
        int maxWidthPx = resources.getDimensionPixelSize(R.dimen.bookmarks_bar_popup_max_width);
        int marginPx = resources.getDimensionPixelSize(R.dimen.bookmarks_bar_popup_margin);
        // The maximum available width on screen is the screen width minus the margin.
        int availableWidth = displayMetrics.widthPixels - marginPx;
        // The final width is the smaller of the desired max width and the available screen width.
        int finalWidth = Math.min(maxWidthPx, availableWidth);

        // Measure size of menu_list. measuredHeight is the total height of all the items
        // inside menu_list plus padding.
        int[] measuredDimensions = popupListMenu.getMenuDimensions();
        int measuredHeight = measuredDimensions[1];

        // Configure the height. When we are in the empty state, there should be a minimum height as
        // defined by the UI spec. If there are bookmark items, the pop-up should shrink to wrap
        // those items without a minimum height.
        int minHeightPx =
                count == 0
                        ? resources.getDimensionPixelSize(R.dimen.bookmarks_bar_popup_min_height)
                        : measuredHeight;

        // Ensures that the height is at least minHeightPx.
        int heightFloor = Math.max(minHeightPx, measuredHeight);
        mAnchoredPopupWindow.setDesiredContentSize(finalWidth, heightFloor);
    }

    private int getIndexInBookmarksBar(BookmarkItem item) {
        // Get the main data model for all bookmarks for the user.
        BookmarkModel bookmarkModel = BookmarkModel.getForProfile(mProfileSupplier.get());
        if (bookmarkModel == null) return INVALID_INDEX;

        // Get the id of the entire bookmarks bar.
        BookmarkId bookmarkBarFolderId = bookmarkModel.getDesktopFolderId();
        if (bookmarkBarFolderId == null) return INVALID_INDEX;

        // Get an ordered list of all the children (both folders and web pages) of the bookmarks
        // bar.
        List<BookmarkId> childrenOfBookmarkBar = bookmarkModel.getChildIds(bookmarkBarFolderId);

        return childrenOfBookmarkBar.indexOf(item.getId());
    }

    private @Nullable View getAnchorViewForBookmark(BookmarkItem item) {
        // Find the pos of the specific folder we selected.
        int index = getIndexInBookmarksBar(item);
        if (index == INVALID_INDEX) return null;

        // Get the view holder of that pos.
        RecyclerView.ViewHolder holder = mItemsRecyclerView.findViewHolderForAdapterPosition(index);
        return (holder != null) ? holder.itemView : null;
    }

    // Recursive method that builds the entire model list for a clicked bookmark in the bookmarks
    // bar. The size of the returned model list will just be the number of the direct children
    // because each folder's SUBMENU_ITEMS contains the children list as a separate model list.
    @VisibleForTesting
    ModelList buildMenuModelListForFolder(BookmarkModel bookmarkModel, BookmarkId folderId) {
        List<BookmarkId> childIds = bookmarkModel.getChildIds(folderId);
        return buildMenuModelListFromIds(bookmarkModel, childIds);
    }

    // A reusable method that returns the ModelList from a specific list of Ids.
    @VisibleForTesting
    ModelList buildMenuModelListFromIds(BookmarkModel bookmarkModel, List<BookmarkId> bookmarkIds) {
        ModelList modelList = new ModelList();

        // Iterate through the ordered list of all the children (both folders and links) of this
        // folder.
        for (BookmarkId childId : bookmarkIds) {
            BookmarkItem childBookmarkItem = bookmarkModel.getBookmarkById(childId);
            if (childBookmarkItem == null) continue;
            if (childBookmarkItem.isFolder()) {
                modelList.add(
                        createListItemForBookmarkFolder(
                                childBookmarkItem,
                                buildMenuModelListForFolder(
                                        bookmarkModel, childBookmarkItem.getId())));
            } else {
                modelList.add(createListItemForBookmarkLeaf(childBookmarkItem));
            }
        }
        return modelList;
    }

    // Folders do not have urls.
    private ListItem createListItemForBookmarkFolder(
            BookmarkItem bookmarkItem, ModelList children) {

        if (sFolderIconBitmap == null) {
            BookmarkModel bookmarkModel = BookmarkModel.getForProfile(mProfileSupplier.get());
            Drawable folderIcon =
                    BookmarkViewUtils.getFolderIcon(
                            mActivity,
                            bookmarkItem.getId(),
                            bookmarkModel,
                            BookmarkRowDisplayPref.VISUAL);
            // Utilize lazy static caching and call this only once for the entire time the app is
            // running.
            sFolderIconBitmap = drawableToBitmap(folderIcon);
        }

        // Convert ModelList to ArrayList.
        List<ListItem> childrenList = new ArrayList<>();
        for (ListItem item : children) {
            childrenList.add(item);
        }

        // TODO(crbug.com/430057288): Add metric for more submenu folder clicks.
        final PropertyModel model =
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, bookmarkItem.getTitle())
                        .with(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END, true)
                        .with(ListMenuSubmenuItemProperties.SUBMENU_ITEMS, childrenList)
                        .with(ListMenuItemProperties.START_ICON_BITMAP, sFolderIconBitmap)
                        .with(ListMenuItemProperties.ENABLED, true)
                        .build();

        ListItem listItem = new ListItem(ListItemType.MENU_ITEM_WITH_SUBMENU, model);
        model.set(
                ListMenuItemProperties.KEY_LISTENER,
                createPopupMenuItemKeyListener(model, bookmarkItem));
        return listItem;
    }

    // Bookmark leaves are web pages and not folders. They do not have any children (sub menu
    // items).
    @SuppressLint("ClickableViewAccessibility")
    private ListItem createListItemForBookmarkLeaf(BookmarkItem bookmarkItem) {
        // Handles all pointer-based input (mouse clicks, touch taps) to support both
        // simple clicks and Ctrl+clicks in one place. Because this listener handles the
        // action directly, a separate OnClickListener is not needed.
        // We return true to consume the event, which prevents any other listeners from
        // firing and allows us to suppress the "performClick" lint warning.
        View.OnTouchListener touchListener =
                (v, event) -> {
                    // We only act when the user lifts their finger/mouse button.
                    if (event.getAction() == MotionEvent.ACTION_UP) {
                        boolean isCtrlPressed = (event.getMetaState() & KeyEvent.META_CTRL_ON) != 0;

                        BookmarkBarUtils.recordClick(BookmarkBarClickType.POP_UP_URL);
                        if (isCtrlPressed) {
                            // Open in new tab.
                            mBookmarkOpener.openBookmarksInNewTabs(
                                    List.of(bookmarkItem.getId()),
                                    mProfileSupplier.get().isOffTheRecord(),
                                    Optional.of(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));
                        } else {
                            // Default behavior (open in current tab).
                            mBookmarkOpener.openBookmarkInCurrentTab(
                                    bookmarkItem.getId(), mProfileSupplier.get().isOffTheRecord());
                        }

                        // Dismiss the popup after any click.
                        if (mAnchoredPopupWindow != null) mAnchoredPopupWindow.dismiss();
                        // It is critical that this listener returns true to consume the event. This
                        // prevents the BasicListMenu's generic click handler from firing, which
                        // would cause a crash because this item's model no longer has a
                        // CLICK_LISTENER.
                        return true;
                    }
                    return false;
                };

        PropertyModel model =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, bookmarkItem.getTitle())
                        .with(ListMenuItemProperties.SUBTITLE, bookmarkItem.getUrl().getSpec())
                        .with(ListMenuItemProperties.IS_SUBTITLE_ELLIPSIZED_AT_END, true)
                        .with(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END, true)
                        .with(ListMenuItemProperties.ENABLED, true)
                        .with(ListMenuItemProperties.TOUCH_LISTENER, touchListener)
                        .build();
        if (mImageFetcher != null) {
            mImageFetcher.fetchFaviconForBookmark(
                    bookmarkItem,
                    (iconDrawable) -> {
                        // Update property model once we fetch the icon.
                        model.set(ListMenuItemProperties.START_ICON_DRAWABLE, iconDrawable);
                    });
        }

        ListItem listItem = new ListItem(ListItemType.MENU_ITEM, model);
        model.set(
                ListMenuItemProperties.KEY_LISTENER,
                createPopupMenuItemKeyListener(model, bookmarkItem));
        return listItem;
    }

    private static Bitmap drawableToBitmap(Drawable drawable) {
        Bitmap bitmap =
                Bitmap.createBitmap(
                        drawable.getIntrinsicWidth(),
                        drawable.getIntrinsicHeight(),
                        Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }

    public void onBrowserControlsChanged(int topControlsHeight, int bottomControlsHeight) {
        if (mBrowserControlsRectProvider != null) {
            mBrowserControlsRectProvider.updateRectAndNotify(
                    topControlsHeight, bottomControlsHeight);
        }
    }

    // A {@link RectProvider} that provides the visible area of the screen, accounting for the
    // browser controls. Flow:
    // 1. BrowserControlsStateProvider.Observer in BookmarkBarCoordinator calls
    // #onBrowserControlsChanged whenever the toolbars change.
    // 2. #onBrowserControlsChanged calls #updateRectAndNotify.
    // 3. #updateRectAndNotify calculates the new Rect and calls #notifyRectChanged, which is picked
    // up by AnchoredPopupWindow.
    // 4. AnchoredPopupWindow calls .getRect() on BrowserControlsRectProvider and gets the new
    // Rect.
    // 5. The new Rect is used in #calculatePopupWindowSpec.

    private static class BrowserControlsRectProvider extends RectProvider {
        private final Activity mActivity;

        BrowserControlsRectProvider(Activity activity) {
            mActivity = activity;
        }

        // Provides the available screen by obtaining the entire device screen and then cutting off
        // the top and bottom controls.
        public void updateRectAndNotify(int topControlsHeight, int bottomControlsHeight) {
            DisplayMetrics displayMetrics = mActivity.getResources().getDisplayMetrics();
            // Creates a Rect that covers the entire physical screen.
            mRect.set(0, 0, displayMetrics.widthPixels, displayMetrics.heightPixels);
            mRect.top += topControlsHeight;
            mRect.bottom -= bottomControlsHeight;
            // Notify the observer that the rect has changed.
            notifyRectChanged();
        }
    }

    // Builds and returns an OnKeyListener for every item in the popup menu.
    private View.OnKeyListener createPopupMenuItemKeyListener(
            PropertyModel model, BookmarkItem bookmarkItem) {
        // view is the root View object inflated from list_menu_item.xml.
        return (view, keyCode, event) -> {
            if (bookmarkItem == null) return false;
            // ACTION_DOWN is used because KeyNavigationUtil#isGoBackward depends on isActionDown to
            // be true.
            if (event.getAction() == KeyEvent.ACTION_DOWN) {
                // Handle Left Arrow to go back to the parent menu.
                if (KeyNavigationUtil.isGoBackward(event)) {
                    // Directly find the submenu header, which the go back onClickListener is
                    // attached to.
                    View headerView = findMenuHeaderView(view);
                    if (headerView != null) {
                        // Calls headerBackClick.run() in ListMenuUtils.
                        headerView.performClick();
                        // We've handled the left arrow, so consume the event.
                        return true;
                    }
                }

                // Handle Right Arrow to drill-down only if the item is a folder.
                if (KeyNavigationUtil.isGoForward(event) && bookmarkItem.isFolder()) {
                    // Get the pre-made "open submenu" click listener from the model.
                    View.OnClickListener clickListener =
                            model.get(ListMenuItemProperties.CLICK_LISTENER);
                    if (clickListener != null) {
                        // Calls ListMenuUtils#onItemWithSubmenuClicked.
                        BookmarkBarUtils.recordClick(BookmarkBarClickType.POP_UP_FOLDER);
                        clickListener.onClick(view);
                    }
                    // We've handled the right arrow, so consume the event.
                    return true;
                }
            }
            // Only proceed if the user has released the Enter key.
            if (event.getAction() == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_ENTER) {
                if (bookmarkItem.isFolder()) {
                    // Get the pre-made "open submenu" click listener from the model.
                    View.OnClickListener clickListener =
                            model.get(ListMenuItemProperties.CLICK_LISTENER);
                    if (clickListener != null) {
                        // Calls ListMenuUtils#onItemWithSubmenuClicked.
                        BookmarkBarUtils.recordClick(BookmarkBarClickType.POP_UP_FOLDER);
                        clickListener.onClick(view);
                    }
                    return true;
                }

                // When not a folder, this must be a URL, which will be opened in either the current
                // tab or a new tab when Ctrl is also pressed.
                BookmarkBarUtils.recordClick(BookmarkBarClickType.POP_UP_URL);
                if (event.isCtrlPressed()) {
                    mBookmarkOpener.openBookmarksInNewTabs(
                            List.of(bookmarkItem.getId()),
                            mProfileSupplier.get().isOffTheRecord(),
                            Optional.of(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));
                } else {
                    mBookmarkOpener.openBookmarkInCurrentTab(
                            bookmarkItem.getId(), mProfileSupplier.get().isOffTheRecord());
                }

                // Dismiss only when opening a bookmark (webpage) and not a folder, and always
                // consume the event to prevent fallback.
                if (mAnchoredPopupWindow != null) mAnchoredPopupWindow.dismiss();
                return true;
            }
            return false;
        };
    }

    /**
     * Finds the header view within the current popup menu by traversing up from a given item
     * (list_menu_item.xml). This method is used to trigger the back navigation for the left arrow
     * key.
     *
     * @param currentItemView The view of a currently focused menu item.
     * @return The menu header view if found, otherwise null.
     */
    private @Nullable View findMenuHeaderView(View currentItemView) {
        ViewParent parent = currentItemView.getParent();
        // Walk up the tree until we find a parent that contains R.id.menu_header.
        while (parent instanceof View) {
            View parentView = (View) parent;
            ListView headerListView = parentView.findViewById(R.id.menu_header);
            if (headerListView != null && headerListView.getChildCount() > 0) {
                // If headerListView is not null, there will only be one item inside it,
                // the headerView that acts as the back button.
                return headerListView.getChildAt(0);
            }
            parent = parentView.getParent();
        }
        return null;
    }

    void setAnchoredPopupWindowForTesting(AnchoredPopupWindow anchoredPopupWindow) {
        mAnchoredPopupWindow = anchoredPopupWindow;
    }
}
