// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.widget.Toolbar;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkTextInputLayout;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.TintedDrawable;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

/**
 * The activity that enables the user to modify the title, url and parent folder of a bookmark.
 */
public class BookmarkEditActivity extends SynchronousInitializationActivity {
    /** The intent extra specifying the ID of the bookmark to be edited. */
    public static final String INTENT_BOOKMARK_ID = "BookmarkEditActivity.BookmarkId";
    /** The code when starting the folder move activity for a result. */
    static final int MOVE_REQUEST_CODE = 15;

    private static final String TAG = "BookmarkEdit";

    private BookmarkModel mModel;
    private BookmarkId mBookmarkId;
    private BookmarkTextInputLayout mTitleEditText;
    private BookmarkTextInputLayout mUrlEditText;
    private TextView mFolderTextView;
    private boolean mInFolderSelect;

    private MenuItem mDeleteButton;

    private BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkModelChanged() {
            if (mModel.doesBookmarkExist(mBookmarkId)) {
                updateViewContent(true);
            } else if (!mInFolderSelect) {
                // This happens either when the user clicks delete button or partner bookmark is
                // removed in background.
                finish();
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mModel = BookmarkModel.getForProfile(Profile.getLastUsedRegularProfile());
        mBookmarkId =
                BookmarkId.getBookmarkIdFromString(getIntent().getStringExtra(INTENT_BOOKMARK_ID));
        mModel.addObserver(mBookmarkModelObserver);
        BookmarkItem item = mModel.getBookmarkById(mBookmarkId);
        if (!mModel.doesBookmarkExist(mBookmarkId) || item == null) {
            finish();
            return;
        }

        setContentView(R.layout.bookmark_edit);
        mTitleEditText = findViewById(R.id.title_text);
        mFolderTextView = (TextView) findViewById(R.id.folder_text);
        mUrlEditText = findViewById(R.id.url_text);

        mFolderTextView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mInFolderSelect = true;
                Intent intent = BookmarkFolderSelectActivity.createIntent(
                        BookmarkEditActivity.this, /*createFolder=*/false, mBookmarkId);
                startActivityForResult(intent, MOVE_REQUEST_CODE);
            }
        });

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        updateViewContent(false);

        View shadow = findViewById(R.id.shadow);
        View scrollView = findViewById(R.id.scroll_view);
        scrollView.getViewTreeObserver().addOnScrollChangedListener(() -> {
            shadow.setVisibility(scrollView.getScrollY() > 0 ? View.VISIBLE : View.GONE);
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == MOVE_REQUEST_CODE && resultCode == RESULT_OK) {
            mInFolderSelect = false;
            mBookmarkId = BookmarkFolderSelectActivity.parseMoveIntentResult(data);
            updateViewContent(true);
        }
    }

    /**
     * @param modelChanged Whether this view update is due to a model change in background.
     */
    private void updateViewContent(boolean modelChanged) {
        BookmarkItem bookmarkItem = mModel.getBookmarkById(mBookmarkId);
        // While the user is editing the bookmark, do not override user's input.
        if (!modelChanged) {
            mTitleEditText.getEditText().setText(bookmarkItem.getTitle());
            mUrlEditText.getEditText().setText(bookmarkItem.getUrl().getSpec());
        }
        mFolderTextView.setText(mModel.getBookmarkTitle(bookmarkItem.getParentId()));
        mTitleEditText.setEnabled(bookmarkItem.isEditable());
        mUrlEditText.setEnabled(bookmarkItem.isUrlEditable());
        mFolderTextView.setEnabled(BookmarkUtils.isMovable(bookmarkItem));
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        mDeleteButton = menu.add(R.string.bookmark_toolbar_delete)
                                .setIcon(TintedDrawable.constructTintedDrawable(
                                        this, R.drawable.ic_delete_white_24dp))
                                .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM);

        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item == mDeleteButton) {
            // Log added for detecting delete button double clicking.
            Log.i(TAG, "Delete button pressed by user! isFinishing() == " + isFinishing());

            mModel.deleteBookmark(mBookmarkId);
            finish();
            return true;
        } else if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onStop() {
        if (mModel.doesBookmarkExist(mBookmarkId)) {
            final GURL originalUrl = mModel.getBookmarkById(mBookmarkId).getUrl();
            final String title = mTitleEditText.getTrimmedText();
            final String url = mUrlEditText.getTrimmedText();

            if (!mTitleEditText.isEmpty()) {
                mModel.setBookmarkTitle(mBookmarkId, title);
            }

            if (!mUrlEditText.isEmpty() && mModel.getBookmarkById(mBookmarkId).isUrlEditable()) {
                GURL fixedUrl = UrlFormatter.fixupUrl(url);
                if (fixedUrl.isValid() && !fixedUrl.equals(originalUrl)) {
                    mModel.setBookmarkUrl(mBookmarkId, fixedUrl);
                }
            }
        }

        super.onStop();
    }

    @Override
    protected void onDestroy() {
        mModel.removeObserver(mBookmarkModelObserver);
        super.onDestroy();
    }

    @VisibleForTesting
    BookmarkTextInputLayout getTitleEditText() {
        return mTitleEditText;
    }

    @VisibleForTesting
    BookmarkTextInputLayout getUrlEditText() {
        return mUrlEditText;
    }

    @VisibleForTesting
    MenuItem getDeleteButton() {
        return mDeleteButton;
    }

    @VisibleForTesting
    TextView getFolderTextView() {
        return mFolderTextView;
    }
}
