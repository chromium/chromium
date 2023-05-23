// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton.PopupMenuShownListener;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableItemViewBase;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListUtils;

/**
 * Common logic for improved bookmark and folder rows.
 */
public class ImprovedBookmarkRow extends SelectableItemViewBase<BookmarkId> {
    private ViewGroup mContainer;
    // The start image view which is shows the favicon.
    private ImageView mStartImageView;
    private ImprovedBookmarkFolderView mFolderIconView;
    // Displays the title of the bookmark.
    private TextView mTitleView;
    // Displays the url of the bookmark.
    private TextView mDescriptionView;
    // Optional views that display below the description. Allows embedders to specify custom
    // content without the row being aware of it.
    private ViewGroup mAccessoryViewGroup;
    // The end image view which is shows the checkmark.
    private ImageView mCheckImageView;
    // 3-dot menu which displays contextual actions.
    private ListMenuButton mMoreButton;

    private boolean mDragEnabled;
    private boolean mBookmarkIdEditable;

    private Runnable mOpenBookmarkCallback;

    /**
     * Factory constructor for building the view programmatically.
     * @param context The calling context, usually the parent view.
     * @param isVisual Whether the visual row should be used.
     */
    protected static ImprovedBookmarkRow buildView(Context context, boolean isVisual) {
        ImprovedBookmarkRow row = new ImprovedBookmarkRow(context, null);
        row.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        LayoutInflater.from(context).inflate(isVisual
                        ? org.chromium.chrome.R.layout.improved_bookmark_row_layout_visual
                        : org.chromium.chrome.R.layout.improved_bookmark_row_layout,
                row);
        row.onFinishInflate();
        row.setStartImageRoundedCornerOutlineProvider(isVisual);
        return row;
    }

    /** Constructor for inflating from XML. */
    public ImprovedBookmarkRow(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    void setStartImageRoundedCornerOutlineProvider(boolean isVisual) {
        assert mStartImageView != null;

        mStartImageView.setOutlineProvider(
                new RoundedCornerOutlineProvider(getContext().getResources().getDimensionPixelSize(
                        isVisual ? R.dimen.improved_bookmark_row_outer_corner_radius
                                 : R.dimen.improved_bookmark_icon_radius)));
        mStartImageView.setClipToOutline(true);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mContainer = findViewById(R.id.container);

        mStartImageView = findViewById(R.id.start_image);
        mFolderIconView = findViewById(R.id.folder_view);

        mTitleView = findViewById(R.id.title);
        mDescriptionView = findViewById(R.id.description);
        mAccessoryViewGroup = findViewById(R.id.custom_content_container);

        mCheckImageView = findViewById(R.id.check_image);

        mMoreButton = findViewById(R.id.more);
    }

    void setTitle(String title) {
        mTitleView.setText(title);
        SelectableListUtils.setContentDescriptionContext(getContext(), mMoreButton, title,
                SelectableListUtils.ContentDescriptionSource.MENU_BUTTON);
    }

    void setDescription(String description) {
        mDescriptionView.setText(description);
    }

    void setStartImageVisible(boolean visible) {
        mStartImageView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setFolderViewVisible(boolean visible) {
        mFolderIconView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setStartIconDrawable(Drawable drawable) {
        mStartImageView.setImageDrawable(drawable);
        mFolderIconView.setStartIconDrawable(drawable);
    }

    void setStartIconTint(ColorStateList tint) {
        mFolderIconView.setStartIconTint(tint);
        mStartImageView.setImageTintList(tint);
    }

    void setStartAreaBackgroundColor(@ColorInt int color) {
        mFolderIconView.setStartAreaBackgroundColor(color);
        mStartImageView.setBackgroundColor(color);
    }

    void setStartImageDrawables(@Nullable Drawable first, @Nullable Drawable second) {
        mFolderIconView.setStartImageDrawables(first, second);
    }

    void setFolderChildCount(int count) {
        mFolderIconView.setChildCount(count);
    }

    void setAccessoryView(@Nullable View view) {
        mAccessoryViewGroup.removeAllViews();
        if (view == null) return;

        mAccessoryViewGroup.addView(view);
    }

    void setListMenuButtonDelegate(ListMenuButtonDelegate listMenuButtonDelegate) {
        mMoreButton.setDelegate(listMenuButtonDelegate);
    }

    void setPopupListener(PopupMenuShownListener listener) {
        mMoreButton.addPopupListener(listener);
    }

    void setIsSelected(boolean selected) {
        setChecked(selected);
    }

    void setSelectionEnabled(boolean selectionEnabled) {
        mMoreButton.setClickable(!selectionEnabled);
        mMoreButton.setEnabled(!selectionEnabled);
        mMoreButton.setImportantForAccessibility(!selectionEnabled
                        ? IMPORTANT_FOR_ACCESSIBILITY_YES
                        : IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    void setDragEnabled(boolean dragEnabled) {
        mDragEnabled = dragEnabled;
    }

    void setOpenBookmarkCallback(Runnable openBookmarkCallback) {
        mOpenBookmarkCallback = openBookmarkCallback;
    }

    void setBookmarkIdEditable(boolean bookmarkIdEditable) {
        mBookmarkIdEditable = bookmarkIdEditable;
        updateView(false);
    }

    // SelectableItemViewBase implementation.

    @Override
    protected void updateView(boolean animate) {
        boolean selected = isChecked();
        mContainer.setBackgroundResource(selected ? R.drawable.rounded_rectangle_surface_1
                                                  : R.drawable.rounded_rectangle_surface_0);

        boolean checkVisible = selected;
        boolean moreVisible = !selected && mBookmarkIdEditable;
        mCheckImageView.setVisibility(checkVisible ? View.VISIBLE : View.GONE);
        mMoreButton.setVisibility(moreVisible ? View.VISIBLE : View.GONE);
    }

    @Override
    protected void onClick() {
        mOpenBookmarkCallback.run();
    }
}
