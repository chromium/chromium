// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.ui.interpolators.Interpolators.STANDARD_INTERPOLATOR;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab_ui.TabCardThemeUtil;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.ui.animation.AnimationHandler;

/** View for a pinned tab strip item. */
@NullMarked
public class PinnedTabStripItemView extends FrameLayout {
    private static final int WIDTH_ANIMATION_DURATION_MS = 400;

    private @Nullable ImageView mFavicon;
    private @Nullable TextView mTitle;
    private @Nullable ImageView mTrailingIcon;
    private final AnimationHandler mWidthAnimationHandler;

    public PinnedTabStripItemView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mWidthAnimationHandler = new AnimationHandler();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mFavicon = findViewById(R.id.tab_favicon);
        mTitle = findViewById(R.id.tab_title);
        mTrailingIcon = findViewById(R.id.trailing_icon);
        setTrailingIcon(ContextCompat.getDrawable(getContext(), R.drawable.ic_keep_24dp));
    }

    /**
     * Sets the favicon for the tab.
     *
     * @param fetcher The {@link TabFaviconFetcher} to fetch the favicon.
     * @param isSelected Whether the tab is selected.
     */
    void setFaviconIcon(@Nullable TabFaviconFetcher fetcher, boolean isSelected) {
        if (mFavicon == null) return;

        if (fetcher == null) {
            mFavicon.setVisibility(View.GONE);
            setFavicon(null, isSelected);
            return;
        }

        mFavicon.setVisibility(View.VISIBLE);
        fetcher.fetch(
                tabFavicon -> {
                    setFavicon(tabFavicon, isSelected);
                });
    }

    /**
     * Sets the favicon drawable for the tab.
     *
     * @param favicon The {@link TabFavicon} to display.
     * @param isSelected Whether the tab is selected.
     */
    private void setFavicon(
            @org.chromium.build.annotations.Nullable TabFavicon favicon, boolean isSelected) {
        if (mFavicon == null) return;

        if (favicon == null) {
            mFavicon.setImageDrawable(null);
            return;
        }

        mFavicon.setImageDrawable(
                isSelected ? favicon.getSelectedDrawable() : favicon.getDefaultDrawable());
    }

    /**
     * Sets the title for the tab.
     *
     * @param title The title to display.
     */
    void setTitle(String title) {
        if (mTitle == null) return;
        mTitle.setText(title);
    }

    /**
     * Sets the trailing icon for the tab.
     *
     * @param drawable The drawable to display.
     */
    void setTrailingIcon(@Nullable Drawable drawable) {
        if (mTrailingIcon == null) return;
        mTrailingIcon.setImageDrawable(drawable);
    }

    /**
     * Sets the size of the tab grid card.
     *
     * @param size The {@link Size} of the tab grid card.
     */
    void setGridCardSize(@Nullable Size size) {
        if (size == null) return;

        updateHeight(size.getHeight());

        int targetWidth = size.getWidth();
        int startWidth = getWidth();

        // If the view is not laid out yet or width is the same, just set the width.
        if (startWidth == 0 || startWidth == targetWidth) {
            updateWidth(targetWidth);
            return;
        }

        ValueAnimator animator = ValueAnimator.ofInt(startWidth, targetWidth);
        animator.setDuration(WIDTH_ANIMATION_DURATION_MS);
        animator.setInterpolator(STANDARD_INTERPOLATOR);
        animator.addUpdateListener(
                animation -> {
                    updateWidth((int) animation.getAnimatedValue());
                });
        mWidthAnimationHandler.startAnimation(animator);
    }

    AnimationHandler getWidthAnimationHandlerForTesting() {
        return mWidthAnimationHandler;
    }

    private void updateWidth(int width) {
        if (width <= 0) return;
        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        if (layoutParams.width == width) return;
        layoutParams.width = width;
        setLayoutParams(layoutParams);
    }

    private void updateHeight(int height) {
        if (height <= 0) return;
        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        if (layoutParams.width == height) return;
        layoutParams.height = height;
        setLayoutParams(layoutParams);
    }

    /**
     * Sets the selected state for the tab.
     *
     * @param isSelected Whether the tab is selected.
     * @param isIncognito Whether the tab is incognito.
     */
    void setSelected(boolean isSelected, boolean isIncognito) {
        Context context = getContext();
        getBackground().mutate();

        final @ColorInt int backgroundColor =
                TabCardThemeUtil.getCardViewBackgroundColor(
                        context, isIncognito, isSelected, /* colorId= */ null);
        ViewCompat.setBackgroundTintList(
                this,
                TabCardThemeUtil.getCardViewBackgroundColorStateList(
                        context, isIncognito, backgroundColor));

        if (mTitle != null) {
            mTitle.setTextColor(
                    TabCardThemeUtil.getTitleTextColor(
                            context, isIncognito, isSelected, /* colorId= */ null));
        }

        if (mTrailingIcon != null) {
            ImageViewCompat.setImageTintList(
                    mTrailingIcon,
                    TabCardThemeUtil.getActionButtonTintList(
                            context, isIncognito, isSelected, /* colorId= */ null));
        }
    }
}
