// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.url.GURL;

/**
 * A row view that shows bookmark info in the bookmarks UI.
 */
// TODO (crbug.com/1424431): Make this class more extensible.
public class BookmarkItemRow extends BookmarkRow implements LargeIconCallback {
    private GURL mUrl;
    private RoundedIconGenerator mIconGenerator;
    private int mMinIconSize;
    private int mDisplayedIconSize;
    private boolean mFaviconCancelled;

    /**
     * Factory constructor for building the view programmatically.
     * @param context The calling context, usually the parent view.
     * @param isVisualRefreshEnabled Whether to show the visual or compact bookmark row.
     */
    public static BookmarkItemRow buildView(Context context, boolean isVisualRefreshEnabled) {
        BookmarkItemRow row = new BookmarkItemRow(context, null);
        BookmarkRow.buildView(row, context, isVisualRefreshEnabled);

        return row;
    }

    /** Constructor for inflating from XML. */
    public BookmarkItemRow(Context context, AttributeSet attrs) {
        super(context, attrs);

        boolean isVisualRefreshEnabled = BookmarkFeatures.isLegacyBookmarksVisualRefreshEnabled();

        mMinIconSize = getResources().getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mDisplayedIconSize = getResources().getDimensionPixelSize(isVisualRefreshEnabled
                        ? R.dimen.bookmark_refresh_preferred_start_icon_size
                        : R.dimen.default_favicon_size);

        mIconGenerator = isVisualRefreshEnabled
                ? new RoundedIconGenerator(mDisplayedIconSize, mDisplayedIconSize,
                        mDisplayedIconSize / 2,
                        getContext().getColor(R.color.default_favicon_background_color),
                        getResources().getDimensionPixelSize(
                                R.dimen.bookmark_refresh_circular_monogram_text_size))
                : FaviconUtils.createCircularIconGenerator(getContext());
    }

    // BookmarkRow implementation.

    @Override
    public void onClick() {
        switch (mDelegate.getCurrentUiMode()) {
            case BookmarkUiMode.FOLDER:
            case BookmarkUiMode.SEARCHING:
                break;
            case BookmarkUiMode.LOADING:
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
    BookmarkItem setBookmarkId(
            BookmarkId bookmarkId, @Location int location, boolean fromFilterView) {
        BookmarkItem item = super.setBookmarkId(bookmarkId, location, fromFilterView);
        mUrl = item.getUrl();
        mStartIconView.setImageDrawable(null);
        mTitleView.setText(item.getTitle());
        mDescriptionView.setText(item.getUrlForDisplay());
        mFaviconCancelled = false;
        mDelegate.getLargeIconBridge().getLargeIconForUrl(mUrl, mMinIconSize, this);
        return item;
    }

    /** Allows cancellation of the favicon. */
    protected void cancelFavicon() {
        mFaviconCancelled = true;
    }

    // LargeIconCallback implementation.

    @Override
    public void onLargeIconAvailable(Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
            @IconType int iconType) {
        if (mFaviconCancelled) return;
        Drawable iconDrawable = FaviconUtils.getIconDrawableWithoutFilter(
                icon, mUrl, fallbackColor, mIconGenerator, getResources(), mDisplayedIconSize);
        setIconDrawable(iconDrawable);
    }

    protected boolean getFaviconCancelledForTesting() {
        return mFaviconCancelled;
    }

    void setRoundedIconGeneratorForTesting(RoundedIconGenerator roundedIconGenerator) {
        mIconGenerator = roundedIconGenerator;
    }
}
