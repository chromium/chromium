// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.chromium.build.NullUtil.assertNonNull;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.drawable.Drawable;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewParent;
import android.widget.ListView;

import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.StyleRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkAddNewFolderCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUndoController;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.BookmarkViewUtils;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils.BookmarkBarClickType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.base.KeyNavigationUtil;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.function.BiConsumer;
import java.util.function.Supplier;

/** Mediator for the bookmark bar which provides users with bookmark access from top chrome. */
@NullMarked
class BookmarkBarMediator
        implements BookmarkBarItemsProvider.Observer, BookmarkBarContextMenuDelegate {

    @FunctionalInterface
    private interface BookmarkItemClickCallback {
        void onClick(BookmarkItem item, int metaState, int buttonState);
    }

    private static final int INVALID_INDEX = -1;
    private @Nullable Bitmap mFolderIconBitmap;
    private final Activity mActivity;
    private final PropertyModel mAllBookmarksButtonModel;

    private final ModelList mItemsModel;
    private final NonNullObservableSupplier<Boolean> mItemsOverflowSupplier;
    private final BookmarkBarItemsLayoutManager mBookmarkBarItemsLayoutManager;
    private final Callback<Boolean> mItemsOverflowSupplierObserver;
    private final PropertyModel mModel;
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver;
    private final Supplier<@Nullable Tab> mCurrentTabSupplier;
    private final BookmarkOpener mBookmarkOpener;
    private final MonotonicObservableSupplier<BookmarkManagerOpener> mBookmarkManagerOpenerSupplier;
    private final Supplier<@Nullable SnackbarManager> mSnackbarManagerSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final RecyclerView mItemsRecyclerView;
    private final BookmarkBar mBookmarkBarView;
    private @Nullable BookmarkUndoController mBookmarkUndoController;
    private @StyleRes int mCurrentTextStyleRes = R.style.TextAppearance_TextMedium_Primary_Baseline;
    private @ColorRes int mCurrentIconTintRes = R.color.default_icon_color_tint_list;
    @DrawableRes private int mCurrentBackgroundId;

    private final Point mLastTouchPoint = new Point();
    private int mLastTouchButtonState;
    private int mLastTouchMetaState;
    private boolean mIsActiveGestureSecondary;

    private BookmarkBarPopupCoordinator mPopupCoordinator;
    private final BookmarkBarContextMenuMediator mContextMenuMediator;
    private @Nullable BookmarkImageFetcher mImageFetcher;
    private @Nullable BookmarkBarItemsProvider mItemsProvider;

    /**
     * Constructs the bookmark bar mediator.
     *
     * @param activity The activity which is hosting the bookmark bar.
     * @param allBookmarksButtonModel The model for the 'All Bookmarks' button.
     * @param itemsModel The model for the items which are rendered within the bookmark bar.
     * @param bookmarkBarItemsLayoutManager The layout manager used to render the horizontal list of
     *     items within the bookmark bar.
     * @param model The model used to read/write bookmark bar properties.
     * @param profileSupplier Used to access the active user profile.
     * @param currentTabSupplier Used to observe or retrieve the active tab.
     * @param bookmarkOpener Used to open bookmarks.
     * @param bookmarkManagerOpenerSupplier Used to open the bookmark manager.
     * @param snackbarManagerSupplier Used to display snackbar notifications.
     * @param modalDialogManagerSupplier Used to display modal dialogs.
     * @param itemsRecyclerView The bookmark_bar_items_container recycler view that is inside the
     *     bookmark_bar view.
     * @param bookmarkBarView The bookmark_bar view that contains the entire bookmarks bar.
     * @param popupCoordinator The coordinator for displaying popups.
     */
    BookmarkBarMediator(
            Activity activity,
            PropertyModel allBookmarksButtonModel,
            ModelList itemsModel,
            BookmarkBarItemsLayoutManager bookmarkBarItemsLayoutManager,
            PropertyModel model,
            MonotonicObservableSupplier<Profile> profileSupplier,
            Supplier<@Nullable Tab> currentTabSupplier,
            BookmarkOpener bookmarkOpener,
            MonotonicObservableSupplier<BookmarkManagerOpener> bookmarkManagerOpenerSupplier,
            Supplier<@Nullable SnackbarManager> snackbarManagerSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            RecyclerView itemsRecyclerView,
            BookmarkBar bookmarkBarView,
            BookmarkBarPopupCoordinator popupCoordinator) {
        mActivity = activity;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;

        mAllBookmarksButtonModel = allBookmarksButtonModel;
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.CLICK_CALLBACK, this::onAllBookmarksButtonClick);
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.ICON_SUPPLIER,
                LazyOneshotSupplier.fromValue(
                        AppCompatResources.getDrawable(
                                mActivity, R.drawable.ic_all_bookmarks_icon_16dp)));
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.TEXT_APPEARANCE_ID,
                R.style.TextAppearance_TextMedium_Primary_Baseline);
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.TITLE,
                mActivity.getString(R.string.bookmark_bar_all_bookmarks_button_title));
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.TOOLTIP,
                mActivity.getString(R.string.bookmark_bar_all_bookmarks_button_title));

        mItemsModel = itemsModel;

        mBookmarkBarItemsLayoutManager = bookmarkBarItemsLayoutManager;
        mItemsOverflowSupplier = mBookmarkBarItemsLayoutManager.getItemsOverflowSupplier();
        mItemsOverflowSupplierObserver = this::onItemsOverflowChange;
        mItemsOverflowSupplier.addSyncObserverAndPostIfNonNull(mItemsOverflowSupplierObserver);

        mModel = model;
        mModel.set(
                BookmarkBarProperties.OVERFLOW_BUTTON_CLICK_CALLBACK, this::onOverflowButtonClick);
        mModel.set(BookmarkBarProperties.OVERFLOW_BUTTON_VISIBILITY, View.INVISIBLE);

        mProfileSupplier = profileSupplier;
        mProfileSupplierObserver = this::onProfileChange;
        mProfileSupplier.addSyncObserverAndPostIfNonNull(mProfileSupplierObserver);

        mCurrentTabSupplier = currentTabSupplier;
        mBookmarkOpener = bookmarkOpener;
        mBookmarkManagerOpenerSupplier = bookmarkManagerOpenerSupplier;
        mItemsRecyclerView = itemsRecyclerView;
        mBookmarkBarView = bookmarkBarView;
        mBookmarkBarView.setContentDescription(
                mActivity.getString(R.string.bookmark_bar_content_description));
        mBookmarkBarView.setRightClickCallback(this::onBookmarksBarEmptySpaceRightClicked);

        mPopupCoordinator = popupCoordinator;
        mContextMenuMediator =
                new BookmarkBarContextMenuMediator(
                        activity,
                        profileSupplier,
                        currentTabSupplier,
                        this,
                        popupCoordinator::dismiss);
    }

    /** Destroys the bookmark bar mediator. */
    public void destroy() {
        mBookmarkBarView.setRightClickCallback(null);
        mPopupCoordinator.dismiss();
        mAllBookmarksButtonModel.set(BookmarkBarButtonProperties.CLICK_CALLBACK, null);
        mItemsOverflowSupplier.removeObserver(mItemsOverflowSupplierObserver);

        mFolderIconBitmap = null;

        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mItemsProvider != null) {
            mItemsProvider.destroy();
            mItemsProvider = null;
        }

        if (mBookmarkUndoController != null) {
            mBookmarkUndoController.destroy();
            mBookmarkUndoController = null;
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
                createListItemFor(
                        this::onBookmarkItemClick,
                        mImageFetcher,
                        item,
                        mCurrentIconTintRes,
                        mCurrentTextStyleRes,
                        mCurrentBackgroundId));
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
                createListItemFor(
                        this::onBookmarkItemClick,
                        mImageFetcher,
                        item,
                        mCurrentIconTintRes,
                        mCurrentTextStyleRes,
                        mCurrentBackgroundId));
    }

    @Override
    public void onBookmarkItemsAdded(
            @BookmarkBarItemsProvider.ObservationId int observationId,
            List<BookmarkItem> items,
            int index) {
        final List<ListItem> batch = new ArrayList<>();
        for (int i = 0; i < items.size(); i++) {
            batch.add(
                    createListItemFor(
                            this::onBookmarkItemClick,
                            mImageFetcher,
                            items.get(i),
                            mCurrentIconTintRes,
                            mCurrentTextStyleRes,
                            mCurrentBackgroundId));
        }
        mItemsModel.addAll(batch, index);
    }

    @Override
    public void onBookmarkItemsRemoved(
            @BookmarkBarItemsProvider.ObservationId int observationId, int index, int count) {
        mItemsModel.removeRange(index, count);
    }

    // Private methods.

    private void showContextMenu(
            ModelList menuModel, View anchorView, @Nullable Point offset, boolean isIncognito) {
        if (!ChromeFeatureList.sBookmarksBarContextMenu.isEnabled()) {
            return;
        }
        mPopupCoordinator.showContextMenuPopup(menuModel, anchorView, offset, isIncognito);
    }

    private void showContextMenuForListItem(
            BookmarkItem item, @Nullable View anchorView, @Nullable Point offset) {
        if (anchorView == null) return;
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    ModelList menuModel =
                            mContextMenuMediator.buildContextMenuModelList(item, model);
                    showContextMenu(menuModel, anchorView, offset, profile.isOffTheRecord());
                });
    }

    // TODO(crbug.com/394614779): Open in popup window instead of bookmark manager.
    private void onAllBookmarksButtonClick(int metaState, int buttonState) {
        if ((buttonState & MotionEvent.BUTTON_SECONDARY) != 0) {
            return;
        }

        // Open the manager iff the active profile and model are unchanged to prevent accidentally
        // opening the manager for the wrong profile/model. We will only record the click event if
        // this guard passes, so the data shows only actions that resulted in a change.
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    BookmarkBarUtils.recordClick(BookmarkBarClickType.ALL_BOOKMARKS);
                    assertNonNull(mBookmarkManagerOpenerSupplier.get())
                            .showBookmarkManager(
                                    mActivity,
                                    mCurrentTabSupplier.get(),
                                    profile,
                                    model.getRootFolderId());
                });
    }

    private void onBookmarksBarEmptySpaceRightClicked(float x, float y) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    ModelList menuModel =
                            mContextMenuMediator.buildBookmarksBarEmptySpaceContextMenuModelList(
                                    model);
                    showContextMenu(
                            menuModel,
                            mBookmarkBarView,
                            new Point((int) x, (int) y),
                            profile.isOffTheRecord());
                });
    }

    private void onBookmarkItemClick(BookmarkItem item, int metaState, int buttonState) {
        final boolean isRightClick = (buttonState & MotionEvent.BUTTON_SECONDARY) != 0;
        if (isRightClick) {
            View anchorView = getAnchorViewForBookmark(item);
            if (anchorView == null) return;
            Point offset = new Point(mLastTouchPoint);
            showContextMenuForListItem(item, anchorView, offset);
            return;
        }

        if (item.isFolder()) {
            // Get the view of the folder that was clicked.
            View anchorView = getAnchorViewForBookmark(item);
            if (anchorView == null) return;
            runIfStillRelevantAfterFinishLoadingBookmarkModel(
                    (profile, model) -> {
                        // Build the entire model list for this folder. The grandchildren are stored
                        // in SUBMENU_PROVIDER.
                        ModelList menuModel = buildMenuModelListForFolder(model, item.getId());
                        BookmarkBarUtils.recordClick(BookmarkBarClickType.BOOKMARK_BAR_FOLDER);
                        mPopupCoordinator.showFolderItemsPopup(
                                anchorView, menuModel, profile.isOffTheRecord());
                    });
            return;
        }

        final Profile profile = assertNonNull(mProfileSupplier.get());
        BookmarkBarUtils.recordClick(BookmarkBarClickType.BOOKMARK_BAR_URL);
        final boolean isCtrlPressed = (metaState & KeyEvent.META_CTRL_ON) != 0;
        final boolean isMiddleClick = (buttonState & MotionEvent.BUTTON_TERTIARY) != 0;
        if (isCtrlPressed || isMiddleClick) {
            mBookmarkOpener.openBookmarksInNewTabs(
                    List.of(item.getId()),
                    profile.isOffTheRecord(),
                    TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND);
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
                (profile, model) -> {
                    // Get an ordered list of all the children (both folders and web pages) of the
                    // bookmarks bar.
                    List<BookmarkId> allBookmarkItems = BookmarkUtils.getDesktopBookmarkIds(model);

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
                    ModelList hiddenItemsModelList = buildMenuModelListFromIds(model, hiddenItems);

                    // Get the anchor view, which is bookmark_bar_overflow_button.
                    View anchorView = mBookmarkBarView.getOverflowButton();
                    if (anchorView == null) return;

                    // Show the popup with the filtered model. Notice that when we call
                    // mContextMenuCoordinator.showContextMenu inside #onBookmarkItemClick, we are
                    // calling it for one specific folder in the bookmarks bar, whereas here it is
                    // for all the hidden items in the entire bookmarks bar ("desktopFolder").
                    BookmarkBarUtils.recordClick(BookmarkBarClickType.OVERFLOW_MENU);
                    mPopupCoordinator.showFolderItemsPopup(
                            anchorView, hiddenItemsModelList, profile.isOffTheRecord());
                });
    }

    private void onProfileChange(@Nullable Profile newProfile) {
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mItemsProvider != null) {
            mItemsProvider.destroy();
            mItemsProvider = null;
        }

        if (mBookmarkUndoController != null) {
            mBookmarkUndoController.destroy();
            mBookmarkUndoController = null;
        }

        mItemsModel.clear();

        mPopupCoordinator.dismiss();

        if (newProfile == null) {
            return;
        }

        // Instantiate dependencies iff the active profile and model are unchanged to prevent
        // accidentally instantiating dependencies for the wrong profile/model.
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    mImageFetcher =
                            new BookmarkImageFetcher(
                                    profile,
                                    mActivity,
                                    model,
                                    ImageFetcherFactory.createImageFetcher(
                                            ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                                            profile.getProfileKey(),
                                            GlobalDiscardableReferencePool.getReferencePool()),
                                    FaviconUtils.createCircularIconGenerator(mActivity));

                    mItemsProvider = new BookmarkBarItemsProvider(model, this);

                    if (mSnackbarManagerSupplier.get() != null) {
                        mBookmarkUndoController =
                                new BookmarkUndoController(
                                        mActivity, model, mSnackbarManagerSupplier.get());
                    }
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
                    final var profileAfterLoading = assertNonNull(mProfileSupplier.get());
                    if (!Objects.equals(profile, profileAfterLoading)) return;

                    // Ensure the active model hasn't changed while loading the model.
                    final var modelAfterLoading = BookmarkModel.getForProfile(profileAfterLoading);
                    if (!Objects.equals(model, modelAfterLoading)) return;

                    // Run the callback iff the active profile and model are unchanged to avoid
                    // running the callback for the wrong profile/model.
                    callback.accept(profileAfterLoading, modelAfterLoading);
                });
    }

    // BookmarkBarContextMenuDelegate implementation.

    @Override
    public void openInNewTab(BookmarkId id) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    mBookmarkOpener.openBookmarksInNewTabs(
                            List.of(id),
                            profile.isOffTheRecord(),
                            TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND);
                });
    }

    @Override
    public void openInNewWindow(BookmarkId id) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    mBookmarkOpener.openBookmarksInNewWindow(List.of(id), /* incognito= */ false);
                });
    }

    @Override
    public void openInIncognitoWindow(BookmarkId id) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    mBookmarkOpener.openBookmarksInNewWindow(List.of(id), /* incognito= */ true);
                });
    }

    @Override
    public void openAll(List<BookmarkId> ids) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    mBookmarkOpener.openBookmarksInNewTabs(
                            ids,
                            profile.isOffTheRecord(),
                            TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND);
                });
    }

    @Override
    public void openAllInNewWindow(List<BookmarkId> ids) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    mBookmarkOpener.openBookmarksInNewWindow(ids, /* incognito= */ false);
                });
    }

    @Override
    public void openAllInIncognitoWindow(List<BookmarkId> ids) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    mBookmarkOpener.openBookmarksInNewWindow(ids, /* incognito= */ true);
                });
    }

    @Override
    public void openAllInNewTabGroup(List<BookmarkId> ids, @Nullable String title) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    mBookmarkOpener.openBookmarksInNewTabGroup(
                            ids, profile.isOffTheRecord(), title);
                });
    }

    @Override
    public void editBookmark(BookmarkId id) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    assertNonNull(mBookmarkManagerOpenerSupplier.get())
                            .startEditActivity(mActivity, profile, id);
                });
    }

    @Override
    public void moveBookmark(BookmarkId id) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    assertNonNull(mBookmarkManagerOpenerSupplier.get())
                            .startFolderPickerActivity(mActivity, profile, id);
                });
    }

    /**
     * Deletes a bookmark from the Bookmarks Bar, passing this mediator's persistent 1:1 {@link
     * BookmarkUndoController} as the originator to isolate snackbar display to this window.
     */
    @Override
    public void deleteBookmark(BookmarkId id) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> model.deleteBookmarks(mBookmarkUndoController, id));
    }

    @Override
    public void addPage(BookmarkId parentId) {
        Tab tab = mCurrentTabSupplier.get();
        if (tab == null) return;
        String title = tab.getTitle();
        GURL url = tab.getUrl();
        if (url == null || !url.isValid() || url.isEmpty()) return;

        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    int index = model.getChildCount(parentId);
                    BookmarkId newId = model.addBookmark(parentId, index, title, url);
                    if (newId != null) {
                        assertNonNull(mBookmarkManagerOpenerSupplier.get())
                                .startEditActivity(mActivity, profile, newId);
                    }
                });
    }

    @Override
    public void addFolder(BookmarkId parentId) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    BookmarkAddNewFolderCoordinator addNewFolderCoordinator =
                            new BookmarkAddNewFolderCoordinator(
                                    mActivity, mModalDialogManagerSupplier.get(), model);
                    addNewFolderCoordinator.show(parentId);
                });
    }

    @Override
    public void openBookmarksManager(BookmarkId folderId) {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    assertNonNull(mBookmarkManagerOpenerSupplier.get())
                            .showBookmarkManager(
                                    mActivity, mCurrentTabSupplier.get(), profile, folderId);
                });
    }

    @Override
    public void toggleBookmarksBar() {
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profile, model) -> {
                    BookmarkBarUtils.toggleShowBookmarksBar(
                            profile, /* fromKeyboardShortcut= */ false);
                });
    }

    public void setVisibility(boolean isVisible) {
        mModel.set(BookmarkBarProperties.VISIBILITY, isVisible ? View.VISIBLE : View.GONE);
    }

    private int getIndexInBookmarksBar(BookmarkItem item) {
        // Get the main data model for all bookmarks for the user.
        BookmarkModel bookmarkModel =
                BookmarkModel.getForProfile(assertNonNull(mProfileSupplier.get()));
        if (bookmarkModel == null) return INVALID_INDEX;

        return BookmarkUtils.getDesktopBookmarkIds(bookmarkModel).indexOf(item.getId());
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
    // because each folder's SUBMENU_PROVIDER contains the children list as a separate model list.
    ModelList buildMenuModelListForFolder(BookmarkModel bookmarkModel, BookmarkId folderId) {
        List<BookmarkId> childIds = bookmarkModel.getChildIds(folderId);
        return buildMenuModelListFromIds(bookmarkModel, childIds);
    }

    // A reusable method that returns the ModelList from a specific list of Ids.
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

    @SuppressLint("ClickableViewAccessibility")
    private PropertyModel.Builder createBasePopupMenuItemBuilder(
            PropertyKey[] keys, BookmarkItem bookmarkItem) {
        final Profile profile = mProfileSupplier.get();
        final boolean isIncognito = profile != null && profile.isOffTheRecord();

        return new PropertyModel.Builder(keys)
                .with(ListMenuItemProperties.TITLE, bookmarkItem.getTitle())
                .with(ListMenuItemProperties.TOOLTIP, bookmarkItem.getTitle())
                .with(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END, true)
                .with(ListMenuItemProperties.ENABLED, true)
                .with(
                        ListMenuItemProperties.LONG_CLICK_LISTENER,
                        (v) -> {
                            showContextMenuForListItem(bookmarkItem, v, new Point(mLastTouchPoint));
                            return true;
                        })
                .with(
                        ListMenuItemProperties.TOUCH_LISTENER,
                        (v, event) -> handlePopupItemTouch(bookmarkItem, v, event))
                .with(
                        ListMenuItemProperties.TEXT_APPEARANCE_ID,
                        isIncognito ? R.style.TextAppearance_TextLarge_Primary_Baseline_Light : 0);
    }

    // Folders do not have urls.
    @SuppressLint("ClickableViewAccessibility")
    private ListItem createListItemForBookmarkFolder(
            BookmarkItem bookmarkItem, ModelList children) {

        if (mFolderIconBitmap == null) {
            BookmarkModel bookmarkModel =
                    BookmarkModel.getForProfile(assertNonNull(mProfileSupplier.get()));
            Drawable folderIcon =
                    BookmarkViewUtils.getFolderIcon(
                            mActivity,
                            bookmarkItem.getId(),
                            bookmarkModel,
                            BookmarkRowDisplayPref.VISUAL);
            // Cache the folder icon bitmap on the mediator instance so we only convert it once.
            mFolderIconBitmap = drawableToBitmap(folderIcon);
        }

        // Convert ModelList to ArrayList.
        List<ListItem> childrenList = new ArrayList<>();
        for (ListItem item : children) {
            childrenList.add(item);
        }

        View.OnClickListener clickListener =
                (v) -> BookmarkBarUtils.recordClick(BookmarkBarClickType.POP_UP_FOLDER);

        final Profile profile = mProfileSupplier.get();
        final boolean isIncognito = profile != null && profile.isOffTheRecord();
        final PropertyModel model =
                createBasePopupMenuItemBuilder(ListMenuSubmenuItemProperties.ALL_KEYS, bookmarkItem)
                        .with(
                                ListMenuItemProperties.CONTENT_DESCRIPTION,
                                mActivity.getString(
                                        R.string.bookmark_bar_folder_content_description,
                                        bookmarkItem.getTitle()))
                        .with(ListMenuSubmenuItemProperties.SUBMENU_PROVIDER, () -> childrenList)
                        .with(ListMenuItemProperties.START_ICON_BITMAP, mFolderIconBitmap)
                        .with(ListMenuItemProperties.CLICK_LISTENER, clickListener)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                isIncognito
                                        ? R.color.default_icon_color_light
                                        : R.color.default_icon_color_tint_list)
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
        // When building this model, we add both a touch and click listener. This click listener is
        // to handle AccessibilityServices, which send click events rather than touch events.
        // Without the listener added here, actions performed on a leaf node in the anchored pop up
        // will have no effect. Taps, keyboard, and mice all send touch events and do not send click
        // events, so there are no cases of double events be received.
        PropertyModel model =
                createBasePopupMenuItemBuilder(ListMenuItemProperties.ALL_KEYS, bookmarkItem)
                        .with(ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN, true)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                Resources.ID_NULL)
                        .with(
                                ListMenuItemProperties.CLICK_LISTENER,
                                (v) -> {
                                    // Open url.
                                    BookmarkBarUtils.recordClick(BookmarkBarClickType.POP_UP_URL);
                                    boolean isOffTheRecord =
                                            assertNonNull(mProfileSupplier.get()).isOffTheRecord();
                                    boolean isCtrlPressed =
                                            (mLastTouchMetaState & KeyEvent.META_CTRL_ON) != 0;
                                    if (isCtrlPressed) {
                                        mBookmarkOpener.openBookmarksInNewTabs(
                                                List.of(bookmarkItem.getId()),
                                                isOffTheRecord,
                                                TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND);
                                    } else {
                                        mBookmarkOpener.openBookmarkInCurrentTab(
                                                bookmarkItem.getId(), isOffTheRecord);
                                    }
                                    mPopupCoordinator.dismiss();
                                })
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

    private boolean handlePopupItemTouch(BookmarkItem bookmarkItem, View v, MotionEvent event) {
        int action = event.getActionMasked();
        int buttonState = event.getButtonState();

        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_BUTTON_PRESS) {
            mLastTouchPoint.set((int) event.getX(), (int) event.getY());
            mLastTouchButtonState = buttonState;
            mLastTouchMetaState = event.getMetaState();

            boolean isSecondary = (buttonState & MotionEvent.BUTTON_SECONDARY) != 0;
            mIsActiveGestureSecondary =
                    isSecondary && ChromeFeatureList.sBookmarksBarContextMenu.isEnabled();
            if (mIsActiveGestureSecondary) {
                showContextMenuForListItem(bookmarkItem, v, new Point(mLastTouchPoint));
                return true;
            }

            boolean isTertiary = (buttonState & MotionEvent.BUTTON_TERTIARY) != 0;
            if (isTertiary) {
                return true;
            }
        }

        if (action == MotionEvent.ACTION_UP
                || action == MotionEvent.ACTION_CANCEL
                || action == MotionEvent.ACTION_BUTTON_RELEASE) {
            // Handle middle click on release if it started as tertiary.
            if ((action == MotionEvent.ACTION_BUTTON_RELEASE
                            && event.getActionButton() == MotionEvent.BUTTON_TERTIARY)
                    || (mLastTouchButtonState & MotionEvent.BUTTON_TERTIARY) != 0) {
                boolean isOffTheRecord = assertNonNull(mProfileSupplier.get()).isOffTheRecord();
                mBookmarkOpener.openBookmarksInNewTabs(
                        List.of(bookmarkItem.getId()),
                        isOffTheRecord,
                        TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND);
                mPopupCoordinator.dismiss();
                mLastTouchButtonState = 0;
                return true;
            }

            if (mIsActiveGestureSecondary) {
                mIsActiveGestureSecondary = false;
                return true;
            }
        }

        return false;
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
        mPopupCoordinator.onBrowserControlsChanged(topControlsHeight, bottomControlsHeight);
    }

    /**
     * Dismisses the pop up menu if it is open, used for upstream clients/owners communicating state
     * changes from external components, e.g. screen width change.
     */
    public void dismissPopupMenu() {
        mPopupCoordinator.dismiss();
    }

    // Builds and returns an OnKeyListener for every item in the popup menu.
    private View.OnKeyListener createPopupMenuItemKeyListener(
            PropertyModel model, BookmarkItem bookmarkItem) {
        // view is the root View object inflated from list_menu_item.xml.
        return (view, keyCode, event) -> {
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
                boolean isOffTheRecord = assertNonNull(mProfileSupplier.get()).isOffTheRecord();

                if (event.isCtrlPressed()) {
                    mBookmarkOpener.openBookmarksInNewTabs(
                            List.of(bookmarkItem.getId()),
                            isOffTheRecord,
                            TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND);
                } else {
                    mBookmarkOpener.openBookmarkInCurrentTab(bookmarkItem.getId(), isOffTheRecord);
                }

                // Dismiss only when opening a bookmark (webpage) and not a folder, and always
                // consume the event to prevent fallback.
                mPopupCoordinator.dismiss();
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

    /**
     * Creates a list item to render in the bookmark bar for the specified bookmark item.
     *
     * @param clickCallback The callback to invoke on list item click events.
     * @param imageFetcher The image fetcher to use for rendering favicons.
     * @param item The bookmark item for which to create a renderable list item.
     * @param iconTintRes The theme-aware color resource ID for the icon tint.
     * @param textStyleRes The theme-aware style resource ID for the text appearance.
     * @param backgroundResId The theme-aware drawable resource ID for the button's background.
     * @return The created list item to render in the bookmark bar.
     */
    private ListItem createListItemFor(
            BookmarkItemClickCallback clickCallback,
            @Nullable BookmarkImageFetcher imageFetcher,
            BookmarkItem item,
            @ColorRes int iconTintRes,
            @StyleRes int textStyleRes,
            @DrawableRes int backgroundResId) {

        View.OnKeyListener keyListener =
                (v, keyCode, event) -> {
                    // Check whether the Enter key is released.
                    if (event.getAction() == KeyEvent.ACTION_UP
                            && keyCode == KeyEvent.KEYCODE_ENTER) {
                        // clickCallback is an object that represents
                        // BookmarkBarMediator#onBookmarkItemClick.
                        clickCallback.onClick(item, event.getMetaState(), /* buttonState= */ 0);
                        // Returning true handles the event, avoids triggering a normal click
                        // (double action).
                        return true;
                    }
                    // We do not handle other keys.
                    return false;
                };

        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(BookmarkBarButtonProperties.ALL_KEYS)
                        .with(
                                BookmarkBarButtonProperties.CLICK_CALLBACK,
                                (metaState, buttonState) ->
                                        clickCallback.onClick(item, metaState, buttonState))
                        .with(
                                BookmarkBarButtonProperties.POINT_CALLBACK,
                                point -> mLastTouchPoint.set(point.x, point.y))
                        .with(BookmarkBarButtonProperties.KEY_LISTENER, keyListener)
                        .with(
                                BookmarkBarButtonProperties.ICON_TINT_LIST_ID,
                                item.isFolder() ? iconTintRes : Resources.ID_NULL)
                        .with(
                                BookmarkBarButtonProperties.FOLDER_CONTENT_DESCRIPTION,
                                item.isFolder()
                                        ? mActivity.getString(
                                                R.string.bookmark_bar_folder_content_description,
                                                item.getTitle())
                                        : null)
                        .with(BookmarkBarButtonProperties.TITLE, item.getTitle())
                        .with(BookmarkBarButtonProperties.TOOLTIP, item.getTitle())
                        .with(BookmarkBarButtonProperties.BOOKMARK_ITEM, item)
                        .with(BookmarkBarButtonProperties.TEXT_APPEARANCE_ID, textStyleRes)
                        .with(BookmarkBarButtonProperties.BACKGROUND_DRAWABLE_ID, backgroundResId);

        if (imageFetcher != null) {
            modelBuilder.with(
                    BookmarkBarButtonProperties.ICON_SUPPLIER,
                    createIconSupplierFor(imageFetcher, item));
        }
        return new ListItem(BookmarkBarUtils.ViewType.ITEM, modelBuilder.build());
    }

    private LazyOneshotSupplier<Drawable> createIconSupplierFor(
            BookmarkImageFetcher imageFetcher, BookmarkItem item) {
        if (item.isFolder()) {
            return LazyOneshotSupplier.fromSupplier(
                    () ->
                            AppCompatResources.getDrawable(
                                    mActivity, R.drawable.ic_folder_outline_24dp));
        }
        return new LazyOneshotSupplierImpl<>() {
            @Override
            public void doSet() {
                imageFetcher.fetchFaviconForBookmark(item, this::set);
            }
        };
    }

    /**
     * Called by the Coordinator when the theme changes or when the tabs are switched. This method
     * is responsible for updating the theme for all bookmark bar components. The flow is:
     * Mediator#onThemeChanged -> Mediator#onProfileChange ->
     * BookmarkBarItemsProvider#onBookmarkItemAdded -> Mediator#onBookmarkItemAdded
     *
     * @param isIncognito Whether the current theme is incognito.
     * @param brandedColorScheme The brandedColorScheme, which accounts for incognito.
     */
    public void onThemeChanged(boolean isIncognito, @BrandedColorScheme int brandedColorScheme) {

        mCurrentIconTintRes =
                ThemeUtils.getThemedToolbarIconTintResForActivityState(
                        brandedColorScheme, /* isActivityFocused= */ true);
        mCurrentTextStyleRes =
                isIncognito
                        ? R.style.TextAppearance_TextMediumThick_Secondary_Baseline_Light
                        : R.style.TextAppearance_TextMediumThick_Secondary;

        // Select the correct ripple drawable based on the theme.
        mCurrentBackgroundId =
                isIncognito
                        ? R.drawable.bookmark_bar_ripple_baseline
                        : R.drawable.bookmark_bar_ripple;

        // Update the "All Bookmarks" star icon based on the correct theme.
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.ICON_TINT_LIST_ID, mCurrentIconTintRes);

        // Update the "All Bookmarks" text based on the correct theme.
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.TEXT_APPEARANCE_ID, mCurrentTextStyleRes);

        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.BACKGROUND_DRAWABLE_ID, mCurrentBackgroundId);

        // Update the background resource of the overflow button.
        View overflowButton = mBookmarkBarView.findViewById(R.id.bookmark_bar_overflow_button);
        if (overflowButton != null) {
            overflowButton.setBackgroundResource(mCurrentBackgroundId);
        }

        // Update all of the item models in the RecyclerView.
        for (ListItem listItem : mItemsModel) {
            PropertyModel model = listItem.model;

            model.set(BookmarkBarButtonProperties.TEXT_APPEARANCE_ID, mCurrentTextStyleRes);
            model.set(BookmarkBarButtonProperties.BACKGROUND_DRAWABLE_ID, mCurrentBackgroundId);

            BookmarkItem item = model.get(BookmarkBarButtonProperties.BOOKMARK_ITEM);
            if (item.isFolder()) {
                // Only update the folder icon. The bookmark favicon is not theme-dependent.
                model.set(BookmarkBarButtonProperties.ICON_TINT_LIST_ID, mCurrentIconTintRes);
            } else {
                model.set(BookmarkBarButtonProperties.ICON_TINT_LIST_ID, Resources.ID_NULL);
            }
        }
    }

    @Nullable Bitmap getFolderIconBitmapForTesting() {
        return mFolderIconBitmap;
    }

    void setPopupCoordinatorForTesting(BookmarkBarPopupCoordinator popupCoordinator) {
        mPopupCoordinator = popupCoordinator;
    }
}
