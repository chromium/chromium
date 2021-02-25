// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bottomsheet;

import android.content.Context;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.text.style.RelativeSizeSpan;
import android.text.style.SuperscriptSpan;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.Pair;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.bottomsheet.BookmarkBottomSheetItemProperties.ItemType;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.util.List;

/**
 * The coordinator used to show the bookmark bottom sheet when trying to add a bookmark. The bottom
 * sheet contains a list of folders that the bookmark can be added to.
 */
public class BookmarkBottomSheetCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final Context mContext;
    private final BookmarkModel mBookmarkModel;
    private Callback<BookmarkItem> mCallback;
    private BookmarkBottomSheetContent mBottomSheetContent;

    /**
     * Constructs the bookmark bottom sheet.
     * @param context The Android context that contains the bookmark bottom sheet.
     * @param bottomSheetController The controller to perform operations on the bottom sheet.
     * @param bookmarkModel Bookmark model that loads data from the backend, must have been
     *         initialized.
     */
    public BookmarkBottomSheetCoordinator(Context context,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull BookmarkModel bookmarkModel) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mBookmarkModel = bookmarkModel;
    }

    /**
     * Shows the bookmark bottom sheet.
     * @param callback Invoked when the user clicked on a certain bookmark item.
     */
    public void show(@NonNull Callback<BookmarkItem> callback) {
        assert mBookmarkModel.isBookmarkModelLoaded();
        mCallback = callback;

        // Load bookmark model data into recycler view.
        View contentView = LayoutInflater.from(mContext).inflate(
                org.chromium.chrome.R.layout.bookmark_bottom_sheet, /*root=*/null);
        RecyclerView sheetItemListView =
                contentView.findViewById(org.chromium.chrome.R.id.sheet_item_list);
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(loadTopLevelFolders());
        adapter.registerType(ItemType.FOLDER_ROW,
                new LayoutViewBuilder(R.layout.bookmark_bottom_sheet_folder_row),
                BookmarkBottomSheetRowViewBinder::bind);
        sheetItemListView.setAdapter(adapter);
        sheetItemListView.setLayoutManager(new LinearLayoutManager(mContext));

        // Show the bottom sheet.
        mBottomSheetContent = new BookmarkBottomSheetContent(
                contentView, sheetItemListView::computeHorizontalScrollOffset);
        mBottomSheetController.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                onBottomSheetClosed();
                mBottomSheetController.removeObserver(this);
            }
        });
        mBottomSheetController.requestShowContent(mBottomSheetContent, /*animate=*/true);
    }

    // Loads top level bookmark folders into a ModelList.
    private ModelList loadTopLevelFolders() {
        List<BookmarkId> topLevelFolderIDs = BookmarkUtils.populateTopLevelFolders(mBookmarkModel);
        ModelList modelList = new ModelList();
        for (BookmarkId folderId : topLevelFolderIDs) {
            BookmarkItem folderItem = mBookmarkModel.getBookmarkById(folderId);
            modelList.add(new ListItem(ItemType.FOLDER_ROW, buildItemModel(folderItem)));
        }
        return modelList;
    }

    // Build the model for a single item in the bookmark bottom sheet.
    private PropertyModel buildItemModel(BookmarkItem bookmarkItem) {
        @BookmarkType
        int type = bookmarkItem.getId().getType();
        PropertyModel model =
                new PropertyModel.Builder(BookmarkBottomSheetItemProperties.ALL_KEYS)
                        .with(BookmarkBottomSheetItemProperties.TITLE, getTitle(bookmarkItem))
                        .with(BookmarkBottomSheetItemProperties.SUBTITLE, getSubtitle(bookmarkItem))
                        .with(BookmarkBottomSheetItemProperties.ICON_DRAWABLE_AND_COLOR,
                                new Pair<>(BookmarkUtils.getFolderIcon(mContext, type),
                                        BookmarkUtils.getFolderIconTint(type)))
                        .with(BookmarkBottomSheetItemProperties.ON_CLICK_LISTENER,
                                () -> onClick(bookmarkItem))
                        .build();
        return model;
    }

    private CharSequence getTitle(@NonNull final BookmarkItem bookmarkItem) {
        if (bookmarkItem.getId().getType() == BookmarkType.READING_LIST) {
            Tracker tracker =
                    TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
            boolean showNew = tracker.isInitialized()
                    && tracker.shouldTriggerHelpUI(
                            FeatureConstants.READ_LATER_BOTTOM_SHEET_FEATURE);
            SpannableString spannableString = new SpannableString(
                    mContext.getResources().getString(R.string.reading_list_title_new));

            // Maybe show a "New" text after the reading list title.
            if (showNew) {
                tracker.dismissed(FeatureConstants.READ_LATER_BOTTOM_SHEET_FEATURE);
                spannableString = SpanApplier.applySpans(spannableString.toString(),
                        new SpanInfo("<new>", "</new>", new RelativeSizeSpan(0.75f),
                                new SuperscriptSpan(),
                                new ForegroundColorSpan(
                                        ApiCompatibilityUtils.getColor(mContext.getResources(),
                                                R.color.default_text_color_blue))));
            } else {
                spannableString = new SpannableString(SpanApplier.removeSpanText(
                        spannableString.toString(), new SpanInfo("<new>", "</new>")));
            }
            return spannableString;
        }

        return bookmarkItem.getTitle();
    }

    private @Nullable CharSequence getSubtitle(@NonNull final BookmarkItem bookmarkItem) {
        switch (bookmarkItem.getId().getType()) {
            case BookmarkType.NORMAL:
                int totalCount = mBookmarkModel.getTotalBookmarkCount(bookmarkItem.getId());
                return totalCount > 0 ? mContext.getResources().getQuantityString(
                               R.plurals.bookmarks_count, totalCount, totalCount)
                                      : mContext.getResources().getString(R.string.no_bookmarks);
            case BookmarkType.READING_LIST:
                int unreadCount = mBookmarkModel.getUnreadCount(bookmarkItem.getId());
                return unreadCount > 0
                        ? mContext.getResources().getQuantityString(
                                R.plurals.reading_list_unread_page_count, unreadCount, unreadCount)
                        : mContext.getResources().getString(R.string.reading_list_intro_text);
            default:
                return null;
        }
    }

    private void onClick(BookmarkItem bookmarkItem) {
        invokeCallback(bookmarkItem);

        // This will result in onBottomSheetClosed() being called.
        mBottomSheetController.hideContent(mBottomSheetContent, /*animate=*/true);
    }

    private void onBottomSheetClosed() {
        invokeCallback(null);
    }

    private void invokeCallback(BookmarkItem bookmarkItem) {
        if (mCallback == null) return;
        mCallback.onResult(bookmarkItem);
        mCallback = null;
    }

    @VisibleForTesting
    BookmarkBottomSheetContent getBottomSheetContentForTesting() {
        return mBottomSheetContent;
    }
}
