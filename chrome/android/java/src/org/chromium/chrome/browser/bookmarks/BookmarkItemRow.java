// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.url.GURL;

/**
 * A row view that shows bookmark info in the bookmarks UI.
 */
public class BookmarkItemRow extends BookmarkRow implements LargeIconCallback {
    private GURL mUrl;
    private RoundedIconGenerator mIconGenerator;
    private final int mMinIconSize;
    private final int mDisplayedIconSize;

    /**
     * Constructor for inflating from XML.
     */
    public BookmarkItemRow(Context context, AttributeSet attrs) {
        super(context, attrs);
        mMinIconSize = (int) getResources().getDimension(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        mIconGenerator = FaviconUtils.createCircularIconGenerator(context.getResources());
    }

    // BookmarkRow implementation.

    @Override
    public void onClick() {
        switch (mDelegate.getCurrentState()) {
            case BookmarkUIState.STATE_FOLDER:
            case BookmarkUIState.STATE_SEARCHING:
                break;
            case BookmarkUIState.STATE_LOADING:
                assert false :
                        "The main content shouldn't be inflated if it's still loading";
                break;
            default:
                assert false : "State not valid";
                break;
        }
        mDelegate.openBookmark(mBookmarkId);
    }

    @Override
    BookmarkItem setBookmarkId(BookmarkId bookmarkId, @Location int location) {
        BookmarkItem item = super.setBookmarkId(bookmarkId, location);
        mUrl = item.getUrl();
        mStartIconView.setImageDrawable(null);
        mTitleView.setText(item.getTitle());
        mDescriptionView.setText(item.getUrlForDisplay());
        mDelegate.getLargeIconBridge().getLargeIconForUrl(mUrl, mMinIconSize, this);
        return item;
    }

    // LargeIconCallback implementation.

    @Override
    public void onLargeIconAvailable(Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
            @IconType int iconType) {
        Drawable iconDrawable = FaviconUtils.getIconDrawableWithoutFilter(icon, mUrl.getSpec(),
                fallbackColor, mIconGenerator, getResources(), mDisplayedIconSize);
        setStartIconDrawable(iconDrawable);
    }
}
