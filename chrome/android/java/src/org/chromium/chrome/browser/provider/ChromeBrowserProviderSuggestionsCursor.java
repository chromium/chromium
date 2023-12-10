// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import android.app.SearchManager;
import android.content.Intent;
import android.database.AbstractCursor;
import android.database.Cursor;

import org.chromium.chrome.R;

/**
 * For bookmarks/history suggestions, wrap the cursor returned in one that can feed
 * the data back to global search in the format it wants.
 */
class ChromeBrowserProviderSuggestionsCursor extends AbstractCursor {

    private static final String[] COLS =
            new String[] {
                BaseColumns.ID,
                SearchManager.SUGGEST_COLUMN_INTENT_ACTION,
                SearchManager.SUGGEST_COLUMN_INTENT_DATA,
                SearchManager.SUGGEST_COLUMN_TEXT_1,
                SearchManager.SUGGEST_COLUMN_TEXT_2,
                SearchManager.SUGGEST_COLUMN_TEXT_2_URL,
                SearchManager.SUGGEST_COLUMN_ICON_1,
                SearchManager.SUGGEST_COLUMN_LAST_ACCESS_HINT
            };

    private static final int COLUMN_ID = 0;
    private static final int COLUMN_SUGGEST_INTENT_ACTION = 1;
    private static final int COLUMN_SUGGEST_INTENT_DATA = 2;
    private static final int COLUMN_SUGGEST_TEXT_1 = 3;
    private static final int COLUMN_SUGGEST_TEXT_2 = 4;
    private static final int COLUMN_SUGGEST_TEXT_2_URL = 5;
    private static final int COLUMN_SUGGEST_ICON_1 = 6;
    private static final int COLUMN_SUGGEST_LAST_ACCESS_HINT = 7;

    private final Cursor mCursor;

    public ChromeBrowserProviderSuggestionsCursor(Cursor c) {
        mCursor = c;
    }

    @Override
    public String[] getColumnNames() {
        return COLS;
    }

    @Override
    public int getCount() {
        return mCursor.getCount();
    }

    @Override
    public String getString(int column) {
        switch (column) {
            case COLUMN_ID:
                return mCursor.getString(mCursor.getColumnIndexOrThrow(BookmarkColumns.ID));
            case COLUMN_SUGGEST_INTENT_ACTION:
                return Intent.ACTION_VIEW;
            case COLUMN_SUGGEST_INTENT_DATA:
                return mCursor.getString(mCursor.getColumnIndexOrThrow(BookmarkColumns.URL));
            case COLUMN_SUGGEST_TEXT_1:
                return mCursor.getString(mCursor.getColumnIndexOrThrow(BookmarkColumns.TITLE));
            case COLUMN_SUGGEST_TEXT_2:
            case COLUMN_SUGGEST_TEXT_2_URL:
                return mCursor.getString(mCursor.getColumnIndexOrThrow(BookmarkColumns.URL));
            case COLUMN_SUGGEST_ICON_1:
                // This is the icon displayed to the left of the result in QSB.
                return Integer.toString(R.mipmap.app_icon);
            case COLUMN_SUGGEST_LAST_ACCESS_HINT:
                // After clearing history, the Chrome bookmarks database will have a last access
                // time of 0 for all bookmarks. In the Android provider, this will yield a negative
                // last access time. A negative last access time will cause global search to discard
                // the result, so fix it up before we return it.
                long lastAccess =
                        mCursor.getLong(mCursor.getColumnIndexOrThrow(BookmarkColumns.DATE));
                return lastAccess < 0 ? "0" : "" + lastAccess;
            default:
                throw new UnsupportedOperationException();
        }
    }

    @Override
    public boolean isNull(int c) {
        return mCursor.isNull(c);
    }

    @Override
    public long getLong(int c) {
        switch (c) {
            case 7:
                // See comments above in getString() re. negative last access times.
                long lastAccess =
                        mCursor.getLong(mCursor.getColumnIndexOrThrow(BookmarkColumns.DATE));
                return lastAccess < 0 ? 0 : lastAccess;
            default:
                throw new UnsupportedOperationException();
        }
    }

    @Override
    public short getShort(int c) {
        throw new UnsupportedOperationException();
    }

    @Override
    public double getDouble(int c) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int getInt(int c) {
        throw new UnsupportedOperationException();
    }

    @Override
    public float getFloat(int c) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean onMove(int oldPosition, int newPosition) {
        return mCursor.moveToPosition(newPosition);
    }
}
