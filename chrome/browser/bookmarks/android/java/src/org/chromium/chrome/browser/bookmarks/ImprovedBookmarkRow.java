// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Outline;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.view.ViewPropertyAnimator;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.RoundedCornerImageView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListUtils;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuDelegate;
import org.chromium.ui.listmenu.ListMenuHost.PopupMenuShownListener;
import org.chromium.ui.util.ValueUtils;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Common logic for improved bookmark and folder rows. */
@NullMarked
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

    private static boolean sEnableIconAnimationForTesting = true;

    private ViewGroup mContainer;
    // The start image view which is shows the favicon.
    private RoundedCornerImageView mStartImageView;
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

    private @Nullable ImageView mDragHandle;
    private boolean mIsDragEnabled;
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
        row.setStartImageRoundedCorners(isVisual);
        row.setStartImageSize();
        return row;
    }

    /** Constructor for inflating from XML. */
    public ImprovedBookmarkRow(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        // The view from buildView should have a focus highlight, so avoid duplicate focus
        setDefaultFocusHighlightEnabled(false);
        setFocusable(true);
    }

    public void setDragEnabled(boolean dragEnabled) {
        mIsDragEnabled = dragEnabled;
        updateView();
    }

    @SuppressLint("ClickableViewAccessibility")
    public void setDragHandleTouchListener(View.OnTouchListener listener) {
        if (mDragHandle != null) {
            mDragHandle.setOnTouchListener(listener);
        }
    }

    public void setRowBodyTouchListener(View.OnTouchListener listener) {
        setOnTouchListener(listener);
    }

    public void setDragHandleHoverListener(View.OnHoverListener listener) {
        if (mDragHandle != null) {
            mDragHandle.setOnHoverListener(listener);
        }
    }

    public void setRowBodyHoverListener(View.OnHoverListener listener) {
        setOnHoverListener(listener);
    }

    @Override
    public void cancelAnimation() {
        if (mFadeAnimator != null) {
            mFadeAnimator.cancel();
            mFadeAnimator = null;
        }
    }

    void setStartImageRoundedCorners(boolean isVisual) {
        assert mStartImageView != null;

        Resources res = getContext().getResources();
        int dimenRes =
                (BookmarkUtils.isDesktopBookmarksLayoutEnabled()
                                || BookmarkUtils.isDesktopBookmarksDialogEnabled())
                        ? R.dimen.improved_bookmark_start_image_corner_radius_desktop
                        : (isVisual
                                ? R.dimen.improved_bookmark_row_outer_corner_radius
                                : R.dimen.improved_bookmark_icon_radius);
        int radius = res.getDimensionPixelSize(dimenRes);
        mStartImageView.setRoundedCorners(radius, radius, radius, radius);
    }

    void setStartImageSize() {
        if ((BookmarkUtils.isDesktopBookmarksLayoutEnabled()
                        || BookmarkUtils.isDesktopBookmarksDialogEnabled())
                && mStartImageView != null) {
            Resources res = getContext().getResources();
            int size =
                    res.getDimensionPixelSize(R.dimen.improved_bookmark_start_image_size_desktop);
            ViewGroup.LayoutParams params = mStartImageView.getLayoutParams();
            if (params != null) {
                params.width = size;
                params.height = size;
                mStartImageView.setLayoutParams(params);
            }
        }
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

        mDragHandle = findViewById(R.id.drag_handle);
        mDragHandle.setClickable(true);
        mDragHandle.setFocusable(true);

        // Define the shadow shape explicitly. This ensures that the shadow appears even if
        // mDraggedBackgroundColor is transparent.
        setOutlineProvider(
                new ViewOutlineProvider() {
                    @Override
                    public void getOutline(View view, Outline outline) {
                        if (mContainer != null && mContainer.getWidth() > 0) {
                            Resources res = getContext().getResources();

                            int radiusRes =
                                    mIsSelected
                                            ? R.dimen.default_rounded_corner_radius
                                            : R.dimen.improved_bookmark_row_outer_corner_radius;

                            float radius = res.getDimension(radiusRes);

                            // Calculate the bounds of the container relative to the parent
                            // (ImprovedBookmarkRow) and draw the shadow.
                            outline.setRoundRect(
                                    mContainer.getLeft(),
                                    mContainer.getTop(),
                                    mContainer.getRight(),
                                    mContainer.getBottom(),
                                    radius);
                            // Force shadow opacity even if view is transparent.
                            outline.setAlpha(1.0f);
                        } else {
                            // Don't show the shadow.
                            outline.setRect(0, 0, view.getWidth(), view.getHeight());
                            outline.setAlpha(0.0f);
                        }
                    }
                });
        // Allow the shadow to draw outside the view bounds if needed.
        setClipToOutline(false);
    }

    void setRowEnabled(boolean enabled) {
        setEnabled(enabled);
        setFocusable(enabled);
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
        // No need to fade-in a null drawable or when animations are disabled in tests.
        if (drawable == null || !sEnableIconAnimationForTesting) return;

        mStartImageView.setAlpha(0f);

        mFadeAnimator = mStartImageView.animate().setDuration(BASE_ANIMATION_DURATION_MS).alpha(1f);
        mFadeAnimator.start();
    }

    void setStartIconTint(ColorStateList tint) {
        mStartImageView.setImageTintList(tint);
    }

    void setStartAreaBackgroundColor(@ColorInt int color) {
        mStartImageView.setRoundedFillColor(color);
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

    void setListMenuDelegate(ListMenuDelegate listMenuDelegate) {
        mMoreButton.setDelegate(listMenuDelegate);
    }

    void setPopupListener(PopupMenuShownListener listener) {
        mMoreButton.addPopupListener(listener);
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        info.setCheckable(mSelectionEnabled);
        info.setChecked(mSelectionEnabled && mIsSelected);
    }

    void setIsSelected(boolean selected) {
        boolean changed = mIsSelected != selected;
        mIsSelected = selected;
        updateView();
        if (changed && mSelectionEnabled) {
            sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        }
    }

    void setSelectionEnabled(boolean selectionEnabled) {
        boolean changed = mSelectionEnabled != selectionEnabled;
        mSelectionEnabled = selectionEnabled;
        mMoreButton.setClickable(!selectionEnabled);
        mMoreButton.setEnabled(!selectionEnabled);
        mMoreButton.setImportantForAccessibility(
                !selectionEnabled
                        ? IMPORTANT_FOR_ACCESSIBILITY_YES
                        : IMPORTANT_FOR_ACCESSIBILITY_NO);
        updateView();
        if (changed) {
            sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        }
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
        setDefaultFocusHighlightEnabled(mIsSelected);
        mContainer.setBackgroundResource(
                mIsSelected
                        ? R.drawable.rounded_rectangle_surface_container_low
                        : R.drawable.improved_bookmark_row_visual_background);

        boolean checkVisible = mSelectionEnabled && mIsSelected;
        boolean moreVisible = mMoreButtonVisible && !mIsSelected && mBookmarkIdEditable;

        // Show handle if row is selected.
        if (mDragHandle != null) {
            mDragHandle.setVisibility((mIsDragEnabled && mIsSelected) ? View.VISIBLE : View.GONE);
        }
        // ViewOutlineProvider re-runs getOutline().
        invalidateOutline();

        mCheckImageView.setVisibility(checkVisible ? View.VISIBLE : View.GONE);
        mMoreButton.setVisibility(moreVisible ? View.VISIBLE : View.GONE);
        mEndImageView.setVisibility(
                !moreVisible && mEndImageViewVisible ? View.VISIBLE : View.GONE);
    }

    ImprovedBookmarkFolderView getFolderView() {
        return mFolderIconView;
    }

    // Testing specific methods below.

    public void setStartImageViewForTesting(RoundedCornerImageView startImageView) {
        mStartImageView = startImageView;
    }

    public boolean isSelectedForTesting() {
        return mIsSelected;
    }

    public String getTitleForTesting() {
        return mTitleView.getText().toString();
    }

    public static void setEnableIconAnimationForTesting(boolean enable) {
        sEnableIconAnimationForTesting = enable;
        ResettersForTesting.register(() -> sEnableIconAnimationForTesting = true);
    }
}
