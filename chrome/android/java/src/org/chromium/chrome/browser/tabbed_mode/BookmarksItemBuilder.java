// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemTheme;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemUtils;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBookmarkItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Supplier;

/** Builds AppMenu items related to bookmarks. */
@NullMarked
public class BookmarksItemBuilder implements Destroyable {
    private final Context mContext;
    private final AppMenuItemTheme mAppMenuItemTheme;
    private final Supplier<@Nullable BookmarkModel> mBookmarkModelSupplier;
    private final TabModelSelector mTabModelSelector;
    private final boolean mIsMenuIconAtStart;
    private final boolean mShouldShowIconBeforeItem;

    private @Nullable BookmarkImageFetcher mImageFetcher;

    /**
     * Constructs a {@link BookmarksItemBuilder} which is responsible for building Bookmarks related
     * menu items for the app menu.
     *
     * @param context The Android Context used to get resources.
     * @param appMenuItemTheme The theme used to style the app menu items.
     * @param bookmarkModelSupplier Supplies the BookmarkModel.
     * @param tabModelSelector The selector used to query tab state.
     * @param isMenuIconAtStart Whether the menu icon is displayed at the start.
     * @param shouldShowIconBeforeItem Whether an icon should be shown before the item text.
     */
    public BookmarksItemBuilder(
            Context context,
            AppMenuItemTheme appMenuItemTheme,
            Supplier<@Nullable BookmarkModel> bookmarkModelSupplier,
            TabModelSelector tabModelSelector,
            boolean isMenuIconAtStart,
            boolean shouldShowIconBeforeItem) {
        mContext = context;
        mAppMenuItemTheme = appMenuItemTheme;
        mBookmarkModelSupplier = bookmarkModelSupplier;
        mTabModelSelector = tabModelSelector;
        mIsMenuIconAtStart = isMenuIconAtStart;
        mShouldShowIconBeforeItem = shouldShowIconBeforeItem;
    }

