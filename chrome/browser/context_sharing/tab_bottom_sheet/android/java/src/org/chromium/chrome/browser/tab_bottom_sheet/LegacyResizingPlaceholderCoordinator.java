// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.context_sharing.R;

/** Legacy coordinator managing the spark resizing placeholder view. */
@NullMarked
public class LegacyResizingPlaceholderCoordinator implements ResizingPlaceholderCoordinator {
    private final View mResizingPlaceholder;
    private final View mResizingContent;
    private final @Px int mResizingFadeOffset;
    private final @Px int mMinHeight;

    /**
     * Constructs a new {@link LegacyResizingPlaceholderCoordinator}.
     *
     * @param context The Android context.
     * @param backgroundColor The background color of the placeholder.
     */
    public LegacyResizingPlaceholderCoordinator(Context context, @ColorInt int backgroundColor) {
        mResizingPlaceholder =
                LayoutInflater.from(context).inflate(R.layout.tab_bottom_sheet_resizing_view, null);
        mResizingContent =
                mResizingPlaceholder.findViewById(R.id.tab_bottom_sheet_resizing_content);
        Resources res = context.getResources();
        mResizingFadeOffset =
                res.getDimensionPixelSize(R.dimen.tab_bottom_sheet_resizing_fade_offset);
        mMinHeight = res.getDimensionPixelSize(R.dimen.tab_bottom_sheet_peek_height_total);

        mResizingPlaceholder.setVisibility(View.INVISIBLE);
        ColorDrawable background = new ColorDrawable();
        background.setColor(backgroundColor);
        mResizingPlaceholder.setBackground(background);
    }

    @Override
    public View getView() {
        return mResizingPlaceholder;
    }

    @Override
    public void updateVisibleHeight(@Px int visibleHeight) {
        ViewGroup.LayoutParams params = mResizingPlaceholder.getLayoutParams();
        if (params != null && params.height != visibleHeight) {
            params.height = visibleHeight;
            mResizingPlaceholder.setLayoutParams(params);
        }

        float minHeight = mMinHeight;
        int contentHeight = mResizingContent.getMeasuredHeight();
        float maxHeight = contentHeight + mResizingFadeOffset;

        float alpha;
        if (visibleHeight <= minHeight) {
            alpha = 0.0f;
        } else if (maxHeight <= minHeight || visibleHeight >= maxHeight) {
            alpha = 1.0f;
        } else {
            alpha = (visibleHeight - minHeight) / (maxHeight - minHeight);
        }
        if (mResizingContent.getAlpha() != alpha) {
            mResizingContent.setAlpha(alpha);
        }
    }

    @Override
    public void destroy() {}

    View getResizingContentForTesting() {
        return mResizingContent;
    }
}
