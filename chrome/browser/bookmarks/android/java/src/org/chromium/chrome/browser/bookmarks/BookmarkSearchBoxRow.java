// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.browser_ui.widget.search.SearchBoxView;

/**
 * A layout that embeds the generic desktop search box and any bookmarks-specific UI elements, like
 * shopping filter chips.
 */
@NullMarked
public class BookmarkSearchBoxRow extends LinearLayout {
    private SearchBoxView mSearchBoxView;
    private ChipView mShoppingChip;
    private View mShoppingChipContainer;

    /** Constructor for inflating from XML. */
    @SuppressWarnings("NullAway.Init")
    public BookmarkSearchBoxRow(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mSearchBoxView = findViewById(R.id.search_box);
        mSearchBoxView.setHintText(getContext().getString(R.string.bookmark_toolbar_search));

        updateDesktopMode(BookmarkUtils.isDesktopBookmarksLayoutEnabled());

        mShoppingChip = findViewById(R.id.shopping_filter_chip);
        mShoppingChipContainer = findViewById(R.id.shopping_filter_chip_container);
    }

    /**
     * Updates the styling for desktop or mobile layout.
     *
     * @param isDesktop Whether the search box should use desktop styling.
     */
    public void updateDesktopMode(boolean isDesktop) {
        Resources res = getContext().getResources();
        int heightPx =
                res.getDimensionPixelSize(
                        isDesktop
                                ? R.dimen.bookmark_search_box_height_desktop
                                : R.dimen.bookmark_search_box_height_default);
        int marginBottomPx =
                res.getDimensionPixelSize(
                        isDesktop
                                ? R.dimen.bookmark_search_box_bottom_margin_desktop
                                : R.dimen.bookmark_search_box_bottom_margin_default);
        int paddingEndPx =
                res.getDimensionPixelSize(
                        isDesktop
                                ? R.dimen.bookmark_search_box_padding_horizontal_desktop
                                : R.dimen.bookmark_search_box_padding_end_default);
        int paddingStartPx = res.getDimensionPixelSize(R.dimen.bookmark_search_box_padding_start);
        int backgroundRes =
                isDesktop ? R.drawable.search_box_background : R.drawable.search_row_modern_bg;

        LinearLayout.LayoutParams params =
                (LinearLayout.LayoutParams) mSearchBoxView.getLayoutParams();
        params.height = heightPx;
        params.bottomMargin = marginBottomPx;
        mSearchBoxView.setLayoutParams(params);
        mSearchBoxView.setPaddingRelative(
                paddingStartPx,
                mSearchBoxView.getPaddingTop(),
                paddingEndPx,
                mSearchBoxView.getPaddingBottom());
        mSearchBoxView.setBackgroundResource(backgroundRes);
    }

    /** Returns the inner SearchBoxView widget. */
    public SearchBoxView getSearchBoxView() {
        return mSearchBoxView;
    }

    /** Toggles visibility for the shopping chip container. */
    public void setChipContainerVisibility(boolean isVisible) {
        mShoppingChipContainer.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /** Sets the icon on the shopping chip. */
    public void setShoppingChipIcon(int resId) {
        mShoppingChip.setIconWithTint(resId, /* tintWithTextColor= */ true);
    }

    /** Sets the text string on the shopping chip. */
    public void setShoppingChipText(int resId) {
        mShoppingChip.getPrimaryTextView().setText(resId);
    }

    /** Binds click listener toggles. */
    public void setShoppingChipToggleListener(@Nullable OnClickListener listener) {
        mShoppingChip.setOnClickListener(listener);
    }

    /** Sets whether the shopping chip is selected. */
    public void setShoppingChipSelected(boolean selected) {
        mShoppingChip.setSelected(selected);
    }
}