    /** Cleans up resources used by this builder, specifically the image fetcher. */
    @Override
    public void destroy() {
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }
    }

    private Profile getProfileFromTabModel() {
        var profile = mTabModelSelector.getModel(false).getProfile();
        assert profile != null;
        return profile;
    }

    /**
     * Determines whether the "Bookmarks" parent item should be shown.
     *
     * @return true if the bookmarks parent item should be shown, false otherwise.
     */
    public boolean shouldShowBookmarksParentItem() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.SUBMENUS_IN_APP_MENU);
    }

    /**
     * Builds the "Bookmarks" parent item, populating its submenu with options to add bookmarks,
     * view reading list, and browse bookmark folders.
     *
     * @param currentTab The current active tab.
     * @return A {@link ListItem} representing the expandable Bookmarks parent item.
     */
    public ListItem buildBookmarksParentItem(@Nullable Tab currentTab) {
        assert shouldShowBookmarksParentItem();

        Supplier<List<ListItem>> submenuItemsSupplier =
                () -> {
                    List<ListItem> submenuItems = new ArrayList<>();

                    submenuItems.add(buildBookmarkThisPageItem());

                    submenuItems.add(
                            new ListItem(
                                    AppMenuHandler.AppMenuItemType.DIVIDER,
                                    AppMenuItemUtils.buildModelForDivider(R.id.divider_line_id)));

                    submenuItems.add(buildBookmarksItem(/* showIcon= */ false));

                    submenuItems.add(buildReadingListItem(currentTab));

                    submenuItems.add(
                            new ListItem(
                                    AppMenuHandler.AppMenuItemType.DIVIDER,
                                    AppMenuItemUtils.buildModelForDivider(R.id.divider_line_id)));

                    submenuItems.add(buildToggleBookmarksBarItem());

                    BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();

                    // TODO(crbug.com/521223427): Implement dynamic updates so that we don't
                    // have to rely on timing to load the {@link BookmarkModel}.
                    if (bookmarkModel != null && bookmarkModel.isBookmarkModelLoaded()) {
                        List<ListItem> bookmarksBarItems =
                                getBookmarkItemList(
                                        BookmarkUtils.getDesktopBookmarkIds(bookmarkModel),
                                        bookmarkModel);
                        if (bookmarksBarItems.size() > 0) {
                            submenuItems.add(
                                    new ListItem(
                                            AppMenuHandler.AppMenuItemType.DIVIDER,
                                            AppMenuItemUtils.buildModelForDivider(
                                                    R.id.divider_line_id)));
                            submenuItems.add(
                                    AppMenuItemUtils.buildHeaderItem(
                                            mContext,
                                            mAppMenuItemTheme,
                                            R.id.bookmarks_header_menu_id,
                                            R.string.bookmarks,
                                            mIsMenuIconAtStart));
                            submenuItems.addAll(bookmarksBarItems);
                        }

                        submenuItems.add(
                                new ListItem(
                                        AppMenuHandler.AppMenuItemType.DIVIDER,
                                        AppMenuItemUtils.buildModelForDivider(
                                                R.id.divider_line_id)));

                        submenuItems.add(
                                buildBookmarkFolderParentItem(
                                        R.string.menu_mobile_bookmarks,
                                        Arrays.asList(
                                                bookmarkModel.getAccountMobileFolderId(),
                                                bookmarkModel.getMobileFolderId())));

                        submenuItems.add(
                                buildBookmarkFolderParentItem(
                                        R.string.menu_other_bookmarks,
                                        Arrays.asList(
                                                bookmarkModel.getAccountOtherFolderId(),
                                                bookmarkModel.getOtherFolderId())));
                    }

                    return submenuItems;
                };

        return new ListItem(
                AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.bookmarks_parent_menu_id,
                        R.string.menu_bookmarks,
                        mShouldShowIconBeforeItem
                                ? R.drawable.ic_star_filled_24dp
                                : Resources.ID_NULL,
                        submenuItemsSupplier,
                        mIsMenuIconAtStart));
    }

    private ListItem buildBookmarkFolderParentItem(
            @StringRes int titleRes, List<BookmarkId> folderIds) {
        Supplier<List<ListItem>> submenuItemsSupplier =
                () -> {
                    List<ListItem> items = new ArrayList<>();
                    BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
                    if (bookmarkModel != null && bookmarkModel.isBookmarkModelLoaded()) {
                        List<BookmarkId> childIds = new ArrayList<>();
                        for (BookmarkId folderId : folderIds) {
                            if (folderId != null) {
                                childIds.addAll(bookmarkModel.getChildIds(folderId));
                            }
                        }
                        items.addAll(getBookmarkItemList(childIds, bookmarkModel));
                    }
                    if (items.isEmpty()) {
                        items.add(TabbedAppMenuPropertiesDelegate.buildEmptySubmenuItem());
                    }
                    return items;
                };

        PropertyModel model =
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.bookmark_folder_menu_id,
                        titleRes,
                        Resources.ID_NULL,
                        submenuItemsSupplier,
                        mIsMenuIconAtStart);
        return AppMenuItemUtils.createMenuItemWithSubmenuListItem(model, /* showIcon= */ false);
    }

    private ListItem buildReadingListItem(@Nullable Tab currentTab) {
        List<ListItem> submenuItems = new ArrayList<>();
        submenuItems.add(
                AppMenuItemUtils.createStandardListItem(
                        AppMenuItemUtils.buildModelForStandardMenuItem(
                                mContext,
                                mAppMenuItemTheme,
                                R.id.show_reading_list_menu_id,
                                R.string.menu_show_reading_list,
                                Resources.ID_NULL,
                                mIsMenuIconAtStart),
                        /* showIcon= */ false));

        if (currentTab != null && BookmarkUtils.isReadingListSupported(currentTab.getUrl())) {
            submenuItems.add(
                    AppMenuItemUtils.createStandardListItem(
                            AppMenuItemUtils.buildModelForStandardMenuItem(
                                    mContext,
                                    mAppMenuItemTheme,
                                    R.id.add_to_reading_list_menu_id,
                                    R.string.menu_add_to_reading_list,
                                    Resources.ID_NULL,
                                    mIsMenuIconAtStart),
                            /* showIcon= */ false));
        }

        PropertyModel model =
                AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.reading_list_parent_menu_id,
                        R.string.menu_reading_list,
                        Resources.ID_NULL,
                        () -> submenuItems,
                        mIsMenuIconAtStart);
        return AppMenuItemUtils.createMenuItemWithSubmenuListItem(model, false);
    }

    private ListItem buildToggleBookmarksBarItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.toggle_bookmarks_bar_menu_id,
                        BookmarkBarUtils.isUserPrefsShowBookmarksBarEnabled(
                                        mTabModelSelector.getCurrentModel().getProfile())
                                ? R.string.menu_hide_bookmarks_bar
                                : R.string.menu_show_bookmarks_bar,
                        Resources.ID_NULL,
                        mIsMenuIconAtStart),
                /* showIcon= */ false);
    }

    private BookmarkImageFetcher getImageFetcher() {
        if (mImageFetcher == null) {
            Profile profile = getProfileFromTabModel();
            BookmarkModel bookmarkModel = mBookmarkModelSupplier.get();
            assert bookmarkModel != null;
            mImageFetcher =
                    new BookmarkImageFetcher(
                            profile,
                            mContext,
                            bookmarkModel,
                            ImageFetcherFactory.createImageFetcher(
                                    ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                                    profile.getProfileKey(),
                                    GlobalDiscardableReferencePool.getReferencePool()),
                            FaviconUtils.createCircularIconGenerator(mContext));
        }
        return mImageFetcher;
    }

    private List<ListItem> getBookmarkItemList(List<BookmarkId> ids, BookmarkModel bookmarkModel) {
        List<ListItem> submenuItems = new ArrayList<>();
        for (BookmarkId id : ids) {
            BookmarkItem item = bookmarkModel.getBookmarkById(id);
            if (item != null) {
                submenuItems.add(buildBookmarkListItem(item, bookmarkModel));
            }
        }
        return submenuItems;
    }

    private ListItem buildBookmarkListItem(BookmarkItem item, BookmarkModel bookmarkModel) {
        if (item.isFolder()) {
            return new ListItem(
                    AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                    AppMenuItemUtils.buildModelForMenuItemWithSubmenu(
                            mContext,
                            mAppMenuItemTheme,
                            R.id.bookmark_folder_menu_id,
                            item.getTitle(),
                            mShouldShowIconBeforeItem
                                    ? R.drawable.ic_folder_outline_24dp
                                    : Resources.ID_NULL,
                            () -> {
                                List<ListItem> items =
                                        getBookmarkItemList(
                                                bookmarkModel.getChildIds(item.getId()),
                                                bookmarkModel);
                                if (items.isEmpty()) {
                                    items.add(
                                            TabbedAppMenuPropertiesDelegate
                                                    .buildEmptySubmenuItem());
                                }
                                return items;
                            },
                            mIsMenuIconAtStart));
        } else {
            PropertyModel model =
                    AppMenuItemUtils.populateBaseModelForTextItem(
                                    new PropertyModel.Builder(
                                            AppMenuBookmarkItemProperties.ALL_KEYS),
                                    mAppMenuItemTheme,
                                    R.id.bookmark_menu_id,
                                    mIsMenuIconAtStart)
                            .with(AppMenuItemProperties.TITLE, item.getTitle())
                            .with(AppMenuBookmarkItemProperties.BOOKMARK_ID, item.getId())
                            .with(
                                    AppMenuItemProperties.ICON_SUPPLIER,
                                    mShouldShowIconBeforeItem
                                            ? createIconSupplierForBookmark(item)
                                            : null)
                            .with(AppMenuItemProperties.ICON_NO_TINT, !item.isFolder())
                            .build();
            return new ListItem(AppMenuHandler.AppMenuItemType.BOOKMARK, model);
        }
    }

    private LazyOneshotSupplier<Drawable> createIconSupplierForBookmark(BookmarkItem item) {
        if (item.isFolder()) {
            return LazyOneshotSupplier.fromSupplier(
                    () ->
                            AppCompatResources.getDrawable(
                                    mContext, R.drawable.ic_folder_outline_24dp));
        }
        return new LazyOneshotSupplierImpl<>() {
            @Override
            public void doSet() {
                getImageFetcher()
                        .fetchFaviconForBookmark(
                                item,
                                icon ->
                                        set(
                                                TabbedAppMenuPropertiesDelegate
                                                        .createInsetFaviconDrawable(
                                                                mContext, icon)));
            }
        };
    }

    /**
     * Builds the standalone "Bookmarks" menu item.
     *
     * @param showIcon Whether to display an icon next to the item.
     * @return A {@link ListItem} representing the Bookmarks menu item.
     */
    public ListItem buildBookmarksItem(boolean showIcon) {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.all_bookmarks_menu_id,
                        R.string.menu_bookmarks,
                        showIcon ? R.drawable.ic_star_filled_24dp : Resources.ID_NULL,
                        mIsMenuIconAtStart),
                showIcon);
    }

    private ListItem buildBookmarkThisPageItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.bookmark_this_page_menu_id,
                        R.string.menu_bookmark_this_page,
                        Resources.ID_NULL,
                        mIsMenuIconAtStart),
                /* showIcon= */ false);
    }

    /**
     * Sets the {@link BookmarkImageFetcher} for testing purposes.
     *
     * @param imageFetcher The image fetcher to use.
     */
    public void setImageFetcherForTesting(BookmarkImageFetcher imageFetcher) {
        mImageFetcher = imageFetcher;
    }
}
