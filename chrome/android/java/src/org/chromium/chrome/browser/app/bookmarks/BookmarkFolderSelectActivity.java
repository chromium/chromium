// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.appcompat.widget.Toolbar;
import androidx.core.view.ViewCompat;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkFolderRow;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;

import java.util.ArrayList;
import java.util.List;

/**
 * Dialog for moving bookmarks from one folder to another. A list of folders are shown and the
 * hierarchy of bookmark model is presented by indentation of list items. This dialog can be shown
 * in two cases. One is when user choose to move an existing bookmark to a new folder. The other is
 * when user creates a new folder/bookmark, they can choose which parent the new folder/bookmark
 * belong to.
 * <p>
 * Note this fragment will not be restarted by OS. It will be dismissed if chrome is killed in
 * background.
 */
public class BookmarkFolderSelectActivity
        extends SynchronousInitializationActivity implements AdapterView.OnItemClickListener {
    static final String INTENT_SELECTED_FOLDER = "BookmarkFolderSelectActivity.selectedFolder";
    static final String INTENT_IS_CREATING_FOLDER = "BookmarkFolderSelectActivity.isCreatingFolder";
    static final String INTENT_BOOKMARKS_TO_MOVE = "BookmarkFolderSelectActivity.bookmarksToMove";
    static final String INTENT_BOOKMARK_MOVE_RESULT =
            "BookmarkFolderSelectActivity.bookmarkMoveResult";
    static final int CREATE_FOLDER_REQUEST_CODE = 13;

    private BookmarkModel mModel;
    private boolean mIsCreatingFolder;
    private List<BookmarkId> mBookmarksToMove;
    private BookmarkId mParentId;
    private FolderListAdapter mBookmarkIdsAdapter;
    private ListView mBookmarkIdsList;

    private BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            updateFolderList();
        }

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            if (mBookmarksToMove.contains(node.getId())) {
                mBookmarksToMove.remove(node.getId());
                if (mBookmarksToMove.isEmpty()) {
                    finishActivity(mBookmarksToMove);
                    return;
                }
            } else if (node.isFolder()) {
                updateFolderList();
            }
        }
    };

    /**
     * Starts a select folder activity.
     */
    public static void startFolderSelectActivity(Context context, BookmarkId... bookmarks) {
        assert bookmarks.length > 0;
        Intent intent = BookmarkFolderSelectActivity.createIntent(
                context, /*createFolder=*/false, bookmarks);
        context.startActivity(intent);
    }

    /**
     * @return An intent created from the given parameters.
     */
    public static Intent createIntent(
            Context context, boolean createFolder, BookmarkId... bookmarks) {
        Intent intent = new Intent(context, BookmarkFolderSelectActivity.class);
        intent.putExtra(INTENT_IS_CREATING_FOLDER, createFolder);
        intent.putStringArrayListExtra(
                INTENT_BOOKMARKS_TO_MOVE, BookmarkUtils.bookmarkIdsToStringList(bookmarks));
        return intent;
    }

    /**
     * @return The {@link BookmarkId} encoded in the given intent.
     */
    public static BookmarkId parseMoveIntentResult(Intent data) {
        String bookmarkString = IntentUtils.safeGetStringExtra(data, INTENT_BOOKMARK_MOVE_RESULT);
        return BookmarkId.getBookmarkIdFromString(bookmarkString);
    }

    /**
     * Starts a select folder activity for the new folder that is about to be created. This method
     * is only supposed to be called by {@link BookmarkAddEditFolderActivity}
     */
    public static void startNewFolderSelectActivity(
            BookmarkAddEditFolderActivity activity, List<BookmarkId> bookmarks) {
        BookmarkId[] bookmarksArray = new BookmarkId[bookmarks.size()];
        bookmarks.toArray(bookmarksArray);
        Intent intent = BookmarkFolderSelectActivity.createIntent(
                activity, /*createFolder=*/true, bookmarksArray);
        activity.startActivityForResult(
                intent, BookmarkAddEditFolderActivity.PARENT_FOLDER_REQUEST_CODE);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mModel = BookmarkModel.getForProfile(Profile.getLastUsedRegularProfile());
        List<String> stringList =
                IntentUtils.safeGetStringArrayListExtra(getIntent(), INTENT_BOOKMARKS_TO_MOVE);

        // If the intent does not contain a list of bookmarks to move, return early. See
        // crbug.com/728244. If the bookmark model is not loaded, return early to avoid crashing
        // when trying to access bookmark model methods. The bookmark model should always be
        // loaded when BookmarkFolderSelectActivity is created unless the entire Chrome process
        // is being recreated. If we add a loading screen, we could wait for the model to be
        // loaded but this flow is rare. See crbug.com/704872.
        if (stringList == null || !mModel.isBookmarkModelLoaded()) {
            finish();
            return;
        }

        mBookmarksToMove = BookmarkUtils.stringListToBookmarkIds(mModel, stringList);
        if (mBookmarksToMove.isEmpty()) {
            finish();
            return;
        }
        mModel.addObserver(mBookmarkModelObserver);

        mIsCreatingFolder = getIntent().getBooleanExtra(INTENT_IS_CREATING_FOLDER, false);
        if (mIsCreatingFolder) {
            mParentId = mModel.getMobileFolderId();
        } else {
            mParentId = mModel.getBookmarkById(mBookmarksToMove.get(0)).getParentId();
        }

        setContentView(R.layout.bookmark_folder_select_activity);
        mBookmarkIdsList = (ListView) findViewById(R.id.bookmark_folder_list);
        mBookmarkIdsList.setOnItemClickListener(this);
        mBookmarkIdsAdapter = new FolderListAdapter(this, mModel);
        mBookmarkIdsList.setAdapter(mBookmarkIdsAdapter);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        updateFolderList();

        View shadow = findViewById(R.id.shadow);
        int listPaddingTop = getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow);
        mBookmarkIdsList.getViewTreeObserver().addOnScrollChangedListener(() -> {
            if (mBookmarkIdsList.getChildCount() < 1) return;

            shadow.setVisibility(mBookmarkIdsList.getChildAt(0).getTop() < listPaddingTop
                            ? View.VISIBLE
                            : View.GONE);
        });
    }

    private void updateFolderList() {
        List<BookmarkId> folderList = new ArrayList<>();
        // Reading List doesn't support folders as children.
        if (!mIsCreatingFolder) {
            folderList.add(mModel.getReadingListFolder());
        }
        List<Integer> depthList = new ArrayList<>();
        depthList.add(0);
        mModel.getMoveDestinations(folderList, depthList, mBookmarksToMove);
        List<FolderListEntry> entryList = new ArrayList<>(folderList.size() + 3);

        if (!mIsCreatingFolder) {
            entryList.add(new FolderListEntry(null, 0, getString(R.string.bookmark_add_folder),
                    false, FolderListEntry.TYPE_NEW_FOLDER));
        }

        FolderListEntry scrollToEntry = null;
        for (int i = 0; i < folderList.size(); i++) {
            BookmarkId folder = folderList.get(i);

            if (!mModel.isFolderVisible(folder)) continue;

            String title = mModel.getBookmarkById(folder).getTitle();
            FolderListEntry entry = new FolderListEntry(folder, depthList.get(i), title,
                    folder.equals(mParentId), FolderListEntry.TYPE_NORMAL);
            entryList.add(entry);
            if (!mIsCreatingFolder && mParentId.equals(folder)) {
                scrollToEntry = entry;
            }
        }

        mBookmarkIdsAdapter.setEntryList(entryList);
        if (scrollToEntry != null) {
            mBookmarkIdsList.smoothScrollToPosition(
                    mBookmarkIdsAdapter.getPositionForEntry(scrollToEntry));
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            onBackPressed();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mModel.removeObserver(mBookmarkModelObserver);
    }

    /**
     * Moves bookmark from original parent to selected folder. In creation mode, directly add the
     * new bookmark to selected folder instead of moving.
     */
    @Override
    public void onItemClick(AdapterView<?> adapter, View view, int position, long id) {
        FolderListEntry entry = (FolderListEntry) adapter.getItemAtPosition(position);
        if (mIsCreatingFolder) {
            BookmarkId selectedFolder = null;
            if (entry.mType == FolderListEntry.TYPE_NORMAL) {
                selectedFolder = entry.mId;
            } else {
                assert false : "New folder items should not be clickable in creating mode";
            }
            Intent intent = new Intent();
            intent.putExtra(INTENT_SELECTED_FOLDER, selectedFolder.toString());
            setResult(RESULT_OK, intent);
            finish();
        } else if (entry.mType == FolderListEntry.TYPE_NEW_FOLDER) {
            BookmarkAddEditFolderActivity.startAddFolderActivity(this, mBookmarksToMove);
        } else if (entry.mType == FolderListEntry.TYPE_NORMAL) {
            moveBookmarksAndFinish(mBookmarksToMove, entry.mId);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        assert !mIsCreatingFolder;
        if (requestCode == CREATE_FOLDER_REQUEST_CODE && resultCode == RESULT_OK) {
            BookmarkId createdBookmark = BookmarkId.getBookmarkIdFromString(
                    data.getStringExtra(BookmarkAddEditFolderActivity.INTENT_CREATED_BOOKMARK));
            moveBookmarksAndFinish(mBookmarksToMove, createdBookmark);
        }
    }

    private void moveBookmarksAndFinish(List<BookmarkId> bookmarkIds, BookmarkId parentId) {
        BookmarkUtils.moveBookmarksToParent(mModel, bookmarkIds, parentId);
        BookmarkUtils.setLastUsedParent(this, parentId);
        finishActivity(bookmarkIds);
    }

    private void finishActivity(List<BookmarkId> bookmarks) {
        // This means BookmarkFolderSelectActivity was called for a result.
        if (getCallingActivity() != null) {
            if (bookmarks.size() == 1) {
                Intent result = new Intent();
                result.putExtra(INTENT_BOOKMARK_MOVE_RESULT, bookmarks.get(0).toString());
                setResult(Activity.RESULT_OK, result);
            } else {
                setResult(Activity.RESULT_CANCELED);
            }
        }
        finish();
    }

    /**
     * Data object representing a folder entry used in FolderListAdapter.
     */
    private static class FolderListEntry {
        public static final int TYPE_NEW_FOLDER = 0;
        public static final int TYPE_NORMAL = 1;

        BookmarkId mId;
        String mTitle;
        int mDepth;
        boolean mIsSelected;
        int mType;

        public FolderListEntry(
                BookmarkId bookmarkId, int depth, String title, boolean isSelected, int type) {
            assert type == TYPE_NEW_FOLDER || type == TYPE_NORMAL;
            mDepth = depth;
            mId = bookmarkId;
            mTitle = title;
            mIsSelected = isSelected;
            mType = type;
        }
    }

    private static class FolderListAdapter extends BaseAdapter {
        // The maximum depth that will be indented. Folders with a depth greater
        // than this will all appear at this same depth.
        private static final int MAX_FOLDER_DEPTH = 5;

        private final int mBasePadding;
        private final int mPaddingIncrement;
        private final BookmarkModel mModel;

        List<FolderListEntry> mEntryList = new ArrayList<>();

        public FolderListAdapter(Context context, BookmarkModel bookmarkModel) {
            mBasePadding =
                    context.getResources().getDimensionPixelSize(R.dimen.bookmark_folder_item_left);
            mPaddingIncrement = mBasePadding * 2;
            mModel = bookmarkModel;
        }

        public int getPositionForEntry(FolderListEntry entry) {
            return mEntryList.indexOf(entry);
        }

        @Override
        public int getCount() {
            return mEntryList.size();
        }

        @Override
        public FolderListEntry getItem(int position) {
            return mEntryList.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        /**
         * There are 2 types of entries: new folder and normal.
         */
        @Override
        public int getViewTypeCount() {
            return 2;
        }

        @Override
        public int getItemViewType(int position) {
            FolderListEntry entry = getItem(position);
            return entry.mType;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final FolderListEntry entry = getItem(position);
            if (convertView != null && entry.mType != FolderListEntry.TYPE_NORMAL) {
                return convertView;
            }
            if (convertView == null) {
                convertView = LayoutInflater.from(parent.getContext())
                                      .inflate(R.layout.modern_list_item_view, parent, false);
            }
            TextView textView = (TextView) convertView.findViewById(R.id.title);
            textView.setText(entry.mTitle);
            convertView.findViewById(R.id.description).setVisibility(View.GONE);

            setUpIcons(entry, convertView);
            setUpPadding(entry, convertView);

            return convertView;
        }

        void setEntryList(List<FolderListEntry> entryList) {
            mEntryList = entryList;
            notifyDataSetChanged();
        }

        /**
         * Sets compound drawables (icons) for different kinds of list entries,
         * i.e. New Folder, Normal and Selected.
         */
        private void setUpIcons(FolderListEntry entry, View view) {
            ImageView startIcon = view.findViewById(R.id.start_icon);

            Drawable iconDrawable;
            if (entry.mType == FolderListEntry.TYPE_NORMAL) {
                iconDrawable = BookmarkUtils.getFolderIcon(
                        view.getContext(), entry.mId, mModel, BookmarkRowDisplayPref.COMPACT);
            } else {
                // For new folder, start_icon is different.
                VectorDrawableCompat vectorDrawable = TraceEventVectorDrawableCompat.create(
                        view.getResources(), R.drawable.ic_add, view.getContext().getTheme());
                vectorDrawable.setTintList(AppCompatResources.getColorStateList(
                        view.getContext(), R.color.default_icon_color_tint_list));
                iconDrawable = vectorDrawable;
            }

            BookmarkFolderRow.applyModernIconStyle(startIcon, iconDrawable, entry.mIsSelected);
        }

        /**
         * Sets up padding for the entry
         */
        private void setUpPadding(FolderListEntry entry, View view) {
            int paddingStart =
                    mBasePadding + Math.min(entry.mDepth, MAX_FOLDER_DEPTH) * mPaddingIncrement;
            ViewCompat.setPaddingRelative(view, paddingStart, view.getPaddingTop(), mBasePadding,
                    view.getPaddingBottom());
        }
    }

    int getFolderPositionForTesting(BookmarkId bookmarkId) {
        for (int i = 0; i < mBookmarkIdsAdapter.mEntryList.size(); i++) {
            FolderListEntry entry = mBookmarkIdsAdapter.mEntryList.get(i);
            if (bookmarkId.equals(entry.mId)) return i;
        }
        return -1;
    }

    void performClickForTesting(int adapterPosition) {
        onItemClick(mBookmarkIdsList, null, adapterPosition, adapterPosition);
    }
}
