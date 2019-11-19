// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v7.widget.Toolbar;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.ui.widget.TintedDrawable;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.ArrayList;
import java.util.List;

/**
 * Activity that allows a user to add or edit a bookmark folder. This activity has two modes: adding
 * mode and editing mode. Depending on different modes, it should be started via two static creator
 * functions.
 */
public class BookmarkAddEditFolderActivity extends SynchronousInitializationActivity
        implements OnClickListener {
    static final String INTENT_IS_ADD_MODE = "BookmarkAddEditFolderActivity.isAddMode";
    static final String INTENT_BOOKMARK_ID = "BookmarkAddEditFolderActivity.BookmarkId";
    static final String
            INTENT_CREATED_BOOKMARK = "BookmarkAddEditFolderActivity.createdBookmark";
    static final int PARENT_FOLDER_REQUEST_CODE = 10;

    private boolean mIsAddMode;
    private BookmarkId mParentId;
    private BookmarkModel mModel;
    private TextView mParentTextView;
    private BookmarkTextInputLayout mFolderTitle;

    // Add mode member variable
    private List<BookmarkId> mBookmarksToMove;
    private MenuItem mSaveButton;

    // Edit mode member variables
    private BookmarkId mFolderId;
    private MenuItem mDeleteButton;

    private BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            if (mIsAddMode) {
                if (mModel.doesBookmarkExist(mParentId)) updateParent(mParentId);
                else updateParent(mModel.getDefaultFolder());
            } else {
                // Partner bookmark deletion is notified via bookmarkModelChanged().
                if (mModel.doesBookmarkExist(mFolderId)) {
                    updateParent(mModel.getBookmarkById(mFolderId).getParentId());
                } else {
                    finish();
                }
            }
        }

        @Override
        public void bookmarkNodeMoved(BookmarkItem oldParent, int oldIndex, BookmarkItem newParent,
                int newIndex) {
            if (!oldParent.getId().equals(newParent.getId())
                    && mModel.getChildAt(newParent.getId(), newIndex).equals(mFolderId)) {
                updateParent(newParent.getId());
            }
        }

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            if (!node.getId().equals(mFolderId)) return;
            finish();
        }
    };

    /**
     * Starts an edit folder activity. Require the context to fire an intent.
     */
    public static void startEditFolderActivity(Context context, BookmarkId idToEdit) {
        RecordUserAction.record("MobileBookmarkManagerEditFolder");
        Intent intent = new Intent(context, BookmarkAddEditFolderActivity.class);
        intent.putExtra(INTENT_IS_ADD_MODE, false);
        intent.putExtra(INTENT_BOOKMARK_ID, idToEdit.toString());
        context.startActivity(intent);
    }

    /**
     * Starts an add folder activity. This method should only be called by
     * {@link BookmarkFolderSelectActivity}.
     */
    public static void startAddFolderActivity(BookmarkFolderSelectActivity activity,
            List<BookmarkId> bookmarksToMove) {
        assert bookmarksToMove.size() > 0;
        Intent intent = new Intent(activity, BookmarkAddEditFolderActivity.class);
        intent.putExtra(INTENT_IS_ADD_MODE, true);
        ArrayList<String> bookmarkStrings = new ArrayList<>(bookmarksToMove.size());
        for (BookmarkId id : bookmarksToMove) {
            bookmarkStrings.add(id.toString());
        }
        intent.putStringArrayListExtra(
                BookmarkFolderSelectActivity.INTENT_BOOKMARKS_TO_MOVE, bookmarkStrings);
        activity.startActivityForResult(intent,
                BookmarkFolderSelectActivity.CREATE_FOLDER_REQUEST_CODE);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mModel = new BookmarkModel();
        mModel.addObserver(mBookmarkModelObserver);
        mIsAddMode = getIntent().getBooleanExtra(INTENT_IS_ADD_MODE, false);
        if (mIsAddMode) {
            List<String> stringList = getIntent().getStringArrayListExtra(
                    BookmarkFolderSelectActivity.INTENT_BOOKMARKS_TO_MOVE);
            mBookmarksToMove = new ArrayList<>(stringList.size());
            for (String string : stringList) {
                mBookmarksToMove.add(BookmarkId.getBookmarkIdFromString(string));
            }
        } else {
            mFolderId = BookmarkId.getBookmarkIdFromString(
                    getIntent().getStringExtra(INTENT_BOOKMARK_ID));
        }
        setContentView(R.layout.bookmark_add_edit_folder_activity);

        mParentTextView = findViewById(R.id.parent_folder);
        mFolderTitle = findViewById(R.id.folder_title);

        mParentTextView.setOnClickListener(this);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        if (mIsAddMode) {
            getSupportActionBar().setTitle(R.string.add_folder);
            updateParent(mModel.getDefaultFolder());
        } else {
            // Edit mode
            getSupportActionBar().setTitle(R.string.edit_folder);
            BookmarkItem bookmarkItem = mModel.getBookmarkById(mFolderId);
            updateParent(bookmarkItem.getParentId());
            final EditText editText = mFolderTitle.getEditText();
            editText.setText(bookmarkItem.getTitle());
            editText.setSelection(editText.getText().length());
            mParentTextView.setEnabled(bookmarkItem.isMovable());
        }

        mParentTextView.setText(mModel.getBookmarkTitle(mParentId));

        View shadow = findViewById(R.id.shadow);
        View scrollView = findViewById(R.id.scroll_view);
        scrollView.getViewTreeObserver().addOnScrollChangedListener(() -> {
            shadow.setVisibility(scrollView.getScrollY() > 0 ? View.VISIBLE : View.GONE);
        });
    }

    @Override
    public void onClick(View v) {
        assert v == mParentTextView;

        if (mIsAddMode) {
            BookmarkFolderSelectActivity.startNewFolderSelectActivity(
                    this, mBookmarksToMove);
        } else {
            BookmarkFolderSelectActivity.startFolderSelectActivity(this, mFolderId);
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        if (mIsAddMode) {
            mSaveButton =
                    menu.add(R.string.save)
                            .setIcon(TintedDrawable.constructTintedDrawable(this,
                                    R.drawable.bookmark_check_gray, R.color.default_icon_color))
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        } else {
            mDeleteButton = menu.add(R.string.bookmark_action_bar_delete)
                                    .setIcon(TintedDrawable.constructTintedDrawable(
                                            this, R.drawable.ic_delete_white_24dp))
                                    .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        }

        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        } else if (item == mSaveButton) {
            assert mIsAddMode;

            if (mFolderTitle.isEmpty()) {
                mFolderTitle.validate();
                mFolderTitle.requestFocus();
                return true;
            }

            BookmarkId newFolder = mModel.addFolder(mParentId, 0, mFolderTitle.getTrimmedText());
            Intent intent = new Intent();
            intent.putExtra(INTENT_CREATED_BOOKMARK, newFolder.toString());
            setResult(RESULT_OK, intent);
            finish();

            return true;
        } else if (item == mDeleteButton) {
            assert !mIsAddMode;
            // When deleting, wait till the model has done its job and notified us via model
            // observer, and then we finish this activity.
            mModel.deleteBookmarks(mFolderId);
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onStop() {
        if (!mIsAddMode && mModel.doesBookmarkExist(mFolderId) && !mFolderTitle.isEmpty()) {
            mModel.setBookmarkTitle(mFolderId, mFolderTitle.getTrimmedText());
        }

        super.onStop();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        assert mIsAddMode;
        if (requestCode == PARENT_FOLDER_REQUEST_CODE && resultCode == RESULT_OK) {
            BookmarkId selectedBookmark = BookmarkId.getBookmarkIdFromString(data.getStringExtra(
                    BookmarkFolderSelectActivity.INTENT_SELECTED_FOLDER));
            updateParent(selectedBookmark);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        mModel.removeObserver(mBookmarkModelObserver);
        mModel.destroy();
        mModel = null;
    }

    private void updateParent(BookmarkId newParent) {
        mParentId = newParent;
        mParentTextView.setText(mModel.getBookmarkTitle(mParentId));
    }
}
