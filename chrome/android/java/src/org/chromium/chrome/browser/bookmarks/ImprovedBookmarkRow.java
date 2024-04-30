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
import android.view.ViewPropertyAnimator;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListUtils;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuButton.PopupMenuShownListener;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.util.ValueUtils;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Common logic for improved bookmark and folder rows. */
public class ImprovedBookmarkRow extends ViewLookupCachingFrameLayout
        implements CancelableAnimator {
    /**
     * The base duration of the settling animation of the sheet. 218 ms is a spec for material
     * design (this is the minimum time a user is guaranteed to pay attention to something).
     */
    @VisibleForTesting static final int BASE_ANIMATION_DURATION_MS = 218;

    @IntDef({Location.TOP, Location.MIDDLE, Location.BOTTOM, Location.SOLO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Location {
        int TOP = 0;
        int MIDDLE = 1;
        int BOTTOM = 2;
        int SOLO = 3;
    }

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
    // The image showing if this bookmark is only available locally.
    private ImageView mLocalBookmarkImageView;
    // The end image view which is shows the checkmark.
    private ImageView mCheckImageView;
    // 3-dot menu which displays contextual actions.
    private ListMenuButton mMoreButton;
    private ImageView mEndImageView;
    private @Nullable ViewPropertyAnimator mFadeAnimator;

    private boolean mDragEnabled;
    private boolean mBookmarkIdEditable;
    private boolean mEndImageViewVisible;
    private boolean mMoreButtonVisible;
    private boolean mSelectionEnabled;
    private boolean mIsSelected;

    /**
     * Factory constructor for building the view programmatically.
     *
     * @param context The calling context, usually the parent view.
     * @param isVisual Whether the visual row should be used.
     */
    public static ImprovedBookmarkRow buildView(Context context, boolean isVisual) {
        ImprovedBookmarkRow row = new ImprovedBookmarkRow(context, null);
        row.setLayoutParams(
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        LayoutInflater.from(context)
                .inflate(
                        isVisual
                                ? R.layout.improved_bookmark_row_layout_visual
                                : R.layout.improved_bookmark_row_layout,
                        row);
        row.onFinishInflate();
        row.setStartImageRoundedCornerOutlineProvider(isVisual);
        return row;
    }

    /** Constructor for inflating from XML. */
    public ImprovedBookmarkRow(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void cancelAnimation() {
        if (mFadeAnimator != null) {
            mFadeAnimator.cancel();
            mFadeAnimator = null;
        }
    }

    void setStartImageRoundedCornerOutlineProvider(boolean isVisual) {
        assert mStartImageView != null;

        mStartImageView.setOutlineProvider(
                new RoundedCornerOutlineProvider(
                        getContext()
                                .getResources()
                                .getDimensionPixelSize(
                                        isVisual
                                                ? R.dimen.improved_bookmark_row_outer_corner_radius
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

        mLocalBookmarkImageView = findViewById(R.id.local_bookmark_image);
        mCheckImageView = findViewById(R.id.check_image);

        mMoreButton = findViewById(R.id.more);
        mEndImageView = findViewById(R.id.end_image);
    }

    void setRowEnabled(boolean enabled) {
        setEnabled(enabled);
        int alphaRes = enabled ? R.dimen.default_enabled_alpha : R.dimen.default_disabled_alpha;
        float alpha = ValueUtils.getFloat(getResources(), alphaRes);
        mContainer.setAlpha(alpha);
    }

    void setTitle(String title) {
        mTitleView.setText(title);
        SelectableListUtils.setContentDescriptionContext(
                getContext(),
                mMoreButton,
                title,
                SelectableListUtils.ContentDescriptionSource.MENU_BUTTON);
    }

    void setDescription(String description) {
        mDescriptionView.setText(description);
    }

    void setDescriptionVisible(boolean visible) {
        mDescriptionView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setStartImageVisible(boolean visible) {
        mStartImageView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setFolderViewVisible(boolean visible) {
        mFolderIconView.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setStartIconDrawable(@Nullable Drawable drawable) {
        cancelAnimation();

        mStartImageView.setImageDrawable(drawable);
        // No need to fade-in a null drawable.
        if (drawable == null) return;

        mStartImageView.setAlpha(0f);

        mFadeAnimator = mStartImageView.animate().setDuration(BASE_ANIMATION_DURATION_MS).alpha(1f);
        mFadeAnimator.start();
    }

    void setStartIconTint(ColorStateList tint) {
        mStartImageView.setImageTintList(tint);
    }

    void setStartAreaBackgroundColor(@ColorInt int color) {
        mStartImageView.setBackgroundColor(color);
    }

    void setAccessoryView(@Nullable View view) {
        mAccessoryViewGroup.removeAllViews();
        if (view == null) return;

        // The view might already have a parent, since the items in BookmarkManager's model list
        // can be rebound to other views. In that case, remove the view from its parent before
        // adding it as a sub-view to prevent crashing.
        if (view.getParent() != null) {
            ((ViewGroup) view.getParent()).removeView(view);
        }
        mAccessoryViewGroup.addView(view);
    }

    void setListMenuButtonDelegate(ListMenuButtonDelegate listMenuButtonDelegate) {
        mMoreButton.setDelegate(listMenuButtonDelegate);
    }

    void setPopupListener(PopupMenuShownListener listener) {
        mMoreButton.addPopupListener(listener);
    }

    void setIsSelected(boolean selected) {
        mIsSelected = selected;
        updateView();
    }

    void setSelectionEnabled(boolean selectionEnabled) {
        mSelectionEnabled = selectionEnabled;
        mMoreButton.setClickable(!selectionEnabled);
        mMoreButton.setEnabled(!selectionEnabled);
        mMoreButton.setImportantForAccessibility(
                !selectionEnabled
                        ? IMPORTANT_FOR_ACCESSIBILITY_YES
                        : IMPORTANT_FOR_ACCESSIBILITY_NO);
        updateView();
    }

    // TODO: Maybe this can be removed.
    void setDragEnabled(boolean dragEnabled) {
        mDragEnabled = dragEnabled;
    }

    // TODO: Maybe this can be removed.
    void setBookmarkIdEditable(boolean bookmarkIdEditable) {
        mBookmarkIdEditable = bookmarkIdEditable;
        updateView();
    }

    void setRowClickListener(View.OnClickListener listener) {
        setOnClickListener(listener);
    }

    void setRowLongClickListener(View.OnLongClickListener listener) {
        setOnLongClickListener(listener);
    }

    void setEndImageVisible(boolean visible) {
        mEndImageViewVisible = visible;
        updateView();
    }

    void setEndMenuVisible(boolean visible) {
        mMoreButtonVisible = visible;
        updateView();
    }

    void setEndImageRes(int res) {
        mEndImageView.setImageResource(res);
    }

    void setIsLocalBookmark(boolean isLocalBookmark) {
        mLocalBookmarkImageView.setVisibility(isLocalBookmark ? View.VISIBLE : View.GONE);
    }

    void updateView() {
        mContainer.setBackgroundResource(
                mIsSelected
                        ? R.drawable.rounded_rectangle_surface_1
                        : R.drawable.rounded_rectangle_surface_0);

        boolean checkVisible = mSelectionEnabled && mIsSelected;
        boolean moreVisible = mMoreButtonVisible && !mIsSelected && mBookmarkIdEditable;
        mCheckImageView.setVisibility(checkVisible ? View.VISIBLE : View.GONE);
        mMoreButton.setVisibility(moreVisible ? View.VISIBLE : View.GONE);
        mEndImageView.setVisibility(
                !moreVisible && mEndImageViewVisible ? View.VISIBLE : View.GONE);
    }

    ImprovedBookmarkFolderView getFolderView() {
        return mFolderIconView;
    }

    // Testing specific methods below.

    public void setStartImageViewForTesting(ImageView startImageView) {
        mStartImageView = startImageView;
    }

    public boolean isSelectedForTesting() {
        return mIsSelected;
    }

    public String getTitleForTesting() {
        return mTitleView.getText().toString();
    }
}
